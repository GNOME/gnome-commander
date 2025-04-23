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

use crate::{
    connection::{
        connection::{Connection, ConnectionExt},
        list::ConnectionList,
        remote::ConnectionRemote,
    },
    dir::Directory,
    file::File,
    file_list::list::{ColumnID, FileList},
    notebook_ext::{GnomeCmdNotebookExt, TabClick},
};
use gettextrs::gettext;
use gtk::{
    ffi::{GtkGestureClick, GtkNotebook},
    gdk, gio,
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_borrow, from_glib_none, Borrowed, IntoGlib, ToGlibPtr},
    },
    graphene,
    prelude::*,
};
use std::{
    collections::{BTreeSet, HashMap, HashSet},
    ffi::c_int,
    path::{Path, PathBuf},
};

pub mod ffi {
    use super::*;
    use crate::{dir::ffi::GnomeCmdDir, file_list::list::ffi::GnomeCmdFileList};
    use gtk::{
        ffi::GtkWidget,
        glib::ffi::{gboolean, GType},
    };

    #[repr(C)]
    pub struct GnomeCmdFileSelector {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_file_selector_get_type() -> GType;

        pub fn gnome_cmd_file_selector_file_list(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GnomeCmdFileList;

        pub fn gnome_cmd_file_selector_file_list_nth(
            fs: *mut GnomeCmdFileSelector,
            n: u32,
        ) -> *mut GnomeCmdFileList;

        pub fn gnome_cmd_file_selector_get_notebook(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GtkNotebook;

        pub fn gnome_cmd_file_selector_new_tab(fs: *mut GnomeCmdFileSelector) -> *const GtkWidget;

        pub fn gnome_cmd_file_selector_new_tab_with_dir(
            fs: *mut GnomeCmdFileSelector,
            dir: *mut GnomeCmdDir,
            activate: gboolean,
        ) -> *const GtkWidget;

        pub fn gnome_cmd_file_selector_new_tab_full(
            fs: *mut GnomeCmdFileSelector,
            dir: *mut GnomeCmdDir,
            sort_col: c_int,
            sort_order: c_int,
            locked: gboolean,
            activate: gboolean,
        ) -> *const GtkWidget;

        pub fn gnome_cmd_file_selector_close_tab(fs: *mut GnomeCmdFileSelector);

        pub fn gnome_cmd_file_selector_close_tab_nth(fs: *mut GnomeCmdFileSelector, n: u32);

        pub fn gnome_cmd_file_selector_tab_count(fs: *mut GnomeCmdFileSelector) -> u32;

        pub fn gnome_cmd_file_selector_is_active(fs: *mut GnomeCmdFileSelector) -> gboolean;
        pub fn gnome_cmd_file_selector_set_active(fs: *mut GnomeCmdFileSelector, active: gboolean);

        pub fn gnome_cmd_file_selector_is_tab_locked(
            fs: *mut GnomeCmdFileSelector,
            fl: *mut GnomeCmdFileList,
        ) -> gboolean;

        pub fn gnome_cmd_file_selector_set_tab_locked(
            fs: *mut GnomeCmdFileSelector,
            fl: *mut GnomeCmdFileList,
            lock: gboolean,
        );
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileSelectorClass {
        pub parent_class: gtk::ffi::GtkBoxClass,
    }
}

glib::wrapper! {
    pub struct FileSelector(Object<ffi::GnomeCmdFileSelector, ffi::GnomeCmdFileSelectorClass>)
        @extends gtk::Box, gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_file_selector_get_type(),
    }
}

impl FileSelector {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn file_list(&self) -> FileList {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_file_list(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn file_list_nth(&self, n: u32) -> FileList {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_file_list_nth(
                self.to_glib_none().0,
                n,
            ))
        }
    }

    fn notebook(&self) -> gtk::Notebook {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_get_notebook(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn new_tab(&self) -> gtk::Widget {
        unsafe { from_glib_none(ffi::gnome_cmd_file_selector_new_tab(self.to_glib_none().0)) }
    }

    pub fn new_tab_with_dir(&self, dir: &Directory, activate: bool) -> gtk::Widget {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_new_tab_with_dir(
                self.to_glib_none().0,
                dir.to_glib_none().0,
                activate as i32,
            ))
        }
    }

    pub fn new_tab_full(
        &self,
        dir: &Directory,
        sort_column: ColumnID,
        sort_order: gtk::SortType,
        locked: bool,
        activate: bool,
    ) -> gtk::Widget {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_new_tab_full(
                self.to_glib_none().0,
                dir.to_glib_none().0,
                sort_column as c_int,
                sort_order.into_glib(),
                locked as gboolean,
                activate as gboolean,
            ))
        }
    }

    pub fn close_tab(&self) {
        unsafe { ffi::gnome_cmd_file_selector_close_tab(self.to_glib_none().0) }
    }

    pub fn close_tab_nth(&self, n: u32) {
        unsafe { ffi::gnome_cmd_file_selector_close_tab_nth(self.to_glib_none().0, n) }
    }

    pub fn tab_count(&self) -> u32 {
        unsafe { ffi::gnome_cmd_file_selector_tab_count(self.to_glib_none().0) }
    }

    pub fn is_active(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_selector_is_active(self.to_glib_none().0) != 0 }
    }

    pub fn set_active(&self, active: bool) {
        unsafe {
            ffi::gnome_cmd_file_selector_set_active(self.to_glib_none().0, active as gboolean);
        }
    }

    pub fn is_tab_locked(&self, fl: &FileList) -> bool {
        unsafe {
            ffi::gnome_cmd_file_selector_is_tab_locked(self.to_glib_none().0, fl.to_glib_none().0)
                != 0
        }
    }

    pub fn set_tab_locked(&self, fl: &FileList, lock: bool) {
        unsafe {
            ffi::gnome_cmd_file_selector_set_tab_locked(
                self.to_glib_none().0,
                fl.to_glib_none().0,
                lock as gboolean,
            )
        }
    }

    pub fn is_current_tab_locked(&self) -> bool {
        self.is_tab_locked(&self.file_list())
    }

    pub fn goto_directory(&self, con: &Connection, path: &Path) {
        if self.file_list().connection().as_ref() == Some(&con) {
            if self.is_current_tab_locked() {
                let dir = Directory::new(con, con.create_path(path));
                self.new_tab_with_dir(&dir, true);
            } else {
                self.file_list().goto_directory(path);
            }
        } else {
            if con.is_open() {
                let dir = Directory::new(con, con.create_path(path));

                if self.is_current_tab_locked() {
                    self.new_tab_with_dir(&dir, true);
                } else {
                    self.file_list().set_connection(con, Some(&dir));
                }
            } else {
                con.set_base_path(con.create_path(path));
                if self.is_current_tab_locked() {
                    self.new_tab_with_dir(&con.default_dir().unwrap(), true);
                } else {
                    self.file_list().set_connection(con, None);
                }
            }
        }
    }

    pub fn close_all_tabs(&self) {
        let current = self.notebook().current_page();
        for index in (0..self.tab_count())
            .rev()
            .filter(|i| Some(*i) != current)
            .filter(|i| !self.is_tab_locked(&self.file_list_nth(*i)))
        {
            self.close_tab_nth(index);
        }
    }

    pub fn close_duplicate_tabs(&self) {
        let mut dirs = HashMap::<Directory, BTreeSet<u32>>::new();
        for index in 0..self.tab_count() {
            let file_list = self.file_list_nth(index);
            if !self.is_tab_locked(&file_list) {
                if let Some(directory) = file_list.directory() {
                    dirs.entry(directory).or_default().insert(index);
                }
            }
        }

        let mut duplicates = BTreeSet::new();
        if let Some(mut indexes) = self.file_list().directory().and_then(|d| dirs.remove(&d)) {
            if let Some(current_index) = self.notebook().current_page() {
                indexes.remove(&current_index);
            }
            duplicates.extend(indexes);
        }

        for indexes in dirs.values() {
            duplicates.extend(indexes.into_iter().skip(1));
        }

        for index in duplicates.into_iter().rev() {
            self.close_tab_nth(index);
        }
    }

    fn goto(&self, connection: &Connection, dir: &str) {
        if self.is_current_tab_locked() {
            self.new_tab_with_dir(
                &Directory::new(connection, connection.create_path(&Path::new(&dir))),
                true,
            );
        } else {
            self.goto_directory(connection, &Path::new(&dir));
        }
    }

    pub fn first(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.first() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn back(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.back() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn forward(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.forward() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn last(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.last() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn can_back(&self) -> bool {
        let Some(connection) = self.file_list().connection() else {
            return false;
        };
        connection.dir_history().can_back()
    }

    pub fn can_forward(&self) -> bool {
        let Some(connection) = self.file_list().connection() else {
            return false;
        };
        connection.dir_history().can_forward()
    }

    pub fn save_tabs(&self, save_all_tabs: bool, save_current: bool) -> Vec<TabVariant> {
        self.notebook()
            .pages()
            .iter::<gtk::NotebookPage>()
            .flatten()
            .map(|p| p.child())
            .filter_map(|c| c.downcast::<FileList>().ok())
            .filter(|file_list| {
                save_all_tabs
                    || (save_current && *file_list == self.file_list())
                    || self.is_tab_locked(file_list)
            })
            .filter_map(|file_list| {
                let directory = file_list.directory()?;
                let uri = directory.upcast_ref::<File>().get_uri_str()?;
                Some(TabVariant {
                    uri,
                    file_felector_id: 0,
                    sort_column: file_list.sort_column() as u8,
                    sort_order: file_list.sort_order() != gtk::SortType::Ascending,
                    locked: self.is_tab_locked(&file_list),
                })
            })
            .collect()
    }

    pub fn open_tabs(&self, tabs: Vec<TabVariant>) {
        let connection_list = ConnectionList::get();

        let mut visited: HashSet<TabVariant> = Default::default();
        for mut stored_tab in tabs {
            stored_tab.file_felector_id = 0;
            if visited.contains(&stored_tab) {
                continue;
            }
            if let Some(directory) = restore_directory(&connection_list, &stored_tab.uri) {
                self.new_tab_full(
                    &directory,
                    stored_tab.sort_column_id(),
                    stored_tab.sort_type(),
                    stored_tab.locked,
                    true,
                );
            }
            visited.insert(stored_tab);
        }

        if self.tab_count() == 0 {
            // Fallback to home directory
            let con = connection_list.home();
            let path = PathBuf::from(glib::home_dir());
            if let Some(directory) =
                Directory::new_startup(con.upcast_ref(), con.create_path(&path))
            {
                self.new_tab_full(
                    &directory,
                    ColumnID::COLUMN_NAME,
                    gtk::SortType::Ascending,
                    false,
                    true,
                );
            } else {
                eprintln!("Stored path {} is invalid. Skipping", path.display());
            }
        }
    }

    pub fn connect_list_clicked<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn() + 'static,
    {
        self.connect_local("list-clicked", false, move |_| {
            (callback)();
            None
        })
    }
}

#[derive(PartialEq, Eq, Hash, glib::Variant)]
pub struct TabVariant {
    pub uri: String,
    pub file_felector_id: u8,
    pub sort_column: u8,
    pub sort_order: bool,
    pub locked: bool,
}

impl TabVariant {
    pub fn assert_variant_type() {
        debug_assert_eq!(*Self::static_variant_type(), "(syybb)");
    }

    pub fn new(uri: impl Into<String>) -> Self {
        Self {
            uri: uri.into(),
            file_felector_id: 0,
            sort_column: ColumnID::COLUMN_NAME as u8,
            sort_order: false,
            locked: false,
        }
    }

    pub fn sort_column_id(&self) -> ColumnID {
        self.sort_column
            .try_into()
            .ok()
            .and_then(ColumnID::from_repr)
            .unwrap_or(ColumnID::COLUMN_NAME)
    }

    pub fn sort_type(&self) -> gtk::SortType {
        match self.sort_order {
            false => gtk::SortType::Ascending,
            true => gtk::SortType::Descending,
        }
    }
}

fn restore_directory(connection_list: &ConnectionList, stored_uri: &str) -> Option<Directory> {
    let (con, path) = match glib::Uri::parse(stored_uri, glib::UriFlags::NONE) {
        Ok(uri) => {
            let con: Connection = if uri.scheme() == "file" {
                connection_list.home().upcast()
            } else {
                // TODO: use connection_list to find or register a connection
                ConnectionRemote::new(stored_uri, &uri).upcast()
            };
            let path = PathBuf::from(uri.path());
            (con, path)
        }
        Err(error) => {
            eprintln!("Stored URI is invalid: {}", error.message());
            let con = connection_list.home().upcast();
            let path = PathBuf::from(stored_uri);
            (con, path)
        }
    };

    if let Some(directory) = Directory::new_startup(&con, con.create_path(&path)) {
        Some(directory)
    } else {
        eprintln!("Stored path {} is invalid. Skipping", path.display());
        None
    }
}

async fn ask_close_locked_tab(parent: &gtk::Window) -> bool {
    gtk::AlertDialog::builder()
        .modal(true)
        .message(gettext("The tab is locked, close anyway?"))
        .buttons([gettext("_Cancel"), gettext("_OK")])
        .cancel_button(0)
        .default_button(1)
        .build()
        .choose_future(Some(parent))
        .await
        == Ok(1)
}

async fn on_notebook_button_pressed(
    file_selector: &FileSelector,
    notebook: &gtk::Notebook,
    n_press: i32,
    button: u32,
    x: f64,
    y: f64,
) {
    let Some(window) = file_selector.root().and_downcast::<gtk::Window>() else {
        eprintln!("No root window.");
        return;
    };

    let Some(tab_clicked) = notebook.find_tab_num_at_pos(&graphene::Point::new(x as f32, y as f32))
    else {
        return;
    };
    match (n_press, button, tab_clicked) {
        (1, 2, TabClick::Tab(index)) | (2, 1, TabClick::Tab(index)) => {
            // mid-click or double-click on a label
            let fl = file_selector.file_list_nth(index);
            if !file_selector.is_tab_locked(&fl) {
                file_selector.close_tab_nth(index);
            } else if ask_close_locked_tab(&window).await {
                file_selector.close_tab_nth(index);
            }
        }
        (1, 3, TabClick::Tab(index)) => {
            // right-click
            // notebook.set_current_page(Some(index));    // switch to the page the mouse is over
            let fl = file_selector.file_list_nth(index);

            let menu = gio::Menu::new();
            menu.append(Some(&gettext("Open in New _Tab")), Some("win.view-new-tab"));

            let section = gio::Menu::new();

            {
                let menuitem = gio::MenuItem::new(
                    Some(&if file_selector.is_tab_locked(&fl) {
                        gettext("_Unlock Tab")
                    } else {
                        gettext("_Lock Tab")
                    }),
                    None,
                );
                menuitem.set_action_and_target_value(
                    Some("fs.toggle-tab-lock"),
                    Some(&(index as i32).to_variant()),
                );
                section.append_item(&menuitem);
            }

            {
                let menuitem = gio::MenuItem::new(Some(&gettext("_Refresh Tab")), None);
                menuitem.set_action_and_target_value(
                    Some("fs.refresh-tab"),
                    Some(&(index as i32).to_variant()),
                );
                section.append_item(&menuitem);
            }

            section.append(
                Some(&gettext("Copy Tab to Other _Pane")),
                Some("win.view-in-inactive-tab"),
            );
            menu.append_section(None, &section);

            menu.append(Some(&gettext("_Close Tab")), Some("win.view-close-tab"));
            menu.append(
                Some(&gettext("Close _All Tabs")),
                Some("win.view-close-all-tabs"),
            );
            menu.append(
                Some(&gettext("Close _Duplicate Tabs")),
                Some("win.view-close-duplicate-tabs"),
            );

            let popover = gtk::PopoverMenu::from_model(Some(&menu));
            popover.set_parent(notebook);
            popover.set_position(gtk::PositionType::Bottom);
            popover.set_pointing_to(Some(&gdk::Rectangle::new(x as i32, y as i32, 0, 0)));
            popover.popup();
        }
        (2, 1, TabClick::Area) => {
            // double-click on a tabs area
            if let Some(dir) = file_selector.file_list().directory() {
                file_selector.new_tab_with_dir(&dir, true);
            }
        }
        _ => {}
    }
}

#[no_mangle]
pub extern "C" fn on_notebook_button_pressed_r(
    gesture: *mut GtkGestureClick,
    file_selector: *mut ffi::GnomeCmdFileSelector,
    notebook: *mut GtkNotebook,
    n_press: i32,
    x: f64,
    y: f64,
) {
    let gesture: gtk::GestureClick = unsafe { from_glib_none(gesture) };
    let file_selector: FileSelector = unsafe { from_glib_none(file_selector) };
    let notebook: gtk::Notebook = unsafe { from_glib_none(notebook) };

    let button = gesture.current_button();
    glib::spawn_future_local(async move {
        on_notebook_button_pressed(&file_selector, &notebook, n_press, button, x, y).await;
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_can_forward(
    fs: *mut ffi::GnomeCmdFileSelector,
) -> gboolean {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.can_forward() as gboolean
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_can_back(fs: *mut ffi::GnomeCmdFileSelector) -> gboolean {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.can_back() as gboolean
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_forward(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.forward()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_back(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.back()
}
