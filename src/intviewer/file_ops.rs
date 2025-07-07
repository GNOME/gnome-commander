/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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

use crate::intviewer::input_modes::InputSource;
use glib::translate::ToGlibPtr;
use std::{path::Path, rc::Rc};

pub mod ffi {
    use std::ffi::c_char;

    #[repr(C)]
    pub struct ViewerFileOps {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gv_fileops_new() -> *mut ViewerFileOps;
        pub fn gv_file_open(ops: *mut ViewerFileOps, file: *const c_char) -> i32;
        pub fn gv_file_get_max_offset(ops: *mut ViewerFileOps) -> u64;
        pub fn gv_file_get_byte(ops: *mut ViewerFileOps, byte_index: u64) -> i32;
        pub fn gv_file_close(ops: *mut ViewerFileOps);
        pub fn gv_file_free(ops: *mut ViewerFileOps);
    }
}

pub struct FileOps(pub *mut ffi::ViewerFileOps);

impl FileOps {
    pub fn new() -> Self {
        unsafe { Self(ffi::gv_fileops_new()) }
    }

    pub fn open(&self, file: &Path) -> bool {
        unsafe { ffi::gv_file_open(self.0, file.to_glib_none().0) == 0 }
    }

    pub fn max_offset(&self) -> u64 {
        unsafe { ffi::gv_file_get_max_offset(self.0) }
    }

    pub fn get_byte(&self, byte_index: u64) -> Option<u8> {
        let result = unsafe { ffi::gv_file_get_byte(self.0, byte_index) };
        result.try_into().ok()
    }
}

impl Drop for FileOps {
    fn drop(&mut self) {
        unsafe {
            ffi::gv_file_close(self.0);
            ffi::gv_file_free(self.0);
        }
        self.0 = std::ptr::null_mut();
    }
}

impl InputSource for FileOps {
    fn byte(&self, offset: u64) -> Option<u8> {
        self.get_byte(offset)
    }
}

impl InputSource for Rc<FileOps> {
    fn byte(&self, offset: u64) -> Option<u8> {
        self.get_byte(offset)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn get_byte_does_read() {
        let file_path = PathBuf::from("./TODO");
        let fops = FileOps::new();

        assert!(fops.open(&file_path));

        let end = fops.max_offset();

        for current in 0..end {
            let value = fops.get_byte(current);
            assert!(value.is_some());
        }

        assert!(fops.open(&file_path));
    }
}
