// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    boyer_moore::{BoyerMoore, ScanResult, boyer_moore_bytes_new},
    input_modes::InputMode,
};
use gtk::{gio, gio::prelude::*};
use std::sync::Arc;

pub struct Searcher {
    input_mode: Arc<InputMode>,
    bm: BoyerMoore<u8>,
    start_offset: u64,
    max_offset: u64,
    forward: bool,
    cancellable: gio::Cancellable,
}

impl Searcher {
    pub fn new(
        input_mode: Arc<InputMode>,
        start_offset: u64,
        max_offset: u64,
        buffer: &[u8],
        case_sensitive: bool,
        forward: bool,
        cancellable: gio::Cancellable,
    ) -> Self {
        let bm = boyer_moore_bytes_new(
            if forward {
                buffer.to_vec()
            } else {
                buffer.iter().rev().copied().collect()
            },
            case_sensitive,
        );

        Self {
            input_mode,
            bm,
            start_offset,
            max_offset,
            forward,
            cancellable,
        }
    }

    pub fn search<F: Fn(u32)>(&self, progress_callback: F) -> Option<u64> {
        progress_callback(self.progress(self.start_offset));

        if self.forward {
            self.search_forward(progress_callback)
        } else {
            self.search_backward(progress_callback)
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

    fn search_forward<F: Fn(u32)>(&self, progress_callback: F) -> Option<u64> {
        let imd = &self.input_mode;
        let update_interval = self.update_interval();

        let m = self.bm.pattern.len() as u64;
        let n = self.max_offset;
        let mut j = self.start_offset;
        let mut update_counter = update_interval;

        while j + m <= n {
            let bytes = imd.bytes(j, m as usize);
            match self.bm.scan(&bytes) {
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

    fn search_backward<F: Fn(u32)>(&self, progress_callback: F) -> Option<u64> {
        let imd = &self.input_mode;
        let update_interval = self.update_interval();

        let m = self.bm.pattern.len() as u64;
        let mut j = self.start_offset;
        let mut update_counter = update_interval;

        j = j.saturating_sub(1);
        while j >= m - 1 {
            let bytes = imd.bytes_backwards(j, m as usize);
            match self.bm.scan(&bytes) {
                Ok(ScanResult::Match(_advancement)) => {
                    return Some(j + 1);
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    j = j.saturating_sub(advancement as u64);
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
