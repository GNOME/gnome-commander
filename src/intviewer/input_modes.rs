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

use glib::translate::{from_glib_none, ToGlibPtr};

pub mod ffi {
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GVInputModesData {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gv_input_modes_new() -> *mut GVInputModesData;
        pub fn gv_free_input_modes(imd: *mut GVInputModesData);

        pub fn gv_get_input_mode(imd: *mut GVInputModesData) -> *const c_char;
        pub fn gv_set_input_mode(imd: *mut GVInputModesData, input_mode: *const c_char);
    }
}

pub struct InputMode(pub *mut ffi::GVInputModesData, bool);

impl InputMode {
    pub fn new() -> Self {
        unsafe { Self(ffi::gv_input_modes_new(), true) }
    }

    pub fn borrow(ptr: *mut ffi::GVInputModesData) -> Self {
        Self(ptr, false)
    }

    pub fn mode(&self) -> String {
        unsafe { from_glib_none(ffi::gv_get_input_mode(self.0)) }
    }

    pub fn set_mode(&self, mode: &str) {
        unsafe { ffi::gv_set_input_mode(self.0, mode.to_glib_none().0) }
    }
}

impl Drop for InputMode {
    fn drop(&mut self) {
        if self.1 {
            unsafe {
                ffi::gv_free_input_modes(self.0);
            }
            self.1 = false;
        }
        self.0 = std::ptr::null_mut();
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn input_mode() {
        let imd = InputMode::new();
        for mode in ["ASCII", "UTF8", "CP437", "CP1251"] {
            imd.set_mode(mode);
            assert_eq!(imd.mode(), mode);
        }
    }
}
