/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */

use super::quick_search::QuickSearch;
use crate::{
    connection::connection::Connection,
    data::{GeneralOptions, GeneralOptionsRead},
    dir::Directory,
    file::File,
    filter::Filter,
    libgcmd::file_descriptor::FileDescriptorExt,
    types::SizeDisplayMode,
    utils::size_to_string,
};
use gettextrs::ngettext;
use gtk::{
    gdk, gio,
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_none, FromGlib, ToGlibPtr},
    },
    prelude::*,
};
use std::{collections::HashSet, ffi::c_char, ops::ControlFlow, path::Path, sync::LazyLock};

pub mod ffi {
    use super::*;
    use crate::{connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir};
    use gtk::{
        ffi::GtkTreeView,
        glib::ffi::{GList, GType},
    };
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdFileList {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_file_list_get_type() -> GType;

        pub fn gnome_cmd_file_list_get_tree_view(fl: *mut GnomeCmdFileList) -> *mut GtkTreeView;

        pub fn gnome_cmd_file_list_get_visible_files(fl: *mut GnomeCmdFileList) -> *mut GList;

        pub fn gnome_cmd_file_list_get_selected_files(fl: *mut GnomeCmdFileList) -> *mut GList;

        pub fn gnome_cmd_file_list_get_cwd(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_file_list_get_connection(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_file_list_get_directory(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_file_list_is_locked(fl: *mut GnomeCmdFileList) -> gboolean;

        pub fn gnome_cmd_file_list_reload(fl: *mut GnomeCmdFileList);

        pub fn gnome_cmd_file_list_set_connection(
            fl: *mut GnomeCmdFileList,
            con: *mut GnomeCmdCon,
            start_dir: *mut GnomeCmdDir,
        );

        pub fn gnome_cmd_file_list_focus_file(
            fl: *mut GnomeCmdFileList,
            focus_file: *const c_char,
            scroll_to_file: gboolean,
        );

        pub fn gnome_cmd_file_list_goto_directory(fl: *mut GnomeCmdFileList, dir: *const c_char);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileListClass {
        pub parent_class: gtk::ffi::GtkWidgetClass,
    }
}

glib::wrapper! {
    pub struct FileList(Object<ffi::GnomeCmdFileList, ffi::GnomeCmdFileListClass>)
        @extends gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_file_list_get_type(),
    }
}

#[allow(non_camel_case_types)]
enum ColumnID {
    COLUMN_ICON = 0,
    COLUMN_NAME,
    COLUMN_EXT,
    COLUMN_DIR,
    COLUMN_SIZE,
    COLUMN_DATE,
    COLUMN_PERM,
    COLUMN_OWNER,
    COLUMN_GROUP,
}

#[allow(non_camel_case_types)]
enum DataColumns {
    DATA_COLUMN_FILE = 9,
    DATA_COLUMN_ICON_NAME,
    DATA_COLUMN_SELECTED,
}

#[derive(Default)]
struct FileListPrivate {
    quick_search: glib::WeakRef<QuickSearch>,
}

impl FileList {
    fn private(&self) -> &mut FileListPrivate {
        static QUARK: LazyLock<glib::Quark> =
            LazyLock::new(|| glib::Quark::from_str("file-list-private"));

        unsafe {
            if let Some(mut private) = self.qdata::<FileListPrivate>(*QUARK) {
                private.as_mut()
            } else {
                self.set_qdata(*QUARK, FileListPrivate::default());
                self.qdata::<FileListPrivate>(*QUARK).unwrap().as_mut()
            }
        }
    }

    fn tree_view(&self) -> gtk::TreeView {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_tree_view(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn visible_files(&self) -> glib::List<File> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_file_list_get_visible_files(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn selected_files(&self) -> glib::List<File> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_file_list_get_selected_files(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn cwd(&self) -> Option<Directory> {
        unsafe { from_glib_none(ffi::gnome_cmd_file_list_get_cwd(self.to_glib_none().0)) }
    }

    pub fn is_locked(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_list_is_locked(self.to_glib_none().0) != 0 }
    }

    pub fn file_at_row(&self, iter: &gtk::TreeIter) -> Option<File> {
        self.tree_view()
            .model()?
            .get_value(iter, DataColumns::DATA_COLUMN_FILE as i32)
            .get()
            .ok()
    }

    pub fn focused_file_iter(&self) -> Option<gtk::TreeIter> {
        let tree_view = self.tree_view();
        let model = tree_view.model()?;
        let path = TreeViewExt::cursor(&tree_view).0?;
        let iter = model.iter(&path)?;
        Some(iter)
    }

    pub fn focused_file(&self) -> Option<File> {
        let iter = self.focused_file_iter()?;
        let file = self.file_at_row(&iter)?;
        Some(file)
    }

    pub fn selected_file(&self) -> Option<File> {
        self.focused_file().filter(|f| !f.is_dotdot())
    }

    pub fn connection(&self) -> Option<Connection> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_connection(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn directory(&self) -> Option<Directory> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_directory(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn reload(&self) {
        unsafe { ffi::gnome_cmd_file_list_reload(self.to_glib_none().0) }
    }

    pub fn set_connection(&self, connection: &Connection, start_dir: Option<&Directory>) {
        unsafe {
            ffi::gnome_cmd_file_list_set_connection(
                self.to_glib_none().0,
                connection.to_glib_none().0,
                start_dir.to_glib_none().0,
            )
        }
    }

    pub(super) fn get_row_from_file(&self, f: &File) -> Option<gtk::TreeIter> {
        let result = self.traverse_files::<gtk::TreeIter>(|file, iter, _store| {
            if file == f {
                ControlFlow::Break(iter.clone())
            } else {
                ControlFlow::Continue(())
            }
        });
        match result {
            ControlFlow::Break(iter) => Some(iter),
            ControlFlow::Continue(()) => None,
        }
    }

    pub(super) fn focus_file_at_row(&self, row: &gtk::TreeIter) {
        let tree_view = self.tree_view();
        let Some(store) = tree_view.model().and_downcast::<gtk::ListStore>() else {
            return;
        };

        let path = store.path(row);
        gtk::prelude::TreeViewExt::set_cursor(&tree_view, &path, None, false);
    }

    pub fn focus_file(&self, focus_file: &Path, scroll_to_file: bool) {
        unsafe {
            ffi::gnome_cmd_file_list_focus_file(
                self.to_glib_none().0,
                focus_file.to_glib_none().0,
                if scroll_to_file { 1 } else { 0 },
            )
        }
    }

    pub fn toggle_with_pattern(&self, pattern: &Filter, mode: bool) {
        let options = GeneralOptions::new();
        let select_dirs = options.select_dirs();
        self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot()
                && (select_dirs || file.downcast_ref::<Directory>().is_none())
                && pattern.matches(&file.file_info().display_name())
            {
                store.set(iter, &[(DataColumns::DATA_COLUMN_SELECTED as u32, &mode)]);
            }
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    pub fn goto_directory(&self, dir: &Path) {
        unsafe {
            ffi::gnome_cmd_file_list_goto_directory(self.to_glib_none().0, dir.to_glib_none().0)
        }
    }

    pub fn traverse_files<T>(
        &self,
        mut visitor: impl FnMut(&File, &gtk::TreeIter, &gtk::ListStore) -> ControlFlow<T>,
    ) -> ControlFlow<T> {
        let Some(store) = self.tree_view().model().and_downcast::<gtk::ListStore>() else {
            return ControlFlow::Continue(());
        };

        if let Some(iter) = store.iter_first() {
            loop {
                let file: File = store.get(&iter, DataColumns::DATA_COLUMN_FILE as i32);
                (visitor)(&file, &iter, &store)?;
                if !store.iter_next(&iter) {
                    break;
                }
            }
        }
        ControlFlow::Continue(())
    }

    fn emit_files_changed(&self) {
        self.emit_by_name::<()>("files-changed", &[]);
    }

    pub fn set_selected_files(&self, files: &HashSet<File>) {
        self.traverse_files::<()>(|file, iter, store| {
            let selected = files.contains(file);
            store.set(
                iter,
                &[(DataColumns::DATA_COLUMN_SELECTED as u32, &selected)],
            );
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    pub fn toggle_files_with_same_extension(&self, select: bool) {
        let Some(ext) = self.selected_file().and_then(|f| f.extension()) else {
            return;
        };
        self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot() && file.extension().as_ref() == Some(&ext) {
                store.set(iter, &[(DataColumns::DATA_COLUMN_SELECTED as u32, &select)]);
            }
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    pub fn invert_selection(&self, select_dirs: bool) {
        self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot() && (select_dirs || file.downcast_ref::<Directory>().is_none()) {
                let selected: bool = store.get(iter, DataColumns::DATA_COLUMN_SELECTED as i32);
                store.set(
                    iter,
                    &[(DataColumns::DATA_COLUMN_SELECTED as u32, &!selected)],
                );
            }
            ControlFlow::Continue(())
        });
    }

    pub fn restore_selection(&self) {
        // TODO: implement
    }

    pub fn stats(&self) -> FileListStats {
        let mut stats = FileListStats::default();
        self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot() {
                let info = file.file_info();
                let selected: bool = store.get(iter, DataColumns::DATA_COLUMN_SELECTED as i32);

                match info.file_type() {
                    gio::FileType::Directory => {
                        stats.total.directories += 1;
                        if selected {
                            stats.selected.directories += 1;
                        }
                        if let Some(size) = file.tree_size() {
                            stats.total.bytes += size;
                            if selected {
                                stats.selected.bytes += size;
                            }
                        }
                    }
                    gio::FileType::Regular => {
                        let size: u64 = info.size().try_into().unwrap_or_default();
                        stats.total.files += 1;
                        stats.total.bytes += size;
                        if selected {
                            stats.selected.files += 1;
                            stats.selected.bytes += size;
                        }
                    }
                    _ => {}
                }
            }
            ControlFlow::Continue(())
        });
        stats
    }

    pub fn stats_str(&self, mode: SizeDisplayMode) -> String {
        let stats = self.stats();

        let file_str = ngettext(
            "{selected_bytes} of {total_bytes} in {selected_files} of {total_files} file",
            "{selected_bytes} of {total_bytes} in {selected_files} of {total_files} files",
            stats.total.files as u32,
        )
        .replace(
            "{selected_bytes}",
            &size_to_string(stats.selected.bytes, mode),
        )
        .replace("{total_bytes}", &size_to_string(stats.total.bytes, mode))
        .replace("{selected_files}", &stats.selected.files.to_string())
        .replace("{total_files}", &stats.total.files.to_string());

        let info_str = ngettext(
            "{}, {selected_dirs} of {total_dirs} dir selected",
            "{}, {selected_dirs} of {total_dirs} dirs selected",
            stats.total.directories as u32,
        )
        .replace("{}", &file_str)
        .replace("{selected_dirs}", &stats.selected.directories.to_string())
        .replace("{total_dirs}", &stats.total.directories.to_string());

        info_str
    }

    pub fn show_quick_search(&self, key: Option<gdk::Key>) {
        let private = self.private();

        let popup_is_visible = private
            .quick_search
            .upgrade()
            .map(|popup| popup.is_visible())
            .unwrap_or_default();

        if !popup_is_visible {
            let popup = QuickSearch::new(self);
            private.quick_search.set(Some(&popup));
            popup.popup();

            if let Some(initial_text) = key.and_then(|k| k.to_unicode()).map(|c| c.to_string()) {
                popup.set_text(&initial_text);
            }
        }
    }
}

#[derive(Default)]
pub struct Stats {
    pub files: u64,
    pub directories: u64,
    pub bytes: u64,
}

#[derive(Default)]
pub struct FileListStats {
    pub total: Stats,
    pub selected: Stats,
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_invert_selection(fl: *mut ffi::GnomeCmdFileList) {
    let fl: FileList = unsafe { from_glib_none(fl) };
    let options = GeneralOptions::new();
    fl.invert_selection(options.select_dirs());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_restore_selection(fl: *mut ffi::GnomeCmdFileList) {
    let fl: FileList = unsafe { from_glib_none(fl) };
    fl.restore_selection();
}

#[no_mangle]
pub extern "C" fn toggle_files_with_same_extension(
    fl: *mut ffi::GnomeCmdFileList,
    select: gboolean,
) {
    let fl: FileList = unsafe { from_glib_none(fl) };
    fl.toggle_files_with_same_extension(select != 0);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_stats(fl: *mut ffi::GnomeCmdFileList) -> *mut c_char {
    let fl: FileList = unsafe { from_glib_none(fl) };
    let options = GeneralOptions::new();
    let stats = fl.stats_str(options.size_display_mode());
    stats.to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_show_quicksearch(
    fl: *mut ffi::GnomeCmdFileList,
    keyval: u32,
) {
    let fl: FileList = unsafe { from_glib_none(fl) };
    let key: gdk::Key = unsafe { gdk::Key::from_glib(keyval) };
    fl.show_quick_search(Some(key));
}
