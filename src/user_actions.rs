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
    data::{GeneralOptions, GeneralOptionsRead, ProgramsOptions, ProgramsOptionsRead},
    dialogs::{
        chmod_dialog::show_chmod_dialog, chown_dialog::show_chown_dialog,
        create_symlink_dialog::show_create_symlink_dialog,
    },
    dir::Directory,
    file::File,
    libgcmd::file_base::{FileBase, FileBaseExt},
    main_win::{ffi::*, MainWindow},
    spawn::{spawn_async, spawn_async_command, SpawnError},
    types::FileSelectorID,
    utils::{
        get_modifiers_state, prompt_message, run_simple_dialog, show_error_message, show_message,
        sudo_command, ErrorMessage,
    },
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gdk,
    gio::{self, ffi::GSimpleAction},
    glib::{self, ffi::GVariant, translate::FromGlibPtrNone, Cast},
    prelude::*,
};
use std::{collections::HashSet, ffi::OsString, path::PathBuf};

#[no_mangle]
pub extern "C" fn file_chmod(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    let files = file_list.selected_files();
    if !files.is_empty() {
        glib::spawn_future_local(async move {
            if show_chmod_dialog(main_win.upcast_ref(), &files).await {
                file_list.reload();
            }
        });
    }
}

#[no_mangle]
pub extern "C" fn file_chown(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    let files = file_list.selected_files();
    if !files.is_empty() {
        glib::spawn_future_local(async move {
            if show_chown_dialog(main_win.upcast_ref(), &files).await {
                file_list.reload();
            }
        });
    }
}

fn symlink_name(file_name: &str, options: &dyn GeneralOptionsRead) -> String {
    let mut format = options.symlink_format();
    if format.is_empty() {
        format = gettext("link to %s");
    }
    if !format.contains("%s") {
        format.push_str(" %s");
    }
    format.replace("%s", file_name)
}

async fn create_symlinks(
    parent_window: &gtk::Window,
    files: &glib::List<File>,
    directory: &Directory,
    options: &dyn GeneralOptionsRead,
) {
    let mut skip_all = false;

    for file in files {
        let target_absolute_name = file.file().parse_name();

        let symlink_file = directory.get_child_gfile(&symlink_name(&file.get_name(), options));

        loop {
            match symlink_file.make_symbolic_link(&target_absolute_name, gio::Cancellable::NONE) {
                Ok(_) => {
                    directory.file_created(&symlink_file.uri());
                    break;
                }
                Err(_) if skip_all => {
                    // do nothing
                }
                Err(error) => {
                    let choice = run_simple_dialog(
                        parent_window,
                        true,
                        gtk::MessageType::Question,
                        &error.message(),
                        &gettext("Create Symbolic Link"),
                        Some(3),
                        &[
                            &gettext("Skip"),
                            &gettext("Skip all"),
                            &gettext("Cancel"),
                            &gettext("Retry"),
                        ],
                    )
                    .await;

                    match choice {
                        gtk::ResponseType::Other(0) /* Skip */ => { break; },
                        gtk::ResponseType::Other(1) /* Skip all */ => { skip_all = true; break; },
                        gtk::ResponseType::Other(3) /* Retry */ => { continue; },
                        _ /* Cancel */ => { return },
                    }
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn file_create_symlink(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let options = GeneralOptions::new();

    glib::MainContext::default().spawn_local(async move {
        let active_fs = main_win.file_selector(FileSelectorID::ACTIVE);
        let inactive_fs = main_win.file_selector(FileSelectorID::INACTIVE);

        let Some(dest_directory) = inactive_fs.directory() else {
            eprintln!("Cannot create symlinks: No destination directory.");
            return;
        };

        let active_fl = active_fs.file_list();
        let selected_files = active_fl.selected_files();
        let selected_files_len = selected_files.len();

        if selected_files_len > 1 {
            let message = ngettext!(
                "Create symbolic links of {} file in {}?",
                "Create symbolic links of {} files in {}?",
                selected_files_len as u32,
                selected_files_len,
                dest_directory.display_path()
            );
            let choice = run_simple_dialog(
                main_win.upcast_ref(),
                true,
                gtk::MessageType::Question,
                &message,
                &gettext("Create Symbolic Link"),
                Some(1),
                &[&gettext("Cancel"), &gettext("Create")],
            )
            .await;
            if choice == gtk::ResponseType::Other(1) {
                create_symlinks(
                    main_win.upcast_ref(),
                    &selected_files,
                    &dest_directory,
                    &options,
                )
                .await;
            }
        } else if let Some(focused_file) = active_fl.focused_file() {
            show_create_symlink_dialog(
                main_win.upcast_ref(),
                &focused_file,
                &dest_directory,
                &symlink_name(&focused_file.get_name(), &options),
            )
            .await;
        }
    });
}

/************** Command Menu **************/

#[no_mangle]
pub extern "C" fn command_execute(
    _action: *const GSimpleAction,
    parameter_ptr: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let parameter = unsafe { glib::Variant::from_glib_none(parameter_ptr) };

    let Some(command_template) = parameter.str() else {
        show_message(
            main_win.upcast_ref(),
            &gettext("No command was given."),
            None,
        );
        return;
    };

    let fs = main_win.file_selector(FileSelectorID::ACTIVE);
    let fl = fs.file_list();

    let sfl = fl.selected_files();

    let grouped = sfl
        .iter()
        .map(|f| f.file().parent().and_then(|p| p.path()))
        .collect::<HashSet<Option<PathBuf>>>();
    let dir_path = if grouped.len() == 1 {
        grouped.into_iter().next().flatten()
    } else {
        None
    };
    // TODO: gnome_cmd_dir_is_local (dir) ? dir_path.c_str() : nullptr

    if let Err(error_message) =
        spawn_async(dir_path.as_deref(), &sfl, command_template).map_err(SpawnError::into_message)
    {
        show_error_message(main_win.upcast_ref(), &error_message);
    }
}

pub fn open_terminal(
    main_win: &MainWindow,
    options: &dyn ProgramsOptionsRead,
) -> Result<(), ErrorMessage> {
    let dpath = main_win
        .file_selector(FileSelectorID::ACTIVE)
        .directory()
        .and_then(|d| d.upcast_ref::<FileBase>().file().path());

    let command = OsString::from(options.terminal_cmd());
    if command.is_empty() {
        return Err(ErrorMessage {
            message: gettext("Terminal command is not configured properly."),
            secondary_text: None,
        });
    }

    spawn_async_command(dpath.as_deref(), &command).map_err(SpawnError::into_message)
}

/// this function is NOT exposed to user as UserAction
#[no_mangle]
pub extern "C" fn command_open_terminal__internal(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let options = ProgramsOptions::new();

    let shift = get_modifiers_state(main_win.upcast_ref())
        .map_or(false, |m| m.contains(gdk::ModifierType::SHIFT_MASK));

    let result = if shift {
        open_terminal_as_root(&main_win, &options)
    } else {
        open_terminal(&main_win, &options)
    };
    if let Err(error_message) = result {
        show_error_message(main_win.upcast_ref(), &error_message);
    }
}

/// Executes the command stored in `gnome_cmd_data.options.termopen` in the active directory.
#[no_mangle]
pub extern "C" fn command_open_terminal(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let options = ProgramsOptions::new();
    if let Err(error_message) = open_terminal(&main_win, &options) {
        show_error_message(main_win.upcast_ref(), &error_message);
    }
}

/// Combines the command stored in `gnome_cmd_data.options.termopen` with a
/// command for a GUI for root login screen. The command is executed in
/// the active directory afterwards.
pub fn open_terminal_as_root(
    main_win: &MainWindow,
    options: &dyn ProgramsOptionsRead,
) -> Result<(), ErrorMessage> {
    let mut command = sudo_command()?;
    command.args(&terminal_cmd(options)?);

    if let Some(d) = main_win
        .file_selector(FileSelectorID::ACTIVE)
        .directory()
        .and_then(|d| d.upcast_ref::<FileBase>().file().path())
    {
        command.current_dir(d);
    }

    command.spawn().map_err(|error| {
        ErrorMessage::with_error(gettext("Unable to open terminal in root mode."), &error)
    })?;
    Ok(())
}

pub fn terminal_cmd(options: &dyn ProgramsOptionsRead) -> Result<Vec<OsString>, ErrorMessage> {
    let cmd = glib::shell_parse_argv(options.terminal_cmd()).map_err(|error| {
        ErrorMessage::with_error(
            gettext("Terminal command is not configured properly."),
            &error,
        )
    })?;
    Ok(cmd)
}

#[no_mangle]
pub extern "C" fn command_open_terminal_as_root(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let options = ProgramsOptions::new();
    if let Err(error_message) = open_terminal_as_root(&main_win, &options) {
        show_error_message(main_win.upcast_ref(), &error_message);
    }
}

pub fn root_mode() -> Result<(), ErrorMessage> {
    let prgname = glib::prgname().ok_or_else(|| ErrorMessage {
        message: gettext("Cannot determine executable of Gnome Commander"),
        secondary_text: None,
    })?;
    sudo_command()?.arg(&prgname).spawn().map_err(|error| {
        ErrorMessage::with_error(
            gettext("Unable to start GNOME Commander in root mode."),
            &error,
        )
    })?;
    Ok(())
}

#[no_mangle]
pub extern "C" fn command_root_mode(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    if let Err(error_message) = root_mode() {
        show_error_message(main_win.upcast_ref(), &error_message);
    }
}

/* ***************************** View Menu ****************************** */

async fn ask_close_locked_tab(parent_window: &gtk::Window) -> bool {
    prompt_message(
        parent_window,
        gtk::MessageType::Question,
        gtk::ButtonsType::OkCancel,
        &gettext("The tab is locked, close anyway?"),
        None,
    )
    .await
        == gtk::ResponseType::Ok
}

#[no_mangle]
pub extern "C" fn view_close_tab(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    glib::MainContext::default().spawn_local(async move {
        let fs = main_win.file_selector(FileSelectorID::ACTIVE);
        if fs.tab_count() > 1 {
            if !fs.file_list().is_locked() || ask_close_locked_tab(main_win.upcast_ref()).await {
                fs.close_tab();
            }
        }
    });
}
