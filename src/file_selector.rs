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
    connection::connection::Connection,
    dir::Directory,
    file_list::FileList,
    notebook_ext::{GnomeCmdNotebookExt, TabClick},
    types::FileSelectorID,
};
use gettextrs::gettext;
use gtk::{
    ffi::{GtkGestureMultiPress, GtkNotebook},
    gdk, gio,
    glib::{
        self,
        translate::{from_glib_none, ToGlibPtr},
    },
    prelude::*,
};

pub mod ffi {
    use crate::{
        connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir,
        file_list::ffi::GnomeCmdFileList,
    };
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

        pub fn gnome_cmd_file_selector_get_directory(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_file_selector_set_connection(
            fs: *mut GnomeCmdFileSelector,
            con: *mut GnomeCmdCon,
            start_dir: *mut GnomeCmdDir,
        );

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
        @extends gtk::Box, gtk::Container, gtk::Widget;

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

    pub fn set_connection(&self, connection: &Connection, start_dir: Option<&Directory>) {
        unsafe {
            ffi::gnome_cmd_file_selector_set_connection(
                self.to_glib_none().0,
                connection.to_glib_none().0,
                start_dir.to_glib_none().0,
            )
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
}

async fn ask_close_locked_tab(parent: &gtk::Window) -> bool {
    let dlg = gtk::MessageDialog::builder()
        .transient_for(parent)
        .message_type(gtk::MessageType::Question)
        .buttons(gtk::ButtonsType::OkCancel)
        .text(gettext("The tab is locked, close anyway?"))
        .build();
    let result = dlg.run_future().await;
    dlg.close();
    result == gtk::ResponseType::Ok
}

async fn on_notebook_button_pressed(
    file_selector: &FileSelector,
    notebook: &gtk::Notebook,
    n_press: i32,
    button: u32,
    x: f64,
    y: f64,
) {
    let Some(window) = file_selector.toplevel().and_downcast::<gtk::Window>() else {
        eprintln!("No root window.");
        return;
    };

    let Some(tab_clicked) = notebook.find_tab_num_at_pos(x as i32, y as i32) else {
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
                    Some("win.view-toggle-tab-lock"),
                    Some(
                        &(file_selector.is_active(), index as i32).to_variant(), // "(bi)"
                    ),
                );
                section.append_item(&menuitem);
            }

            {
                let menuitem = gio::MenuItem::new(Some(&gettext("_Refresh Tab")), None);
                menuitem.set_action_and_target_value(
                    Some("win.view-refresh-tab"),
                    Some(
                        &(file_selector.id() as i32, index as i32).to_variant(), // "(ii)"
                    ),
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

            let popover = gtk::Popover::from_model(Some(notebook), &menu);
            popover.set_position(gtk::PositionType::Bottom);
            popover.set_pointing_to(&gdk::Rectangle::new(x as i32, y as i32, 0, 0));
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
    gesture: *mut GtkGestureMultiPress,
    file_selector: *mut ffi::GnomeCmdFileSelector,
    notebook: *mut GtkNotebook,
    n_press: i32,
    x: f64,
    y: f64,
) {
    let gesture: gtk::GestureMultiPress = unsafe { from_glib_none(gesture) };
    let file_selector: FileSelector = unsafe { from_glib_none(file_selector) };
    let notebook: gtk::Notebook = unsafe { from_glib_none(notebook) };

    let button = gesture.current_button();
    glib::MainContext::default().spawn_local(async move {
        on_notebook_button_pressed(&file_selector, &notebook, n_press, button, x, y).await;
    });
}
