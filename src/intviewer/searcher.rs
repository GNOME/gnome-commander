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
    bm_byte::{BMByte, ByteScanResult},
    bm_chartype::{BMCharType, CharScanResult},
    input_modes::InputMode,
};
use gtk::glib::{self, subclass::prelude::*};
use std::sync::{atomic::Ordering, Arc};

mod imp {
    use super::*;
    use std::sync::{
        atomic::{AtomicBool, AtomicU64},
        Mutex, OnceLock,
    };

    pub enum SearchMode {
        Text(BMCharType, BMCharType),
        Hex(BMByte, BMByte),
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
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn setup_new_text_search(
        imd: Arc<InputMode>,
        start_offset: u64,
        max_offset: u64,
        text: &str,
        case_sensitive: bool,
    ) -> Option<Self> {
        if start_offset > max_offset || text.is_empty() {
            return None;
        }

        let bm = BMCharType::new(text, case_sensitive)?;
        let bm_reverse = BMCharType::new(&text.chars().rev().collect::<String>(), case_sensitive)?;

        let this = Self::new();
        this.imp().input_mode.set(imd.clone()).ok().unwrap();
        this.imp()
            .search_mode
            .set(imp::SearchMode::Text(bm, bm_reverse))
            .ok()
            .unwrap();
        this.imp()
            .start_offset
            .store(start_offset, Ordering::SeqCst);
        this.imp().max_offset.store(max_offset, Ordering::SeqCst);
        Some(this)
    }

    pub fn setup_new_hex_search(
        imd: Arc<InputMode>,
        start_offset: u64,
        max_offset: u64,
        buffer: &[u8],
    ) -> Option<Self> {
        if start_offset > max_offset || buffer.is_empty() {
            return None;
        }

        let bm = BMByte::new(buffer)?;
        let bm_reverse = BMByte::new(&buffer.into_iter().rev().cloned().collect::<Vec<u8>>())?;

        let this = Self::new();
        this.imp().input_mode.set(imd.clone()).ok().unwrap();
        this.imp()
            .search_mode
            .set(imp::SearchMode::Hex(bm, bm_reverse))
            .ok()
            .unwrap();
        this.imp()
            .start_offset
            .store(start_offset, Ordering::SeqCst);
        this.imp().max_offset.store(max_offset, Ordering::SeqCst);
        Some(this)
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

    fn search_text_forward(&self, bm: &BMCharType) -> bool {
        let imd = self.imp().input_mode.get().unwrap();
        let update_interval = self.update_interval();

        let m = bm.pattern_len() as u64;
        let n = self.imp().max_offset.load(Ordering::SeqCst);
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        while j <= n - m {
            let chars = imd.characters(j, m as usize);
            match bm.scan(&chars) {
                Ok(CharScanResult::Match(advancement)) => {
                    self.imp().search_result.store(j, Ordering::SeqCst);
                    for _ in 0..advancement {
                        j = imd.next_char_offset(j);
                    }
                    // Store the current offset, we'll use it if the user chooses "find next"
                    self.imp().start_offset.store(j, Ordering::SeqCst);
                    return true;
                }
                Ok(CharScanResult::NoMatch(advancement)) => {
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

    fn search_text_backward(&self, bm: &BMCharType) -> bool {
        let imd = self.imp().input_mode.get().unwrap();
        let update_interval = self.update_interval();

        let m = bm.pattern_len() as u64;
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        j = imd.previous_char_offset(j);
        while j >= m {
            let chars = imd.characters_backwards(j, m as usize);
            match bm.scan(&chars) {
                Ok(CharScanResult::Match(advancement)) => {
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
                Ok(CharScanResult::NoMatch(advancement)) => {
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

    fn search_hex_forward(&self, bm: &BMByte) -> bool {
        let imd = self.imp().input_mode.get().unwrap();
        let update_interval = self.update_interval();

        let m = bm.pattern_len() as u64;
        let n = self.imp().max_offset.load(Ordering::SeqCst);
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        while j <= n - m {
            let bytes = imd.bytes(j, m as usize);
            match bm.scan(&bytes) {
                Ok(ByteScanResult::Match(advancement)) => {
                    self.imp().search_result.store(j, Ordering::SeqCst);
                    j += advancement as u64;
                    // Store the current offset, we'll use it if the user chooses "find next"
                    self.imp().start_offset.store(j, Ordering::SeqCst);
                    return true;
                }
                Ok(ByteScanResult::NoMatch(advancement)) => {
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

    fn search_hex_backward(&self, bm: &BMByte) -> bool {
        let imd = self.imp().input_mode.get().unwrap();
        let update_interval = self.update_interval();

        let m = bm.pattern_len() as u64;
        let mut j = self.imp().start_offset.load(Ordering::SeqCst);
        let mut update_counter = update_interval;

        j = j.saturating_sub(1);
        while j >= m {
            let bytes = imd.bytes_backwards(j, m as usize);
            match bm.scan(&bytes) {
                Ok(ByteScanResult::Match(advancement)) => {
                    self.imp().search_result.store(j + 1, Ordering::SeqCst);
                    j -= advancement as u64;
                    self.imp().start_offset.store(j, Ordering::SeqCst);
                    return true;
                }
                Ok(ByteScanResult::NoMatch(advancement)) => {
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
