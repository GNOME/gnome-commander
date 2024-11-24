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

use super::file_metadata::FileMetadata;
use crate::file::File;
use glib::translate::{from_glib_none, ToGlibPtr};

mod ffi {
    use super::*;
    use crate::file::ffi::GnomeCmdFile;
    use std::ffi::{c_char, c_int};

    extern "C" {
        pub fn gcmd_tags_bulk_load(f: *mut GnomeCmdFile) -> *mut FileMetadata;
        pub fn gcmd_tags_get_name(tag: c_int) -> *const c_char;
        pub fn gcmd_tags_get_class(tag: c_int) -> c_int;
        pub fn gcmd_tags_get_class_name(tag: c_int) -> *const c_char;
        pub fn gcmd_tags_get_title(tag: c_int) -> *const c_char;
        pub fn gcmd_tags_get_description(tag: c_int) -> *const c_char;
    }
}

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct GnomeCmdTagClass(pub i32);

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct GnomeCmdTag(pub i32);

impl GnomeCmdTag {
    pub fn name(self) -> String {
        unsafe { from_glib_none(ffi::gcmd_tags_get_name(self.0)) }
    }

    pub fn title(self) -> String {
        unsafe { from_glib_none(ffi::gcmd_tags_get_title(self.0)) }
    }

    pub fn description(self) -> String {
        unsafe { from_glib_none(ffi::gcmd_tags_get_description(self.0)) }
    }

    pub fn class(self) -> GnomeCmdTagClass {
        unsafe { GnomeCmdTagClass(ffi::gcmd_tags_get_class(self.0)) }
    }

    pub fn class_name(self) -> String {
        unsafe { from_glib_none(ffi::gcmd_tags_get_class_name(self.0)) }
    }
}

pub fn gcmd_tags_bulk_load(file: &File) -> &FileMetadata {
    unsafe { &*ffi::gcmd_tags_bulk_load(file.to_glib_none().0) }
}
