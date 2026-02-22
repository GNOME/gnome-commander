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
        connection::{ConnectionExt, ConnectionInterface},
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
    iter::Iterator,
    path::{Path, PathBuf},
};

async fn file_copy(main_win: MainWindow) {
    let src_fs = main_win.file_selector(FileSelectorID::Active);
    let dst_fs = main_win.file_selector(FileSelectorID::Inactive);
    let options = ConfirmOptions::new();
    prepare_copy_dialog_show(&main_win, &src_fs, &dst_fs, &options).await;
}

async fn file_copy_as(main_win: MainWindow) {
    let file_list = main_win.file_selector(FileSelectorID::Active).file_list();

    let Some(file) = file_list.selected_file() else {
        return;
    };
    let Some(dir) = file_list.directory() else {
        return;
    };

    make_copy_dialog(&file, &dir, &main_win).await;
}

async fn file_move(main_win: MainWindow) {
    let src_fs = main_win.file_selector(FileSelectorID::Active);
    let dst_fs = main_win.file_selector(FileSelectorID::Inactive);
    let options = ConfirmOptions::new();
    prepare_move_dialog_show(&main_win, &src_fs, &dst_fs, &options).await;
}

async fn file_delete(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .show_delete_dialog(false)
        .await;
}

async fn file_view(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    if let Err(error) = file_list.activate_action("fl.file-view", Some(&None::<bool>.to_variant()))
    {
        eprintln!("Cannot activate action `file-view`: {}", error);
    }
}

async fn file_internal_view(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    if let Err(error) = file_list.activate_action("fl.file-view", Some(&true.to_variant())) {
        eprintln!("Cannot activate action `file-view`: {}", error);
    }
}

async fn file_external_view(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    if let Err(error) = file_list.activate_action("fl.file-view", Some(&false.to_variant())) {
        eprintln!("Cannot activate action `file-view`: {}", error);
    }
}

async fn file_edit(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    let mask = get_modifiers_state(main_win.upcast_ref());

    if mask.is_some_and(|m| m.contains(gdk::ModifierType::SHIFT_MASK)) {
        show_new_textfile_dialog(main_win.upcast_ref(), &file_list).await;
    } else if let Err(error) = file_list.activate_action("fl.file-edit", None) {
        eprintln!("Cannot activate action `file-edit`: {}", error);
    }
}

async fn file_edit_new_doc(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();
    show_new_textfile_dialog(main_win.upcast_ref(), &file_list).await;
}

async fn file_search(main_win: MainWindow) {
    let options = ProgramsOptions::new();

    let file_selector = main_win.file_selector(FileSelectorID::Active);
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
}

pub async fn file_quick_search(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    file_list.show_quick_search(None);
}

async fn file_chmod(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    let files = file_list.selected_files();
    if !files.is_empty() && show_chmod_dialog(main_win.upcast_ref(), &files).await {
        file_list.reload().await;
    }
}

async fn file_chown(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    let files = file_list.selected_files();
    if !files.is_empty() && show_chown_dialog(main_win.upcast_ref(), &files).await {
        file_list.reload().await;
    }
}

async fn file_mkdir(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    if let Some(dir) = file_list.directory() {
        let selected_file = file_list.selected_file();
        let new_dir =
            show_mkdir_dialog(main_win.upcast_ref(), &dir.file(), selected_file.as_ref()).await;

        if let Some(new_dir) = new_dir {
            // focus the created directory (if possible)
            if new_dir.parent().is_some_and(|p| p.equal(&dir.file())) {
                dir.file_created(&new_dir.uri());
                file_list.focus_file(&new_dir.basename().unwrap(), true);
            }
        }
    }
}

async fn file_properties(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
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
}

fn ensure_file_list_is_local(file_list: &FileList) -> Result<(), ErrorMessage> {
    if file_list.connection().is_some_and(|c| c.is_local()) {
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
    let active_fl = main_win.file_selector(FileSelectorID::Active).file_list();
    let inactive_fl = main_win.file_selector(FileSelectorID::Inactive).file_list();

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

async fn file_diff(main_win: MainWindow) {
    let options = ProgramsOptions::new();
    if let Err(error) = do_file_diff(&main_win, &options).await {
        error.show(main_win.upcast_ref()).await;
    }
}

async fn do_file_sync_dirs(
    main_win: &MainWindow,
    options: &ProgramsOptions,
) -> Result<(), ErrorMessage> {
    let active_fl = main_win.file_selector(FileSelectorID::Active).file_list();
    let inactive_fl = main_win.file_selector(FileSelectorID::Inactive).file_list();

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

async fn file_sync_dirs(main_win: MainWindow) {
    let options = ProgramsOptions::new();
    if let Err(error) = do_file_sync_dirs(&main_win, &options).await {
        error.show(main_win.upcast_ref()).await;
    }
}

pub async fn file_rename(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();
    file_list.show_rename_dialog().await;
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

async fn file_create_symlink(main_win: MainWindow) {
    let options = GeneralOptions::new();

    let active_fs = main_win.file_selector(FileSelectorID::Active);
    let inactive_fs = main_win.file_selector(FileSelectorID::Inactive);

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
}

async fn file_advrename(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();
    let file_metadata_service = main_win.file_metadata_service();
    advanced_rename_dialog_show(main_win.upcast_ref(), &file_list, &file_metadata_service);
}

async fn file_sendto(main_win: MainWindow) {
    let options = ProgramsOptions::new();

    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();
    let files = file_list.selected_files();

    let command_template = options.sendto_cmd.get();

    if command_template == "xdg-email --attach %s" && files.len() > 1 {
        ErrorMessage::new(gettext("Warning"), Some(gettext("The default send-to command only supports one selected file at a time. You can change the command in the program options."))).show(main_win.upcast_ref()).await;
    }

    if let Err(error) = spawn_async(None, &files, &command_template) {
        error.into_message().show(main_win.upcast_ref()).await;
    }
}

async fn file_exit(main_win: MainWindow) {
    main_win.close();
}

/************** Mark Menu **************/

async fn mark_toggle(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .toggle()
}

async fn mark_toggle_and_step(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .toggle_and_step()
}

async fn mark_select_all(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .select_all()
}

async fn mark_unselect_all(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .unselect_all()
}

async fn mark_select_all_files(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .select_all_files()
}

async fn mark_unselect_all_files(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .unselect_all_files()
}

async fn mark_select_with_pattern(main_win: MainWindow) {
    let file_list = main_win.file_selector(FileSelectorID::Active).file_list();
    select_by_pattern(&file_list, true).await;
}

async fn mark_unselect_with_pattern(main_win: MainWindow) {
    let file_list = main_win.file_selector(FileSelectorID::Active).file_list();
    select_by_pattern(&file_list, false).await;
}

async fn mark_invert_selection(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .invert_selection();
}

async fn mark_select_all_with_same_extension(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .toggle_files_with_same_extension(true);
}

async fn mark_unselect_all_with_same_extension(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .toggle_files_with_same_extension(false);
}

async fn mark_restore_selection(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .restore_selection();
}

async fn mark_compare_directories(main_win: MainWindow) {
    let fl1 = main_win.file_selector(FileSelectorID::Active).file_list();
    let fl2 = main_win.file_selector(FileSelectorID::Inactive).file_list();

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

async fn edit_cap_cut(main_win: MainWindow) {
    main_win.cut_files();
}

async fn edit_cap_copy(main_win: MainWindow) {
    main_win.copy_files();
}

async fn edit_cap_paste(main_win: MainWindow) {
    main_win.paste_files().await;
}

async fn edit_filter(main_win: MainWindow) {
    main_win.file_selector(FileSelectorID::Active).show_filter();
}

async fn edit_copy_fnames(main_win: MainWindow) {
    let mask = get_modifiers_state(main_win.upcast_ref());

    let files = main_win
        .file_selector(FileSelectorID::Active)
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

async fn command_execute(main_win: MainWindow, command_template: String) {
    let fs = main_win.file_selector(FileSelectorID::Active);
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
        spawn_async(dir_path.as_deref(), &sfl, &command_template).map_err(SpawnError::into_message)
    {
        error_message.show(main_win.upcast_ref()).await;
    }
}

fn open_terminal(main_win: &MainWindow, options: &ProgramsOptions) -> Result<(), ErrorMessage> {
    let dpath = main_win
        .file_selector(FileSelectorID::Active)
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
async fn command_open_terminal(main_win: MainWindow) {
    let options = ProgramsOptions::new();
    if let Err(error_message) = open_terminal(&main_win, &options) {
        error_message.show(main_win.upcast_ref()).await;
    }
}

/* ***************************** View Menu ****************************** */

async fn view_dir_history(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .show_history();
}

async fn view_up(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    if file_selector.is_tab_locked(&file_list) {
        if let Some(directory) = file_list.directory().and_then(|d| d.parent()) {
            file_selector.new_tab_with_dir(&directory, true, true);
        }
    } else {
        file_list.goto_directory(Path::new(".."));
    }
}

async fn view_first(main_win: MainWindow) {
    main_win.file_selector(FileSelectorID::Active).first();
}

async fn view_back(main_win: MainWindow) {
    main_win.file_selector(FileSelectorID::Active).back();
}

async fn view_forward(main_win: MainWindow) {
    main_win.file_selector(FileSelectorID::Active).forward();
}

async fn view_last(main_win: MainWindow) {
    main_win.file_selector(FileSelectorID::Active).last();
}

async fn view_refresh(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .reload()
        .await;
}

async fn view_equal_panes(main_win: MainWindow) {
    main_win.ensure_slide_position(50).await
}

async fn view_maximize_pane(main_win: MainWindow) {
    main_win
        .ensure_slide_position(if main_win.current_panel() == 0 {
            100
        } else {
            0
        })
        .await
}

async fn view_in_left_pane(main_win: MainWindow) {
    main_win.set_directory_to_opposite(FileSelectorID::Left);
}

async fn view_in_right_pane(main_win: MainWindow) {
    main_win.set_directory_to_opposite(FileSelectorID::Right);
}

async fn view_in_active_pane(main_win: MainWindow) {
    main_win.set_directory_to_opposite(FileSelectorID::Active);
}

async fn view_in_inactive_pane(main_win: MainWindow) {
    main_win.set_directory_to_opposite(FileSelectorID::Inactive);
}

async fn view_directory(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();
    if let Some(file) = file_list.selected_file()
        && file.file_info().file_type() == gio::FileType::Directory
    {
        file_selector.do_file_specific_action(&file_list, &file);
    }
}

async fn view_home(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    let home = ConnectionList::get().home();
    if !file_selector.is_tab_locked(&file_list) {
        file_list.set_connection(&home, None);
        file_list.goto_directory(Path::new("~"));
    } else {
        let directory = match Directory::try_new(&home, home.create_path(&glib::home_dir())) {
            Ok(directory) => directory,
            Err(_) => {
                eprintln!("Unexpected: could not get home directory");
                return;
            }
        };
        file_selector.new_tab_with_dir(&directory, true, true);
    }
}

async fn view_root(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();

    if file_selector.is_tab_locked(&file_list) {
        if let Some(connection) = file_list.connection() {
            let directory =
                match Directory::try_new(&connection, connection.create_path(Path::new("/"))) {
                    Ok(directory) => directory,
                    Err(_) => {
                        eprintln!("Unexpected: could not get root directory");
                        return;
                    }
                };
            file_selector.new_tab_with_dir(&directory, true, true);
        }
    } else {
        file_list.goto_directory(Path::new("/"));
    }
}

async fn view_new_tab(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
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

async fn view_close_tab(main_win: MainWindow) {
    let fs = main_win.file_selector(FileSelectorID::Active);
    if fs.tab_count() > 1
        && (!fs.is_current_tab_locked() || ask_close_locked_tab(main_win.upcast_ref()).await)
    {
        fs.close_tab();
    }
}

async fn view_close_all_tabs(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .close_all_tabs();
}

async fn view_close_duplicate_tabs(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .close_duplicate_tabs();
}

async fn view_prev_tab(main_win: MainWindow) {
    main_win.file_selector(FileSelectorID::Active).prev_tab();
}

async fn view_next_tab(main_win: MainWindow) {
    main_win.file_selector(FileSelectorID::Active).next_tab();
}

async fn view_in_new_tab(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    if let Some(dir) = file_selector
        .file_list()
        .selected_file()
        .and_downcast::<Directory>()
        .or_else(|| file_selector.file_list().directory())
    {
        file_selector.new_tab_with_dir(&dir, false, false);
    }
}

async fn view_in_inactive_tab(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    if let Some(dir) = file_selector
        .file_list()
        .selected_file()
        .and_downcast::<Directory>()
        .or_else(|| file_selector.file_list().directory())
    {
        main_win
            .file_selector(FileSelectorID::Inactive)
            .new_tab_with_dir(&dir, false, false);
    }
}

async fn view_toggle_tab_lock(main_win: MainWindow) {
    let file_selector = main_win.file_selector(FileSelectorID::Active);
    let file_list = file_selector.file_list();
    file_selector.toggle_tab_lock(&file_list);
}

async fn view_step_up(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .focus_prev();
}

async fn view_step_down(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .focus_next();
}

async fn swap_panels(main_win: MainWindow) {
    main_win.swap_panels();
}

async fn show_slide_popup(main_win: MainWindow) {
    main_win.show_slide_popup();
}

async fn view_slide(main_win: MainWindow, percentage: i32) {
    main_win.set_slide(percentage);
}

/************** Bookmarks Menu **************/

async fn bookmarks_add_current(main_win: MainWindow) {
    let fs = main_win.file_selector(FileSelectorID::Active);
    let Some(dir) = fs.file_list().directory() else {
        eprintln!("No directory. Nothing to bookmark.");
        return;
    };
    let options = GeneralOptions::new();
    bookmark_directory(main_win.upcast_ref(), &dir, &options).await;
}

async fn bookmarks_edit(main_win: MainWindow) {
    let connection_list = ConnectionList::get();
    let shortcuts = main_win.shortcuts();
    let result = BookmarksDialog::show(main_win.upcast_ref(), connection_list, shortcuts).await;
    if let Some(bookmark) = result {
        let fs = main_win.file_selector(FileSelectorID::Active);
        fs.goto_directory(&bookmark.connection, Path::new(&bookmark.bookmark.path()));
    }
}

async fn bookmarks_goto(main_win: MainWindow, goto: BookmarkGoToVariant) {
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

    let fs = main_win.file_selector(FileSelectorID::Active);
    fs.goto_directory(&connection, Path::new(&bookmark.path()));
}

async fn bookmarks_view(main_win: MainWindow) {
    main_win
        .file_selector(FileSelectorID::Active)
        .show_bookmarks();
}

/************** Options Menu **************/

async fn options_edit(main_win: MainWindow) {
    if show_options_dialog(&main_win).await {
        main_win.update_view();
    }
}

async fn options_edit_shortcuts(main_win: MainWindow) {
    let shortcuts = main_win.shortcuts();
    ShortcutsDialog::run(main_win.upcast_ref(), shortcuts).await;
}

/************** Connections Menu **************/

async fn connections_open(main_win: MainWindow) {
    let dialog = RemoteDialog::new(&main_win);
    dialog.present();
}

async fn connections_new(main_win: MainWindow) {
    let options = NetworkOptions::new();
    let uri = glib::Uri::parse(&options.quick_connect_uri.get(), glib::UriFlags::NONE).ok();
    if let Some(connection) = ConnectDialog::new_connection(main_win.upcast_ref(), false, uri).await
    {
        let fs = main_win.file_selector(FileSelectorID::Active);
        if fs.is_current_tab_locked() {
            fs.new_tab();
        }
        fs.file_list().set_connection(&connection, None);
        if let Some(quick_connect_uri) = connection.uri().map(|uri| uri.to_str())
            && let Err(error) = options.quick_connect_uri.set(quick_connect_uri)
        {
            eprintln!("Failed to save quick connect parameters: {error}");
        }
    }
}

async fn connections_change_left(main_win: MainWindow) {
    main_win.change_connection(FileSelectorID::Left);
}

async fn connections_change_right(main_win: MainWindow) {
    main_win.change_connection(FileSelectorID::Right);
}

async fn connections_set_current(main_win: MainWindow, uuid: String) {
    let con_list = ConnectionList::get();
    let Some(con) = con_list.find_by_uuid(&uuid) else {
        eprintln!("No connection corresponds to {uuid}");
        return;
    };

    main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .set_connection(&con, None);
}

async fn connections_close(main_win: MainWindow, uuid: String) {
    let con_list = ConnectionList::get();
    let Some(con) = con_list.find_by_uuid(&uuid) else {
        eprintln!("No connection corresponds to {uuid}");
        return;
    };

    con.close(Some(main_win.upcast_ref())).await;
}

async fn connections_close_current(main_win: MainWindow) {
    if let Some(con) = main_win
        .file_selector(FileSelectorID::Active)
        .file_list()
        .connection()
        .filter(|c| c.downcast_ref::<ConnectionHome>().is_none())
    {
        con.close(Some(main_win.upcast_ref())).await;
    }
}

/************** Plugins Menu ***********/

async fn plugins_configure(main_win: MainWindow) {
    let plugin_manager = main_win.plugin_manager();
    show_plugin_manager(&plugin_manager, main_win.upcast_ref());
}

async fn plugin_action(main_win: MainWindow, plugin_action: PluginActionVariant) {
    if let Some(plugin) = main_win
        .plugin_manager()
        .active_plugins()
        .into_iter()
        .find_map(|(name, plugin)| (name == plugin_action.plugin).then_some(plugin))
        && let Some(file_actions) = plugin.downcast_ref::<FileActions>()
    {
        file_actions.execute(
            &plugin_action.action,
            Some(&plugin_action.parameter),
            main_win.upcast_ref(),
            &main_win.state(),
        );
    }
}

/************** Help Menu **************/

async fn help_help(main_win: MainWindow) {
    display_help(main_win.upcast_ref(), None).await;
}

async fn help_keyboard(main_win: MainWindow) {
    display_help(main_win.upcast_ref(), Some("gnome-commander-keyboard")).await;
}

async fn help_web(main_win: MainWindow) {
    if let Err(error) = gtk::UriLauncher::new(PACKAGE_URL)
        .launch_future(Some(&main_win))
        .await
    {
        ErrorMessage::with_error(gettext("There was an error opening home page."), &error)
            .show(main_win.upcast_ref())
            .await;
    }
}

async fn help_problem(main_win: MainWindow) {
    if let Err(error) = gtk::UriLauncher::new(PACKAGE_BUGREPORT)
        .launch_future(Some(&main_win))
        .await
    {
        ErrorMessage::with_error(gettext("There was an error reporting problem."), &error)
            .show(main_win.upcast_ref())
            .await;
    }
}

async fn help_about(main_win: MainWindow) {
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

trait Activatable {
    fn activate(self, action: &UserAction, mw: MainWindow, parameter: Option<&glib::Variant>);
    fn parameter_type(self) -> Option<Cow<'static, glib::VariantTy>>;
    fn bound_property(self) -> Option<&'static str>;
}

// It would have been easier to implement Activatable directly for the actual handler function.
// However, this results in "unbound type" compilation errors. In order to make parameter type
// "bound" we need ActionHandler as intermediate type as well as marker types.
struct NoParameter {}
struct BoundProperty {}

struct ActionHandler<Handler, T> {
    handler: Handler,
    _data: std::marker::PhantomData<T>,
}

// User actions without parameter, handler is a function taking MainWindow parameter only.

impl<Handler: AsyncFnOnce(MainWindow)> From<Handler> for ActionHandler<Handler, NoParameter> {
    fn from(handler: Handler) -> Self {
        Self {
            handler,
            _data: std::marker::PhantomData,
        }
    }
}

impl<Handler: AsyncFnOnce(MainWindow) + 'static> Activatable
    for ActionHandler<Handler, NoParameter>
{
    fn activate(self, action: &UserAction, mw: MainWindow, parameter: Option<&glib::Variant>) {
        if parameter.is_some() {
            eprintln!(
                "Unexpected parameter {parameter:?} for action {}",
                action.name()
            );
        }
        glib::spawn_future_local((self.handler)(mw));
    }

    fn parameter_type(self) -> Option<Cow<'static, glib::VariantTy>> {
        None
    }

    fn bound_property(self) -> Option<&'static str> {
        None
    }
}

// User actions with parameter, handler is a function taking MainWindow and a generic parameter.

impl<T: FromVariant, Handler: AsyncFnOnce(MainWindow, T)> From<Handler>
    for ActionHandler<Handler, T>
{
    fn from(handler: Handler) -> Self {
        Self {
            handler,
            _data: std::marker::PhantomData,
        }
    }
}

impl<T: FromVariant + 'static, Handler: AsyncFnOnce(MainWindow, T) + 'static> Activatable
    for ActionHandler<Handler, T>
{
    fn activate(self, action: &UserAction, mw: MainWindow, parameter: Option<&glib::Variant>) {
        if let Some(param) = parameter.and_then(|v| v.get::<T>()) {
            glib::spawn_future_local((self.handler)(mw, param));
        } else {
            eprintln!(
                "Invalid parameter {parameter:?} for action {}",
                action.name()
            );
        }
    }

    fn parameter_type(self) -> Option<Cow<'static, glib::VariantTy>> {
        Some(T::static_variant_type())
    }

    fn bound_property(self) -> Option<&'static str> {
        None
    }
}

// User actions bound to a property, handler is a string (property name).

impl From<&'static str> for ActionHandler<&'static str, BoundProperty> {
    fn from(handler: &'static str) -> Self {
        Self {
            handler,
            _data: std::marker::PhantomData,
        }
    }
}

impl Activatable for ActionHandler<&'static str, BoundProperty> {
    fn activate(self, _action: &UserAction, _mw: MainWindow, _parameter: Option<&glib::Variant>) {}

    fn parameter_type(self) -> Option<Cow<'static, glib::VariantTy>> {
        None
    }

    fn bound_property(self) -> Option<&'static str> {
        Some(self.handler)
    }
}

macro_rules! user_actions {
    (
        $(
            $(#[$meta:meta])*
            $id:ident($name:literal $(| $($legacy_name:literal)|+)?, $description:expr, $handler:expr$(,)?),
        )+
    ) => {
        #[derive(Debug, Default, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
        pub enum UserAction {
            $($(#[$meta])* $id,)*
        }

        impl UserAction {
            pub fn from_name(name: &str) -> Option<Self> {
                match name {
                    $($name $(| $($legacy_name)|+)? => Some(Self::$id),)*
                    _ => None,
                }
            }

            pub fn all() -> impl Iterator<Item=Self> {
                $(
                    let _max_index = Self::$id as usize;
                )*
                (0..=_max_index).map(|n| Self::try_from(n).unwrap_or_default())
            }

            pub fn name(&self) -> &'static str {
                match self {
                    $(Self::$id => $name,)*
                }
            }

            pub fn menu_description(&self) -> String {
                match self {
                    $(Self::$id => $description,)*
                }
            }

            pub fn activate(&self, mw: MainWindow, parameter: Option<&glib::Variant>) {
                match self {
                    $(Self::$id => {
                        ActionHandler::from($handler).activate(self, mw, parameter);
                    })*
                }
            }

            pub fn parameter_type(&self) -> Option<Cow<'static, glib::VariantTy>> {
                match self {
                    $(Self::$id => {
                        ActionHandler::from($handler).parameter_type()
                    })*
                }
            }

            pub fn bound_property(&self) -> Option<&'static str> {
                match self {
                    $(Self::$id => {
                        ActionHandler::from($handler).bound_property()
                    })*
                }
            }
        }

        impl TryFrom<usize> for UserAction {
            type Error = ();

            fn try_from(value: usize) -> Result<Self, Self::Error> {
                match value {
                    $(value if value == Self::$id as usize => Ok(Self::$id),)*
                    _ => Err(()),
                }
            }
        }
    }
}

impl UserAction {
    pub fn description(&self) -> String {
        let mut description = self.menu_description();
        description.retain(|c| c != '_' && c != '…');
        description
    }

    pub fn has_parameter(&self) -> bool {
        self.parameter_type().is_some()
    }
}

user_actions! {
    // File actions
    #[default]
    FileCopy(
        "file-copy" | "file.copy",
        gettext("Copy Files"),
        file_copy,
    ),

    FileCopyAs(
        "file-copy-as" | "file.copy_as",
        gettext("Copy files with rename"),
        file_copy_as,
    ),

    FileMove(
        "file-move" | "file.move",
        gettext("Move Files"),
        file_move,
    ),

    FileDelete(
        "file-delete" | "file.delete",
        gettext("_Delete"),
        file_delete,
    ),

    FileView(
        "file-view" | "file.view",
        gettext("View File"),
        file_view,
    ),

    FileInternalView(
        "file-internal-view" | "file.internal_view",
        gettext("View with internal viewer"),
        file_internal_view,
    ),

    FileExternalView(
        "file-external-view" | "file.external_view",
        gettext("View with external viewer"),
        file_external_view,
    ),

    FileEdit(
        "file-edit" | "file.edit",
        gettext("Edit (SHIFT for new document)"),
        file_edit,
    ),

    FileEditNewDoc(
        "file-edit-new-doc" | "file.edit_new_doc",
        gettext("New _Text File"),
        file_edit_new_doc,
    ),

    FileSearch(
        "file-search" | "file.search",
        gettext("_Search…"),
        file_search,
    ),

    FileQuickSearch(
        "file-quick-search" | "file.quick_search",
        gettext("_Quick Search…"),
        file_quick_search,
    ),

    FileChmod(
        "file-chmod" | "file.chmod",
        gettext("Change Per_missions"),
        file_chmod,
    ),

    FileChown(
        "file-chown" | "file.chown",
        gettext("Change _Owner/Group"),
        file_chown,
    ),

    FileMkdir(
        "file-mkdir" | "file.mkdir",
        gettext("New _Directory"),
        file_mkdir,
    ),

    FileProperties(
        "file-properties" | "file.properties",
        gettext("_Properties…"),
        file_properties,
    ),

    FileDiff(
        "file-diff" | "file.diff",
        gettext("_Diff"),
        file_diff,
    ),

    FileSyncDirs(
        "file-sync-dirs" | "file.synchronize_directories",
        gettext("S_ynchronize Directories"),
        file_sync_dirs,
    ),

    FileRename(
        "file-rename" | "file.rename",
        gettext("_Rename"),
        file_rename,
    ),

    FileCreateSymlink(
        "file-create-symlink" | "file.create_symlink",
        gettext("Create _Symbolic Link"),
        file_create_symlink,
    ),

    FileAdvrename(
        "file-advrename" | "file.advrename",
        gettext("Advanced _Rename Tool"),
        file_advrename,
    ),

    FileSendto(
        "file-sendto" | "file.sendto",
        gettext("_Send Files"),
        file_sendto,
    ),

    FileExit(
        "file-exit" | "file.exit",
        gettext("Quit"),
        file_exit,
    ),

    // Mark actions
    MarkToggle(
        "mark-toggle" | "mark.toggle",
        gettext("Toggle selection"),
        mark_toggle,
    ),

    MarkToggleAndStep(
        "mark-toggle-and-step" | "mark.toggle_and_step",
        gettext("Toggle selection and move cursor downward"),
        mark_toggle_and_step,
    ),

    MarkSelectAll(
        "mark-select-all" | "mark.select_all",
        gettext("_Select All"),
        mark_select_all,
    ),

    MarkUnselectAll(
        "mark-unselect-all" | "mark.unselect_all",
        gettext("_Unselect All"),
        mark_unselect_all,
    ),

    MarkSelectAllFiles(
        "mark-select-all-files" | "mark.select_all_files",
        gettext("Select all _Files"),
        mark_select_all_files,
    ),

    MarkUnselectAllFiles(
        "mark-unselect-all-files" | "mark.unselect_all_files",
        gettext("Unselect all Fi_les"),
        mark_unselect_all_files,
    ),

    MarkSelectWithPattern(
        "mark-select-with-pattern" | "mark.select_with_pattern",
        gettext("Select with _Pattern"),
        mark_select_with_pattern,
    ),

    MarkUnselectWithPattern(
        "mark-unselect-with-pattern" | "mark.unselect_with_pattern",
        gettext("Unselect with P_attern"),
        mark_unselect_with_pattern,
    ),

    MarkInvertSelection(
        "mark-invert-selection" | "mark.invert",
        gettext("_Invert Selection"),
        mark_invert_selection,
    ),

    MarkSelectAllWithSameExtension(
        "mark-select-all-with-same-extension" | "mark.select_all_with_same_extension",
        gettext("Select with same _Extension"),
        mark_select_all_with_same_extension,
    ),

    MarkUnselectAllWithSameExtension(
        "mark-unselect-all-with-same-extension" | "mark.unselect_all_with_same_extension",
        gettext("Unselect with same E_xtension"),
        mark_unselect_all_with_same_extension,
    ),

    MarkRestoreSelection(
        "mark-restore-selection" | "mark.restore_selection",
        gettext("_Restore Selection"),
        mark_restore_selection,
    ),

    MarkCompareDirectories(
        "mark-compare-directories" | "mark.compare_directories",
        gettext("_Compare Directories"),
        mark_compare_directories,
    ),

    // Edit actions
    EditCapCut(
        "edit-cap-cut" | "edit.cut",
        gettext("Cu_t"),
        edit_cap_cut,
    ),

    EditCapCopy(
        "edit-cap-copy" | "edit.copy",
        gettext("_Copy"),
        edit_cap_copy,
    ),

    EditCapPaste(
        "edit-cap-paste" | "edit.paste",
        gettext("_Paste"),
        edit_cap_paste,
    ),

    EditFilter(
        "edit-filter" | "edit.filter",
        gettext("_Enable Filter…"),
        edit_filter,
    ),

    EditCopyNames(
        "edit-copy-fnames" | "edit.copy_filenames",
        gettext("Copy _File Names (SHIFT for full paths, ALT for URIs)"),
        edit_copy_fnames,
    ),

    // Command actions
    CommandExecute(
        "command-execute" | "command.execute",
        gettext("Execute command"),
        command_execute,
    ),

    CommandOpenTerminal(
        "command-open-terminal" | "command.open_terminal",
        gettext("Open _Terminal"),
        command_open_terminal,
    ),

    // View actions
    ViewConbuttons(
        "view-conbuttons" | "view.conbuttons",
        gettext("Show Device Buttons"),
        "connection-buttons-visible",
    ),

    ViewDevlist(
        "view-devlist" | "view.devlist",
        gettext("Show Device List"),
        "connection-list-visible",
    ),

    ViewToolbar(
        "view-toolbar" | "view.toolbar",
        gettext("Show Toolbar"),
        "toolbar-visible",
    ),

    ViewButtonbar(
        "view-buttonbar" | "view.buttonbar",
        gettext("Show Buttonbar"),
        "buttonbar-visible",
    ),

    ViewCmdline(
        "view-cmdline" | "view.cmdline",
        gettext("Show Command Line"),
        "command-line-visible",
    ),

    ViewDirHistory(
        "view-dir-history" | "view.dir_history",
        gettext("Show directory history"),
        view_dir_history,
    ),

    ViewHiddenFiles(
        "view-hidden-files" | "view.hidden_files",
        gettext("Show Hidden Files"),
        "view-hidden-files",
    ),

    ViewBackupFiles(
        "view-backup-files" | "view.backup_files",
        gettext("Show Backup Files"),
        "view-backup-files",
    ),

    ViewUp(
        "view-up" | "view.up",
        gettext("Up one directory"),
        view_up,
    ),

    ViewFirst(
        "view-first" | "view.first",
        gettext("Back to oldest"),
        view_first,
    ),

    ViewBack(
        "view-back" | "view.back",
        gettext("_Back"),
        view_back,
    ),

    ViewForward(
        "view-forward" | "view.forward",
        gettext("_Forward"),
        view_forward,
    ),

    ViewLast(
        "view-last" | "view.last",
        gettext("Forward to latest"),
        view_last,
    ),

    ViewRefresh(
        "view-refresh" | "view.refresh",
        gettext("_Refresh"),
        view_refresh,
    ),

    ViewEqualPanes(
        "view-equal-panes" | "view.equal_panes",
        gettext("_Equal Panel Size"),
        view_equal_panes,
    ),

    ViewMaximizePane(
        "view-maximize-pane" | "view.maximize_pane",
        gettext("Maximize Panel Size"),
        view_maximize_pane,
    ),

    ViewInLeftPane(
        "view-in-left-pane" | "view.in_left_pane",
        gettext("Open directory in the left window"),
        view_in_left_pane,
    ),

    ViewInRightPane(
        "view-in-right-pane" | "view.in_right_pane",
        gettext("Open directory in the right window"),
        view_in_right_pane,
    ),

    ViewInActivePane(
        "view-in-active-pane" | "view.in_active_pane",
        gettext("Open directory in the active window"),
        view_in_active_pane,
    ),

    ViewInInactivePane(
        "view-in-inactive-pane" | "view.in_inactive_pane",
        gettext("Open directory in the inactive window"),
        view_in_inactive_pane,
    ),

    ViewDirectory(
        "view-directory" | "view.directory",
        gettext("Change directory"),
        view_directory,
    ),

    ViewHome(
        "view-home" | "view.home",
        gettext("Home directory"),
        view_home,
    ),

    ViewRoot(
        "view-root" | "view.root",
        gettext("Root directory"),
        view_root,
    ),

    ViewNewTab(
        "view-new-tab" | "view.new_tab",
        gettext("Open in New _Tab"),
        view_new_tab,
    ),

    ViewCloseTab(
        "view-close-tab" | "view.close_tab",
        gettext("_Close Tab"),
        view_close_tab,
    ),

    ViewCloseAllTabs(
        "view-close-all-tabs" | "view.close_all_tabs",
        gettext("Close _All Tabs"),
        view_close_all_tabs,
    ),

    ViewCloseDuplicateTabs(
        "view-close-duplicate-tabs" | "view.close_duplicate_tabs",
        gettext("Close duplicate tabs"),
        view_close_duplicate_tabs,
    ),

    ViewPrevTab(
        "view-prev-tab" | "view.prev_tab",
        gettext("Previous tab"),
        view_prev_tab,
    ),

    ViewNextTab(
        "view-next-tab" | "view.next_tab",
        gettext("Next tab"),
        view_next_tab,
    ),

    ViewInNewTab(
        "view-in-new-tab" | "view.in_new_tab",
        gettext("Open directory in the new tab"),
        view_in_new_tab,
    ),

    ViewInInactiveTab(
        "view-in-inactive-tab" | "view.in_inactive_tab",
        gettext("Open directory in the new tab (inactive window)"),
        view_in_inactive_tab,
    ),

    ViewToggleTabLock(
        "view-toggle-tab-lock" | "view.toggle_lock_tab",
        gettext("Lock/unlock tab"),
        view_toggle_tab_lock,
    ),

    ViewHorizontalOrientation(
        "view-horizontal-orientation" | "view.horizontal-orientation",
        gettext("Horizontal Orientation"),
        "horizontal-orientation",
    ),

    ViewMainMenu(
        "view-main-menu" | "view.main_menu",
        gettext("Display main menu"),
        "menu-visible",
    ),

    ViewStepUp(
        "view-step-up" | "view.step_up",
        gettext("Move cursor one step up"),
        view_step_up,
    ),

    ViewStepDown(
        "view-step-down" | "view.step_down",
        gettext("Move cursor one step down"),
        view_step_down,
    ),

    SwapPanes(
        "swap-panes",
        gettext("Swap panels"),
        swap_panels,
    ),

    ShowSlidePopup(
        "show-slide-popup",
        gettext("Show panel size selector"),
        show_slide_popup,
    ),

    ViewSlide(
        "view-slide",
        String::new(), // invisible to users
        view_slide,
    ),

    // Bookmark actions
    BookmarksAddCurrent(
        "bookmarks-add-current" | "bookmarks.add_current",
        gettext("_Bookmark this Directory…"),
        bookmarks_add_current,
    ),

    BookmarksEdit(
        "bookmarks-edit" | "bookmarks.edit",
        gettext("_Manage Bookmarks…"),
        bookmarks_edit,
    ),

    BookmarksGoto(
        "bookmarks-goto" | "bookmarks.goto",
        gettext("Go to bookmarked location"),
        bookmarks_goto,
    ),

    BookmarksView(
        "bookmarks-view" | "bookmarks.view",
        gettext("Show bookmarks of current device"),
        bookmarks_view,
    ),

    // Option actions
    OptionsEdit(
        "options-edit" | "options.edit",
        gettext("_Options…"),
        options_edit,
    ),

    OptionsEditShortcuts(
        "options-edit-shortcuts" | "options.shortcuts",
        gettext("_Keyboard Shortcuts…"),
        options_edit_shortcuts,
    ),

    // Connections actions
    ConnectionsOpen(
        "connections-open" | "connections.open",
        gettext("_Remote Server…"),
        connections_open,
    ),

    ConnectionsNew(
        "connections-new" | "connections.new",
        gettext("New Connection…"),
        connections_new,
    ),

    ConnectionsSetCurrent(
        "connections-set-current" | "connections.set-uuid",
        gettext("Set connection"),
        connections_set_current,
    ),

    ConnectionsChangeLeft(
        "connections-change-left" | "connections.change_left",
        gettext("Change left connection"),
        connections_change_left,
    ),

    ConnectionsChangeRight(
        "connections-change-right" | "connections.change_right",
        gettext("Change right connection"),
        connections_change_right,
    ),

    ConnectionsClose(
        "connections-close" | "connections.close-uuid",
        gettext("Close connection"),
        connections_close,
    ),

    ConnectionsCloseCurrent(
        "connections-close-current" | "connections.close",
        gettext("Drop Connection"),
        connections_close_current,
    ),

    // Plugin actions
    PluginsConfigure(
        "plugins-configure" | "plugins.configure",
        gettext("_Configure Plugins…"),
        plugins_configure,
    ),

    PluginAction(
        "plugin-action" | "plugin.action",
        String::new(), // invisible to users
        plugin_action,
    ),

    // Help actions
    HelpHelp(
        "help-help" | "help.help",
        gettext("_Documentation"),
        help_help,
    ),

    HelpKeyboard(
        "help-keyboard" | "help.keyboard",
        gettext("_Keyboard Shortcuts"),
        help_keyboard,
    ),

    HelpWeb(
        "help-web" | "help.web",
        gettext("GNOME Commander on the _Web"),
        help_web,
    ),

    HelpProblem(
        "help-problem" | "help.problem",
        gettext("Report a _Problem"),
        help_problem,
    ),

    HelpAbout(
        "help-about" | "help.about",
        gettext("_About"),
        help_about,
    ),
}
