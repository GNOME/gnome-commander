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
    data::{ProgramsOptions, ProgramsOptionsRead},
    file::{ffi::GnomeCmdFile, File},
    libgcmd::file_base::FileBaseExt,
    transfer::gnome_cmd_tmp_download,
    utils::{
        run_simple_dialog, show_error_message, temp_file, ErrorMessage, GNOME_CMD_PERM_USER_EXEC,
    },
};
use gettextrs::gettext;
use gtk::{
    ffi::GtkWindow,
    gio,
    glib::{self, ffi::gboolean, translate::FromGlibPtrNone},
    prelude::*,
};

async fn ask_make_executable(parent_window: &gtk::Window, file: &File) -> bool {
    let msg = gettext!("“{}” seems to be a binary executable file but it lacks the executable bit. Do you want to set it and then run the file?", &file.get_name());
    let ret = run_simple_dialog(
        parent_window,
        false,
        gtk::MessageType::Question,
        &msg,
        &gettext("Make Executable?"),
        None,
        &[&gettext("Cancel"), &gettext("OK")],
    )
    .await;
    ret == gtk::ResponseType::Other(1)
}

enum OpenText {
    Cancel,
    Display,
    Run,
}

async fn ask_open_text(parent_window: &gtk::Window, file: &File) -> OpenText {
    let msg = gettext!(
        "“{}” is an executable text file. Do you want to run it, or display its contents?",
        file.get_name()
    );
    let ret = run_simple_dialog(
        parent_window,
        false,
        gtk::MessageType::Question,
        &msg,
        &gettext("Run or Display"),
        None,
        &[&gettext("Cancel"), &gettext("Display"), &gettext("Run")],
    )
    .await;

    if ret == gtk::ResponseType::Other(1) {
        OpenText::Display
    } else if ret == gtk::ResponseType::Other(2) {
        OpenText::Run
    } else {
        OpenText::Cancel
    }
}

async fn ask_download_tmp(parent_window: &gtk::Window, app: &App) -> bool {
    let msg = gettext!("{} does not know how to open remote file. Do you want to download the file to a temporary location and then open it?", app.name());
    let dialog = gtk::MessageDialog::builder()
        .transient_for(parent_window)
        .modal(true)
        .message_type(gtk::MessageType::Question)
        .buttons(gtk::ButtonsType::YesNo)
        .text(msg)
        .build();
    let response = dialog.run_future().await;
    dialog.close();

    response == gtk::ResponseType::Yes
}

async fn mime_exec_single(
    parent_window: &gtk::Window,
    file: &File,
    options: &dyn ProgramsOptionsRead,
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
            file.execute();
            return Ok(());
        } else if content_type
            .as_ref()
            .map_or(false, |c| c.starts_with("text/"))
        {
            match ask_open_text(parent_window, file).await {
                OpenText::Cancel => return Ok(()),
                OpenText::Display => {}
                OpenText::Run => {
                    file.execute();
                    return Ok(());
                }
            }
        }
    }

    let Some(app_info) = file.app_info_for_content_type() else {
        return Err(ErrorMessage {
            message: gettext!(
                "No default application found for the file type {}.",
                content_type.as_deref().unwrap_or("Unknown")
            ),
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
    } else if app.handles_uris() && options.dont_download() {
        let uri = file.get_uri_str().ok_or_else(|| ErrorMessage {
            message: gettext!("A file {} has no URI", file.get_name()),
            secondary_text: None,
        })?;
        app_info.launch_uris(&[&uri], gio::AppLaunchContext::NONE)
    } else {
        if !ask_download_tmp(parent_window, &app).await {
            return Ok(());
        }

        let tmp_file = temp_file(file)?;
        if !gnome_cmd_tmp_download(
            parent_window.clone(),
            single_file_list(file.file()),
            single_file_list(tmp_file.file()),
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
        ErrorMessage::with_error(gettext!("Launch of {} failed.", app.name()), &error)
    })?;

    Ok(())
}

fn single_file_list(file: gio::File) -> glib::List<gio::File> {
    let mut list = glib::List::new();
    list.push_back(file);
    list
}

#[no_mangle]
pub extern "C" fn mime_exec_file(
    parent_window_ptr: *mut GtkWindow,
    file_ptr: *mut GnomeCmdFile,
) -> gboolean {
    if file_ptr.is_null() {
        return 0;
    }

    let parent_window = unsafe { gtk::Window::from_glib_none(parent_window_ptr) };
    let file = unsafe { File::from_glib_none(file_ptr) };

    if file.file_info().file_type() == gio::FileType::Regular {
        glib::MainContext::default().spawn_local(async move {
            let options = ProgramsOptions::new();
            if let Err(error) = mime_exec_single(&parent_window, &file, &options).await {
                show_error_message(&parent_window, &error);
            }
        });
        1
    } else {
        0
    }
}
