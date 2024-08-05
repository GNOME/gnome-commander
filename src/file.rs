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

use gtk::{
    gio,
    glib::{self, translate::*, Cast},
};
use std::{
    ffi::{CStr, CString},
    path::PathBuf,
};

use crate::libgcmd::file_base::{FileBase, FileBaseExt};

pub mod ffi {
    use gtk::{gio::ffi::GFile, glib::ffi::GType};
    use std::ffi::{c_char, c_int};

    use crate::libgcmd::file_base::ffi::GnomeCmdFileBaseClass;

    #[repr(C)]
    pub struct GnomeCmdFile {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_file_get_type() -> GType;

        pub fn gnome_cmd_file_get_gfile(f: *const GnomeCmdFile, name: *const c_char) -> *mut GFile;
        pub fn gnome_cmd_file_get_name(f: *const GnomeCmdFile) -> *const c_char;
        pub fn gnome_cmd_file_get_real_path(f: *const GnomeCmdFile) -> *mut c_char;
        pub fn gnome_cmd_file_get_uri_str(f: *const GnomeCmdFile) -> *mut c_char;
        pub fn gnome_cmd_file_is_local(f: *const GnomeCmdFile) -> c_int;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileClass {
        pub parent_class: GnomeCmdFileBaseClass,
    }
}

glib::wrapper! {
    pub struct File(Object<ffi::GnomeCmdFile, ffi::GnomeCmdFileClass>)
        @extends FileBase;

    match fn {
        type_ => || ffi::gnome_cmd_file_get_type(),
    }
}

impl FileBaseExt for File {
    fn file(&self) -> gio::File {
        self.upcast_ref::<FileBase>().file()
    }

    fn file_info(&self) -> gio::FileInfo {
        self.upcast_ref::<FileBase>().file_info()
    }
}

impl File {
    pub fn gfile(&self, name: Option<&str>) -> gio::File {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_get_gfile(
                self.to_glib_none().0,
                name.to_glib_none().0,
            ))
        }
    }

    pub fn get_name(&self) -> Option<String> {
        let ptr = unsafe { CStr::from_ptr(ffi::gnome_cmd_file_get_name(self.to_glib_none().0)) };
        Some(ptr.to_str().ok()?.to_string())
    }

    pub fn get_real_path(&self) -> PathBuf {
        unsafe { from_glib_full(ffi::gnome_cmd_file_get_real_path(self.to_glib_none().0)) }
    }

    pub fn get_uri_str(&self) -> Option<String> {
        let ptr =
            unsafe { CString::from_raw(ffi::gnome_cmd_file_get_uri_str(self.to_glib_none().0)) };
        Some(ptr.to_str().ok()?.to_string())
    }

    pub fn is_local(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_is_local(self.to_glib_none().0) != 0 }
    }
}
