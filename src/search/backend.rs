// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{content_search::ContentSearch, profile::SearchProfile};
use crate::{
    dir::Directory,
    file::{File, FileOps},
    filter::{Filter, PatternType},
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::gio::{self, prelude::*};

pub enum SearchMessage {
    Status(String),
    File(File),
}

pub async fn search(
    profile: &SearchProfile,
    start_dir: &Directory,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), ErrorMessage> {
    let max_depth = profile.max_depth();
    let start_dir = start_dir.uri();

    let mut filename_pattern = profile.path_pattern();
    let filename_filter = if !filename_pattern.is_empty() {
        if matches!(profile.path_syntax(), PatternType::FnMatch) {
            if !filename_pattern.contains('*') && !filename_pattern.contains('?') {
                filename_pattern = format!("*{filename_pattern}*")
            }
            filename_pattern = if filename_pattern.starts_with('/') {
                format!("**{filename_pattern}")
            } else {
                format!("**/{filename_pattern}")
            };
        }

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

    let mut content_search = if !profile.content_pattern().is_empty() {
        Some(ContentSearch::new(
            &profile.content_pattern(),
            profile.content_match_case(),
        )?)
    } else {
        None
    };

    let cancellable = cancellable.clone();
    let (sender, receiver) = async_channel::unbounded::<(bool, String)>();

    let attributes = format!(
        "{},{},{},{}",
        gio::FILE_ATTRIBUTE_STANDARD_NAME,
        gio::FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
        gio::FILE_ATTRIBUTE_STANDARD_TYPE,
        gio::FILE_ATTRIBUTE_STANDARD_IS_SYMLINK
    );

    std::thread::spawn(move || {
        search_dir_recursive(
            &start_dir,
            max_depth,
            &filename_filter,
            &mut content_search,
            &sender,
            &attributes,
            &cancellable,
        )
    });

    while let Ok((is_result, value)) = receiver.recv().await {
        if is_result {
            let file = gio::File::for_uri(&value);
            match file
                .query_info_future(
                    File::DEFAULT_ATTRIBUTES,
                    gio::FileQueryInfoFlags::NONE,
                    glib::Priority::DEFAULT,
                )
                .await
            {
                Ok(info) => (on_message)(SearchMessage::File(File::new_from_file(&file, &info))),
                Err(error) => eprintln!("Cannot query attributes for '{value}': {error}"),
            }
        } else {
            (on_message)(SearchMessage::Status(value));
        }
    }

    Ok(())
}

fn get_path(uri: &str) -> String {
    glib::Uri::parse(uri, glib::UriFlags::NONE)
        .ok()
        .map(|uri| uri.path())
        .unwrap_or_default()
        .to_string()
}

fn search_dir_recursive(
    dir: &str,
    level: i32,
    filename_filter: &Option<Filter>,
    content_search: &mut Option<ContentSearch>,
    sender: &async_channel::Sender<(bool, String)>,
    attributes: &str,
    cancellable: &gio::Cancellable,
) {
    if cancellable.is_cancelled() {
        return;
    }

    let message = (
        false,
        gettext("Searching in: {}").replace("{}", &get_path(dir)),
    );
    if let Err(error) = sender.send_blocking(message) {
        eprintln!("Sending search result to channel failed: {error}");
        return;
    }

    let dir_file = gio::File::for_uri(dir);
    let files: Vec<_> = match dir_file.enumerate_children(
        attributes,
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
    for info in &files {
        if cancellable.is_cancelled() {
            return;
        }

        let file = dir_file.child(info.name());
        if let Some(filter) = filename_filter
            && !filter.matches(&get_path(&file.uri()))
        {
            continue;
        }

        if let Some(content_search) = content_search
            && (info.file_type() != gio::FileType::Regular
                || !content_search.check(&file, cancellable))
        {
            continue;
        }

        // The file matched the search criteria, let's add it to the list
        let message = (true, file.uri().to_string());
        if let Err(error) = sender.send_blocking(message) {
            eprintln!("Sending search result to channel failed: {error}");
            return;
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
        if info.file_type() != gio::FileType::Directory {
            continue;
        }
        // we don't want to go backwards or to follow symlinks
        let display_name = info.display_name();
        if display_name != "." && display_name != ".." && !info.is_symlink() {
            search_dir_recursive(
                &dir_file.child(info.name()).uri(),
                level - 1,
                filename_filter,
                content_search,
                sender,
                attributes,
                cancellable,
            );
        }
    }
}
