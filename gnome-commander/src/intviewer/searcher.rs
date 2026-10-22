// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use async_channel::{Receiver, Sender};
use gtk::{gio, gio::prelude::*};
use regex_cursor::{
    Cursor, Input,
    engines::dfa::{Regex, try_search, try_search_fwd, try_search_rev},
    regex_automata::{
        Anchored, MatchKind,
        dfa::{StartKind, dense},
        nfa::thompson,
        util::syntax,
    },
};
use std::time::{Duration, Instant};

struct FileCursor {
    stream: gio::FileInputStream,
    page: glib::Bytes,
    current_offset: usize,
    size: usize,
    cancellable: gio::Cancellable,
    sender: Sender<u32>,
    start: Instant,
    last_report: Option<Duration>,
}

impl FileCursor {
    const PAGE_SIZE: usize = 0x2000;

    /// Only send out progress updates every 100 ms
    const REPORT_INTERVAL: Duration = Duration::from_millis(100);

    pub fn new(
        file: &gio::File,
        current_offset: usize,
        size: usize,
        cancellable: gio::Cancellable,
        sender: Sender<u32>,
    ) -> Result<Self, glib::Error> {
        let stream = file.read(Some(&cancellable))?;
        let current_offset = current_offset / Self::PAGE_SIZE * Self::PAGE_SIZE;

        let mut result = Self {
            stream,
            page: glib::Bytes::from_static(&[]),
            current_offset,
            size,
            cancellable,
            sender,
            start: Instant::now(),
            last_report: None,
        };
        if size > 0 {
            result.read_page(current_offset)?;
        }
        Ok(result)
    }

    fn read_page(&mut self, offset: usize) -> Result<(), glib::Error> {
        self.stream
            .seek(offset as i64, glib::SeekType::Set, Some(&self.cancellable))?;

        let bytes = self
            .stream
            .read_bytes(Self::PAGE_SIZE, Some(&self.cancellable))?;
        self.page = bytes;
        self.current_offset = offset;

        let elapsed = self.start.elapsed();
        if self
            .last_report
            .is_none_or(|last_report| elapsed - last_report >= Self::REPORT_INTERVAL)
        {
            let progress = (if let Some(offset) = self.current_offset.checked_mul(1000) {
                offset.checked_div(self.size)
            } else {
                //  Overflow, file is too large
                self.current_offset.checked_div(self.size / 1000)
            })
            .unwrap_or_default() as u32;

            let _ = self.sender.send_blocking(progress);
            self.last_report = Some(elapsed);
        }

        Ok(())
    }
}

impl Cursor for FileCursor {
    fn chunk(&self) -> &[u8] {
        &self.page
    }

    fn advance(&mut self) -> bool {
        self.page.len() == Self::PAGE_SIZE
            && self
                .read_page(self.current_offset + Self::PAGE_SIZE)
                .is_ok()
    }

    fn backtrack(&mut self) -> bool {
        self.current_offset > 0
            && self
                .read_page(self.current_offset.saturating_sub(Self::PAGE_SIZE))
                .is_ok()
    }

    fn total_bytes(&self) -> Option<usize> {
        Some(self.size)
    }

    fn offset(&self) -> usize {
        self.current_offset
    }

    fn utf8_aware(&self) -> bool {
        false
    }
}

/// This type implements the file search logic using the `regex-cursor` crate. This crate is still
/// experimental, so this choice may seem odd. However, the requirements this type has to fulfill
/// are:
///
/// * Efficient searching of large and potentially remote files without loading them into memory
/// * Search both in forward and backward direction
/// * Arbitrary character encodings, meaning that search strings are essentially binary data
/// * Case-insensitive matching
/// * Regex-based matching
///
/// Alternatives were:
/// * Our original Boyer-Moore implementation: comparatively slow, only supported case-insensitive
///   matching for ASCII text, no regex support
/// * ripgrep: searching a reader in multi-line mode will read the entire file into memory, no
///   search in backward direction
/// * regex crate: no search in backward direction, only supports searching in memory
/// * regex-automata crate: can search in backward direction (some effort required), only supports
///   searching in memory
///
/// `regex-cursor` is a proof of concept for a `regex-automata` change that would allow searching
/// data that can be paged in as required. While searching backwards still requires some effort,
/// it can be done without wasting memory and performance is good.
///
/// The DFA regex engine uses two automata: one is used in forward direction and finds the end of a
/// match, the other in reverse direction to find the start. We have to create these automata
/// “manually” because the usual assumption is that the forward automaton runs first, then the
/// reverse automaton runs from the end of the match. So the reverse automaton is created anchored
/// by default. In backwards direction we have the opposite scenario however, and the forward
/// automaton needs to be anchored to run from the known start of the match.
pub struct Searcher {
    regex: Regex,
    forward: bool,
    sender: Sender<u32>,
    receiver: Receiver<u32>,
}

impl Searcher {
    #[allow(clippy::result_large_err)]
    pub fn new(pattern: &str, forward: bool) -> Result<Self, dense::BuildError> {
        let (sender, receiver) = async_channel::unbounded();

        let (dfa_forward, dfa_reverse) = if forward {
            (
                dense::DFA::builder()
                    .syntax(syntax::Config::new().utf8(false))
                    .build(pattern)?,
                dense::DFA::builder()
                    .configure(
                        dense::Config::new()
                            .prefilter(None)
                            .specialize_start_states(false)
                            .start_kind(StartKind::Anchored)
                            .match_kind(MatchKind::All),
                    )
                    .syntax(syntax::Config::new().utf8(false))
                    .thompson(thompson::Config::new().reverse(true))
                    .build(pattern)?,
            )
        } else {
            (
                dense::DFA::builder()
                    .configure(
                        dense::Config::new()
                            .prefilter(None)
                            .specialize_start_states(false)
                            .start_kind(StartKind::Anchored)
                            .match_kind(MatchKind::All),
                    )
                    .syntax(syntax::Config::new().utf8(false))
                    .build(pattern)?,
                dense::DFA::builder()
                    .thompson(thompson::Config::new().reverse(true))
                    .syntax(syntax::Config::new().utf8(false))
                    .build(pattern)?,
            )
        };
        let regex = Regex::builder().build_from_dfas(dfa_forward, dfa_reverse);

        Ok(Self {
            regex,
            forward,
            sender,
            receiver,
        })
    }

    pub fn progress_receiver(&self) -> Receiver<u32> {
        self.receiver.clone()
    }

    pub fn search(
        &self,
        file: &gio::File,
        start_offset: usize,
        max_offset: usize,
        cancellable: gio::Cancellable,
    ) -> Option<(usize, usize)> {
        let cursor = match FileCursor::new(
            file,
            start_offset,
            max_offset,
            cancellable,
            self.sender.clone(),
        ) {
            Ok(cursor) => cursor,
            Err(error) => {
                eprintln!("Failed opening file {} for searching: {error}", file.uri());
                return None;
            }
        };

        let mut input = Input::new(cursor);
        if self.forward {
            self.search_forward(&mut input, start_offset)
        } else {
            self.search_backward(&mut input, start_offset)
        }
    }

    fn search_forward(
        &self,
        input: &mut Input<FileCursor>,
        start_offset: usize,
    ) -> Option<(usize, usize)> {
        input.set_span(start_offset..input.end());
        try_search(&self.regex, input)
            .unwrap_or_default()
            .map(|m| (m.start(), m.end()))
    }

    fn search_backward(
        &self,
        input: &mut Input<FileCursor>,
        start_offset: usize,
    ) -> Option<(usize, usize)> {
        // There is no try_search_reverse(), so we replicate try_search() logic.
        input.set_span(0..start_offset);
        let start = try_search_rev(self.regex.reverse(), input).unwrap_or_default()?;

        if start.offset() == input.end() {
            // Empty match at the end of the search area
            return Some((start.offset(), start.offset()));
        }

        // Perform search in opposite direction to find the end of the match
        input.set_span(start.offset()..input.end());
        input.set_anchored(Anchored::Yes);
        input.set_earliest(false);
        let end = try_search_fwd(self.regex.forward(), input).unwrap_or_default()?;

        Some((start.offset(), end.offset()))
    }
}
