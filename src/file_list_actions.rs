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
    app::{App, RegularApp},
    data::{ProgramsOptions, ProgramsOptionsRead},
    file::File,
    file_list::{ffi::GnomeCmdFileList, FileList},
    libgcmd::file_base::FileBaseExt,
    transfer::gnome_cmd_tmp_download,
    utils::{run_simple_dialog, show_error_message, show_message, temp_file, ErrorMessage},
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gio::{self, ffi::GSimpleAction},
    glib::{self, ffi::GVariant, translate::FromGlibPtrNone, FromVariant, Variant},
    prelude::*,
};

async fn ask_download_remote_files(
    parent_window: &gtk::Window,
    app_name: &str,
    no_of_remote_files: u32,
) -> bool {
    if no_of_remote_files > 0 {
        let msg = ngettext!(
            "{} does not know how to open remote file. Do you want to download the file to a temporary location and then open it?",
            "{} does not know how to open remote files. Do you want to download the files to a temporary location and then open them?", no_of_remote_files,
            app_name);
        run_simple_dialog(
            parent_window,
            true,
            gtk::MessageType::Question,
            &msg,
            "",
            None,
            &[&gettext("No"), &gettext("Yes")],
        )
        .await
            == gtk::ResponseType::Other(1)
    } else {
        false
    }
}

fn create_temporary_files(files: &[File]) -> Result<Vec<File>, ErrorMessage> {
    let mut temp_files = Vec::new();
    for file in files {
        temp_files.push(temp_file(file)?);
    }
    Ok(temp_files)
}

async fn mime_exec_multiple(
    files: glib::List<File>,
    app: App,
    parent_window: &gtk::Window,
    options: &dyn ProgramsOptionsRead,
) {
    if files.is_empty() {
        show_message(
            parent_window,
            &gettext("No files were given to open."),
            None,
        );
        return;
    }

    let files_to_open = if app.handles_uris() && options.dont_download() {
        files
    } else {
        let (mut local, remote): (Vec<File>, Vec<File>) =
            files.into_iter().partition(|f| f.is_local());

        if ask_download_remote_files(parent_window, &app.name(), remote.len() as u32).await {
            let tmp_files = match create_temporary_files(&remote) {
                Ok(files) => files,
                Err(error) => {
                    show_error_message(parent_window, &error);
                    return;
                }
            };

            if gnome_cmd_tmp_download(
                parent_window.clone(),
                remote.iter().map(|f| f.file()).collect(),
                tmp_files.iter().map(|f| f.file()).collect(),
                gio::FileCopyFlags::OVERWRITE,
            )
            .await
            {
                local.extend(tmp_files);
            } else {
                show_message(parent_window, &gettext("Download failed."), None);
                return;
            }
        }

        local.into_iter().collect()
    };

    if let Err(error) = app.launch(&files_to_open, options) {
        show_error_message(parent_window, &error);
    }
}

/// Executes a list of files with the same application
#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_action_open_with(
    _action: *const GSimpleAction,
    parameter_ptr: *const GVariant,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let file_list = unsafe { FileList::from_glib_none(file_list_ptr) };
    let parameter = unsafe { Variant::from_glib_none(parameter_ptr) };
    let options = ProgramsOptions::new();
    glib::MainContext::default().spawn_local(async move {
        let Some(app) = App::from_variant(&parameter) else {
            eprintln!("Cannot load app from a variant");
            return;
        };

        let Some(parent_window) = file_list.toplevel().and_downcast::<gtk::Window>() else {
            eprintln!("File list has no parent window");
            return;
        };
        let files = file_list.selected_files();

        mime_exec_multiple(files, app, &parent_window, &options).await;
    });
}

/// Iterates through all files and gets their default application.
/// All files with the same default app are grouped together and opened in one call.
#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_action_open_with_default(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let file_list = unsafe { FileList::from_glib_none(file_list_ptr) };
    let options = ProgramsOptions::new();
    glib::MainContext::default().spawn_local(async move {
        let Some(parent_window) = file_list.toplevel().and_downcast::<gtk::Window>() else {
            eprintln!("File list has no parent window");
            return;
        };
        let files = file_list.selected_files();

        let mut grouped: Vec<(gio::AppInfo, glib::List<File>)> = Vec::new();

        for file in files {
            if let Some(app_info) = file.app_info_for_content_type() {
                let pos = grouped
                    .iter()
                    .position(|(app, _)| app_info.equal(app))
                    .unwrap_or_else(|| {
                        let len = grouped.len();
                        grouped.push((app_info, glib::List::new()));
                        len
                    });
                grouped[pos].1.push_back(file);
            } else {
                show_message(
                    &parent_window,
                    &file.file_info().display_name(),
                    Some(&gettext("Couldn’t retrieve MIME type of the file.")),
                );
            }
        }

        for (app_info, files) in grouped {
            let app = App::Regular(RegularApp { app_info });
            mime_exec_multiple(files, app, &parent_window, &options).await;
        }
    });
}
