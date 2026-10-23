// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::profile::SearchProfile;
use crate::{
    dir::Directory,
    file::FileOps,
    filter::{Filter, PatternType},
    intviewer::{
        search_bar::{Mode, SearchSettings, hex_to_bytes},
        searcher::Searcher,
    },
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::gio::{self, prelude::*};
use std::collections::BTreeSet;

const ATTRIBUTES: &str = const_format::formatcp!(
    "{},{},{},{},{}",
    gio::FILE_ATTRIBUTE_STANDARD_NAME.as_str(),
    gio::FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME.as_str(),
    gio::FILE_ATTRIBUTE_STANDARD_TYPE.as_str(),
    gio::FILE_ATTRIBUTE_STANDARD_SIZE.as_str(),
    gio::FILE_ATTRIBUTE_STANDARD_IS_SYMLINK.as_str()
);

pub enum SearchMessage {
    Status(String),
    File(String, Option<(usize, usize)>),
}

pub async fn search(
    profile: &SearchProfile,
    start_dir: &Directory,
    on_message: impl AsyncFn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), ErrorMessage> {
    let max_depth = profile.max_depth();
    let start_dir = start_dir.uri();

    let filename_pattern = profile.path_pattern();
    let filename_filter = if !filename_pattern.is_empty() {
        let filename_pattern = if matches!(profile.path_syntax(), PatternType::FnMatch)
            && !filename_pattern.contains('/')
        {
            if !filename_pattern.contains('*') && !filename_pattern.contains('?') {
                format!("**/*{filename_pattern}*")
            } else {
                format!("**/{filename_pattern}")
            }
        } else {
            filename_pattern
        };

        Some(
            Filter::new(
                &filename_pattern,
                profile.path_match_case(),
                profile.path_syntax(),
            )
            .map_err(|error| {
                ErrorMessage::with_error(gettext("Invalid file name pattern."), &*error)
            })?,
        )
    } else {
        None
    };

    let content_search_settings = match profile.content_mode() {
        Mode::Text => {
            let pattern = profile.content_pattern();
            (!pattern.is_empty()).then(|| SearchSettings::Text {
                pattern,
                match_case: profile.content_match_case(),
            })
        }
        Mode::Binary => {
            let pattern = profile.content_pattern();

            // Whitespace is ignored in hex patterns
            if pattern.trim().is_empty() {
                None
            } else if let Some(pattern) = hex_to_bytes(&pattern) {
                Some(SearchSettings::Binary { pattern })
            } else {
                return Err(ErrorMessage::brief(gettext("Invalid hexadecimal pattern")));
            }
        }
    };
    let content_search = content_search_settings
        .and_then(|settings| settings.into_regex_pattern("UTF8"))
        .and_then(|regex_pattern| match Searcher::new(&regex_pattern, true) {
            Ok(searcher) => Some(searcher),
            Err(error) => {
                eprintln!("Failed compiling search expression: {error}");
                None
            }
        });

    let cancellable = cancellable.clone();
    let (sender, receiver) = async_channel::unbounded();

    std::thread::spawn(move || {
        search_dir_recursive(
            &gio::File::for_uri(&start_dir),
            &start_dir,
            max_depth,
            &filename_filter,
            &content_search,
            &sender,
            &cancellable,
        )
    });

    while let Ok(message) = receiver.recv().await {
        (on_message)(message).await;
    }

    Ok(())
}

fn get_path(start: &gio::File, uri: &str) -> String {
    start
        .relative_path(&gio::File::for_uri(uri))
        .map(|path| path.into_os_string().to_string_lossy().to_string())
        .unwrap_or_default()
}

fn search_dir_recursive(
    start: &gio::File,
    dir: &str,
    level: i32,
    filename_filter: &Option<Filter>,
    content_search: &Option<Searcher>,
    sender: &async_channel::Sender<SearchMessage>,
    cancellable: &gio::Cancellable,
) {
    if cancellable.is_cancelled() {
        return;
    }

    if start.uri() != dir {
        let message =
            SearchMessage::Status(gettext("Searching in: {}").replace("{}", &get_path(start, dir)));
        if let Err(error) = sender.send_blocking(message) {
            eprintln!("Sending search result to channel failed: {error}");
            return;
        }
    }

    let dir_file = gio::File::for_uri(dir);
    let files: Vec<_> = match dir_file.enumerate_children(
        ATTRIBUTES,
        gio::FileQueryInfoFlags::NONE,
        Some(cancellable),
    ) {
        Ok(enumerator) => enumerator.flatten().collect(),
        Err(error) => {
            if !error.matches(gio::IOErrorEnum::Cancelled) {
                eprintln!("Failed listing directory '{dir}': {error}");
            }
            return;
        }
    };

    // first process the files
    let mut matched_dirs = BTreeSet::new();
    for info in &files {
        if cancellable.is_cancelled() {
            return;
        }

        let file = dir_file.child(info.name());
        if let Some(filter) = filename_filter
            && !filter.matches(&get_path(start, &file.uri()))
        {
            continue;
        }

        let mut content_match = None;
        if let Some(content_search) = content_search {
            if info.file_type() != gio::FileType::Regular {
                continue;
            }

            content_match =
                content_search.search(&file, 0, info.size() as usize, cancellable.clone());
            if content_match.is_none() {
                continue;
            }
        }

        // The file matched the search criteria, let's add it to the list
        let message = SearchMessage::File(file.uri().to_string(), content_match);
        if let Err(error) = sender.send_blocking(message) {
            eprintln!("Sending search result to channel failed: {error}");
            return;
        }

        if info.file_type() == gio::FileType::Directory {
            matched_dirs.insert(info.name());
        }
    }

    if level == 0 {
        return;
    }

    // then process the directories
    for info in &files {
        if cancellable.is_cancelled() {
            return;
        }
        if info.file_type() != gio::FileType::Directory || matched_dirs.contains(&info.name()) {
            continue;
        }
        // we don't want to go backwards or to follow symlinks
        let display_name = info.display_name();
        if display_name != "." && display_name != ".." && !info.is_symlink() {
            search_dir_recursive(
                start,
                &dir_file.child(info.name()).uri(),
                level - 1,
                filename_filter,
                content_search,
                sender,
                cancellable,
            );
        }
    }
}
