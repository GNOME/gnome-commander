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
    gio::{self, ffi::GFile},
    glib::translate::{from_glib_full, from_glib_none},
};
use std::{
    ffi::{self, CStr, CString},
    path::PathBuf,
};

#[repr(C)]
pub struct GnomeCmdFile(
    [u8; 0],
    std::marker::PhantomData<std::marker::PhantomPinned>,
);

extern "C" {
    pub fn gnome_cmd_file_get_file(f: *const GnomeCmdFile) -> *mut GFile;
    pub fn gnome_cmd_file_get_name(f: *const GnomeCmdFile) -> *const ffi::c_char;
    pub fn gnome_cmd_file_get_real_path(f: *const GnomeCmdFile) -> *mut ffi::c_char;
    pub fn gnome_cmd_file_get_uri_str(f: *const GnomeCmdFile) -> *mut ffi::c_char;
    pub fn gnome_cmd_file_is_local(f: *const GnomeCmdFile) -> ffi::c_int;
}

impl GnomeCmdFile {
    pub fn get_file(&self) -> gio::File {
        unsafe { from_glib_none(gnome_cmd_file_get_file(self as *const GnomeCmdFile)) }
    }

    pub fn get_name(&self) -> Option<String> {
        let ptr = unsafe { CStr::from_ptr(gnome_cmd_file_get_name(self as *const GnomeCmdFile)) };
        Some(ptr.to_str().ok()?.to_string())
    }

    pub fn get_real_path(&self) -> PathBuf {
        unsafe { from_glib_full(gnome_cmd_file_get_real_path(self as *const GnomeCmdFile)) }
    }

    pub fn get_uri_str(&self) -> Option<String> {
        let ptr =
            unsafe { CString::from_raw(gnome_cmd_file_get_uri_str(self as *const GnomeCmdFile)) };
        Some(ptr.to_str().ok()?.to_string())
    }

    pub fn is_local(&self) -> bool {
        unsafe { gnome_cmd_file_is_local(self as *const GnomeCmdFile) != 0 }
    }
}
