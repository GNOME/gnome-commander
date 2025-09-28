/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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

use super::{
    boyer_moore::{BoyerMoore, ScanResult, boyer_moore_bytes_new, boyer_moore_string_new},
    input_modes::InputMode,
};
use gtk::glib::{self, subclass::prelude::*};
use std::sync::{Arc, atomic::Ordering};

mod imp {
    use super::*;
    use std::sync::{
        Mutex, OnceLock,
        atomic::{AtomicBool, AtomicU64},
    };

    pub enum SearchMode {
        Text(BoyerMoore<char>, BoyerMoore<char>),
        Hex(BoyerMoore<u8>, BoyerMoore<u8>),
    }

    #[derive(Default)]
    pub struct Searcher {
        pub input_mode: OnceLock<Arc<InputMode>>,
        pub search_mode: OnceLock<SearchMode>,
        pub start_offset: AtomicU64,
        pub max_offset: AtomicU64,
        pub search_result: AtomicU64,
        pub progress_callback: Mutex<Option<Box<dyn Fn(u32) + Send + Sync + 'static>>>,
        pub abort_indicator: AtomicBool,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Searcher {
        const NAME: &'static str = "GnomeCmdSearcher";
        type Type = super::Searcher;
    }

    impl ObjectImpl for Searcher {}
}

glib::wrapper! {
    pub struct Searcher(ObjectSubclass<imp::Searcher>);
}

#[derive(Clone, Copy)]
pub enum SearchProgress {
    Progress(u32),
    Done(Option<u64>),
}

impl Searcher {
    fn new(
        input_mode: Arc<InputMode>,
        search_mode: imp::SearchMode,
        start_offset: u64,
        max_offset: u64,
    ) -> Self {
        let this: Self = glib::Object::builder().build();
        this.imp().input_mode.set(input_mode).ok().unwrap();
        this.imp().search_mode.set(search_mode).ok().unwrap();
        this.imp()
            .start_offset
            .store(start_offset, Ordering::SeqCst);
        this.imp().max_offset.store(max_offset, Ordering::SeqCst);
        this
    }

    pub fn new_text_search(
        input_mode: Arc<InputMode>,
        start_offset: u64,
        max_offset: u64,
        text: &str,
        case_sensitive: bool,
    ) -> Option<Self> {
        if start_offset > max_offset || text.is_empty() {
            return None;
        }

        let bm = boyer_moore_string_new(text, case_sensitive)?;
        let bm_reverse =
            boyer_moore_string_new(&text.chars().rev().collect::<String>(), case_sensitive)?;

        Some(Self::new(
            input_mode,
            imp::SearchMode::Text(bm, bm_reverse),
            start_offset,
            max_offset,
        ))
    }

    pub fn new_hex_search(
        input_mode: Arc<InputMode>,
        start_offset: u64,
        max_offset: u64,
        buffer: &[u8],
    ) -> Option<Self> {
        if start_offset > max_offset || buffer.is_empty() {
            return None;
        }

        let bm = boyer_moore_bytes_new(buffer.to_vec())?;
        let bm_reverse =
            boyer_moore_bytes_new(buffer.into_iter().rev().cloned().collect::<Vec<u8>>())?;

        Some(Self::new(
            input_mode,
            imp::SearchMode::Hex(bm, bm_reverse),
            start_offset,
            max_offset,
        ))
    }

    fn input_mode(&self) -> Arc<InputMode> {
        self.imp().input_mode.get().unwrap().clone()
    }

    pub fn search<F: Fn(u32) + Send + Sync + 'static>(&self, forward: bool, f: F) -> Option<u64> {
        let mode = self.imp().search_mode.get()?;
        self.imp()
            .progress_callback
            .lock()
            .unwrap()
            .replace(Box::new(f));
        self.imp().abort_indicator.store(false, Ordering::SeqCst);

        self.update_progress_indicator(self.imp().start_offset.load(Ordering::SeqCst));

        let found = match (mode, forward) {
            (imp::SearchMode::Text(bm, _), true) => self.search_text_forward(bm),
            (imp::SearchMode::Text(_, bm), false) => self.search_text_backward(bm),
            (imp::SearchMode::Hex(bm, _), true) => self.search_hex_forward(bm),
            (imp::SearchMode::Hex(_, bm), false) => self.search_hex_backward(bm),
        };
        if found {
            Some(self.imp().search_result.load(Ordering::SeqCst))
        } else {
            None
        }
    }

    pub fn abort(&self) {
        self.imp().abort_indicator.store(true, Ordering::SeqCst);
    }

    fn update_progress_indicator(&self, position: u64) {
        let value = ((position * 1000) / self.imp().max_offset.load(Ordering::SeqCst)) as u32;
        self.imp()
            .progress_callback
            .lock()
            .unwrap()
            .as_ref()
            .map(|cb| {
                (cb)(value);
            });
    }

    fn check_abort_request(&self) -> bool {
        self.imp().abort_indicator.load(Ordering::SeqCst)
    }

    fn update_interval(&self) -> u32 {
        let max_offset = self.imp().max_offset.load(Ordering::SeqCst);
        if max_offset > 1000 {
            (max_offset / 1000) as u32
        } else {
            10
        }
    }

    fn search_text_forward(&self, bm: &BoyerMoore<char>) -> bool {
        let imd = self.input_mode();
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let n = self.imp().max_offset.load(Ordering::SeqCst);
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        while j <= n - m {
            let chars = imd.characters(j, m as usize);
            match bm.scan(&chars) {
                Ok(ScanResult::Match(advancement)) => {
                    self.imp().search_result.store(j, Ordering::SeqCst);
                    for _ in 0..advancement {
                        j = imd.next_char_offset(j);
                    }
                    // Store the current offset, we'll use it if the user chooses "find next"
                    self.imp().start_offset.store(j, Ordering::SeqCst);
                    return true;
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    for _ in 0..advancement {
                        j = imd.next_char_offset(j);
                    }
                }
                _ => {
                    return false;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                self.update_progress_indicator(j);
                update_counter = update_interval;
            }

            if self.check_abort_request() {
                break;
            }
        }
        false
    }

    fn search_text_backward(&self, bm: &BoyerMoore<char>) -> bool {
        let imd = self.input_mode();
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        j = imd.previous_char_offset(j);
        while j >= m {
            let chars = imd.characters_backwards(j, m as usize);
            match bm.scan(&chars) {
                Ok(ScanResult::Match(advancement)) => {
                    self.imp()
                        .search_result
                        .store(imd.next_char_offset(j), Ordering::SeqCst);
                    for _ in 0..advancement {
                        j = imd.previous_char_offset(j);
                    }
                    // Store the current offset, we'll use it if the user chooses "find next"
                    self.imp().start_offset.store(j, Ordering::SeqCst);
                    return true;
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    for _ in 0..advancement {
                        j = imd.previous_char_offset(j);
                    }
                }
                _ => {
                    return false;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                self.update_progress_indicator(j);
                update_counter = update_interval;
            }

            if self.check_abort_request() {
                break;
            }
        }
        false
    }

    fn search_hex_forward(&self, bm: &BoyerMoore<u8>) -> bool {
        let imd = self.input_mode();
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let n = self.imp().max_offset.load(Ordering::SeqCst);
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        while j <= n - m {
            let bytes = imd.bytes(j, m as usize);
            match bm.scan(&bytes) {
                Ok(ScanResult::Match(advancement)) => {
                    self.imp().search_result.store(j, Ordering::SeqCst);
                    j += advancement as u64;
                    // Store the current offset, we'll use it if the user chooses "find next"
                    self.imp().start_offset.store(j, Ordering::SeqCst);
                    return true;
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    j += advancement as u64;
                }
                _ => {
                    return false;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                self.update_progress_indicator(j);
                update_counter = update_interval;
            }

            if self.check_abort_request() {
                break;
            }
        }
        false
    }

    fn search_hex_backward(&self, bm: &BoyerMoore<u8>) -> bool {
        let imd = self.input_mode();
        let update_interval = self.update_interval();

        let m = bm.pattern.len() as u64;
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        j = j.saturating_sub(1);
        while j >= m {
            let bytes = imd.bytes_backwards(j, m as usize);
            match bm.scan(&bytes) {
                Ok(ScanResult::Match(advancement)) => {
                    self.imp().search_result.store(j + 1, Ordering::SeqCst);
                    j -= advancement as u64;
                    self.imp().start_offset.store(j, Ordering::SeqCst);
                    return true;
                }
                Ok(ScanResult::NoMatch(advancement)) => {
                    j -= advancement as u64;
                }
                _ => {
                    return false;
                }
            }

            update_counter -= 1;
            if update_counter == 0 {
                self.update_progress_indicator(j);
                update_counter = update_interval;
            }

            if self.check_abort_request() {
                break;
            }
        }
        false
    }
}
