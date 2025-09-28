/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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
    advanced_rename::advanced_rename_dialog::advanced_rename_dialog_show,
    config::{PACKAGE_BUGREPORT, PACKAGE_NAME, PACKAGE_URL, PACKAGE_VERSION},
    connection::{
        bookmark::{Bookmark, BookmarkGoToVariant},
        connection::{Connection, ConnectionExt, ConnectionInterface},
        home::ConnectionHome,
        list::ConnectionList,
        remote::ConnectionRemoteExt,
    },
    dialogs::{
        chmod_dialog::show_chmod_dialog,
        chown_dialog::show_chown_dialog,
        connect_dialog::ConnectDialog,
        create_symlink_dialog::show_create_symlink_dialog,
        file_properties_dialog::FilePropertiesDialog,
        make_copy_dialog::make_copy_dialog,
        manage_bookmarks_dialog::{BookmarksDialog, bookmark_directory},
        mkdir_dialog::show_mkdir_dialog,
        new_text_file::show_new_textfile_dialog,
        options::options_dialog::show_options_dialog,
        pattern_selection_dialog::select_by_pattern,
        prepare_copy_dialog::prepare_copy_dialog_show,
        prepare_move_dialog::prepare_move_dialog_show,
        remote_dialog::RemoteDialog,
        shortcuts::shortcuts_dialog::ShortcutsDialog,
    },
    dir::Directory,
    file::File,
    file_list::list::FileList,
    libgcmd::{
        file_actions::{FileActions, FileActionsExt},
        file_descriptor::FileDescriptorExt,
    },
    main_win::MainWindow,
    options::options::{ConfirmOptions, GeneralOptions, NetworkOptions, ProgramsOptions},
    plugin_manager::{PluginActionVariant, show_plugin_manager},
    spawn::{SpawnError, spawn_async, spawn_async_command},
    types::FileSelectorID,
    utils::{ErrorMessage, display_help, get_modifiers_state},
};
use gettextrs::{gettext, ngettext};
use gtk::{gdk, gio, glib, prelude::*};
use std::{
    borrow::Cow,
    cmp::Ordering,
    collections::{HashMap, HashSet},
    ffi::OsString,
    path::{Path, PathBuf},
    sync::LazyLock,
};

pub fn file_copy(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let src_fs = main_win.file_selector(FileSelectorID::ACTIVE);
    let dst_fs = main_win.file_selector(FileSelectorID::INACTIVE);
    let options = ConfirmOptions::new();
    glib::spawn_future_local(async move {
        prepare_copy_dialog_show(&main_win, &src_fs, &dst_fs, &options).await;
    });
}

pub fn file_copy_as(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
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

pub fn file_move(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let src_fs = main_win.file_selector(FileSelectorID::ACTIVE);
    let dst_fs = main_win.file_selector(FileSelectorID::INACTIVE);
    let options = ConfirmOptions::new();
    glib::spawn_future_local(async move {
        prepare_move_dialog_show(&main_win, &src_fs, &dst_fs, &options).await;
    });
}

pub fn file_delete(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        main_win
            .file_selector(FileSelectorID::ACTIVE)
            .file_list()
            .show_delete_dialog(false)
            .await;
    });
}

pub fn file_view(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    if let Err(error) = file_list.activate_action("fl.file-view", Some(&None::<bool>.to_variant()))
    {
        eprintln!("Cannot activate action `file-view`: {}", error);
    }
}

pub fn file_internal_view(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    if let Err(error) = file_list.activate_action("fl.file-view", Some(&true.to_variant())) {
        eprintln!("Cannot activate action `file-view`: {}", error);
    }
}

pub fn file_external_view(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    if let Err(error) = file_list.activate_action("fl.file-view", Some(&false.to_variant())) {
        eprintln!("Cannot activate action `file-view`: {}", error);
    }
}

pub fn file_edit(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    let mask = get_modifiers_state(main_win.upcast_ref());

    glib::spawn_future_local(async move {
        if mask.map_or(false, |m| m.contains(gdk::ModifierType::SHIFT_MASK)) {
            show_new_textfile_dialog(main_win.upcast_ref(), &file_list).await;
        } else {
            if let Err(error) = file_list.activate_action("fl.file-edit", None) {
                eprintln!("Cannot activate action `file-edit`: {}", error);
            }
        }
    });
}

pub fn file_edit_new_doc(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    glib::spawn_future_local(async move {
        show_new_textfile_dialog(main_win.upcast_ref(), &file_list).await;
    });
}

pub fn file_search(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let options = ProgramsOptions::new();

    glib::spawn_future_local(async move {
        let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
        let file_list = file_selector.file_list();

        if options.use_internal_search.get() {
            let start_dir = file_list.directory();
            let dlg = main_win.get_or_create_search_dialog();
            dlg.show_and_set_focus(start_dir.as_ref());
        } else {
            fn no_search_command_error() -> String {
                format!(
                    "{}\n{}",
                    gettext("No search command given."),
                    gettext("You can set a command for a search tool in the program options.")
                )
            }

            let files = file_list.selected_files();
            let error_message = match spawn_async(None, &files, &options.search_cmd.get()) {
                Ok(_) => return,
                Err(SpawnError::InvalidTemplate) => ErrorMessage::brief(no_search_command_error()),
                Err(SpawnError::InvalidCommand(e)) => {
                    ErrorMessage::with_error(no_search_command_error(), &e)
                }
                Err(SpawnError::Failure(e)) => {
                    ErrorMessage::with_error(gettext("Unable to execute command."), &e)
                }
            };
            if let Some(window) = main_win.root().and_downcast::<gtk::Window>() {
                error_message.show(&window).await;
            } else {
                eprintln!(
                    "No window. Search failed: {}\n{}",
                    error_message.message,
                    error_message.secondary_text.unwrap_or_default()
                );
            }
        }
    });
}

pub fn file_quick_search(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    file_list.show_quick_search(None);
}

pub fn file_chmod(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    let files = file_list.selected_files();
    if !files.is_empty() {
        glib::spawn_future_local(async move {
            if show_chmod_dialog(main_win.upcast_ref(), &files).await {
                file_list.reload().await;
            }
        });
    }
}

pub fn file_chown(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    let files = file_list.selected_files();
    if !files.is_empty() {
        glib::spawn_future_local(async move {
            if show_chown_dialog(main_win.upcast_ref(), &files).await {
                file_list.reload().await;
            }
        });
    }
}

pub fn file_mkdir(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
        let file_list = file_selector.file_list();

        if let Some(dir) = file_list.directory() {
            let selected_file = file_list.selected_file();
            let new_dir =
                show_mkdir_dialog(main_win.upcast_ref(), &dir.file(), selected_file.as_ref()).await;

            if let Some(new_dir) = new_dir {
                // focus the created directory (if possible)
                if new_dir.parent().map_or(false, |p| p.equal(&dir.file())) {
                    dir.file_created(&new_dir.uri());
                    file_list.focus_file(&new_dir.basename().unwrap(), true);
                }
            }
        }
    });
}

pub fn file_properties(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
        let file_list = file_selector.file_list();

        if let Some(file) = file_list.selected_file() {
            let changed = FilePropertiesDialog::show(
                main_win.upcast_ref(),
                &main_win.file_metadata_service(),
                &file,
            )
            .await;

            if changed {
                file_list.focus_file(&file.file_info().name(), true);
            }
        }
    });
}

fn ensure_file_list_is_local(file_list: &FileList) -> Result<(), ErrorMessage> {
    if file_list.connection().map_or(false, |c| c.is_local()) {
        Ok(())
    } else {
        Err(ErrorMessage::brief(gettext(
            "Operation not supported on remote file systems",
        )))
    }
}

async fn do_file_diff(
    main_win: &MainWindow,
    options: &ProgramsOptions,
) -> Result<(), ErrorMessage> {
    let active_fl = main_win.file_selector(FileSelectorID::ACTIVE).file_list();
    let inactive_fl = main_win.file_selector(FileSelectorID::INACTIVE).file_list();

    ensure_file_list_is_local(&active_fl)?;

    let selected_files = active_fl.selected_files();
    match selected_files.len() {
        0 => Ok(()),
        1 => {
            ensure_file_list_is_local(&inactive_fl)?;

            let active_file = selected_files.front().unwrap().clone();

            let inactive_file = inactive_fl
                .selected_files()
                .front()
                .ok_or_else(|| ErrorMessage::new(gettext("No file selected"), None::<String>))?
                .clone();

            let mut files = glib::List::new();
            files.push_back(active_file);
            files.push_back(inactive_file);

            spawn_async(None, &files, &options.differ_cmd.get()).map_err(SpawnError::into_message)
        }
        2 | 3 => spawn_async(None, &selected_files, &options.differ_cmd.get())
            .map_err(SpawnError::into_message),

        _ => Err(ErrorMessage::brief(gettext("Too many selected files"))),
    }
}

pub fn file_diff(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let options = ProgramsOptions::new();
    glib::spawn_future_local(async move {
        if let Err(error) = do_file_diff(&main_win, &options).await {
            error.show(main_win.upcast_ref()).await;
        }
    });
}

async fn do_file_sync_dirs(
    main_win: &MainWindow,
    options: &ProgramsOptions,
) -> Result<(), ErrorMessage> {
    let active_fl = main_win.file_selector(FileSelectorID::ACTIVE).file_list();
    let inactive_fl = main_win.file_selector(FileSelectorID::INACTIVE).file_list();

    ensure_file_list_is_local(&active_fl)?;
    ensure_file_list_is_local(&inactive_fl)?;

    let (active_dir, inactive_dir) = active_fl
        .directory()
        .zip(inactive_fl.directory())
        .ok_or_else(|| ErrorMessage::brief(gettext("Nothing to compare")))?;

    let mut files = glib::List::new();
    files.push_back(active_dir.upcast());
    files.push_back(inactive_dir.upcast());

    spawn_async(None, &files, &options.differ_cmd.get()).map_err(SpawnError::into_message)
}

pub fn file_sync_dirs(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let options = ProgramsOptions::new();
    glib::spawn_future_local(async move {
        if let Err(error) = do_file_sync_dirs(&main_win, &options).await {
            error.show(main_win.upcast_ref()).await;
        }
    });
}

pub fn file_rename(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();
    glib::spawn_future_local(async move {
        file_list.show_rename_dialog().await;
    });
}

fn symlink_name(file_name: &str, options: &GeneralOptions) -> String {
    let mut format = options.symlink_format.get();
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
    options: &GeneralOptions,
) {
    let mut skip_all = false;

    for file in files {
        let target_absolute_name = file.file().parse_name();

        let symlink_file =
            directory.get_child_gfile(Path::new(&symlink_name(&file.get_name(), options)));

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
                    let choice = gtk::AlertDialog::builder()
                        .modal(true)
                        .message(error.message())
                        .buttons([
                            gettext("Skip"),
                            gettext("Skip all"),
                            gettext("Cancel"),
                            gettext("Retry"),
                        ])
                        .cancel_button(0)
                        .default_button(3)
                        .build()
                        .choose_future(Some(parent_window))
                        .await;
                    match choice {
                        Ok(0) /* Skip */ => { break; },
                        Ok(1) /* Skip all */ => { skip_all = true; break; },
                        Ok(3) /* Retry */ => { continue; },
                        _ /* Cancel */ => { return },
                    }
                }
            }
        }
    }
}

pub fn file_create_symlink(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let options = GeneralOptions::new();

    glib::spawn_future_local(async move {
        let active_fs = main_win.file_selector(FileSelectorID::ACTIVE);
        let inactive_fs = main_win.file_selector(FileSelectorID::INACTIVE);

        let Some(dest_directory) = inactive_fs.file_list().directory() else {
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

            let choice = gtk::AlertDialog::builder()
                .modal(true)
                .message(message)
                .buttons([gettext("Cancel"), gettext("Create")])
                .cancel_button(0)
                .default_button(1)
                .build()
                .choose_future(Some(&main_win))
                .await;
            if choice == Ok(1) {
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

pub fn file_advrename(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();
    let file_metadata_service = main_win.file_metadata_service();
    advanced_rename_dialog_show(main_win.upcast_ref(), &file_list, &file_metadata_service);
}

pub fn file_sendto(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let options = ProgramsOptions::new();

    glib::spawn_future_local(async move {
        let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
        let file_list = file_selector.file_list();
        let files = file_list.selected_files();

        let command_template = options.sendto_cmd.get();

        if command_template == "xdg-email --attach %s" && files.len() > 1 {
            ErrorMessage::new(gettext("Warning"), Some(gettext("The default send-to command only supports one selected file at a time. You can change the command in the program options."))).show(main_win.upcast_ref()).await;
        }

        if let Err(error) = spawn_async(None, &files, &command_template) {
            error.into_message().show(main_win.upcast_ref()).await;
        }
    });
}

pub fn file_exit(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.close();
}

/************** Mark Menu **************/

pub fn mark_toggle(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .toggle()
}

pub fn mark_toggle_and_step(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .toggle_and_step()
}

pub fn mark_select_all(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .select_all()
}

pub fn mark_unselect_all(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .unselect_all()
}

pub fn mark_select_all_files(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .select_all_files()
}

pub fn mark_unselect_all_files(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .unselect_all_files()
}

pub fn mark_select_with_pattern(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_list = main_win.file_selector(FileSelectorID::ACTIVE).file_list();
    glib::spawn_future_local(async move {
        select_by_pattern(&file_list, true).await;
    });
}

pub fn mark_unselect_with_pattern(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_list = main_win.file_selector(FileSelectorID::ACTIVE).file_list();
    glib::spawn_future_local(async move {
        select_by_pattern(&file_list, false).await;
    });
}

pub fn mark_invert_selection(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .invert_selection();
}

pub fn mark_select_all_with_same_extension(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .toggle_files_with_same_extension(true);
}

pub fn mark_unselect_all_with_same_extension(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .toggle_files_with_same_extension(false);
}

pub fn mark_restore_selection(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .restore_selection();
}

pub fn mark_compare_directories(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();

    let fl1 = main_win.file_selector(FileSelectorID::ACTIVE).file_list();
    let fl2 = main_win.file_selector(FileSelectorID::INACTIVE).file_list();

    let mut files2: HashMap<String, File> = fl2
        .visible_files()
        .into_iter()
        .filter(|f| !f.is_dotdot())
        .filter(|f| f.file_info().file_type() != gio::FileType::Directory)
        .map(|f| (f.get_name(), f))
        .collect();

    let mut selection1 = HashSet::new();
    for f1 in fl1.visible_files() {
        if f1.is_dotdot() || f1.file_info().file_type() == gio::FileType::Directory {
            continue;
        }

        let name = f1.get_name();

        if let Some(f2) = files2.get(&name) {
            let date1 = f1.file_info().modification_date_time();
            let date2 = f2.file_info().modification_date_time();

            // select the younger one
            match date1.cmp(&date2) {
                Ordering::Less => {
                    // f2 stays selected
                }
                Ordering::Equal => {
                    if f1.file_info().size() == f2.file_info().size() {
                        // no selection
                        files2.remove(&name);
                    } else {
                        // select both
                        selection1.insert(f1);
                    }
                }
                Ordering::Greater => {
                    // f1 selected
                    selection1.insert(f1);
                    files2.remove(&name);
                }
            }
        } else {
            selection1.insert(f1);
        }
    }

    fl1.set_selected_files(&selection1);
    fl2.set_selected_files(&files2.into_values().collect());
}

/************** Edit Menu **************/

pub fn edit_cap_cut(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.cut_files();
}

pub fn edit_cap_copy(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.copy_files();
}

pub fn edit_cap_paste(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        main_win.paste_files().await;
    });
}

pub fn edit_filter(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.file_selector(FileSelectorID::ACTIVE).show_filter();
}

pub fn edit_copy_fnames(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let mask = get_modifiers_state(main_win.upcast_ref());

    let files = main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .selected_files();

    let names: Vec<String> = match mask {
        Some(gdk::ModifierType::SHIFT_MASK) => files
            .into_iter()
            .filter_map(|f| f.get_real_path())
            .map(|p| p.to_string_lossy().to_string())
            .collect(),
        Some(gdk::ModifierType::ALT_MASK) => files.into_iter().map(|f| f.get_uri_str()).collect(),
        _ => files.into_iter().map(|f| f.get_name()).collect(),
    };

    main_win.clipboard().set_text(&names.join(" "));
}

/************** Command Menu **************/

pub fn command_execute(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let parameter = parameter.cloned();
    glib::spawn_future_local(async move {
        let Some(command_template) = parameter.as_ref().and_then(|p| p.str()) else {
            ErrorMessage::new(gettext("No command was given."), None::<String>)
                .show(main_win.upcast_ref())
                .await;
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

        if let Err(error_message) = spawn_async(dir_path.as_deref(), &sfl, command_template)
            .map_err(SpawnError::into_message)
        {
            error_message.show(main_win.upcast_ref()).await;
        }
    });
}

pub fn open_terminal(main_win: &MainWindow, options: &ProgramsOptions) -> Result<(), ErrorMessage> {
    let dpath = main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .directory()
        .and_then(|d| d.file().path());

    let command = OsString::from(options.terminal_cmd.get());
    if command.is_empty() {
        return Err(ErrorMessage {
            message: gettext("Terminal command is not configured properly."),
            secondary_text: None,
        });
    }

    spawn_async_command(dpath.as_deref(), &command).map_err(SpawnError::into_message)
}

/// Executes the command stored in `terminal-cmd` in the active directory.
pub fn command_open_terminal(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let options = ProgramsOptions::new();
    glib::spawn_future_local(async move {
        if let Err(error_message) = open_terminal(&main_win, &options) {
            error_message.show(main_win.upcast_ref()).await;
        }
    });
}

/* ***************************** View Menu ****************************** */

pub fn view_dir_history(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .show_history();
}

pub fn view_up(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    if file_selector.is_tab_locked(&file_list) {
        if let Some(directory) = file_list.directory().and_then(|d| d.parent()) {
            file_selector.new_tab_with_dir(&directory, true, true);
        }
    } else {
        file_list.goto_directory(&Path::new(".."));
    }
}

pub fn view_first(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.file_selector(FileSelectorID::ACTIVE).first();
}

pub fn view_back(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.file_selector(FileSelectorID::ACTIVE).back();
}

pub fn view_forward(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.file_selector(FileSelectorID::ACTIVE).forward();
}

pub fn view_last(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.file_selector(FileSelectorID::ACTIVE).last();
}

pub fn view_refresh(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        main_win
            .file_selector(FileSelectorID::ACTIVE)
            .file_list()
            .reload()
            .await;
    });
}

pub fn view_in_left_pane(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.set_directory_to_opposite(FileSelectorID::LEFT);
}

pub fn view_in_right_pane(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.set_directory_to_opposite(FileSelectorID::RIGHT);
}

pub fn view_in_active_pane(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.set_directory_to_opposite(FileSelectorID::ACTIVE);
}

pub fn view_in_inactive_pane(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.set_directory_to_opposite(FileSelectorID::INACTIVE);
}

pub fn view_directory(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();
    if let Some(file) = file_list.selected_file() {
        if file.file_info().file_type() == gio::FileType::Directory {
            file_selector.do_file_specific_action(&file_list, &file);
        }
    }
}

pub fn view_home(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    let home = ConnectionList::get().home();
    if !file_selector.is_tab_locked(&file_list) {
        file_list.set_connection(&home, None);
        file_list.goto_directory(&Path::new("~"));
    } else {
        let directory = Directory::new(&home, home.create_path(&glib::home_dir()));
        file_selector.new_tab_with_dir(&directory, true, true);
    }
}

pub fn view_root(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();

    if file_selector.is_tab_locked(&file_list) {
        if let Some(connection) = file_list.connection() {
            let directory = Directory::new(&connection, connection.create_path(&Path::new("/")));
            file_selector.new_tab_with_dir(&directory, true, true);
        }
    } else {
        file_list.goto_directory(&Path::new("/"));
    }
}

pub fn view_new_tab(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();
    if let Some(directory) = file_list.directory() {
        file_selector.new_tab_with_dir(&directory, true, true);
    }
}

async fn ask_close_locked_tab(parent_window: &gtk::Window) -> bool {
    gtk::AlertDialog::builder()
        .modal(true)
        .message(gettext("The tab is locked, close anyway?"))
        .buttons([gettext("_Cancel"), gettext("_OK")])
        .cancel_button(0)
        .default_button(1)
        .build()
        .choose_future(Some(parent_window))
        .await
        == Ok(1)
}

pub fn view_close_tab(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        let fs = main_win.file_selector(FileSelectorID::ACTIVE);
        if fs.tab_count() > 1 {
            if !fs.is_current_tab_locked() || ask_close_locked_tab(main_win.upcast_ref()).await {
                fs.close_tab();
            }
        }
    });
}

pub fn view_close_all_tabs(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .close_all_tabs();
}

pub fn view_close_duplicate_tabs(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .close_duplicate_tabs();
}

pub fn view_prev_tab(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.file_selector(FileSelectorID::ACTIVE).prev_tab();
}

pub fn view_next_tab(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.file_selector(FileSelectorID::ACTIVE).next_tab();
}

pub fn view_in_new_tab(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    if let Some(dir) = file_selector
        .file_list()
        .selected_file()
        .and_downcast::<Directory>()
        .or_else(|| file_selector.file_list().directory())
    {
        file_selector.new_tab_with_dir(&dir, false, false);
    }
}

pub fn view_in_inactive_tab(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    if let Some(dir) = file_selector
        .file_list()
        .selected_file()
        .and_downcast::<Directory>()
        .or_else(|| file_selector.file_list().directory())
    {
        main_win
            .file_selector(FileSelectorID::INACTIVE)
            .new_tab_with_dir(&dir, false, false);
    }
}

pub fn view_toggle_tab_lock(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let file_selector = main_win.file_selector(FileSelectorID::ACTIVE);
    let file_list = file_selector.file_list();
    file_selector.toggle_tab_lock(&file_list);
}

pub fn view_step_up(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .focus_prev();
}

pub fn view_step_down(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .focus_next();
}

/************** Bookmarks Menu **************/

pub fn bookmarks_add_current(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let fs = main_win.file_selector(FileSelectorID::ACTIVE);
    let Some(dir) = fs.file_list().directory() else {
        eprintln!("No directory. Nothing to bookmark.");
        return;
    };
    let options = GeneralOptions::new();

    glib::spawn_future_local(async move {
        bookmark_directory(main_win.upcast_ref(), &dir, &options).await;
    });
}

pub fn bookmarks_edit(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let connection_list = ConnectionList::get();
    glib::spawn_future_local(async move {
        let shortcuts = main_win.shortcuts();
        let result =
            BookmarksDialog::show(main_win.upcast_ref(), &connection_list, shortcuts).await;
        if let Some(bookmark) = result {
            let fs = main_win.file_selector(FileSelectorID::ACTIVE);
            fs.goto_directory(&bookmark.connection, Path::new(&bookmark.bookmark.path()));
        }
    });
}

pub fn bookmarks_goto(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    parameter: Option<&glib::Variant>,
) {
    let Some(goto) = parameter.and_then(BookmarkGoToVariant::from_variant) else {
        return;
    };
    let Some(connection) = ConnectionList::get().find_by_uuid(&goto.connection_uuid) else {
        eprintln!(
            "[{}] Unsupported bookmark group: '{}' - ignored",
            goto.bookmark_name, goto.connection_uuid
        );
        return;
    };

    let Some(bookmark) = connection.bookmarks().iter().find_map(|b| {
        let bookmark: Bookmark = b.ok()?;
        if bookmark.name() == goto.bookmark_name {
            Some(bookmark)
        } else {
            None
        }
    }) else {
        eprintln!(
            "[{}] Unknown bookmark name: '{}' - ignored",
            connection.alias().unwrap_or_default(),
            goto.bookmark_name
        );
        return;
    };

    let fs = main_win.file_selector(FileSelectorID::ACTIVE);
    fs.goto_directory(&connection, Path::new(&bookmark.path()));
}

pub fn bookmarks_view(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win
        .file_selector(FileSelectorID::ACTIVE)
        .show_bookmarks();
}

/************** Options Menu **************/

pub fn options_edit(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        if show_options_dialog(&main_win).await {
            main_win.update_view();
        }
    });
}

pub fn options_edit_shortcuts(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        let shortcuts = main_win.shortcuts();
        ShortcutsDialog::run(main_win.upcast_ref(), shortcuts).await;
    });
}

/************** Connections Menu **************/

pub fn connections_open(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let dialog = RemoteDialog::new(main_win);
    dialog.present();
}

pub fn connections_new(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    let options = NetworkOptions::new();
    glib::spawn_future_local(async move {
        let uri = glib::Uri::parse(&options.quick_connect_uri.get(), glib::UriFlags::NONE).ok();
        if let Some(connection) =
            ConnectDialog::new_connection(main_win.upcast_ref(), false, uri).await
        {
            let fs = main_win.file_selector(FileSelectorID::ACTIVE);
            if fs.is_current_tab_locked() {
                fs.new_tab();
            }
            fs.file_list().set_connection(&connection, None);
            if let Some(quick_connect_uri) = connection.uri().map(|uri| uri.to_str()) {
                if let Err(error) = options.quick_connect_uri.set(quick_connect_uri) {
                    eprintln!("Failed to save quick connect parameters: {error}");
                }
            }
        }
    });
}

pub fn connections_change_left(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.change_connection(FileSelectorID::LEFT);
}

pub fn connections_change_right(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    main_win.change_connection(FileSelectorID::RIGHT);
}

pub fn connections_set_current(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    parameter: Option<&glib::Variant>,
) {
    let Some(uuid) = parameter.and_then(|p| p.str()) else {
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

async fn close_connection(main_win: &MainWindow, con: &Connection) {
    con.close(Some(main_win.upcast_ref())).await;
}

pub fn connections_close(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    parameter: Option<&glib::Variant>,
) {
    let Some(uuid) = parameter.and_then(|p| p.str()) else {
        eprintln!("No connection uuid was given.");
        return;
    };

    let con_list = ConnectionList::get();
    let Some(con) = con_list.find_by_uuid(uuid) else {
        eprintln!("No connection corresponds to {uuid}");
        return;
    };

    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        close_connection(&main_win, &con).await;
    });
}

pub fn connections_close_current(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    if let Some(con) = main_win
        .file_selector(FileSelectorID::ACTIVE)
        .file_list()
        .connection()
        .filter(|c| c.downcast_ref::<ConnectionHome>().is_none())
    {
        let main_win = main_win.clone();
        glib::spawn_future_local(async move {
            close_connection(&main_win, &con).await;
        });
    }
}

/************** Plugins Menu ***********/

pub fn plugins_configure(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let plugin_manager = main_win.plugin_manager();
    show_plugin_manager(&plugin_manager, main_win.upcast_ref());
}

pub fn plugin_action(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    parameter: Option<&glib::Variant>,
) {
    let Some(plugin_action) = parameter.and_then(PluginActionVariant::from_variant) else {
        eprintln!("Unknown plugin action. Nothing to do.");
        return;
    };

    if let Some(plugin) = main_win
        .plugin_manager()
        .active_plugins()
        .into_iter()
        .find_map(|(name, plugin)| (name == plugin_action.plugin).then_some(plugin))
    {
        if let Some(file_actions) = plugin.downcast_ref::<FileActions>() {
            file_actions.execute(
                &plugin_action.action,
                Some(&plugin_action.parameter),
                main_win.upcast_ref(),
                &main_win.state(),
            );
        }
    }
}

/************** Help Menu **************/

pub fn help_help(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        display_help(main_win.upcast_ref(), None).await;
    });
}

pub fn help_keyboard(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        display_help(main_win.upcast_ref(), Some("gnome-commander-keyboard")).await;
    });
}

pub fn help_web(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        if let Err(error) = gtk::UriLauncher::new(PACKAGE_URL)
            .launch_future(Some(&main_win))
            .await
        {
            ErrorMessage::with_error(gettext("There was an error opening home page."), &error)
                .show(main_win.upcast_ref())
                .await;
        }
    });
}

pub fn help_problem(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
    let main_win = main_win.clone();
    glib::spawn_future_local(async move {
        if let Err(error) = gtk::UriLauncher::new(PACKAGE_BUGREPORT)
            .launch_future(Some(&main_win))
            .await
        {
            ErrorMessage::with_error(gettext("There was an error reporting problem."), &error)
                .show(main_win.upcast_ref())
                .await;
        }
    });
}

pub fn help_about(
    main_win: &MainWindow,
    _action: &gio::SimpleAction,
    _parameter: Option<&glib::Variant>,
) {
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
        .transient_for(main_win)
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

pub struct UserAction {
    pub action_name: &'static str,
    pub activate: Option<fn(&MainWindow, &gio::SimpleAction, Option<&glib::Variant>)>,
    pub parameter_type: Option<Cow<'static, glib::VariantTy>>,
    pub name: &'static str,
    pub description: String,
}

impl UserAction {
    const fn new(
        action_name: &'static str,
        activate: fn(&MainWindow, &gio::SimpleAction, Option<&glib::Variant>),
        name: &'static str,
        description: String,
    ) -> Self {
        Self {
            action_name,
            activate: Some(activate),
            parameter_type: None,
            name,
            description,
        }
    }

    const fn with_param(
        action_name: &'static str,
        activate: fn(&MainWindow, &gio::SimpleAction, Option<&glib::Variant>),
        parameter_type: Cow<'static, glib::VariantTy>,
        name: &'static str,
        description: String,
    ) -> Self {
        Self {
            action_name,
            activate: Some(activate),
            parameter_type: Some(parameter_type),
            name,
            description,
        }
    }

    const fn predefined(
        action_name: &'static str,
        name: &'static str,
        description: String,
    ) -> Self {
        Self {
            action_name,
            activate: None,
            parameter_type: None,
            name,
            description,
        }
    }

    pub fn action_entry(&self) -> Option<gio::ActionEntry<MainWindow>> {
        Some(
            gio::ActionEntry::builder(&self.action_name)
                .activate(self.activate?)
                .parameter_type(self.parameter_type.as_deref())
                .build(),
        )
    }
}

pub const USER_ACTIONS: LazyLock<Vec<UserAction>> = LazyLock::new(|| {
    vec![
        // File actions
        UserAction::new("file-copy", file_copy, "file.copy", gettext("Copy files")),
        UserAction::new(
            "file-copy-as",
            file_copy_as,
            "file.copy_as",
            gettext("Copy files with rename"),
        ),
        UserAction::new("file-move", file_move, "file.move", gettext("Move files")),
        UserAction::new(
            "file-delete",
            file_delete,
            "file.delete",
            gettext("Delete files"),
        ),
        UserAction::new("file-view", file_view, "file.view", gettext("View file")),
        UserAction::new(
            "file-internal-view",
            file_internal_view,
            "file.internal_view",
            gettext("View with internal viewer"),
        ),
        UserAction::new(
            "file-external-view",
            file_external_view,
            "file.external_view",
            gettext("View with external viewer"),
        ),
        UserAction::new("file-edit", file_edit, "file.edit", gettext("Edit file")),
        UserAction::new(
            "file-edit-new-doc",
            file_edit_new_doc,
            "file.edit_new_doc",
            gettext("Edit a new file"),
        ),
        UserAction::new("file-search", file_search, "file.search", gettext("Search")),
        UserAction::new(
            "file-quick-search",
            file_quick_search,
            "file.quick_search",
            gettext("Quick search"),
        ),
        UserAction::new(
            "file-chmod",
            file_chmod,
            "file.chmod",
            gettext("Change permissions"),
        ),
        UserAction::new(
            "file-chown",
            file_chown,
            "file.chown",
            gettext("Change owner/group"),
        ),
        UserAction::new(
            "file-mkdir",
            file_mkdir,
            "file.mkdir",
            gettext("Create directory"),
        ),
        UserAction::new(
            "file-properties",
            file_properties,
            "file.properties",
            gettext("Properties"),
        ),
        UserAction::new(
            "file-diff",
            file_diff,
            "file.diff",
            gettext("Compare files (diff)"),
        ),
        UserAction::new(
            "file-sync-dirs",
            file_sync_dirs,
            "file.synchronize_directories",
            gettext("Synchronize directories"),
        ),
        UserAction::new(
            "file-rename",
            file_rename,
            "file.rename",
            gettext("Rename files"),
        ),
        UserAction::new(
            "file-create-symlink",
            file_create_symlink,
            "file.create_symlink",
            gettext("Create symbolic link"),
        ),
        UserAction::new(
            "file-advrename",
            file_advrename,
            "file.advrename",
            gettext("Advanced rename tool"),
        ),
        UserAction::new(
            "file-sendto",
            file_sendto,
            "file.sendto",
            gettext("Send files"),
        ),
        UserAction::new("file-exit", file_exit, "file.exit", gettext("Quit")),
        // Mark actions
        UserAction::new(
            "mark-toggle",
            mark_toggle,
            "mark.toggle",
            gettext("Toggle selection"),
        ),
        UserAction::new(
            "mark-toggle-and-step",
            mark_toggle_and_step,
            "mark.toggle_and_step",
            gettext("Toggle selection and move cursor downward"),
        ),
        UserAction::new(
            "mark-select-all",
            mark_select_all,
            "mark.select_all",
            gettext("Select all"),
        ),
        UserAction::new(
            "mark-unselect-all",
            mark_unselect_all,
            "mark.unselect_all",
            gettext("Unselect all"),
        ),
        UserAction::new(
            "mark-select-all-files",
            mark_select_all_files,
            "mark.select_all_files",
            gettext("Select all files"),
        ),
        UserAction::new(
            "mark-unselect-all-files",
            mark_unselect_all_files,
            "mark.unselect_all_files",
            gettext("Unselect all files"),
        ),
        UserAction::new(
            "mark-select-with-pattern",
            mark_select_with_pattern,
            "mark.select_with_pattern",
            gettext("Select with pattern"),
        ),
        UserAction::new(
            "mark-unselect-with-pattern",
            mark_unselect_with_pattern,
            "mark.unselect_with_pattern",
            gettext("Unselect with pattern"),
        ),
        UserAction::new(
            "mark-invert-selection",
            mark_invert_selection,
            "mark.invert",
            gettext("Invert selection"),
        ),
        UserAction::new(
            "mark-select-all-with-same-extension",
            mark_select_all_with_same_extension,
            "mark.select_all_with_same_extension",
            gettext("Select with same extension"),
        ),
        UserAction::new(
            "mark-unselect-all-with-same-extension",
            mark_unselect_all_with_same_extension,
            "mark.unselect_all_with_same_extension",
            gettext("Unselect with same extension"),
        ),
        UserAction::new(
            "mark-restore-selection",
            mark_restore_selection,
            "mark.restore_selection",
            gettext("Restore selection"),
        ),
        UserAction::new(
            "mark-compare-directories",
            mark_compare_directories,
            "mark.compare_directories",
            gettext("Compare directories"),
        ),
        // Edit actions
        UserAction::new("edit-cap-cut", edit_cap_cut, "edit.cut", gettext("Cut")),
        UserAction::new("edit-cap-copy", edit_cap_copy, "edit.copy", gettext("Copy")),
        UserAction::new(
            "edit-cap-paste",
            edit_cap_paste,
            "edit.paste",
            gettext("Paste"),
        ),
        UserAction::new(
            "edit-filter",
            edit_filter,
            "edit.filter",
            gettext("Show user defined files"),
        ),
        UserAction::new(
            "edit-copy-fnames",
            edit_copy_fnames,
            "edit.copy_filenames",
            gettext("Copy file names"),
        ),
        // Command actions
        UserAction::with_param(
            "command-execute",
            command_execute,
            glib::VariantTy::STRING.into(),
            "command.execute",
            gettext("Execute command"),
        ),
        UserAction::new(
            "command-open-terminal",
            command_open_terminal,
            "command.open_terminal",
            gettext("Open terminal"),
        ),
        // View actions
        UserAction::predefined(
            "view-conbuttons",
            "view.conbuttons",
            gettext("Show device buttons"),
        ),
        UserAction::predefined("view-devlist", "view.devlist", gettext("Show device list")),
        UserAction::predefined("view-toolbar", "view.toolbar", gettext("Show toolbar")),
        UserAction::predefined(
            "view-buttonbar",
            "view.buttonbar",
            gettext("Show buttonbar"),
        ),
        UserAction::predefined("view-cmdline", "view.cmdline", gettext("Show command line")),
        UserAction::new(
            "view-dir-history",
            view_dir_history,
            "view.dir_history",
            gettext("Show directory history"),
        ),
        UserAction::predefined(
            "view-hidden-files",
            "view.hidden_files",
            gettext("Show hidden files"),
        ),
        UserAction::predefined(
            "view-backup-files",
            "view.backup_files",
            gettext("Show backup files"),
        ),
        UserAction::new("view-up", view_up, "view.up", gettext("Up one directory")),
        UserAction::new(
            "view-first",
            view_first,
            "view.first",
            gettext("Back to the first directory"),
        ),
        UserAction::new(
            "view-back",
            view_back,
            "view.back",
            gettext("Back one directory"),
        ),
        UserAction::new(
            "view-forward",
            view_forward,
            "view.forward",
            gettext("Forward one directory"),
        ),
        UserAction::new(
            "view-last",
            view_last,
            "view.last",
            gettext("Forward to the last directory"),
        ),
        UserAction::new(
            "view-refresh",
            view_refresh,
            "view.refresh",
            gettext("Refresh"),
        ),
        UserAction::predefined(
            "view-equal-panes",
            "view.equal_panes",
            gettext("Equal panel size"),
        ),
        UserAction::predefined(
            "view-maximize-pane",
            "view.maximize_pane",
            gettext("Maximize panel size"),
        ),
        UserAction::new(
            "view-in-left-pane",
            view_in_left_pane,
            "view.in_left_pane",
            gettext("Open directory in the left window"),
        ),
        UserAction::new(
            "view-in-right-pane",
            view_in_right_pane,
            "view.in_right_pane",
            gettext("Open directory in the right window"),
        ),
        UserAction::new(
            "view-in-active-pane",
            view_in_active_pane,
            "view.in_active_pane",
            gettext("Open directory in the active window"),
        ),
        UserAction::new(
            "view-in-inactive-pane",
            view_in_inactive_pane,
            "view.in_inactive_pane",
            gettext("Open directory in the inactive window"),
        ),
        UserAction::new(
            "view-directory",
            view_directory,
            "view.directory",
            gettext("Change directory"),
        ),
        UserAction::new(
            "view-home",
            view_home,
            "view.home",
            gettext("Home directory"),
        ),
        UserAction::new(
            "view-root",
            view_root,
            "view.root",
            gettext("Root directory"),
        ),
        UserAction::new(
            "view-new-tab",
            view_new_tab,
            "view.new_tab",
            gettext("Open directory in a new tab"),
        ),
        UserAction::new(
            "view-close-tab",
            view_close_tab,
            "view.close_tab",
            gettext("Close the current tab"),
        ),
        UserAction::new(
            "view-close-all-tabs",
            view_close_all_tabs,
            "view.close_all_tabs",
            gettext("Close all tabs"),
        ),
        UserAction::new(
            "view-close-duplicate-tabs",
            view_close_duplicate_tabs,
            "view.close_duplicate_tabs",
            gettext("Close duplicate tabs"),
        ),
        UserAction::new(
            "view-prev-tab",
            view_prev_tab,
            "view.prev_tab",
            gettext("Previous tab"),
        ),
        UserAction::new(
            "view-next-tab",
            view_next_tab,
            "view.next_tab",
            gettext("Next tab"),
        ),
        UserAction::new(
            "view-in-new-tab",
            view_in_new_tab,
            "view.in_new_tab",
            gettext("Open directory in the new tab"),
        ),
        UserAction::new(
            "view-in-inactive-tab",
            view_in_inactive_tab,
            "view.in_inactive_tab",
            gettext("Open directory in the new tab (inactive window)"),
        ),
        UserAction::new(
            "view-toggle-tab-lock",
            view_toggle_tab_lock,
            "view.toggle_lock_tab",
            gettext("Lock/unlock tab"),
        ),
        UserAction::predefined(
            "view-horizontal-orientation",
            "view.horizontal-orientation",
            gettext("Horizontal Orientation"),
        ),
        UserAction::predefined(
            "view-main-menu",
            "view.main_menu",
            gettext("Display main menu"),
        ),
        UserAction::new(
            "view-step-up",
            view_step_up,
            "view.step_up",
            gettext("Move cursor one step up"),
        ),
        UserAction::new(
            "view-step-down",
            view_step_down,
            "view.step_down",
            gettext("Move cursor one step down"),
        ),
        // Bookmark actions
        UserAction::new(
            "bookmarks-add-current",
            bookmarks_add_current,
            "bookmarks.add_current",
            gettext("Bookmark current directory"),
        ),
        UserAction::new(
            "bookmarks-edit",
            bookmarks_edit,
            "bookmarks.edit",
            gettext("Manage bookmarks"),
        ),
        UserAction::with_param(
            "bookmarks-goto",
            bookmarks_goto,
            BookmarkGoToVariant::static_variant_type(),
            "bookmarks.goto",
            gettext("Go to bookmarked location"),
        ),
        UserAction::new(
            "bookmarks-view",
            bookmarks_view,
            "bookmarks.view",
            gettext("Show bookmarks of current device"),
        ),
        // Option actions
        UserAction::new(
            "options-edit",
            options_edit,
            "options.edit",
            gettext("Options"),
        ),
        UserAction::new(
            "options-edit-shortcuts",
            options_edit_shortcuts,
            "options.shortcuts",
            gettext("Keyboard shortcuts"),
        ),
        // Connections actions
        UserAction::new(
            "connections-open",
            connections_open,
            "connections.open",
            gettext("Open connection"),
        ),
        UserAction::new(
            "connections-new",
            connections_new,
            "connections.new",
            gettext("New connection"),
        ),
        UserAction::with_param(
            "connections-set-current",
            connections_set_current,
            glib::VariantTy::STRING.into(),
            "connections.set-uuid",
            gettext("Set connection"),
        ),
        UserAction::new(
            "connections-change-left",
            connections_change_left,
            "connections.change_left",
            gettext("Change left connection"),
        ),
        UserAction::new(
            "connections-change-right",
            connections_change_right,
            "connections.change_right",
            gettext("Change right connection"),
        ),
        UserAction::with_param(
            "connections-close",
            connections_close,
            glib::VariantTy::STRING.into(),
            "connections.close-uuid",
            gettext("Close connection"),
        ),
        UserAction::new(
            "connections-close-current",
            connections_close_current,
            "connections.close",
            gettext("Close connection"),
        ),
        // Plugin actions
        UserAction::new(
            "plugins-configure",
            plugins_configure,
            "plugins.configure",
            gettext("Configure plugins"),
        ),
        UserAction::with_param(
            "plugin-action",
            plugin_action,
            PluginActionVariant::static_variant_type(),
            "plugin.action",
            String::new(), // invisible to users
        ),
        // Help actions
        UserAction::new(
            "help-help",
            help_help,
            "help.help",
            gettext("Help contents"),
        ),
        UserAction::new(
            "help-keyboard",
            help_keyboard,
            "help.keyboard",
            gettext("Help on keyboard shortcuts"),
        ),
        UserAction::new(
            "help-web",
            help_web,
            "help.web",
            gettext("GNOME Commander on the web"),
        ),
        UserAction::new(
            "help-problem",
            help_problem,
            "help.problem",
            gettext("Report a problem"),
        ),
        UserAction::new(
            "help-about",
            help_about,
            "help.about",
            gettext("About GNOME Commander"),
        ),
    ]
});
