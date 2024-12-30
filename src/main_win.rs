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
    file_selector::FileSelector,
    libgcmd::{
        file_descriptor::FileDescriptorExt,
        state::{State, StateExt},
    },
    shortcuts::Shortcuts,
    types::FileSelectorID,
    user_actions,
};
use gtk::{
    gio,
    glib::{
        self,
        translate::{from_glib_none, FromGlibPtrNone, ToGlibPtr},
    },
    prelude::*,
};

pub mod ffi {
    use crate::file_selector::ffi::GnomeCmdFileSelector;
    use crate::shortcuts::Shortcuts;
    use crate::types::FileSelectorID;
    use gtk::glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdMainWin {
        _data: [u8; 0],
        _phantom: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_main_win_get_type() -> GType;

        pub fn gnome_cmd_main_win_get_fs(
            main_win: *mut GnomeCmdMainWin,
            id: FileSelectorID,
        ) -> *mut GnomeCmdFileSelector;

        pub fn gnome_cmd_main_win_change_connection(
            main_win: *mut GnomeCmdMainWin,
            id: FileSelectorID,
        );

        pub fn gnome_cmd_main_win_focus_file_lists(main_win: *mut GnomeCmdMainWin);

        pub fn gnome_cmd_main_win_update_bookmarks(main_win: *mut GnomeCmdMainWin);

        pub fn gnome_cmd_main_win_shortcuts(main_win: *mut GnomeCmdMainWin) -> *mut Shortcuts;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdMainWinClass {
        pub parent_class: gtk::ffi::GtkApplicationWindowClass,
    }
}

glib::wrapper! {
    pub struct MainWindow(Object<ffi::GnomeCmdMainWin, ffi::GnomeCmdMainWinClass>)
        @extends gtk::ApplicationWindow, gtk::Window, gtk::Widget,
        @implements gio::ActionMap;

    match fn {
        type_ => || ffi::gnome_cmd_main_win_get_type(),
    }
}

impl MainWindow {
    pub fn file_selector(&self, id: FileSelectorID) -> FileSelector {
        unsafe {
            FileSelector::from_glib_none(ffi::gnome_cmd_main_win_get_fs(self.to_glib_none().0, id))
        }
    }

    pub fn change_connection(&self, id: FileSelectorID) {
        unsafe { ffi::gnome_cmd_main_win_change_connection(self.to_glib_none().0, id) }
    }

    pub fn focus_file_lists(&self) {
        unsafe { ffi::gnome_cmd_main_win_focus_file_lists(self.to_glib_none().0) }
    }

    pub fn update_bookmarks(&self) {
        unsafe { ffi::gnome_cmd_main_win_update_bookmarks(self.to_glib_none().0) }
    }

    pub fn shortcuts(&self) -> &mut Shortcuts {
        unsafe { &mut *ffi::gnome_cmd_main_win_shortcuts(self.to_glib_none().0) }
    }

    pub fn state(&self) -> State {
        let fs1 = self.file_selector(FileSelectorID::ACTIVE);
        let fs2 = self.file_selector(FileSelectorID::INACTIVE);
        let dir1 = fs1.directory();
        let dir2 = fs2.directory();

        let state = State::new();
        state.set_active_dir(dir1.and_upcast_ref());
        state.set_inactive_dir(dir2.and_upcast_ref());
        state.set_active_dir_files(&fs1.file_list().visible_files());
        state.set_inactive_dir_files(&fs2.file_list().visible_files());
        state.set_active_dir_selected_files(&fs1.file_list().selected_files());
        state.set_inactive_dir_selected_files(&fs2.file_list().selected_files());

        state
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_main_win_install_actions(mw_ptr: *mut ffi::GnomeCmdMainWin) {
    let mw: MainWindow = unsafe { from_glib_none(mw_ptr) };
    mw.add_action_entries(user_actions::USER_ACTIONS.iter().map(|a| a.action_entry()));
}
