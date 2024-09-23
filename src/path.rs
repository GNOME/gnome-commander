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

use glib::translate::{from_glib_none, ToGlibPtr};
use std::ffi::c_void;

mod ffi {
    use super::*;
    use std::ffi::c_char;

    extern "C" {
        pub fn gnome_cmd_path_get_path(p: *mut c_void) -> *const c_char;
        pub fn gnome_cmd_path_get_child(p: *mut c_void, child: *const c_char) -> *mut c_void;

        pub fn gnome_cmd_path_clone(p: *mut c_void) -> *mut c_void;
        pub fn gnome_cmd_path_free(p: *mut c_void);
    }
}

pub struct GnomeCmdPath(*mut c_void);

impl GnomeCmdPath {
    pub unsafe fn from_raw(ptr: *mut c_void) -> Self {
        Self(ptr)
    }

    pub unsafe fn from_raw_borrow(ptr: *mut c_void) -> Self {
        unsafe { Self::from_raw(ffi::gnome_cmd_path_clone(ptr)) }
    }

    pub unsafe fn into_raw(mut self) -> *mut c_void {
        std::mem::replace(&mut self.0, std::ptr::null_mut())
    }
}

impl GnomeCmdPath {
    pub fn path(&self) -> String {
        unsafe { from_glib_none(ffi::gnome_cmd_path_get_path(self.0)) }
    }

    pub fn child(&self, child: &str) -> Self {
        unsafe {
            Self::from_raw(ffi::gnome_cmd_path_get_child(
                self.0,
                child.to_glib_none().0,
            ))
        }
    }
}

impl Drop for GnomeCmdPath {
    fn drop(&mut self) {
        let ptr = std::mem::replace(&mut self.0, std::ptr::null_mut());
        if !ptr.is_null() {
            unsafe { ffi::gnome_cmd_path_free(self.0) }
        }
    }
}

impl Clone for GnomeCmdPath {
    fn clone(&self) -> Self {
        unsafe { Self::from_raw(ffi::gnome_cmd_path_clone(self.0)) }
    }
}
