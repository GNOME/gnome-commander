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
use crate::{
    dir::Directory, dirlist::list_directory, file::File, filter::Filter,
    libgcmd::file_descriptor::FileDescriptorExt, utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::gio::{self, prelude::*};
use regex::RegexBuilder;
use std::error::Error;

pub async fn generic_search(
    profile: &SearchProfile,
    start_dir: &Directory,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), ErrorMessage> {
    let name_filter = Filter::new(
        &profile.filename_pattern(),
        profile.match_case(),
        profile.pattern_type(),
    )
    .map_err(|error| ErrorMessage::with_error(gettext("Bad expression"), &*error))?;

    let content_search = profile
        .content_search()
        .then(|| (profile.text_pattern(), profile.match_case()));

    search_dir_recursive(
        start_dir,
        profile.max_depth(),
        &name_filter,
        content_search.as_ref(),
        on_message,
        cancellable,
    )
    .await
    .map_err(|error| ErrorMessage::with_error(gettext("Search failed"), &*error))?;

    Ok(())
}

async fn search_dir_recursive(
    dir: &Directory,
    level: i32,
    name_filter: &Filter,
    content_search: Option<&(String, bool)>,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), Box<dyn Error>> {
    if cancellable.is_cancelled() {
        return Ok(());
    }

    (on_message)(SearchMessage::Status(
        gettext("Searching in: {}").replace("{}", &dir.display_path()),
    ));

    enum DirectoryItem {
        File(File),
        Info(gio::FileInfo),
    }

    let dir_files = dir.files();
    let files: Vec<DirectoryItem> = if dir_files.n_items() == 0 || dir.connection().is_local() {
        // if list is not available or it's a local directory then create a new list, otherwise use already available list
        // gnome_cmd_dir_list_files is not used for creating a list, because it's tied to the GUI and that's not usable from other threads
        list_directory(dir, None)
            .await?
            .into_iter()
            .map(DirectoryItem::Info)
            .collect()
    } else {
        dir_files
            .iter::<File>()
            .flatten()
            .map(DirectoryItem::File)
            .collect()
    };

    // first process the files
    for item in &files {
        if cancellable.is_cancelled() {
            return Ok(());
        }
        let info = match item {
            DirectoryItem::File(file) => file.file_info(),
            DirectoryItem::Info(info) => info.clone(),
        };
        if info.file_type() != gio::FileType::Regular {
            continue;
        }
        if !name_filter.matches(&info.display_name()) {
            continue;
        }
        if let Some((pattern, match_case)) = content_search {
            let file = match item {
                DirectoryItem::File(file) => file.file(),
                DirectoryItem::Info(info) => dir.file().child(info.name()),
            };
            match content_matches(&file, pattern, *match_case).await {
                Ok(true) => {}
                Ok(false) => continue,
                Err(error) => {
                    eprintln!(
                        "Content matching for a file '{:?}' failed: {}",
                        file.path(),
                        error
                    );
                    continue;
                }
            }
        }

        // the file matched the search criteria, let's add it to the list
        match item {
            DirectoryItem::File(file) => (on_message)(SearchMessage::File(file.clone())),
            DirectoryItem::Info(info) => {
                let file = File::new(&info, dir);
                (on_message)(SearchMessage::File(file));
            }
        }
    }

    if level == 0 {
        return Ok(());
    }

    // then process the directories
    for item in &files {
        if cancellable.is_cancelled() {
            return Ok(());
        }
        let info = match item {
            DirectoryItem::File(file) => file.file_info(),
            DirectoryItem::Info(info) => info.clone(),
        };
        if info.file_type() != gio::FileType::Directory {
            continue;
        }
        // we don't want to go backwards or to follow symlinks
        let display_name = info.display_name();
        if display_name != "." && display_name != ".." && !info.is_symlink() {
            let new_dir = match item {
                DirectoryItem::File(file) => file.downcast_ref::<Directory>().cloned(),
                DirectoryItem::Info(info) => Directory::new_from_file_info(&info, dir),
            };
            if let Some(new_dir) = new_dir {
                Box::pin(search_dir_recursive(
                    &new_dir,
                    level - 1,
                    name_filter,
                    content_search,
                    on_message,
                    cancellable,
                ))
                .await?;
            }
        }
    }

    Ok(())
}

async fn content_matches(
    file: &gio::File,
    pattern: &str,
    match_case: bool,
) -> Result<bool, Box<dyn Error>> {
    const IO_PRIORITY: glib::Priority = glib::Priority::DEFAULT;
    const OVERLAP_SIZE: usize = 4096;
    const BUFFER_SIZE: usize = 4096 * 10;

    let regex = RegexBuilder::new(pattern)
        .case_insensitive(!match_case)
        .build()?;

    let stream = file.read_future(IO_PRIORITY).await?;

    let mut buffer: [u8; OVERLAP_SIZE + BUFFER_SIZE] = [0; OVERLAP_SIZE + BUFFER_SIZE];
    let mut buffer_start = 0;
    let result = loop {
        let (input, size) = stream
            .read_future([0; BUFFER_SIZE], IO_PRIORITY)
            .await
            .map_err(|e| e.1)?;
        if size == 0 {
            break false;
        }
        buffer[buffer_start..buffer_start + size].copy_from_slice(&input[0..size]);
        let len = buffer_start + size;

        // ToDo: this only works on text files
        let data = String::from_utf8_lossy(&buffer[0..len]);
        if regex.is_match(&data) {
            break true;
        }

        if len < OVERLAP_SIZE {
            buffer_start = len;
        } else {
            buffer.copy_within(len - OVERLAP_SIZE..len, 0);
            buffer_start = OVERLAP_SIZE;
        }
    };
    stream.close_future(IO_PRIORITY).await?;

    Ok(result)
}
