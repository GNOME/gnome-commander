// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    boyer_moore::{BoyerMoore, ScanResult, boyer_moore_bytes_new, boyer_moore_string_new},
    input_modes::InputMode,
};
use gtk::{gio, gio::prelude::*};
use std::sync::Arc;

enum SearchMode {
    Text(BoyerMoore<char>, BoyerMoore<char>),
    Hex(BoyerMoore<u8>, BoyerMoore<u8>),
}

pub struct Searcher {
    input_mode: Arc<InputMode>,
    search_mode: SearchMode,
    start_offset: u64,
    max_offset: u64,
    cancellable: gio::Cancellable,
}

impl Searcher {
    fn new(
        input_mode: Arc<InputMode>,
        search_mode: SearchMode,
        start_offset: u64,
        max_offset: u64,
        cancellable: gio::Cancellable,
    ) -> Self {
        Self {
            input_mode,
            search_mode,
            start_offset,
            max_offset,
            cancellable,
        }
    }

    pub fn new_text_search(
        input_mode: Arc<InputMode>,
        start_offset: u64,
        max_offset: u64,
        text: &str,
        case_sensitive: bool,
        cancellable: gio::Cancellable,
    ) -> Self {
        let bm = boyer_moore_string_new(text, case_sensitive);
        let bm_reverse =
            boyer_moore_string_new(&text.chars().rev().collect::<String>(), case_sensitive);

        Self::new(
            input_mode,
            SearchMode::Text(bm, bm_reverse),
            start_offset,
            max_offset,
            cancellable,
        )
    }

    pub fn new_hex_search(
        input_mode: Arc<InputMode>,
        start_offset: u64,
        max_offset: u64,
        buffer: &[u8],
        cancellable: gio::Cancellable,
    ) -> Self {
        let bm = boyer_moore_bytes_new(buffer.to_vec());
        let bm_reverse = boyer_moore_bytes_new(buffer.iter().rev().cloned().collect::<Vec<u8>>());

        Self::new(
            input_mode,
            SearchMode::Hex(bm, bm_reverse),
            start_offset,
            max_offset,
            cancellable,
        )
    }

    pub fn search<F: Fn(u32)>(&self, forward: bool, progress_callback: F) -> Option<u64> {
        progress_callback(self.progress(self.start_offset));

        match (&self.search_mode, forward) {
            (SearchMode::Text(bm, _), true) => self.search_text_forward(bm, progress_callback),
            (SearchMode::Text(_, bm), false) => self.search_text_backward(bm, progress_callback),
            (SearchMode::Hex(bm, _), true) => self.search_hex_forward(bm, progress_callback),
            (SearchMode::Hex(_, bm), false) => self.search_hex_backward(bm, progress_callback),
        }
    }

    fn progress(&self, position: u64) -> u32 {
        if self.max_offset == 0 {
            0
        } else {
            (if let Some(position) = position.checked_mul(1000) {
                position / self.max_offset
            } else {
                //  Overflow, file is too large
                position / (self.max_offset / 1000)
            }) as u32
        }
    }

    #[inline]
    fn is_aborted(&self) -> bool {
        self.cancellable.is_cancelled()
    }

    fn update_interval(&self) -> u32 {
        if self.max_offset > 1000 {
            (self.max_offset / 1000) as u32
        } else {
            10
        }
    }

    fn search_text_forward<F: Fn(u32)>(
        &self,
        bm: &BoyerMoore<char>,
        progress_callback: F,
    ) -> Option<u64> {
        let imd = &self.input_mode;
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let n = self.max_offset;
        let mut j = self.start_offset;
        let mut update_counter = update_interval;

        while j + m <= n {
            let chars = imd.characters(j, m as usize);
            match bm.scan(&chars) {
                Ok(ScanResult::Match(_advancement)) => {
                    return Some(j);
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    for _ in 0..advancement {
                        j = imd.next_char_offset(j);
                    }
                }
                _ => {
                    break;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                progress_callback(self.progress(j));
                update_counter = update_interval;
            }

            if self.is_aborted() {
                break;
            }
        }
        None
    }

    fn search_text_backward<F: Fn(u32)>(
        &self,
        bm: &BoyerMoore<char>,
        progress_callback: F,
    ) -> Option<u64> {
        let imd = &self.input_mode;
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let mut j = self.start_offset;
        let mut update_counter = update_interval;

        j = imd.previous_char_offset(j);
        while j >= m {
            let chars = imd.characters_backwards(j, m as usize);
            match bm.scan(&chars) {
                Ok(ScanResult::Match(_advancement)) => {
                    return Some(imd.next_char_offset(j));
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    for _ in 0..advancement {
                        j = imd.previous_char_offset(j);
                    }
                }
                _ => {
                    break;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                progress_callback(self.progress(j));
                update_counter = update_interval;
            }

            if self.is_aborted() {
                break;
            }
        }
        None
    }

    fn search_hex_forward<F: Fn(u32)>(
        &self,
        bm: &BoyerMoore<u8>,
        progress_callback: F,
    ) -> Option<u64> {
        let imd = &self.input_mode;
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let n = self.max_offset;
        let mut j = self.start_offset;
        let mut update_counter = update_interval;

        while j + m <= n {
            let bytes = imd.bytes(j, m as usize);
            match bm.scan(&bytes) {
                Ok(ScanResult::Match(_advancement)) => {
                    return Some(j);
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    j += advancement as u64;
                }
                _ => {
                    break;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                progress_callback(self.progress(j));
                update_counter = update_interval;
            }

            if self.is_aborted() {
                break;
            }
        }
        None
    }

    fn search_hex_backward<F: Fn(u32)>(
        &self,
        bm: &BoyerMoore<u8>,
        progress_callback: F,
    ) -> Option<u64> {
        let imd = &self.input_mode;
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let mut j = self.start_offset;
        let mut update_counter = update_interval;

        j = j.saturating_sub(1);
        while j >= m {
            let bytes = imd.bytes_backwards(j, m as usize);
            match bm.scan(&bytes) {
                Ok(ScanResult::Match(_advancement)) => {
                    return Some(j + 1);
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    j -= advancement as u64;
                }
                _ => {
                    break;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                progress_callback(self.progress(j));
                update_counter = update_interval;
            }

            if self.is_aborted() {
                break;
            }
        }
        None
    }
}
