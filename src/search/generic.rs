// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{backend::SearchMessage, content_search::ContentSearch, profile::SearchProfile};
use crate::{
    dir::Directory,
    dirlist::list_directory,
    file::{File, FileOps},
    filter::Filter,
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::gio::{self, prelude::*};
use std::error::Error;

pub async fn generic_search(
    profile: &SearchProfile,
    start_dir: &Directory,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), ErrorMessage> {
    let name_filter = Filter::new(
        &profile.path_pattern(),
        profile.content_match_case(),
        profile.path_syntax(),
    )
    .map_err(|error| ErrorMessage::with_error(gettext("Bad expression"), &*error))?;

    let mut content_search = if !profile.content_pattern().is_empty() {
        Some(ContentSearch::new(
            &profile.content_pattern(),
            profile.content_match_case(),
        )?)
    } else {
        None
    };

    search_dir_recursive(
        start_dir,
        profile.max_depth(),
        &name_filter,
        &mut content_search,
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
    content_search: &mut Option<ContentSearch>,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), Box<dyn Error>> {
    if cancellable.is_cancelled() {
        return Ok(());
    }

    (on_message)(SearchMessage::Status(
        gettext("Searching in: {}").replace("{}", &dir.display_path()),
    ));

    let dir_files = dir.files();
    let files: Vec<_> = if dir_files.n_items() == 0 || dir.connection().is_local() {
        // if list is not available or it's a local directory then create a new list, otherwise use already available list
        // gnome_cmd_dir_list_files is not used for creating a list, because it's tied to the GUI and that's not usable from other threads
        list_directory(dir, None).await?.into_iter().collect()
    } else {
        dir_files
            .iter::<File>()
            .flatten()
            .map(|file| file.file_info())
            .collect()
    };

    // first process the files
    for info in &files {
        if cancellable.is_cancelled() {
            return Ok(());
        }
        if info.file_type() != gio::FileType::Regular {
            continue;
        }
        if !name_filter.matches(&info.display_name()) {
            continue;
        }

        let file = dir.file().child(info.name());
        if let Some(content_search) = content_search
            && !content_search.check(&file, cancellable)
        {
            continue;
        }

        // the file matched the search criteria, let's add it to the list
        (on_message)(SearchMessage::File(File::new_from_file(file, info)));
    }

    if level == 0 {
        return Ok(());
    }

    // then process the directories
    for info in &files {
        if cancellable.is_cancelled() {
            return Ok(());
        }
        if info.file_type() != gio::FileType::Directory {
            continue;
        }
        // we don't want to go backwards or to follow symlinks
        let display_name = info.display_name();
        if display_name != "." && display_name != ".." && !info.is_symlink() {
            Box::pin(search_dir_recursive(
                &dir.child(info.name()),
                level - 1,
                name_filter,
                content_search,
                on_message,
                cancellable,
            ))
            .await?;
        }
    }

    Ok(())
}
