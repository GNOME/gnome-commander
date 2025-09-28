/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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

use crate::{
    file::File,
    intviewer::window::ViewerWindow,
    options::options::ProgramsOptions,
    spawn::{SpawnError, spawn_async},
    tags::tags::FileMetadataService,
    transfer::download_to_temporary,
    utils::{ErrorMessage, temp_file},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

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
        if !download_to_temporary(
            parent_window.clone(),
            vec![f.file()],
            vec![tmp_file.file()],
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
    let mut files = glib::List::new();
    files.push_back(file.clone());
    spawn_async(None, &files, &options.viewer_cmd.get()).map_err(SpawnError::into_message)
}

pub async fn file_view(
    parent_window: &gtk::Window,
    file: &File,
    use_internal_viewer: Option<bool>,
    options: &ProgramsOptions,
    file_metadata_service: &FileMetadataService,
) -> Result<(), ErrorMessage> {
    if file.file_info().file_type() == gio::FileType::Directory {
        return Err(ErrorMessage::new(
            gettext("Not an ordinary file."),
            Some(file.file_info().display_name().as_str()),
        ));
    }

    let use_internal_viewer =
        use_internal_viewer.unwrap_or_else(|| options.use_internal_viewer.get());

    if use_internal_viewer {
        file_view_internal(&parent_window, &file, &file_metadata_service).await?;
    } else {
        file_view_external(&file, options).await?;
    }
    Ok(())
}
