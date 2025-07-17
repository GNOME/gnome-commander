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

use glib::{ffi::gboolean, translate::ToGlibPtr};

pub mod ffi {
    use glib::ffi::gboolean;
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GViewerBMChartypeData {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn create_bm_chartype_data(
            pattern: *const c_char,
            case_sensitive: gboolean,
        ) -> *mut GViewerBMChartypeData;
        pub fn free_bm_chartype_data(d: *mut GViewerBMChartypeData);

        pub fn bm_chartype_data_pattern_len(d: *mut GViewerBMChartypeData) -> i32;
        pub fn bm_chartype_equal(
            d: *mut GViewerBMChartypeData,
            pattern_index: i32,
            ch: u32,
        ) -> gboolean;
        pub fn bm_chartype_get_advancement(
            d: *mut GViewerBMChartypeData,
            pattern_index: i32,
            ch: u32,
        ) -> i32;
        pub fn bm_chartype_get_good_match_advancement(d: *mut GViewerBMChartypeData) -> i32;
    }
}

/// Boyer-Moore jump tables
pub struct BMCharType(pub *mut ffi::GViewerBMChartypeData);

impl BMCharType {
    pub fn new(pattern: &str, case_sensitive: bool) -> Option<Self> {
        let ptr = unsafe {
            ffi::create_bm_chartype_data(pattern.to_glib_none().0, case_sensitive as gboolean)
        };
        if ptr.is_null() {
            None
        } else {
            Some(Self(ptr))
        }
    }

    pub fn pattern_len(&self) -> usize {
        unsafe { ffi::bm_chartype_data_pattern_len(self.0) as usize }
    }

    pub fn is_equal(&self, pattern_index: usize, ch: u32) -> bool {
        unsafe { ffi::bm_chartype_equal(self.0, pattern_index as i32, ch) != 0 }
    }

    pub fn advancement(&self, pattern_index: usize, ch: u32) -> usize {
        unsafe { ffi::bm_chartype_get_advancement(self.0, pattern_index as i32, ch) as usize }
    }

    pub fn good_match_advancement(&self) -> usize {
        unsafe { ffi::bm_chartype_get_good_match_advancement(self.0) as usize }
    }

    pub fn scan(&self, characters: &[char]) -> Result<CharScanResult, ()> {
        if characters.len() != self.pattern_len() {
            return Err(());
        }
        for i in (0..self.pattern_len()).rev() {
            let value = characters[i];
            if !self.is_equal(i, value as u32) {
                let adv = self.advancement(i, value as u32);
                return Ok(CharScanResult::NoMatch(adv));
            }
        }
        Ok(CharScanResult::Match(self.good_match_advancement()))
    }
}

impl Drop for BMCharType {
    fn drop(&mut self) {
        unsafe {
            ffi::free_bm_chartype_data(self.0);
        }
        self.0 = std::ptr::null_mut();
    }
}

// Once it is created it is used for reading only, so it is safe to send across threads.
unsafe impl Send for BMCharType {}
unsafe impl Sync for BMCharType {}

pub enum CharScanResult {
    Match(usize),
    NoMatch(usize),
}

fn convert_utf8_to_chartype_array(text: &str) -> Vec<u32> {
    text.chars()
        .map(|c| {
            let mut buf = [0; 4];
            let _ = c.encode_utf8(&mut buf);
            u32::from_le_bytes(buf)
        })
        .collect()
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn chartype_test() {
        // In this test a pattern of UTF-8 encoded letters is searched in a text with UTF-8 encoded letters.

        /*
         * This is a valid UTF8 string, with four hebrew letters in it:
         *  0xD7 0x90 = Aleph (Unicode U+5D0)
         *  0xD7 0x95 = Vav   (Unicode U+5D5)
         *  0xD7 0x94 = He    (Unicode U+5D4)
         *  0xD7 0x91 = Bet   (Unicode U+5D1)
         *  (Aleph-Vav-He-Bet, pronounced "ohev", means "love" in hebrew, FYI :-)
         */
        const PATTERN: &str = "I \u{5D0}\u{5D5}\u{5D4}\u{5D1} you";

        // This is a valid UTF8 text, with pangrams in several languages (I hope I got it right...)
        const TEXT: &str = concat!("English:",
"The quick brown fox jumps over the lazy dog",
"Irish:",
"An \u{1E03} fuil do \u{010B}ro\u{00ED} ag buala\u{1E0B} \u{00F3} \u{1E1F} ait\u{00ED}os an \u{0121}r\u{00E1} a \u{1E41} eall lena \u{1E57}\u{00F3}g \u{00E9} ada \u{00F3} \u{1E61}l\u{00ED} do leasa \u{1E6B}\u{00FA}?",
"Swedish:",
"Flygande b\u{00E4} ckasiner s\u{00F6}ka strax hwila p\u{00E5} mjuka tuvor",
"(our match: I \u{5D0}\u{5D5}\u{5D4}\u{5D1} You)",
"Hebrew:",
"\u{05D6}\u{05D4} \u{05DB}\u{05D9}\u{05E3} \u{05E1}\u{05EA}\u{05DD} \u{05DC}\u{05E9}\u{05DE}\u{05D5}\u{05E2} \u{05D0}\u{05D9}\u{05DA} \u{05EA}\u{05E0}\u{05E6}\u{05D7} \u{05E7}\u{05E8}\u{05E4}\u{05D3} \u{05E2}\u{05E5} \u{05D8}\u{05D5}\u{05D1} \u{05D1}\u{05D2}\u{05DF}",
"French:",
 "Les na\u{00EF} fs \u{00E6}githales h\u{00E2}tifs pondant \u{00E0} No\u{00EB}l o\u{00F9} il g\u{00E8}le sont s\u{00FB}rs d'\u{00EA}tre d\u{00E9}\u{00E7}us et de voir leurs dr\u{00F4}les d'\u{0153}ufs ab\u{00EE}m\u{00E9}s.",
);
        let ct_text = convert_utf8_to_chartype_array(TEXT);

        let bm = BMCharType::new(PATTERN, false).unwrap();

        // Do the actual search
        let m = bm.pattern_len();
        let n = ct_text.len();
        let mut j = 0;
        let mut found = Vec::new();
        while j <= n - m {
            let mut i: isize = m as isize - 1;
            while i >= 0 && bm.is_equal(i as usize, ct_text[j.checked_add_signed(i).unwrap()]) {
                i -= 1;
            }

            if i < 0 {
                found.push(j);
                j += bm.good_match_advancement();
            } else {
                let i = i as usize;
                j += bm.advancement(i, ct_text[i + j]);
            }
        }

        assert_eq!(&found, &[217]);
    }
}
