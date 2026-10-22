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
use std::{
    fs::File,
    io::{Error, Read, Seek, SeekFrom},
    path::Path,
    time::{Duration, Instant},
};

struct FileCursor {
    file: File,
    page: Vec<u8>,
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
        path: &Path,
        current_offset: usize,
        size: usize,
        cancellable: gio::Cancellable,
        sender: Sender<u32>,
    ) -> Result<Self, Error> {
        let file = File::open(path)?;
        let current_offset = current_offset / Self::PAGE_SIZE * Self::PAGE_SIZE;
        let page = vec![0; Self::PAGE_SIZE];

        let mut result = Self {
            file,
            page,
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

    fn read_page(&mut self, offset: usize) -> Result<(), Error> {
        if offset >= self.size {
            return Err(Error::other("EOF reached"));
        }

        if self.file.seek(SeekFrom::Start(offset as u64))? != offset as u64 {
            return Err(Error::other("Failed setting file position"));
        }

        let size = Self::PAGE_SIZE.min(self.size - offset);
        if self.file.read(&mut self.page[0..size])? != size {
            return Err(Error::other(
                "Didn't get the requested number of bytes from file",
            ));
        }

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
        !self.cancellable.is_cancelled()
            && self
                .read_page(self.current_offset + Self::PAGE_SIZE)
                .is_ok()
    }

    fn backtrack(&mut self) -> bool {
        !self.cancellable.is_cancelled()
            && self.current_offset > 0
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
    input: Input<FileCursor>,
    regex: Regex,
    start_offset: usize,
    max_offset: usize,
    forward: bool,
    receiver: Receiver<u32>,
}

impl Searcher {
    pub fn new(
        path: &Path,
        start_offset: usize,
        max_offset: usize,
        pattern: &str,
        forward: bool,
        cancellable: gio::Cancellable,
    ) -> Option<Self> {
        let (sender, receiver) = async_channel::unbounded();
        let cursor = match FileCursor::new(path, start_offset, max_offset, cancellable, sender) {
            Ok(cursor) => cursor,
            Err(error) => {
                eprintln!("{error}");
                return None;
            }
        };
        let input = Input::new(cursor);

        fn build_automaton(
            builder: &dense::Builder,
            pattern: &str,
        ) -> Option<dense::DFA<Vec<u32>>> {
            match builder.build(pattern) {
                Ok(automaton) => Some(automaton),
                Err(error) => {
                    eprintln!("{error}");
                    None
                }
            }
        }

        let (dfa_forward, dfa_reverse) = if forward {
            (
                build_automaton(
                    dense::DFA::builder().syntax(syntax::Config::new().utf8(false)),
                    pattern,
                )?,
                build_automaton(
                    dense::DFA::builder()
                        .configure(
                            dense::Config::new()
                                .prefilter(None)
                                .specialize_start_states(false)
                                .start_kind(StartKind::Anchored)
                                .match_kind(MatchKind::All),
                        )
                        .syntax(syntax::Config::new().utf8(false))
                        .thompson(thompson::Config::new().reverse(true)),
                    pattern,
                )?,
            )
        } else {
            (
                build_automaton(
                    dense::DFA::builder()
                        .configure(
                            dense::Config::new()
                                .prefilter(None)
                                .specialize_start_states(false)
                                .start_kind(StartKind::Anchored)
                                .match_kind(MatchKind::All),
                        )
                        .syntax(syntax::Config::new().utf8(false)),
                    pattern,
                )?,
                build_automaton(
                    dense::DFA::builder()
                        .thompson(thompson::Config::new().reverse(true))
                        .syntax(syntax::Config::new().utf8(false)),
                    pattern,
                )?,
            )
        };
        let regex = Regex::builder().build_from_dfas(dfa_forward, dfa_reverse);

        Some(Self {
            input,
            regex,
            start_offset,
            max_offset,
            forward,
            receiver,
        })
    }

    pub fn progress_receiver(&self) -> Receiver<u32> {
        self.receiver.clone()
    }

    pub fn search(&mut self) -> Option<(usize, usize)> {
        if self.forward {
            self.search_forward()
        } else {
            self.search_backward()
        }
    }

    fn search_forward(&mut self) -> Option<(usize, usize)> {
        self.input.set_span(self.start_offset..self.max_offset);
        self.input.set_anchored(Anchored::No);
        self.input.set_earliest(false);
        try_search(&self.regex, &mut self.input)
            .unwrap_or_default()
            .map(|m| (m.start(), m.end()))
    }

    fn search_backward(&mut self) -> Option<(usize, usize)> {
        // There is no try_search_reverse(), so we replicate try_search() logic.
        self.input.set_span(0..self.start_offset);
        self.input.set_anchored(Anchored::No);
        self.input.set_earliest(false);
        let start = try_search_rev(self.regex.reverse(), &mut self.input).unwrap_or_default()?;

        if start.offset() == self.input.end() {
            // Empty match at the end of the search area
            return Some((start.offset(), start.offset()));
        }

        // Perform search in opposite direction to find the end of the match
        self.input.set_span(start.offset()..self.input.end());
        self.input.set_anchored(Anchored::Yes);
        self.input.set_earliest(false);
        let end = try_search_fwd(self.regex.forward(), &mut self.input).unwrap_or_default()?;

        Some((start.offset(), end.offset()))
    }
}
