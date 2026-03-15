// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    file::{File, FileOps},
    intviewer::window::ViewerWindow,
    options::ProgramsOptions,
    spawn::{SpawnError, spawn_async},
    tags::FileMetadataService,
    transfer::download_to_temporary,
    utils::{ErrorMessage, temp_file},
};
use gettextrs::gettext;
use gtk::{gio, prelude::*};

pub async fn file_view_internal(
    parent_window: &gtk::Window,
    f: &File,
    file_metadata_service: &FileMetadataService,
) -> Result<(), ErrorMessage> {
    let file_to_view = if f.is_local() {
        // If the file is local there is no need to download it
        f.clone()
    } else {
        // The file is remote, let's download it to a temporary file first
        let tmp_file = temp_file(f)?;
        let src_files = vec![f.file().clone()];
        let dst_files = vec![tmp_file.file().clone()];
        if !download_to_temporary(
            parent_window.clone(),
            src_files,
            dst_files,
            gio::FileCopyFlags::OVERWRITE,
        )
        .await
        {
            return Err(ErrorMessage {
                message: gettext("Failed to download a file"),
                secondary_text: None,
            });
        }
        tmp_file
    };

    let viewer = ViewerWindow::file_view(&file_to_view, file_metadata_service);
    viewer.present();
    Ok(())
}

pub async fn file_view_external(
    file: &File,
    options: &ProgramsOptions,
) -> Result<(), ErrorMessage> {
    spawn_async(None, std::slice::from_ref(file), &options.viewer_cmd.get())
        .map_err(SpawnError::into_message)
}

pub async fn file_view(
    parent_window: &gtk::Window,
    file: &File,
    use_internal_viewer: Option<bool>,
    options: &ProgramsOptions,
    file_metadata_service: &FileMetadataService,
) -> Result<(), ErrorMessage> {
    if file.is_directory() {
        return Err(ErrorMessage::new(
            gettext("Not an ordinary file."),
            Some(&file.name()),
        ));
    }

    let use_internal_viewer =
        use_internal_viewer.unwrap_or_else(|| options.use_internal_viewer.get());

    if use_internal_viewer {
        file_view_internal(parent_window, file, file_metadata_service).await?;
    } else {
        file_view_external(file, options).await?;
    }
    Ok(())
}
