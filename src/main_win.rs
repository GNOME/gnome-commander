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

use crate::{file_selector::FileSelector, types::FileSelectorID};
use gtk::glib::{
    self,
    translate::{FromGlibPtrNone, ToGlibPtr},
};

pub mod ffi {
    use gtk::glib::ffi::GType;

    use crate::file_selector::ffi::GnomeCmdFileSelector;
    use crate::types::FileSelectorID;

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

        pub fn gnome_cmd_main_win_focus_file_lists(main_win: *mut GnomeCmdMainWin);

        pub fn gnome_cmd_main_win_update_bookmarks(main_win: *mut GnomeCmdMainWin);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdMainWinClass {
        pub parent_class: gtk::ffi::GtkApplicationWindowClass,
    }
}

glib::wrapper! {
    pub struct MainWindow(Object<ffi::GnomeCmdMainWin, ffi::GnomeCmdMainWinClass>)
        @extends gtk::ApplicationWindow, gtk::Window, gtk::Bin, gtk::Container, gtk::Widget;

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

    pub fn focus_file_lists(&self) {
        unsafe { ffi::gnome_cmd_main_win_focus_file_lists(self.to_glib_none().0) }
    }

    pub fn update_bookmarks(&self) {
        unsafe { ffi::gnome_cmd_main_win_update_bookmarks(self.to_glib_none().0) }
    }
}
