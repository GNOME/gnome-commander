/*
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

use crate::file::File;
use gtk::glib::{self, translate::*};
use std::ptr;

pub mod ffi {
    use crate::file::ffi::GnomeCmdFile;
    use gtk::{ffi::GtkWindowClass, glib::ffi::GType};
    use std::ffi::c_void;

    #[repr(C)]
    pub struct GViewerWindow {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gviewer_window_get_type() -> GType;

        pub fn gviewer_window_file_view(
            f: *mut GnomeCmdFile,
            initial_settings: /* GViewerWindowSettings */ *mut c_void,
        ) -> *mut GViewerWindow;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GViewerWindowClass {
        pub parent_class: GtkWindowClass,
    }
}

glib::wrapper! {
    pub struct ViewerWindow(Object<ffi::GViewerWindow, ffi::GViewerWindowClass>)
        @extends gtk::Window, gtk::Widget;

    match fn {
        type_ => || ffi::gviewer_window_get_type(),
    }
}

impl ViewerWindow {
    pub fn file_view(file: &File) -> Self {
        unsafe {
            from_glib_none(ffi::gviewer_window_file_view(
                file.to_glib_none().0,
                ptr::null_mut(),
            ))
        }
    }
}
