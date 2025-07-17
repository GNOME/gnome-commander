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
    pub struct GViewerBMByteData {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn create_bm_byte_data(pattern: *const u8, length: i32) -> *mut GViewerBMByteData;
        pub fn free_bm_byte_data(d: *mut GViewerBMByteData);

        pub fn bm_byte_data_pattern_len(d: *mut GViewerBMByteData) -> i32;
        pub fn bm_byte_data_pattern_at(d: *mut GViewerBMByteData, index: u32) -> u8;
        pub fn bm_byte_data_good(d: *mut GViewerBMByteData) -> *mut i32;
        pub fn bm_byte_data_bad(d: *mut GViewerBMByteData) -> *mut i32;
    }
}

/// Boyer-Moore jump tables
pub struct BMByte(pub *mut ffi::GViewerBMByteData);

impl BMByte {
    /// Create the Boyer-Moore jump tables.
    /// pattern is the search pattern, UTF8 string (null-terminated)
    pub fn new(pattern: &[u8]) -> Option<Self> {
        let ptr = unsafe { ffi::create_bm_byte_data(pattern.as_ptr(), pattern.len() as i32) };
        if ptr.is_null() {
            None
        } else {
            Some(Self(ptr))
        }
    }

    pub fn pattern_len(&self) -> usize {
        unsafe { ffi::bm_byte_data_pattern_len(self.0) as usize }
    }

    pub fn pattern_at(&self, index: usize) -> u8 {
        unsafe { ffi::bm_byte_data_pattern_at(self.0, index as u32) }
    }

    pub fn good(&self) -> Vec<i32> {
        let len = self.pattern_len();
        let s = unsafe { std::slice::from_raw_parts(ffi::bm_byte_data_good(self.0), len) };
        let mut result: Vec<i32> = vec![0; len];
        result.copy_from_slice(s);
        result
    }

    pub fn bad(&self) -> [i32; 256] {
        let s = unsafe { std::slice::from_raw_parts(ffi::bm_byte_data_bad(self.0), 256) };
        let mut result = [0; 256];
        result.copy_from_slice(s);
        result
    }

    pub fn advancement(&self, pattern_index: usize, value: u8) -> usize {
        usize::max(
            self.good()[pattern_index] as usize,
            self.bad()[value as usize] as usize + 1 + pattern_index - self.pattern_len(),
        )
    }

    pub fn good_match_advancement(&self) -> usize {
        self.good()[0] as usize
    }

    pub fn scan(&self, bytes: &[u8]) -> Result<ByteScanResult, ()> {
        if bytes.len() != self.pattern_len() {
            return Err(());
        }
        for i in (0..self.pattern_len()).rev() {
            let value = bytes[i];
            if self.pattern_at(i) != value {
                return Ok(ByteScanResult::NoMatch(self.advancement(i, value)));
            }
        }
        Ok(ByteScanResult::Match(self.good_match_advancement()))
    }
}

impl Drop for BMByte {
    fn drop(&mut self) {
        unsafe {
            ffi::free_bm_byte_data(self.0);
        }
        self.0 = std::ptr::null_mut();
    }
}

// Once it is created it is used for reading only, so it is safe to send across threads.
unsafe impl Send for BMByte {}
unsafe impl Sync for BMByte {}

pub enum ByteScanResult {
    Match(usize),
    NoMatch(usize),
}

#[cfg(test)]
mod test {
    use super::*;

    const PATTERN: &[u8] = &[0x01, 0x04, 0xB4, 0xFE, 0x01, 0x01, 0x04];
    const TEXT: &[u8] = &[
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
        0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d,
        0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c,
        0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b,
        0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x01, 0x04, 0x59, 0x5a,
        0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x6b, 0x6c, 0x6d, 0x01, 0x01, 0x04, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, /* a match */
        0x01, 0x04, 0xB4, 0xFE, 0x01, 0x01, 0x04, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d,
        0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac,
        0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb,
        0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
        0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0x05, 0x04, 0xB4, 0xFe, 0x01, 0x01, 0x04, 0xd8, 0xd9,
        0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
        0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, /* a match */
        0x01, 0x04, 0xb4, 0xfe, 0x01, 0x01, 0x04, 0x00,
    ];

    #[test]
    fn match_test() {
        let bm = BMByte::new(PATTERN).unwrap();

        let good = bm.good();
        assert_eq!(good, vec![5, 5, 5, 5, 5, 7, 1]);

        let bad = bm.bad();
        assert_eq!(
            bad,
            [
                7, 1, 7, 7, 5, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 4, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                7, 7, 3, 7,
            ]
        );

        // Do the actual search: The test search should find a match at
        // position 142 and 248, nowhere else in the sample text.
        let m = bm.pattern_len();
        let n = TEXT.len();
        let mut j = 0;
        let mut found = Vec::new();
        while j <= n - m {
            let mut i: isize = m as isize - 1;
            while i >= 0 && PATTERN[i as usize] == TEXT[j.checked_add_signed(i).unwrap()] {
                i -= 1;
            }

            if i < 0 {
                found.push(j);
                j += good[0] as usize;
            } else {
                let i = i as usize;
                j += usize::max(
                    good[i] as usize,
                    bad[TEXT[i + j] as usize] as usize + 1 + i - m,
                );
            }
        }

        assert_eq!(&found, &[142, 248]);
    }
}
