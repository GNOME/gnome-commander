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

use crate::intviewer::input_modes::InputMode;

pub mod ffi {
    use crate::intviewer::input_modes::ffi::GVInputModesData;

    #[repr(C)]
    pub struct GVDataPresentation {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gv_data_presentation_new() -> *mut GVDataPresentation;
        pub fn gv_free_data_presentation(dp: *mut GVDataPresentation);

        pub fn gv_init_data_presentation(
            dp: *mut GVDataPresentation,
            imd: *mut GVInputModesData,
            max_offset: u64,
        );

        pub fn gv_get_data_presentation_mode(dp: *mut GVDataPresentation) -> i32;
        pub fn gv_set_data_presentation_mode(dp: *mut GVDataPresentation, mode: i32);

        pub fn gv_align_offset_to_line_start(dp: *mut GVDataPresentation, offset: u64) -> u64;
        pub fn gv_get_end_of_line_offset(dp: *mut GVDataPresentation, start_of_line: u64) -> u64;
        pub fn gv_scroll_lines(dp: *mut GVDataPresentation, current_offset: u64, delta: i32)
            -> u64;

        pub fn gv_set_tab_size(dp: *mut GVDataPresentation, tab_size: u32);
        pub fn gv_set_fixed_count(dp: *mut GVDataPresentation, chars_per_line: u32);
        pub fn gv_set_wrap_limit(dp: *mut GVDataPresentation, chars_per_line: u32);
    }
}

pub struct DataPresentation(pub *mut ffi::GVDataPresentation);

impl DataPresentation {
    pub fn new() -> Self {
        unsafe { Self(ffi::gv_data_presentation_new()) }
    }

    pub fn init(&self, imd: &InputMode, max_offset: u64) {
        unsafe { ffi::gv_init_data_presentation(self.0, imd.0, max_offset) }
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

    pub fn end_of_line_offset(&self, start_of_line: u64) -> u64 {
        unsafe { ffi::gv_get_end_of_line_offset(self.0, start_of_line) }
    }

    pub fn scroll_lines(&self, current_offset: u64, delta: i32) -> u64 {
        unsafe { ffi::gv_scroll_lines(self.0, current_offset, delta) }
    }

    pub fn set_tab_size(&self, tab_size: u32) {
        unsafe { ffi::gv_set_tab_size(self.0, tab_size) }
    }

    pub fn set_fixed_count(&self, chars_per_line: u32) {
        unsafe { ffi::gv_set_fixed_count(self.0, chars_per_line) }
    }

    pub fn set_wrap_limit(&self, chars_per_line: u32) {
        unsafe { ffi::gv_set_wrap_limit(self.0, chars_per_line) }
    }
}

impl Drop for DataPresentation {
    fn drop(&mut self) {
        unsafe {
            ffi::gv_free_data_presentation(self.0);
        }
        self.0 = std::ptr::null_mut();
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

pub fn next_tab_position(column: u32, tab_size: u32) -> u32 {
    (column + tab_size) / tab_size * tab_size
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

    #[test]
    fn test_next_tab_position() {
        assert_eq!(next_tab_position(0, 8), 8);
        assert_eq!(next_tab_position(1, 8), 8);
        assert_eq!(next_tab_position(7, 8), 8);
        assert_eq!(next_tab_position(8, 8), 16);
        assert_eq!(next_tab_position(9, 8), 16);
    }
}
