// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    dir::Directory,
    file::{File, FileOps},
};
use gtk::{gio, glib, prelude::*};

const FILES_PER_UPDATE: i32 = 50;

pub async fn list_directory(
    dir: &Directory,
    cancellable: Option<&gio::Cancellable>,
) -> Result<Vec<gio::FileInfo>, glib::Error> {
    let file = dir.file().clone();

    let mut files = Vec::new();

    let enumerator = file
        .enumerate_children_future(
            File::DEFAULT_ATTRIBUTES,
            gio::FileQueryInfoFlags::NONE,
            glib::Priority::DEFAULT,
        )
        .await?;

    let mut error = None;
    loop {
        match enumerator
            .next_files_future(FILES_PER_UPDATE, glib::Priority::DEFAULT)
            .await
        {
            Ok(chunk) if chunk.is_empty() => {
                break;
            }
            Ok(chunk) => {
                files.extend(chunk);
                if cancellable.is_some_and(|c| c.is_cancelled()) {
                    break;
                }
            }
            Err(e) => {
                error = Some(e);
                break;
            }
        }
    }

    enumerator.close_future(glib::Priority::DEFAULT).await?;

    match error {
        Some(error) => Err(error),
        None => Ok(files),
    }
}
