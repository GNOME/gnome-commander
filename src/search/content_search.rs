// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::utils::ErrorMessage;
use gettextrs::gettext;
use grep::{
    regex::{RegexMatcher, RegexMatcherBuilder},
    searcher::{Searcher, SearcherBuilder, Sink, SinkMatch},
};
use gtk::gio::{self, prelude::*};
use std::io::{Error, ErrorKind};

struct SearchSink<'a> {
    match_found: &'a mut bool,
}

impl<'a> SearchSink<'a> {
    pub fn new(match_found: &'a mut bool) -> Self {
        Self { match_found }
    }
}

impl Sink for SearchSink<'_> {
    type Error = Error;

    fn matched(&mut self, _searcher: &Searcher, _mat: &SinkMatch<'_>) -> Result<bool, Self::Error> {
        *self.match_found = true;
        Ok(false)
    }
}

pub struct ContentSearch {
    matcher: RegexMatcher,
    searcher: Searcher,
}

impl ContentSearch {
    pub fn new(pattern: &str, match_case: bool) -> Result<Self, ErrorMessage> {
        Ok(Self {
            matcher: RegexMatcherBuilder::new()
                .fixed_strings(true)
                .case_insensitive(match_case)
                .dot_matches_new_line(true)
                .build(pattern)
                .map_err(|error| {
                    ErrorMessage::with_error(
                        gettext("Invalid text content regular expression."),
                        &error,
                    )
                })?,
            searcher: SearcherBuilder::new()
                .line_number(false)
                .multi_line(true)
                .max_matches(Some(1))
                .build(),
        })
    }

    pub fn check(&mut self, file: &gio::File, cancellable: &gio::Cancellable) -> bool {
        let mut match_found = false;
        if let Err(error) = file
            .read(Some(cancellable))
            .map_err(std::io::Error::other)
            .and_then(|stream| {
                self.searcher.search_reader(
                    &self.matcher,
                    stream.into_read(),
                    SearchSink::new(&mut match_found),
                )
            })
        {
            if error.kind() != ErrorKind::Interrupted {
                eprintln!("Content matching for file '{}' failed: {error}", file.uri(),);
            }
            false
        } else {
            match_found
        }
    }
}
