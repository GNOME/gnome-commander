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

use super::{backend::SearchMessage, profile::SearchProfile};
use crate::{dir::Directory, file::File, filter::PatternType, utils::ErrorMessage};
use gettextrs::gettext;
use glob::{MatchOptions, Pattern};
use grep::{
    matcher::Matcher,
    regex::{RegexMatcher, RegexMatcherBuilder},
    searcher::{Searcher, SearcherBuilder, Sink, SinkMatch},
};
use gtk::gio::{self, prelude::*};
use std::{
    cell::Cell,
    io::{Error, ErrorKind, Read},
    path::Path,
    path::PathBuf,
};
use walkdir::WalkDir;

struct SearchSink<'a> {
    match_found: &'a Cell<bool>,
}

impl<'a> SearchSink<'a> {
    pub fn new(match_found: &'a Cell<bool>) -> Self {
        Self { match_found }
    }
}

impl Sink for SearchSink<'_> {
    type Error = Error;

    fn matched(&mut self, _searcher: &Searcher, _mat: &SinkMatch<'_>) -> Result<bool, Self::Error> {
        self.match_found.replace(true);
        Ok(false)
    }
}

struct CancellableReader {
    file: std::fs::File,
    cancellable: gio::Cancellable,
}

impl CancellableReader {
    pub fn new(path: &Path, cancellable: &gio::Cancellable) -> Result<Self, Error> {
        Ok(Self {
            file: std::fs::File::open(path)?,
            cancellable: cancellable.clone(),
        })
    }
}

impl Read for CancellableReader {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        if self.cancellable.is_cancelled() {
            Err(Error::new(ErrorKind::Interrupted, "Cancelled"))
        } else {
            self.file.read(buf)
        }
    }
}

enum FilenamePattern {
    Regex(RegexMatcher),
    Glob(Pattern),
    None,
}

pub async fn local_search(
    profile: &SearchProfile,
    start_dir: &Directory,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), ErrorMessage> {
    let start_dir = start_dir.path().path();
    let max_depth = profile.max_depth();

    let filename_pattern = profile.filename_pattern();
    let filename_pattern = if !filename_pattern.is_empty() {
        match profile.pattern_type() {
            PatternType::Regex => FilenamePattern::Regex(
                RegexMatcherBuilder::new()
                    .case_smart(true)
                    .build(&filename_pattern)
                    .map_err(|error| {
                        ErrorMessage::with_error(
                            gettext("Invalid file name regular expression."),
                            &error,
                        )
                    })?,
            ),
            PatternType::FnMatch => {
                let filename_pattern =
                    if !filename_pattern.contains('*') && !filename_pattern.contains('?') {
                        format!("*{filename_pattern}*")
                    } else {
                        filename_pattern
                    };

                let filename_pattern = if filename_pattern.starts_with('/') {
                    format!("**{filename_pattern}")
                } else {
                    format!("**/{filename_pattern}")
                };

                FilenamePattern::Glob(Pattern::new(&filename_pattern).map_err(|error| {
                    ErrorMessage::with_error(gettext("Invalid file name pattern."), &error)
                })?)
            }
        }
    } else {
        FilenamePattern::None
    };

    let (matcher, mut searcher) = if profile.content_search() {
        let matcher = RegexMatcherBuilder::new()
            .fixed_strings(true)
            .case_insensitive(!profile.match_case())
            .dot_matches_new_line(true)
            .build(&profile.text_pattern())
            .map_err(|error| {
                ErrorMessage::with_error(
                    gettext("Invalid text content regular expression."),
                    &error,
                )
            })?;
        let searcher = SearcherBuilder::new()
            .line_number(false)
            .multi_line(true)
            .max_matches(Some(1))
            .build();

        (Some(matcher), Some(searcher))
    } else {
        (None, None)
    };

    let cancellable = cancellable.clone();
    let (sender, receiver) = async_channel::unbounded::<PathBuf>();
    std::thread::spawn(move || {
        let walker = WalkDir::new(start_dir).min_depth(1);
        let walker = if max_depth != -1 {
            walker.max_depth((max_depth + 1) as usize)
        } else {
            walker
        };

        for entry in walker {
            if cancellable.is_cancelled() {
                break;
            }

            let entry = match entry {
                Ok(entry) => entry,
                Err(error) => {
                    eprintln!("{error}");
                    continue;
                }
            };

            match &filename_pattern {
                FilenamePattern::Regex(regex) => {
                    if !regex
                        .is_match(entry.path().as_os_str().as_encoded_bytes())
                        .unwrap_or_default()
                    {
                        continue;
                    }
                }
                FilenamePattern::Glob(glob) => {
                    if !glob.matches_path_with(
                        entry.path(),
                        MatchOptions {
                            case_sensitive: false,
                            require_literal_separator: true,
                            require_literal_leading_dot: false,
                        },
                    ) {
                        continue;
                    }
                }
                FilenamePattern::None => {}
            }

            if let Some(matcher) = matcher.as_ref()
                && let Some(searcher) = searcher.as_mut()
            {
                if !entry.file_type().is_file() {
                    continue;
                }

                let match_found = Cell::new(false);
                if let Err(error) =
                    CancellableReader::new(entry.path(), &cancellable).and_then(|reader| {
                        searcher.search_reader(matcher, reader, SearchSink::new(&match_found))
                    })
                {
                    if error.kind() != ErrorKind::Interrupted {
                        eprintln!("{error}");
                    }
                    continue;
                }
                if !match_found.into_inner() {
                    continue;
                }
            }

            if let Err(error) = sender.send_blocking(entry.path().to_owned()) {
                eprintln!("Sending search result to channel failed: {error}");
                break;
            }
        }
    });

    while let Ok(path) = receiver.recv().await {
        match File::new_from_path(&path) {
            Ok(file) => (on_message)(SearchMessage::File(file)),
            Err(error) => eprintln!("Cannot create a file for a path: {error}"),
        };
    }

    Ok(())
}
