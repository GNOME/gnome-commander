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

pub mod ffi {
    #[repr(C)]
    pub struct ViewerFileOps {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gv_file_get_max_offset(ops: *mut ViewerFileOps) -> u64;
    }
}

pub struct FileOps(pub *mut ffi::ViewerFileOps);

impl FileOps {
    pub fn max_offset(&self) -> u64 {
        unsafe { ffi::gv_file_get_max_offset(self.0) }
    }
}
