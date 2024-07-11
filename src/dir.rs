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
    connection::connection::{Connection, GnomeCmdPath},
    file::File,
};
use gtk::glib::{
    self,
    translate::{from_glib_full, from_glib_none, ToGlibPtr},
};
use std::ffi::{c_int, CStr};

pub mod ffi {
    use crate::connection::connection::ffi::GnomeCmdCon;
    use gtk::glib::ffi::GType;
    use std::{
        ffi::{c_char, c_int},
        os::raw::c_void,
    };

    #[repr(C)]
    pub struct GnomeCmdDir {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_dir_get_type() -> GType;

        pub fn gnome_cmd_dir_new(dir: *mut GnomeCmdCon, path: *const c_void) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_dir_get_display_path(dir: *mut GnomeCmdDir) -> *const c_char;

        pub fn gnome_cmd_dir_relist_files(
            parent_window: *const gtk::ffi::GtkWindow,
            dir: *const GnomeCmdDir,
            visual_progress: c_int,
        );

        pub fn gnome_cmd_dir_get_connection(dir: *mut GnomeCmdDir) -> *mut GnomeCmdCon;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdDirClass {
        pub parent_class: crate::file::ffi::GnomeCmdFileClass,
    }
}

glib::wrapper! {
    pub struct Directory(Object<ffi::GnomeCmdDir, ffi::GnomeCmdDirClass>)
        @extends File;

    match fn {
        type_ => || ffi::gnome_cmd_dir_get_type(),
    }
}

impl Directory {
    pub fn new(connection: &Connection, path: GnomeCmdPath) -> Self {
        unsafe { from_glib_full(ffi::gnome_cmd_dir_new(connection.to_glib_none().0, path.0)) }
    }

    pub fn display_path(&self) -> String {
        let ptr = unsafe { ffi::gnome_cmd_dir_get_display_path(self.to_glib_none().0) };
        let str = unsafe { CStr::from_ptr(ptr).to_string_lossy() };
        str.to_string()
    }

    pub fn relist_files(&self, parent_window: &gtk::Window, visual_progress: bool) {
        unsafe {
            ffi::gnome_cmd_dir_relist_files(
                parent_window.to_glib_none().0,
                self.to_glib_none().0,
                visual_progress as c_int,
            );
        }
    }

    pub fn connection(&self) -> Connection {
        unsafe { from_glib_none(ffi::gnome_cmd_dir_get_connection(self.to_glib_none().0)) }
    }
}
