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

use gtk::glib::translate::ToGlibPtr;
use std::os::raw::c_void;

mod ffi {
    use super::PatternType;
    use glib::ffi::gboolean;
    use std::ffi::{c_char, c_void};

    extern "C" {
        pub fn gnome_cmd_filter_new(
            exp: *const c_char,
            case_sens: gboolean,
            typ: PatternType,
        ) -> *mut c_void;
        pub fn gnome_cmd_filter_free(filter: *mut c_void);
        pub fn gnome_cmd_filter_match(filter: *mut c_void, text: *const c_char) -> gboolean;

        pub fn gnome_cmd_filter_fnmatch(
            pattern: *const c_char,
            string: *const c_char,
            case_sens: gboolean,
        ) -> gboolean;
    }
}

#[derive(Clone, Copy)]
#[repr(C)]
pub enum PatternType {
    Regex = 0,
    FnMatch,
}

pub struct Filter(*mut c_void);

impl Filter {
    pub fn new(exp: &str, case_sens: bool, pattern_type: PatternType) -> Self {
        unsafe {
            Self(ffi::gnome_cmd_filter_new(
                exp.to_glib_none().0,
                if case_sens { 1 } else { 0 },
                pattern_type,
            ))
        }
    }

    pub fn matches(&self, text: &str) -> bool {
        unsafe { ffi::gnome_cmd_filter_match(self.0, text.to_glib_none().0) != 0 }
    }

    pub unsafe fn as_ptr(&self) -> *mut c_void {
        self.0
    }
}

impl Drop for Filter {
    fn drop(&mut self) {
        unsafe {
            ffi::gnome_cmd_filter_free(self.0);
        }
    }
}

pub fn fnmatch(pattern: &str, string: &str, case_sens: bool) -> bool {
    unsafe {
        ffi::gnome_cmd_filter_fnmatch(
            pattern.to_glib_none().0,
            string.to_glib_none().0,
            if case_sens { 1 } else { 0 },
        ) != 0
    }
}
