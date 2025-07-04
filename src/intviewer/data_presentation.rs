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
    pub struct GVDataPresentation {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gv_data_presentation_new() -> *mut GVDataPresentation;
        pub fn gv_free_data_presentation(dp: *mut GVDataPresentation);

        pub fn gv_get_data_presentation_mode(dp: *mut GVDataPresentation) -> i32;
        pub fn gv_set_data_presentation_mode(dp: *mut GVDataPresentation, mode: i32);

        pub fn gv_align_offset_to_line_start(dp: *mut GVDataPresentation, offset: u64) -> u64;
    }
}

pub struct DataPresentation(pub *mut ffi::GVDataPresentation);

impl DataPresentation {
    pub fn new() -> Self {
        unsafe { Self(ffi::gv_data_presentation_new()) }
    }

    pub fn mode(&self) -> DataPresentationMode {
        unsafe {
            DataPresentationMode::from_repr(ffi::gv_get_data_presentation_mode(self.0) as usize)
                .unwrap_or_default()
        }
    }

    pub fn set_mode(&self, mode: DataPresentationMode) {
        unsafe { ffi::gv_set_data_presentation_mode(self.0, mode as i32) }
    }

    pub fn align_offset_to_line_start(&self, offset: u64) -> u64 {
        unsafe { ffi::gv_align_offset_to_line_start(self.0, offset) }
    }
}

impl Drop for DataPresentation {
    fn drop(&mut self) {
        unsafe {
            ffi::gv_free_data_presentation(self.0);
            self.0 = std::ptr::null_mut();
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, strum::FromRepr, Default)]
#[repr(C)]
pub enum DataPresentationMode {
    #[default]
    NoWrap = 0,
    Wrap,
    /// fixed number of binary characters per line.
    /// e.g. CHAR=BYTE, no UTF-8 or other translations
    BinaryFixed,
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn set_data_presentation_mode() {
        let dp = DataPresentation::new();
        for mode in [
            DataPresentationMode::Wrap,
            DataPresentationMode::NoWrap,
            DataPresentationMode::BinaryFixed,
        ] {
            dp.set_mode(mode);
            assert_eq!(dp.mode(), mode);
        }
    }
}
