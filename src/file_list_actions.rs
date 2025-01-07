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
    dialogs::open_with_other_dialog::show_open_with_other_dialog,
    file::File,
    file_edit::file_edit,
    file_list::{ffi::GnomeCmdFileList, FileList},
    file_view::file_view,
    libgcmd::file_descriptor::FileDescriptorExt,
    spawn::run_command_indir,
    transfer::gnome_cmd_tmp_download,
    utils::{get_modifiers_state, temp_file, ErrorMessage},
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gdk,
    gio::{self, ffi::GSimpleAction},
    glib::{self, ffi::GVariant, translate::FromGlibPtrNone, Variant},
    prelude::*,
};
use std::{ffi::OsString, rc::Rc};

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_action_file_view(
    _action: *const GSimpleAction,
    parameter_ptr: *const GVariant,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let file_list = unsafe { FileList::from_glib_none(file_list_ptr) };
    let parameter = unsafe { Option::<Variant>::from_glib_none(parameter_ptr) };
    let options = ProgramsOptions::new();

    let use_internal_viewer = parameter.and_then(|v| v.get::<bool>());

    let Some(parent_window) = file_list.root().and_downcast::<gtk::Window>() else {
        eprintln!("No window");
        return;
    };
    glib::spawn_future_local(async move {
        let Some(file) = file_list.selected_file() else {
            return;
        };
        if let Err(error) = file_view(&parent_window, &file, use_internal_viewer, &options).await {
            error.show(&parent_window).await;
        }
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_action_file_edit(
    _action: *const GSimpleAction,
    _parameter_ptr: *const GVariant,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let file_list = unsafe { FileList::from_glib_none(file_list_ptr) };
    let options = ProgramsOptions::new();

    let Some(parent_window) = file_list.root().and_downcast::<gtk::Window>() else {
        eprintln!("No window");
        return;
    };
    glib::spawn_future_local(async move {
        let files = file_list.selected_files();
        if let Err(error) = file_edit(&files, &options).await {
            error.show(&parent_window).await;
        }
    });
}

async fn ask_download_remote_files(
    parent_window: &gtk::Window,
    app_name: &str,
    no_of_remote_files: u32,
) -> bool {
    if no_of_remote_files > 0 {
        let msg = ngettext(
            "{} does not know how to open remote file. Do you want to download the file to a temporary location and then open it?",
            "{} does not know how to open remote files. Do you want to download the files to a temporary location and then open them?", no_of_remote_files)
            .replace("{}", app_name);
        gtk::AlertDialog::builder()
            .modal(true)
            .message(msg)
            .buttons([gettext("No"), gettext("Yes")])
            .cancel_button(0)
            .default_button(1)
            .build()
            .choose_future(Some(parent_window))
            .await
            == Ok(1)
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
        ErrorMessage::new(gettext("No files were given to open."), None::<String>)
            .show(parent_window)
            .await;
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
                    error.show(parent_window).await;
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
                ErrorMessage::new(gettext("Download failed."), None::<String>)
                    .show(parent_window)
                    .await;
                return;
            }
        }

        local.into_iter().collect()
    };

    if let Err(error) = app.launch(&files_to_open, options) {
        error.show(parent_window).await;
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
    glib::spawn_future_local(async move {
        let Some(app) = App::from_variant(&parameter) else {
            eprintln!("Cannot load app from a variant");
            return;
        };

        let Some(parent_window) = file_list.root().and_downcast::<gtk::Window>() else {
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
    glib::spawn_future_local(async move {
        let Some(parent_window) = file_list.root().and_downcast::<gtk::Window>() else {
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
                ErrorMessage::new(
                    file.file_info().display_name(),
                    Some(&gettext("Couldn’t retrieve MIME type of the file.")),
                )
                .show(&parent_window)
                .await;
            }
        }

        for (app_info, files) in grouped {
            let app = App::Regular(RegularApp { app_info });
            mime_exec_multiple(files, app, &parent_window, &options).await;
        }
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_action_open_with_other(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let file_list = unsafe { FileList::from_glib_none(file_list_ptr) };
    let Some(parent_window) = file_list.root().and_downcast() else {
        eprintln!("No window");
        return;
    };
    let options = Rc::new(ProgramsOptions::new());
    glib::spawn_future_local(async move {
        show_open_with_other_dialog(
            &parent_window,
            &file_list.selected_files(),
            file_list.cwd(),
            options,
        )
        .await;
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_action_execute(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let file_list = unsafe { FileList::from_glib_none(file_list_ptr) };
    let Some(parent_window) = file_list.root().and_downcast() else {
        eprintln!("No window");
        return;
    };
    let options = ProgramsOptions::new();
    let Some(file) = file_list.selected_files().front().cloned() else {
        eprintln!("No file selected");
        return;
    };

    glib::spawn_future_local(async move {
        match file.execute(&options) {
            Ok(_) => {}
            Err(error) => {
                error.into_message().show(&parent_window).await;
            }
        }
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_action_execute_script(
    _action: *const GSimpleAction,
    parameter_ptr: *const GVariant,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let file_list = unsafe { FileList::from_glib_none(file_list_ptr) };
    let parameter = unsafe { glib::Variant::from_glib_none(parameter_ptr) };

    let script_path: String = parameter.child_get(0);
    let run_in_terminal: bool = parameter.child_get(1);

    let Some(parent_window) = file_list.root().and_downcast() else {
        eprintln!("No window");
        return;
    };
    let options = ProgramsOptions::new();
    let files = file_list.selected_files();

    let mask = get_modifiers_state(&parent_window);
    let is_shift_pressed = mask.map_or(false, |m| {
        m.contains(gdk::ModifierType::SHIFT_MASK)
            && !m.contains(gdk::ModifierType::CONTROL_MASK)
            && !m.contains(gdk::ModifierType::ALT_MASK)
    });

    glib::spawn_future_local(async move {
        if is_shift_pressed {
            // Run script per file
            for file in files {
                let mut command = OsString::from(&script_path);
                command.push(" ");
                command.push(&glib::shell_quote(file.file_info().display_name()));

                let working_directory = file.file().parent().and_then(|p| p.path());
                if let Err(error) = run_command_indir(
                    working_directory.as_deref(),
                    &command,
                    run_in_terminal,
                    &options,
                ) {
                    error.into_message().show(&parent_window).await;
                }
            }
        } else {
            // Run script with list of files
            let mut command = OsString::from(script_path);
            for file in &files {
                command.push(" ");
                command.push(&glib::shell_quote(file.file_info().display_name()));
            }

            let working_directory = files
                .front()
                .and_then(|f| f.file().parent())
                .and_then(|p| p.path());
            if let Err(error) = run_command_indir(
                working_directory.as_deref(),
                &command,
                run_in_terminal,
                &options,
            ) {
                error.into_message().show(&parent_window).await;
            }
        }
    });
}
