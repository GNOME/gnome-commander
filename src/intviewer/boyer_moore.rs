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

use std::{collections::HashMap, hash::Hash, marker::PhantomData};

/// Boyer-Moore jump tables
pub struct BoyerMoore<T: Eq + Hash + Copy> {
    pub pattern: Vec<T>,
    pub eq_class: Box<dyn EqClass<T> + Send + Sync>,
    pub good: Vec<usize>,
    pub bad: HashMap<T, usize>,
}

impl<T: Eq + Hash + Copy> BoyerMoore<T> {
    pub fn new(pattern: Vec<T>, eq_class: Box<dyn EqClass<T> + Send + Sync>) -> Option<Self> {
        if pattern.is_empty() {
            return None;
        }
        Some(Self {
            good: good_suffix_table(&pattern, &|a, b| eq_class.equal(a, b)),
            bad: compute_bad_map(&pattern, &*eq_class),
            pattern,
            eq_class,
        })
    }

    pub fn advancement(&self, pattern_index: usize, value: &T) -> usize {
        let m = self.pattern.len();
        usize::max(
            self.good[pattern_index],
            self.bad.get(value).unwrap_or(&m) + 1 + pattern_index - m,
        )
    }

    pub fn good_match_advancement(&self) -> usize {
        self.good[0]
    }

    pub fn scan(&self, values: &[T]) -> Result<ScanResult, ()> {
        if values.len() != self.pattern.len() {
            return Err(());
        }
        for i in (0..self.pattern.len()).rev() {
            let value = &values[i];
            if !self.eq_class.equal(&self.pattern[i], value) {
                return Ok(ScanResult::NoMatch(
                    self.advancement(i, &self.eq_class.normal(value)),
                ));
            }
        }
        Ok(ScanResult::Match(self.good_match_advancement()))
    }
}

pub trait EqClass<T> {
    fn equal(&self, a: &T, b: &T) -> bool;
    fn normal(&self, a: &T) -> T;
}

pub enum ScanResult {
    Match(usize),
    NoMatch(usize),
}

#[derive(Default)]
pub struct SimpleEqClass<T>(PhantomData<T>);

impl<T: Eq + Clone> EqClass<T> for SimpleEqClass<T> {
    fn equal(&self, a: &T, b: &T) -> bool {
        *a == *b
    }

    fn normal(&self, a: &T) -> T {
        a.clone()
    }
}

/// Helper function to compute the suffices of each character for the good-suffices array
fn suffix_table<T>(pattern: &[T], eq: &dyn Fn(&T, &T) -> bool) -> Vec<usize> {
    let m = pattern.len();
    let mut suff = vec![m; m];
    if m < 2 {
        return suff;
    }
    let mut f = 0;
    // This implementation differs from the classical one.
    // instead of a variable `g` it uses `g1` which is greated then `g` by one.
    // `g1` cannot be negative (and be stored as `usize`) while `g` may become -1 sometimes.
    let mut g1 = m;
    for i in (0..=(m - 2)).rev() {
        if i + 1 > g1 && suff[i + m - 1 - f] < i + 1 - g1 {
            suff[i] = suff[i + m - 1 - f];
        } else {
            if i + 1 < g1 {
                g1 = i + 1;
            }
            f = i;
            while g1 > 0 && eq(&pattern[g1 - 1], &pattern[g1 + m - 2 - f]) {
                g1 -= 1;
            }
            suff[i] = f + 1 - g1;
        }
    }
    suff
}

fn good_suffix_table<T>(pattern: &[T], eq: &dyn Fn(&T, &T) -> bool) -> Vec<usize> {
    let m = pattern.len();
    let suff = suffix_table(pattern, eq);

    let mut good = vec![m; m];
    let mut j = 0;
    for i in (0..m).rev() {
        if suff[i] == i + 1 {
            while j < m - 1 - i {
                good[j] = m - 1 - i;
                j += 1;
            }
        }
    }
    for i in 0..(m - 1) {
        good[m - 1 - suff[i]] = m - 1 - i;
    }
    good
}

fn compute_bad_map<T: Eq + Hash>(pattern: &[T], eq_class: &dyn EqClass<T>) -> HashMap<T, usize> {
    let m = pattern.len();
    let mut bad = HashMap::new();
    for i in 0..(m - 1) {
        bad.insert(eq_class.normal(&pattern[i]), m - i - 1);
    }
    bad
}

pub fn boyer_moore_bytes_new(pattern: Vec<u8>) -> Option<BoyerMoore<u8>> {
    BoyerMoore::<u8>::new(pattern, Box::new(SimpleEqClass::<u8>::default()))
}

pub fn boyer_moore_string_new(pattern: &str, case_sensitive: bool) -> Option<BoyerMoore<char>> {
    if pattern.is_empty() {
        return None;
    }
    BoyerMoore::new(
        pattern.chars().collect::<Vec<_>>(),
        if case_sensitive {
            Box::new(SimpleEqClass::<char>::default())
        } else {
            Box::new(AsciiCaseFold)
        },
    )
}

pub struct AsciiCaseFold;

impl EqClass<char> for AsciiCaseFold {
    fn equal(&self, a: &char, b: &char) -> bool {
        a.eq_ignore_ascii_case(b)
    }

    fn normal(&self, a: &char) -> char {
        a.to_ascii_uppercase()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_suffixes() {
        assert_eq!(suffix_table(b"?", &u8::eq), vec![1]);
        assert_eq!(suffix_table(b"ab", &u8::eq), vec![0, 2]);
        assert_eq!(suffix_table(b"aa", &u8::eq), vec![1, 2]);
        assert_eq!(suffix_table(b"ababa", &u8::eq), vec![1, 0, 3, 0, 5]);
        assert_eq!(
            suffix_table(b"abaababaaba", &u8::eq),
            vec![1, 0, 3, 1, 0, 6, 0, 3, 1, 0, 11]
        );
        assert_eq!(
            suffix_table(b"abABc", &u8::eq_ignore_ascii_case),
            vec![0, 0, 0, 0, 5]
        );
        assert_eq!(
            suffix_table(b"aAaAA", &u8::eq_ignore_ascii_case),
            vec![1, 2, 3, 4, 5]
        );
        assert_eq!(
            suffix_table(b"abcAB", &u8::eq_ignore_ascii_case),
            vec![0, 2, 0, 0, 5]
        );
        assert_eq!(
            suffix_table(b"abacabad", &u8::eq_ignore_ascii_case),
            vec![0, 0, 0, 0, 0, 0, 0, 8]
        );
        assert_eq!(
            suffix_table(&[0x01, 0x04, 0xB4, 0xFE, 0x01, 0x01, 0x04], &u8::eq),
            vec![0, 2, 0, 0, 0, 0, 7]
        );
    }

    #[test]
    fn test_good_suffixes() {
        assert_eq!(good_suffix_table(b"?", &u8::eq), vec![1]);
        assert_eq!(good_suffix_table(b"ab", &u8::eq), vec![2, 1]);
        assert_eq!(good_suffix_table(b"aa", &u8::eq), vec![1, 2]);
        assert_eq!(good_suffix_table(b"ababa", &u8::eq), vec![2, 2, 4, 4, 1]);
        assert_eq!(
            good_suffix_table(b"abaababaaba", &u8::eq),
            vec![5, 5, 5, 5, 5, 8, 8, 3, 10, 2, 1]
        );
        assert_eq!(
            good_suffix_table(b"abABc", &u8::eq_ignore_ascii_case),
            vec![5, 5, 5, 5, 1]
        );
        assert_eq!(
            good_suffix_table(b"aAaAA", &u8::eq_ignore_ascii_case),
            vec![1, 2, 3, 4, 5]
        );
        assert_eq!(
            good_suffix_table(b"abcAB", &u8::eq_ignore_ascii_case),
            vec![3, 3, 3, 5, 1]
        );
        assert_eq!(
            good_suffix_table(b"abacabad", &u8::eq_ignore_ascii_case),
            vec![8, 8, 8, 8, 8, 8, 8, 1]
        );
        assert_eq!(
            good_suffix_table(&[0x01, 0x04, 0xB4, 0xFE, 0x01, 0x01, 0x04], &u8::eq),
            vec![5, 5, 5, 5, 5, 7, 1]
        );
    }

    #[test]
    fn match_bytes_test() {
        const PATTERN: &[u8] = &[0x01, 0x04, 0xB4, 0xFE, 0x01, 0x01, 0x04];
        const TEXT: &[u8] = &[
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
            0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
            0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
            0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
            0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
            0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54,
            0x55, 0x56, 0x01, 0x04, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62,
            0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x01, 0x01, 0x04,
            0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
            0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
            0x8d, 0x8e, /* a match */
            0x01, 0x04, 0xB4, 0xFE, 0x01, 0x01, 0x04, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c,
            0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
            0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
            0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
            0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0x05, 0x04, 0xB4, 0xFe,
            0x01, 0x01, 0x04, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2,
            0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
            0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, /* a match */
            0x01, 0x04, 0xb4, 0xfe, 0x01, 0x01, 0x04, 0x00,
        ];

        let bm = boyer_moore_bytes_new(PATTERN.to_vec()).unwrap();

        assert_eq!(bm.good, vec![5, 5, 5, 5, 5, 7, 1]);

        assert_eq!(
            bm.bad,
            [(0x01, 1), (0x04, 5), (0xB4, 4), (0xFE, 3)]
                .into_iter()
                .collect()
        );

        // Do the actual search: The test search should find a match at
        // position 142 and 248, nowhere else in the sample text.
        let m = bm.pattern.len();
        let n = TEXT.len();
        let mut j = 0;
        let mut found = Vec::new();
        while j <= n - m {
            match bm.scan(&TEXT[j..(j + m)]) {
                Ok(ScanResult::Match(advancement)) => {
                    found.push(j);
                    j += advancement;
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    j += advancement;
                }
                _ => {
                    unreachable!();
                }
            }
        }

        assert_eq!(&found, &[142, 248]);
    }

    #[test]
    fn match_chars_test() {
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
        const TEXT: &str = concat!(
            "English:",
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
        let ct_text: Vec<char> = TEXT.chars().collect();

        let bm = boyer_moore_string_new(PATTERN, false).unwrap();

        // Do the actual search
        let m = bm.pattern.len();
        let n = ct_text.len();
        let mut j = 0;
        let mut found = Vec::new();
        while j <= n - m {
            match bm.scan(&ct_text[j..(j + m)]) {
                Ok(ScanResult::Match(advancement)) => {
                    found.push(j);
                    j += advancement;
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    j += advancement;
                }
                _ => {
                    unreachable!();
                }
            }
        }

        assert_eq!(&found, &[217]);
    }
}
