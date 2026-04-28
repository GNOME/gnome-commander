// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    file::{File, FileOps},
    options::ProgramsOptions,
    spawn::{SpawnError, spawn_async},
    utils::ErrorMessage,
};
use gettextrs::gettext;

pub async fn file_edit(files: Vec<File>, options: &ProgramsOptions) -> Result<(), ErrorMessage> {
    let files = files
        .into_iter()
        .filter_map(|file| {
            if file.is_directory() {
                return Some(Err(ErrorMessage::new(
                    gettext("Not an ordinary file."),
                    Some(&file.name()),
                )));
            }
            if !file.is_local() {
                return None;
            }
            Some(Ok(file))
        })
        .collect::<Result<Vec<_>, _>>()?;

    if !files.is_empty() {
        spawn_async(None, &files, &options.editor_cmd.get()).map_err(SpawnError::into_message)?;
    }
    Ok(())
}
