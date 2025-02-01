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

use crate::{connection::connection::Connection, dir::Directory, file::File, filter::Filter};
use gtk::{
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_none, ToGlibPtr},
    },
    prelude::*,
};
use std::{collections::HashSet, ops::ControlFlow, path::Path};

pub mod ffi {
    use super::*;
    use crate::{connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir};
    use gtk::{
        ffi::GtkTreeView,
        glib::ffi::{GList, GType},
    };
    use std::ffi::{c_char, c_void};

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

        pub fn gnome_cmd_file_list_toggle_with_pattern(
            fl: *mut GnomeCmdFileList,
            pattern: *mut c_void,
            mode: gboolean,
        );
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

impl FileList {
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
        unsafe {
            ffi::gnome_cmd_file_list_toggle_with_pattern(
                self.to_glib_none().0,
                pattern.as_ptr(),
                if mode { 1 } else { 0 },
            )
        }
    }

    pub fn goto_directory(&self, dir: &Path) {
        unsafe {
            ffi::gnome_cmd_file_list_goto_directory(self.to_glib_none().0, dir.to_glib_none().0)
        }
    }

    pub fn traverse_files<T>(
        &self,
        visitor: impl Fn(&File, &gtk::TreeIter, &gtk::ListStore) -> ControlFlow<T>,
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

    pub fn restore_selection(&self) {
        // TODO: implement
    }
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
