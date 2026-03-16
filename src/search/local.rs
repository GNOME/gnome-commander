// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{backend::SearchMessage, content_search::ContentSearch, profile::SearchProfile};
use crate::{
    dir::Directory,
    file::{File, FileOps},
    filter::{Filter, PatternType},
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::gio::{self, prelude::*};
use std::path::PathBuf;
use walkdir::WalkDir;

pub async fn local_search(
    profile: &SearchProfile,
    start_dir: &Directory,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), ErrorMessage> {
    let Some(start_dir) = start_dir.local_path() else {
        return Err(ErrorMessage::brief(gettext(
            "Something went wrong, cannot get start directory.",
        )));
    };
    let max_depth = profile.max_depth();

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
            Filter::new(&filename_pattern, false, profile.path_syntax()).map_err(|error| {
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

            if let Some(ref filter) = filename_filter
                && !filter.matches(entry.path().to_str().unwrap_or_default())
            {
                continue;
            }

            if let Some(content_search) = content_search.as_mut()
                && (!entry.file_type().is_file()
                    || !content_search.check(&gio::File::for_path(entry.path()), &cancellable))
            {
                continue;
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
