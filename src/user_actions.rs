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
    config::{PACKAGE_BUGREPORT, PACKAGE_NAME, PACKAGE_URL, PACKAGE_VERSION},
    connection::{
        connection::{Connection, ConnectionExt},
        home::ConnectionHome,
        list::ConnectionList,
    },
    data::{
        ConfirmOptions, GeneralOptions, GeneralOptionsRead, ProgramsOptions, ProgramsOptionsRead,
    },
    dialogs::{
        chmod_dialog::show_chmod_dialog, chown_dialog::show_chown_dialog,
        connect_dialog::ConnectDialog, create_symlink_dialog::show_create_symlink_dialog,
        make_copy_dialog::make_copy_dialog, prepare_copy_dialog::prepare_copy_dialog_show,
        prepare_move_dialog::prepare_move_dialog_show, remote_dialog::RemoteDialog,
    },
    dir::Directory,
    file::File,
    libgcmd::file_base::{FileBase, FileBaseExt},
    main_win::{ffi::*, MainWindow},
    spawn::{spawn_async, spawn_async_command, SpawnError},
    types::FileSelectorID,
    utils::{
        display_help, get_modifiers_state, prompt_message, run_simple_dialog, show_error_message,
        show_message, sudo_command, ErrorMessage,
    },
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gdk,
    gio::{self, ffi::GSimpleAction},
    glib::{self, ffi::GVariant, translate::FromGlibPtrNone},
    prelude::*,
};
use std::{collections::HashSet, ffi::OsString, path::PathBuf};

#[no_mangle]
pub extern "C" fn file_copy(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    let src_fs = main_win.file_selector(FileSelectorID::ACTIVE);
    let dst_fs = main_win.file_selector(FileSelectorID::INACTIVE);
    let options = ConfirmOptions::new();
    glib::spawn_future_local(async move {
        prepare_copy_dialog_show(&main_win, &src_fs, &dst_fs, &options).await;
    });
}

#[no_mangle]
pub extern "C" fn file_copy_as(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let file_list = main_win.file_selector(FileSelectorID::ACTIVE).file_list();

    let Some(file) = file_list.selected_file() else {
        return;
    };
    let Some(dir) = file_list.directory() else {
        return;
    };

    glib::spawn_future_local(async move {
        make_copy_dialog(&file, &dir, &main_win).await;
    });
}

#[no_mangle]
pub extern "C" fn file_move(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    let src_fs = main_win.file_selector(FileSelectorID::ACTIVE);
    let dst_fs = main_win.file_selector(FileSelectorID::INACTIVE);
    let options = ConfirmOptions::new();
    glib::spawn_future_local(async move {
        prepare_move_dialog_show(&main_win, &src_fs, &dst_fs, &options).await;
    });
}

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

    glib::spawn_future_local(async move {
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
            let message = ngettext(
                "Create symbolic links of {count} file in {dir}?",
                "Create symbolic links of {count} files in {dir}?",
                selected_files_len as u32,
            )
            .replace("{count}", &selected_files_len.to_string())
            .replace("{dir}", &dest_directory.display_path());

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

#[no_mangle]
pub extern "C" fn file_sendto(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let options = ProgramsOptions::new();

    glib::spawn_future_local(async move {
        let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
        let file_list = file_selector.file_list();
        let files = file_list.selected_files();

        let command_template = options.sendto_cmd();

        if command_template == "xdg-email --attach %s" && files.len() > 1 {
            ErrorMessage::new(gettext("Warning"), Some(gettext("The default send-to command only supports one selected file at a time. You can change the command in the program options."))).show(main_win.upcast_ref()).await;
        }

        if let Err(error) = spawn_async(None, &files, &command_template) {
            error.into_message().show(main_win.upcast_ref()).await;
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
    glib::spawn_future_local(async move {
        let fs = main_win.file_selector(FileSelectorID::ACTIVE);
        if fs.tab_count() > 1 {
            if !fs.file_list().is_locked() || ask_close_locked_tab(main_win.upcast_ref()).await {
                fs.close_tab();
            }
        }
    });
}

/************** Connections Menu **************/

#[no_mangle]
pub extern "C" fn connections_open(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    let dialog = RemoteDialog::new(&main_win);
    dialog.present();
}

#[no_mangle]
pub extern "C" fn connections_new(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    glib::spawn_future_local(async move {
        // let con: GnomeCmdConRemote = gnome_cmd_data.get_quick_connect();
        if let Some(connection) = ConnectDialog::new_connection(main_win.upcast_ref(), false).await
        {
            let fs = main_win.file_selector(FileSelectorID::ACTIVE);
            if fs.file_list().is_locked() {
                fs.new_tab();
            }
            fs.set_connection(connection.upcast_ref(), None);
        }
    });
}

#[no_mangle]
pub extern "C" fn connections_change_left(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    main_win.change_connection(FileSelectorID::LEFT);
}

#[no_mangle]
pub extern "C" fn connections_change_right(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    main_win.change_connection(FileSelectorID::RIGHT);
}

#[no_mangle]
pub extern "C" fn connections_set_current(
    _action: *const GSimpleAction,
    parameter_ptr: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let parameter = unsafe { glib::Variant::from_glib_none(parameter_ptr) };

    let Some(uuid) = parameter.str() else {
        eprintln!("No connection uuid was given.");
        return;
    };

    let con_list = ConnectionList::get();
    let Some(con) = con_list.find_by_uuid(uuid) else {
        eprintln!("No connection corresponds to {uuid}");
        return;
    };

    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .set_connection(&con, None);
}

fn close_connection(main_win: &MainWindow, con: &Connection) {
    let active = main_win.file_selector(FileSelectorID::ACTIVE).file_list();
    let inactive = main_win.file_selector(FileSelectorID::INACTIVE).file_list();

    let home = ConnectionList::get().home();

    if active.connection().as_ref() == Some(con) {
        active.set_connection(&home, None);
    }

    if inactive.connection().as_ref() == Some(con) {
        inactive.set_connection(&home, None);
    }

    con.close();
}

#[no_mangle]
pub extern "C" fn connections_close(
    _action: *const GSimpleAction,
    parameter_ptr: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    let parameter = unsafe { glib::Variant::from_glib_none(parameter_ptr) };

    let Some(uuid) = parameter.str() else {
        eprintln!("No connection uuid was given.");
        return;
    };

    let con_list = ConnectionList::get();
    let Some(con) = con_list.find_by_uuid(uuid) else {
        eprintln!("No connection corresponds to {uuid}");
        return;
    };

    close_connection(&main_win, &con);
}

#[no_mangle]
pub extern "C" fn connections_close_current(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    if let Some(con) = main_win
        .file_selector(FileSelectorID::ACTIVE)
        .connection()
        .filter(|c| c.downcast_ref::<ConnectionHome>().is_none())
    {
        close_connection(&main_win, &con);
    }
}

/************** Help Menu **************/

#[no_mangle]
pub extern "C" fn help_help(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    display_help(main_win.upcast_ref(), None);
}

#[no_mangle]
pub extern "C" fn help_keyboard(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    display_help(main_win.upcast_ref(), Some("gnome-commander-keyboard"));
}

#[no_mangle]
pub extern "C" fn help_web(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    gtk::show_uri(Some(&main_win), PACKAGE_URL, gdk::CURRENT_TIME);
    // show_error(
    //     main_win.upcast_ref(),
    //     &gettext("There was an error opening home page."),
    //     &error,
    // );
}

#[no_mangle]
pub extern "C" fn help_problem(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };
    gtk::show_uri(Some(&main_win), PACKAGE_BUGREPORT, gdk::CURRENT_TIME);
    // show_error(
    //     main_win.upcast_ref(),
    //     &gettext("There was an error reporting problem."),
    //     &error,
    // );
}

#[no_mangle]
pub extern "C" fn help_about(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    let authors = [
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        "Assaf Gordon <agordon88@gmail.com>",
        "Uwe Scholz <u.scholz83@gmx.de>",
        "Andrey Kuteiko <andy128k@gmail.com>",
    ];

    let documenters = [
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        "Laurent Coudeur <laurentc@eircom.net>",
        "Uwe Scholz <u.scholz83@gmx.de>",
    ];

    let copyright = "Copyright \u{00A9} 2001-2006 Marcus Bjurman
Copyright \u{00A9} 2007-2012 Piotr Eljasiak
Copyright \u{00A9} 2013-2024 Uwe Scholz
Copyright \u{00A9} 2024 Andrey Kuteiko";

    let license = format!(
        "{}\n\n{}\n\n{}",
        gettext(
            "GNOME Commander is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version."
        ),
        gettext(
            "GNOME Commander is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details."
        ),
        gettext(
            "You should have received a copy of the GNU General Public License along with GNOME Commander; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA."
        )
    );

    gtk::AboutDialog::builder()
        .transient_for(&main_win)
        .name("GNOME Commander")
        .version(PACKAGE_VERSION)
        .comments(gettext(
            "A fast and powerful file manager for the GNOME desktop",
        ))
        .copyright(copyright)
        .license(license)
        .wrap_license(true)
        .authors(authors)
        .documenters(documenters)
        .logo_icon_name(PACKAGE_NAME)
        .translator_credits(gettext("translator-credits"))
        .website("https://gcmd.github.io")
        .website_label("GNOME Commander Website")
        .build()
        .present();
}
