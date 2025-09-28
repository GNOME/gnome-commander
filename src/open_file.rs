/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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
    app::{App, RegularApp},
    file::File,
    options::options::ProgramsOptions,
    spawn::SpawnError,
    transfer::download_to_temporary,
    utils::{ErrorMessage, GNOME_CMD_PERM_USER_EXEC, temp_file},
};
use gettextrs::gettext;
use gtk::{gio, prelude::*};

async fn ask_make_executable(parent_window: &gtk::Window, file: &File) -> bool {
    let msg = gettext("“{}” seems to be a binary executable file but it lacks the executable bit. Do you want to set it and then run the file?")
        .replace("{}", &file.get_name());
    gtk::AlertDialog::builder()
        .modal(true)
        .message(msg)
        .buttons([gettext("_Cancel"), gettext("_OK")])
        .cancel_button(0)
        .default_button(1)
        .build()
        .choose_future(Some(parent_window))
        .await
        == Ok(1)
}

enum OpenText {
    Cancel,
    Display,
    Run,
}

async fn ask_open_text(parent_window: &gtk::Window, file: &File) -> OpenText {
    let msg =
        gettext("“{}” is an executable text file. Do you want to run it, or display its contents?")
            .replace("{}", &file.get_name());
    let response = gtk::AlertDialog::builder()
        .modal(true)
        .message(msg)
        .buttons([gettext("_Cancel"), gettext("_Display"), gettext("_Run")])
        .cancel_button(0)
        .build()
        .choose_future(Some(parent_window))
        .await;
    match response {
        Ok(1) => OpenText::Display,
        Ok(2) => OpenText::Run,
        _ => OpenText::Cancel,
    }
}

async fn ask_download_tmp(parent_window: &gtk::Window, app: &App) -> bool {
    let msg = gettext("{} does not know how to open remote file. Do you want to download the file to a temporary location and then open it?")
        .replace("{}", &app.name());
    gtk::AlertDialog::builder()
        .modal(true)
        .message(msg)
        .buttons([gettext("_No"), gettext("_Yes")])
        .cancel_button(0)
        .default_button(1)
        .build()
        .choose_future(Some(parent_window))
        .await
        == Ok(1)
}

pub async fn mime_exec_single(
    parent_window: &gtk::Window,
    file: &File,
    options: &ProgramsOptions,
) -> Result<(), ErrorMessage> {
    // Check if the file is a binary executable that lacks the executable bit

    let content_type = file.content_type();
    let is_executable_content_type = content_type.as_ref().map_or(false, |c| {
        c == "application/x-executable" || c == "application/x-executable-binary"
    });

    if !file.is_executable() && is_executable_content_type {
        if !ask_make_executable(parent_window, file).await {
            return Ok(());
        }

        file.chmod(
            file.file_info()
                .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE)
                | GNOME_CMD_PERM_USER_EXEC,
        )
        .map_err(|error| ErrorMessage {
            message: gettext("Change of a file mode failed"),
            secondary_text: Some(error.to_string()),
        })?;
    }

    // If the file is executable but not a binary file, check if the user wants to exec it or open it

    if file.is_executable() {
        if is_executable_content_type {
            file.execute(options).map_err(SpawnError::into_message)?;
            return Ok(());
        } else if content_type
            .as_ref()
            .map_or(false, |c| c.starts_with("text/"))
        {
            match ask_open_text(parent_window, file).await {
                OpenText::Cancel => return Ok(()),
                OpenText::Display => {}
                OpenText::Run => {
                    file.execute(options).map_err(SpawnError::into_message)?;
                    return Ok(());
                }
            }
        }
    }

    let Some(app_info) = file.app_info_for_content_type() else {
        return Err(ErrorMessage {
            message: gettext("No default application found for the file type {}.")
                .replace("{}", content_type.as_deref().unwrap_or("Unknown")),
            secondary_text: Some(gettext(
                "Open the \"Applications\" page in the Control Center to add one.",
            )),
        });
    };

    let app = App::Regular(RegularApp {
        app_info: app_info.clone(),
    });

    if file.is_local() {
        app_info.launch(&[file.file()], gio::AppLaunchContext::NONE)
    } else if app.handles_uris() && options.dont_download.get() {
        app_info.launch_uris(&[&file.get_uri_str()], gio::AppLaunchContext::NONE)
    } else {
        if !ask_download_tmp(parent_window, &app).await {
            return Ok(());
        }

        let tmp_file = temp_file(file)?;
        if !download_to_temporary(
            parent_window.clone(),
            vec![file.file()],
            vec![tmp_file.file()],
            gio::FileCopyFlags::OVERWRITE,
        )
        .await
        {
            // TODO ensure that error is shown
            return Ok(());
        }

        app_info.launch(&[tmp_file.file()], gio::AppLaunchContext::NONE)
    }
    .map_err(|error| {
        ErrorMessage::with_error(
            gettext("Launch of {} failed.").replace("{}", &app.name()),
            &error,
        )
    })?;

    Ok(())
}
