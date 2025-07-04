/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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
    data::{FiltersOptions, FiltersOptionsRead, GeneralOptions, GeneralOptionsRead},
    dialogs::rename_popover::show_rename_popover,
    dir::Directory,
    file::{ffi::GnomeCmdFile, File},
    file_list::{list::ffi::GnomeCmdFileList, popup::file_popup_menu},
    filter::{fnmatch, Filter},
    libgcmd::file_descriptor::FileDescriptorExt,
    main_win::MainWindow,
    tags::tags::FileMetadataService,
    types::{ExtensionDisplayMode, SizeDisplayMode},
    utils::{size_to_string, ErrorMessage},
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gdk::{self, ffi::GdkRectangle},
    gio,
    glib::{
        self,
        ffi::{gboolean, GType},
        translate::{
            from_glib, from_glib_borrow, from_glib_none, Borrowed, FromGlib, IntoGlib, ToGlibPtr,
        },
    },
    prelude::*,
    subclass::prelude::*,
};
use std::{
    collections::HashSet,
    ffi::{c_char, c_int},
    ops::ControlFlow,
    path::Path,
};

mod imp {
    use super::*;
    use crate::{
        data::ColorOptions,
        tags::tags::FileMetadataService,
        types::{
            ExtensionDisplayMode, GraphicalLayoutMode, LeftMouseButtonMode, MiddleMouseButtonMode,
            PermissionDisplayMode, QuickSearchShortcut, RightMouseButtonMode,
        },
    };
    use std::{
        cell::{Cell, OnceCell, RefCell},
        sync::OnceLock,
    };

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::FileList)]
    pub struct FileList {
        pub quick_search: glib::WeakRef<QuickSearch>,
        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,

        #[property(get, set)]
        pub font_name: RefCell<String>,

        #[property(get, set)]
        pub row_height: Cell<u32>,

        #[property(get, set, builder(ExtensionDisplayMode::default()))]
        pub extension_display_mode: Cell<ExtensionDisplayMode>,

        #[property(get, set, builder(GraphicalLayoutMode::default()))]
        graphical_layout_mode: Cell<GraphicalLayoutMode>,

        #[property(get, set, builder(SizeDisplayMode::default()))]
        size_display_mode: Cell<SizeDisplayMode>,

        #[property(get, set, builder(PermissionDisplayMode::default()))]
        permissions_display_mode: Cell<PermissionDisplayMode>,

        #[property(get, set)]
        date_display_format: RefCell<String>,

        #[property(get, set)]
        pub use_ls_colors: Cell<bool>,

        #[property(get, set, default = true)]
        case_sensitive: Cell<bool>,

        #[property(get, set)]
        symbolic_links_as_regular_files: Cell<bool>,

        #[property(get, set, builder(LeftMouseButtonMode::default()))]
        left_mouse_button_mode: Cell<LeftMouseButtonMode>,

        #[property(get, set, builder(MiddleMouseButtonMode::default()))]
        middle_mouse_button_mode: Cell<MiddleMouseButtonMode>,

        #[property(get, set, builder(RightMouseButtonMode::default()))]
        right_mouse_button_mode: Cell<RightMouseButtonMode>,

        #[property(get, set, default = true)]
        left_mouse_button_unselects: Cell<bool>,

        #[property(get, set, default = true)]
        select_dirs: Cell<bool>,

        #[property(get, set, builder(QuickSearchShortcut::default()))]
        quick_search_shortcut: Cell<QuickSearchShortcut>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileList {
        const NAME: &'static str = "GnomeCmdFileList";
        type Type = super::FileList;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileList {
        fn constructed(&self) {
            self.parent_constructed();

            let fl = self.obj();
            fl.set_layout_manager(Some(gtk::BinLayout::new()));

            unsafe {
                ffi::gnome_cmd_file_list_init(fl.to_glib_none().0);
            }

            let general_options = GeneralOptions::new();
            general_options
                .0
                .bind("list-font", &*fl, "font-name")
                .build();
            general_options
                .0
                .bind("list-row-height", &*fl, "row-height")
                .build();
            general_options
                .0
                .bind("extension-display-mode", &*fl, "extension-display-mode")
                .build();
            general_options
                .0
                .bind("graphical-layout-mode", &*fl, "graphical-layout-mode")
                .build();
            general_options
                .0
                .bind("size-display-mode", &*fl, "size-display-mode")
                .build();
            general_options
                .0
                .bind("perm-display-mode", &*fl, "permissions-display-mode")
                .build();
            general_options
                .0
                .bind("date-disp-format", &*fl, "date-display-format")
                .build();
            general_options
                .0
                .bind("case-sensitive", &*fl, "case-sensitive")
                .build();
            general_options
                .0
                .bind(
                    "symbolic-links-as-regular-files",
                    &*fl,
                    "symbolic-links-as-regular-files",
                )
                .build();
            general_options
                .0
                .bind("clicks-to-open-item", &*fl, "left-mouse-button-mode")
                .build();
            general_options
                .0
                .bind("middle-mouse-btn-mode", &*fl, "middle-mouse-button-mode")
                .build();
            general_options
                .0
                .bind("right-mouse-btn-mode", &*fl, "right-mouse-button-mode")
                .build();
            general_options
                .0
                .bind(
                    "left-mouse-btn-unselects",
                    &*fl,
                    "left-mouse-button-unselects",
                )
                .build();
            general_options
                .0
                .bind("select-dirs", &*fl, "select-dirs")
                .build();
            general_options
                .0
                .bind("quick-search", &*fl, "quick-search-shortcut")
                .build();

            for (column, key) in fl.tree_view().columns().iter().zip([
                "column-width-icon",
                "column-width-name",
                "column-width-ext",
                "column-width-dir",
                "column-width-size",
                "column-width-date",
                "column-width-perm",
                "column-width-owner",
                "column-width-group",
            ]) {
                general_options.0.bind(key, column, "fixed-width").build();
            }

            let color_options = ColorOptions::new();
            color_options
                .0
                .bind("use-ls-colors", &*fl, "use-ls-colors")
                .build();
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
            unsafe {
                ffi::gnome_cmd_file_list_finalize(self.obj().to_glib_none().0);
            }
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    // A file in the list was clicked
                    glib::subclass::Signal::builder("file-clicked")
                        .param_types([glib::Pointer::static_type()]) // GnomeCmdFileListButtonEvent
                        .build(),
                    // A file in the list has been clicked and mouse button has been released
                    glib::subclass::Signal::builder("file-released")
                        .param_types([glib::Pointer::static_type()]) // GnomeCmdFileListButtonEvent
                        .build(),
                    // The file list widget was clicked
                    glib::subclass::Signal::builder("list-clicked")
                        .param_types([u32::static_type(), Option::<File>::static_type()])
                        .build(),
                    // The visible content of the file list has changed (files have been: selected, created, deleted or modified)
                    glib::subclass::Signal::builder("files-changed").build(),
                    // The current directory has been changed
                    glib::subclass::Signal::builder("dir-changed")
                        .param_types([Directory::static_type()])
                        .build(),
                    // The current connection has been changed
                    glib::subclass::Signal::builder("con-changed")
                        .param_types([Connection::static_type()])
                        .build(),
                    // Column's width was changed
                    glib::subclass::Signal::builder("resize-column")
                        .param_types([u32::static_type(), gtk::TreeViewColumn::static_type()])
                        .build(),
                    // A file in the list has been activated for opening
                    glib::subclass::Signal::builder("file-activated")
                        .param_types([File::static_type()])
                        .build(),
                    // A text to be added to a command line
                    glib::subclass::Signal::builder("cmdline-append")
                        .param_types([String::static_type()])
                        .build(),
                    // Execute a command in a command line
                    glib::subclass::Signal::builder("cmdline-execute")
                        .return_type::<bool>()
                        .build(),
                ]
            })
        }
    }

    impl WidgetImpl for FileList {
        fn grab_focus(&self) -> bool {
            self.obj().tree_view().grab_focus()
        }
    }
}

pub mod ffi {
    use super::*;
    use crate::{connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir};
    use gtk::{ffi::GtkTreeView, glib::ffi::GList};

    pub type GnomeCmdFileList = <super::FileList as glib::object::ObjectType>::GlibType;

    extern "C" {
        pub fn gnome_cmd_file_list_init(fl: *mut GnomeCmdFileList);
        pub fn gnome_cmd_file_list_finalize(fl: *mut GnomeCmdFileList);

        pub fn gnome_cmd_file_list_get_tree_view(fl: *mut GnomeCmdFileList) -> *mut GtkTreeView;

        pub fn gnome_cmd_file_list_get_selected_files(fl: *mut GnomeCmdFileList) -> *mut GList;

        pub fn gnome_cmd_file_list_get_connection(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_file_list_get_directory(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdDir;
        pub fn gnome_cmd_file_list_set_directory(fl: *mut GnomeCmdFileList, dir: *mut GnomeCmdDir);

        pub fn gnome_cmd_file_list_get_sort_column(fl: *mut GnomeCmdFileList) -> c_int;
        pub fn gnome_cmd_file_list_get_sort_order(fl: *mut GnomeCmdFileList) -> c_int;
        pub fn gnome_cmd_file_list_set_sorting(
            fl: *mut GnomeCmdFileList,
            sort_col: c_int,
            sort_order: c_int,
        );

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

        pub fn gnome_cmd_file_list_update_style(fl: *mut GnomeCmdFileList);

        pub fn gnome_cmd_file_list_invalidate_tree_size(fl: *mut GnomeCmdFileList);

        pub fn gnome_cmd_file_list_show_files(fl: *mut GnomeCmdFileList, dir: *mut GnomeCmdDir);

        pub fn gnome_cmd_file_list_show_column(
            fl: *mut GnomeCmdFileList,
            col: c_int,
            value: gboolean,
        );
    }
}

glib::wrapper! {
    pub struct FileList(ObjectSubclass<imp::FileList>)
        @extends gtk::Widget;
}

#[allow(non_camel_case_types)]
#[derive(Clone, Copy, strum::FromRepr)]
#[repr(C)]
pub enum ColumnID {
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
    pub fn new(file_metadata_service: &FileMetadataService) -> Self {
        glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build()
    }

    fn tree_view(&self) -> gtk::TreeView {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_tree_view(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn size(&self) -> usize {
        self.tree_view()
            .model()
            .and_then(|m| m.iter_n_children(None).try_into().ok())
            .unwrap_or_default()
    }

    pub fn clear(&self) {
        if let Some(store) = self.tree_view().model().and_downcast::<gtk::ListStore>() {
            store.clear();
        }
    }

    pub fn visible_files(&self) -> glib::List<File> {
        let mut files = glib::List::new();
        let _ = self.traverse_files::<()>(|file, _iter, _store| {
            files.push_back(file.clone());
            ControlFlow::Continue(())
        });
        files
    }

    pub fn selected_files(&self) -> glib::List<File> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_file_list_get_selected_files(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn sort_column(&self) -> ColumnID {
        unsafe { ffi::gnome_cmd_file_list_get_sort_column(self.to_glib_none().0) }
            .try_into()
            .ok()
            .and_then(ColumnID::from_repr)
            .unwrap_or(ColumnID::COLUMN_NAME)
    }

    pub fn sort_order(&self) -> gtk::SortType {
        unsafe {
            from_glib(ffi::gnome_cmd_file_list_get_sort_order(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn set_sorting(&self, column: ColumnID, order: gtk::SortType) {
        unsafe {
            ffi::gnome_cmd_file_list_set_sorting(
                self.to_glib_none().0,
                column as _,
                order.into_glib(),
            )
        }
    }

    pub fn show_column(&self, col: ColumnID, value: bool) {
        unsafe {
            ffi::gnome_cmd_file_list_show_column(
                self.to_glib_none().0,
                col as c_int,
                value as gboolean,
            )
        }
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

    pub fn set_directory(&self, directory: &Directory) {
        unsafe {
            ffi::gnome_cmd_file_list_set_directory(
                self.to_glib_none().0,
                directory.to_glib_none().0,
            )
        }
    }

    pub fn reload(&self) {
        unsafe { ffi::gnome_cmd_file_list_reload(self.to_glib_none().0) }
    }

    pub fn set_connection(&self, connection: &impl IsA<Connection>, start_dir: Option<&Directory>) {
        unsafe {
            ffi::gnome_cmd_file_list_set_connection(
                self.to_glib_none().0,
                connection.as_ref().to_glib_none().0,
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

    pub fn select_row(&self, row: Option<gtk::TreeIter>) {
        let tree_view = self.tree_view();
        let Some(store) = tree_view.model().and_downcast::<gtk::ListStore>() else {
            return;
        };
        if let Some(row) = row.or_else(|| store.iter_first()) {
            self.focus_file_at_row(&row);
        }
    }

    pub fn toggle_with_pattern(&self, pattern: &Filter, mode: bool) {
        let select_dirs = self.select_dirs();
        let _ = self.traverse_files::<()>(|file, iter, store| {
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
        let _ = self.traverse_files::<()>(|file, iter, store| {
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
        let _ = self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot() && file.extension().as_ref() == Some(&ext) {
                store.set(iter, &[(DataColumns::DATA_COLUMN_SELECTED as u32, &select)]);
            }
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    pub fn invert_selection(&self) {
        let select_dirs = self.select_dirs();
        let _ = self.traverse_files::<()>(|file, iter, store| {
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
        let _ = self.traverse_files::<()>(|file, iter, store| {
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

        let sentence1 = gettext("A total of {selected_bytes} has been selected.").replace(
            "{selected_bytes}",
            &size_to_string(stats.selected.bytes, mode),
        );

        let sentence2 = ngettext(
            "{selected_files} file selected.",
            "{selected_files} files selected.",
            stats.selected.files as u32,
        )
        .replace("{selected_files}", &stats.selected.files.to_string());

        let sentence3 = ngettext(
            "{selected_dirs} directory selected.",
            "{selected_dirs} directories selected.",
            stats.selected.directories as u32,
        )
        .replace("{selected_dirs}", &stats.selected.directories.to_string());

        format!("{sentence1} {sentence2} {sentence3}")
    }

    pub async fn show_rename_dialog(&self) {
        let Some(window) = self.root().and_downcast::<gtk::Window>() else {
            eprintln!("No window");
            return;
        };
        let Some(file) = self.selected_file() else {
            return;
        };
        let Some(rect) = self.focus_row_coordinates() else {
            eprintln!("Selected file is not visible");
            return;
        };

        if let Some(new_name) = show_rename_popover(&file.get_name(), self, &rect).await {
            match file.rename(&new_name) {
                Ok(_) => {
                    self.focus_file(&Path::new(&new_name), true);
                    self.grab_focus();
                }
                Err(error) => {
                    ErrorMessage::with_error(
                        gettext("Cannot rename a file to {new_name}")
                            .replace("{new_name}", &new_name),
                        &error,
                    )
                    .show(&window)
                    .await;
                }
            }
            self.grab_focus();
        }
    }

    pub fn show_quick_search(&self, key: Option<gdk::Key>) {
        let popup_is_visible = self
            .imp()
            .quick_search
            .upgrade()
            .map(|popup| popup.is_visible())
            .unwrap_or_default();

        if !popup_is_visible {
            let popup = QuickSearch::new(self);
            self.imp().quick_search.set(Some(&popup));
            popup.popup();

            if let Some(initial_text) = key.and_then(|k| k.to_unicode()).map(|c| c.to_string()) {
                popup.set_text(&initial_text);
            }
        }
    }

    pub fn update_style(&self) {
        unsafe { ffi::gnome_cmd_file_list_update_style(self.to_glib_none().0) }
    }

    pub fn invalidate_tree_size(&self) {
        unsafe { ffi::gnome_cmd_file_list_invalidate_tree_size(self.to_glib_none().0) }
    }

    pub fn show_files(&self, dir: &Directory) {
        unsafe { ffi::gnome_cmd_file_list_show_files(self.to_glib_none().0, dir.to_glib_none().0) }
    }

    pub fn connect_con_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &Connection) + 'static,
    {
        self.connect_closure(
            "con-changed",
            false,
            glib::closure_local!(move |this, connection| (callback)(this, connection)),
        )
    }

    pub fn connect_dir_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &Directory) + 'static,
    {
        self.connect_closure(
            "dir-changed",
            false,
            glib::closure_local!(move |this, directory| (callback)(this, directory)),
        )
    }

    pub fn connect_files_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self) + 'static,
    {
        self.connect_closure(
            "files-changed",
            false,
            glib::closure_local!(move |this| (callback)(this)),
        )
    }

    pub fn connect_file_activated<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &File) + 'static,
    {
        self.connect_closure(
            "file-activated",
            false,
            glib::closure_local!(move |this, file| (callback)(this, file)),
        )
    }

    pub fn connect_cmdline_append<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &str) + 'static,
    {
        self.connect_closure(
            "cmdline-append",
            false,
            glib::closure_local!(move |this, text| (callback)(this, text)),
        )
    }

    pub fn connect_cmdline_execute<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self) -> bool + 'static,
    {
        self.connect_closure(
            "cmdline-execute",
            false,
            glib::closure_local!(move |this| (callback)(this)),
        )
    }

    pub fn connect_list_clicked<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, u32, Option<File>) + 'static,
    {
        self.connect_closure(
            "list-clicked",
            false,
            glib::closure_local!(move |this, button, file| (callback)(this, button, file)),
        )
    }

    fn focus_row_coordinates(&self) -> Option<gdk::Rectangle> {
        let options = GeneralOptions::new();

        let view = self.tree_view();
        let path = gtk::prelude::TreeViewExt::cursor(&view).0?;
        let name_rect = view.cell_area(
            Some(&path),
            Some(&view.column(ColumnID::COLUMN_NAME as i32)?),
        );
        let mut rect = if options.extension_display_mode() != ExtensionDisplayMode::Both {
            let ext_rect = view.cell_area(
                Some(&path),
                Some(&view.column(ColumnID::COLUMN_EXT as i32)?),
            );
            name_rect.union(&ext_rect)
        } else {
            name_rect
        };
        let (x, y) = view.convert_bin_window_to_widget_coords(rect.x(), rect.y());
        rect.set_x(x);
        rect.set_y(y);
        Some(rect)
    }

    fn show_file_popup(&self, point_to: Option<&gdk::Rectangle>) {
        let Some(main_win) = self.root().and_downcast::<MainWindow>() else {
            return;
        };
        let Some(menu) = file_popup_menu(&main_win, self) else {
            return;
        };

        let popover = gtk::PopoverMenu::from_model(Some(&menu));
        popover.set_parent(self);
        popover.set_position(gtk::PositionType::Bottom);
        popover.set_flags(gtk::PopoverMenuFlags::NESTED);
        popover.set_pointing_to(
            point_to
                .cloned()
                .or_else(|| self.focus_row_coordinates())
                .as_ref(),
        );
        popover.present();
        popover.popup();
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
pub extern "C" fn gnome_cmd_file_list_get_type() -> GType {
    FileList::static_type().into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_invert_selection(fl: *mut ffi::GnomeCmdFileList) {
    let fl: FileList = unsafe { from_glib_none(fl) };
    fl.invert_selection();
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
pub extern "C" fn gnome_cmd_file_list_show_quicksearch(
    fl: *mut ffi::GnomeCmdFileList,
    keyval: u32,
) {
    let fl: FileList = unsafe { from_glib_none(fl) };
    let key: gdk::Key = unsafe { gdk::Key::from_glib(keyval) };
    fl.show_quick_search(Some(key));
}

fn file_is_wanted(file: &File, options: &dyn FiltersOptionsRead) -> bool {
    match file.file().query_info(
        "standard::*",
        gio::FileQueryInfoFlags::NOFOLLOW_SYMLINKS,
        gio::Cancellable::NONE,
    ) {
        Ok(file_info) => {
            let basename = file.file().basename();
            let Some(basename) = basename.as_ref().and_then(|b| b.to_str()) else {
                return false;
            };

            if basename == "." {
                return false;
            }
            if file.is_dotdot() {
                return false;
            }
            let hide = match file_info.file_type() {
                gio::FileType::Unknown => options.hide_unknown(),
                gio::FileType::Regular => options.hide_regular(),
                gio::FileType::Directory => options.hide_directory(),
                gio::FileType::SymbolicLink => options.hide_symlink(),
                gio::FileType::Special => options.hide_special(),
                gio::FileType::Shortcut => options.hide_shortcut(),
                gio::FileType::Mountable => options.hide_mountable(),
                _ => false,
            };
            if hide {
                return false;
            }
            if options.hide_symlink() && file_info.is_symlink() {
                return false;
            }
            if options.hide_hidden() && file_info.is_hidden() {
                return false;
            }
            if options.hide_virtual() && file_info.boolean(gio::FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL)
            {
                return false;
            }
            if options.hide_volatile()
                && file_info.boolean(gio::FILE_ATTRIBUTE_STANDARD_IS_VOLATILE)
            {
                return false;
            }
            if options.hide_backup() && matches_pattern(basename, &options.backup_pattern()) {
                return false;
            }
            true
        }
        Err(error) => {
            eprintln!(
                "file_is_wanted: retrieving file info for {} failed: {}",
                file.file().uri(),
                error.message()
            );
            true
        }
    }
}

fn matches_pattern(file: &str, patterns: &str) -> bool {
    patterns
        .split(';')
        .any(|pattern| fnmatch(pattern, file, false))
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_is_wanted(f: *mut GnomeCmdFile) -> gboolean {
    let f: Borrowed<File> = unsafe { from_glib_borrow(f) };
    let options = FiltersOptions::new();
    file_is_wanted(&f, &options).into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_show_file_popup(
    f: *mut GnomeCmdFileList,
    point_to: *mut GdkRectangle,
) {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(f) };
    let point_to: Borrowed<Option<gdk::Rectangle>> = unsafe { from_glib_borrow(point_to) };
    fl.show_file_popup((&*point_to).as_ref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_show_rename_dialog(f: *mut GnomeCmdFileList) {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(f) };
    glib::spawn_future_local(async move {
        fl.show_rename_dialog().await;
    });
}
