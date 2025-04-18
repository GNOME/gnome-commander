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

use crate::{
    connection::connection::{Connection, ConnectionExt},
    dir::Directory,
    file_list::list::FileList,
    notebook_ext::{GnomeCmdNotebookExt, TabClick},
    types::FileSelectorID,
};
use gettextrs::gettext;
use gtk::{
    ffi::{GtkGestureClick, GtkNotebook},
    gdk, gio,
    glib::{
        self,
        translate::{from_glib_none, ToGlibPtr},
    },
    graphene,
    prelude::*,
};
use std::{
    collections::{BTreeSet, HashMap},
    path::Path,
};

pub mod ffi {
    use crate::{
        connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir,
        file_list::list::ffi::GnomeCmdFileList,
    };
    use gtk::{
        ffi::{GtkNotebook, GtkWidget},
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

        pub fn gnome_cmd_file_selector_get_directory(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_file_selector_get_connection(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_file_selector_set_connection(
            fs: *mut GnomeCmdFileSelector,
            con: *mut GnomeCmdCon,
            start_dir: *mut GnomeCmdDir,
        );

        pub fn gnome_cmd_file_selector_get_notebook(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GtkNotebook;

        pub fn gnome_cmd_file_selector_new_tab(fs: *mut GnomeCmdFileSelector) -> *const GtkWidget;

        pub fn gnome_cmd_file_selector_new_tab_with_dir(
            fs: *mut GnomeCmdFileSelector,
            dir: *mut GnomeCmdDir,
            activate: gboolean,
        ) -> *const GtkWidget;

        pub fn gnome_cmd_file_selector_close_tab(fs: *mut GnomeCmdFileSelector);

        pub fn gnome_cmd_file_selector_close_tab_nth(fs: *mut GnomeCmdFileSelector, n: u32);

        pub fn gnome_cmd_file_selector_tab_count(fs: *mut GnomeCmdFileSelector) -> u32;

        pub fn gnome_cmd_file_selector_get_fs_id(fs: *mut GnomeCmdFileSelector) -> i32;
        pub fn gnome_cmd_file_selector_is_active(fs: *mut GnomeCmdFileSelector) -> gboolean;
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

    pub fn directory(&self) -> Option<Directory> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_get_directory(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn connection(&self) -> Option<Connection> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_get_connection(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn set_connection(&self, connection: &Connection, start_dir: Option<&Directory>) {
        unsafe {
            ffi::gnome_cmd_file_selector_set_connection(
                self.to_glib_none().0,
                connection.to_glib_none().0,
                start_dir.to_glib_none().0,
            )
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

    pub fn close_tab(&self) {
        unsafe { ffi::gnome_cmd_file_selector_close_tab(self.to_glib_none().0) }
    }

    pub fn close_tab_nth(&self, n: u32) {
        unsafe { ffi::gnome_cmd_file_selector_close_tab_nth(self.to_glib_none().0, n) }
    }

    pub fn tab_count(&self) -> u32 {
        unsafe { ffi::gnome_cmd_file_selector_tab_count(self.to_glib_none().0) }
    }

    pub fn id(&self) -> FileSelectorID {
        let id = unsafe { ffi::gnome_cmd_file_selector_get_fs_id(self.to_glib_none().0) };
        match id {
            0 => FileSelectorID::LEFT,
            _ => FileSelectorID::RIGHT,
        }
    }

    pub fn is_active(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_selector_is_active(self.to_glib_none().0) != 0 }
    }

    pub fn goto_directory(&self, con: &Connection, path: &Path) {
        if self.connection().as_ref() == Some(&con) {
            let fl = self.file_list();
            if fl.is_locked() {
                let dir = Directory::new(con, con.create_path(path));
                self.new_tab_with_dir(&dir, true);
            } else {
                fl.goto_directory(path);
            }
        } else {
            if con.is_open() {
                let dir = Directory::new(con, con.create_path(path));

                if self.file_list().is_locked() {
                    self.new_tab_with_dir(&dir, true);
                } else {
                    self.set_connection(&con, Some(&dir));
                }
            } else {
                con.set_base_path(con.create_path(path));
                if self.file_list().is_locked() {
                    self.new_tab_with_dir(&con.default_dir().unwrap(), true);
                } else {
                    self.set_connection(&con, None);
                }
            }
        }
    }

    pub fn close_all_tabs(&self) {
        let current = self.notebook().current_page();
        for index in (0..self.tab_count())
            .rev()
            .filter(|i| Some(*i) != current)
            .filter(|i| !self.file_list_nth(*i).is_locked())
        {
            self.close_tab_nth(index);
        }
    }

    pub fn close_duplicate_tabs(&self) {
        let mut dirs = HashMap::<Directory, BTreeSet<u32>>::new();
        for index in 0..self.tab_count() {
            let file_list = self.file_list_nth(index);
            if !file_list.is_locked() {
                if let Some(directory) = file_list.directory() {
                    dirs.entry(directory).or_default().insert(index);
                }
            }
        }

        let mut duplicates = BTreeSet::new();
        if let Some(mut indexes) = self.directory().and_then(|d| dirs.remove(&d)) {
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
            if !fl.is_locked() {
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
                    Some(&if fl.is_locked() {
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
            if let Some(dir) = file_selector.directory() {
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
