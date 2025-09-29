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

use super::{actions::Script, list::FileList};
use crate::{
    app::{App, AppTarget, RegularApp, UserDefinedApp, load_favorite_apps},
    config::PACKAGE,
    debug::debug,
    file::File,
    filter::fnmatch,
    libgcmd::file_actions::{FileActions, FileActionsExt},
    main_win::MainWindow,
    options::options::GeneralOptions,
    plugin_manager::wrap_plugin_menu,
    utils::MenuBuilderExt,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};
use std::{
    fs,
    io::{self, BufRead},
    path::Path,
};

const MAX_OPEN_WITH_APPS: usize = 20;

const COPYFILENAMES_STOCKID: &str = "gnome-commander-copy-file-names";
const GTK_MAILSEND_STOCKID: &str = "mail-send";
const GTK_TERMINAL_STOCKID: &str = "utilities-terminal";
const FILETYPEDIR_STOCKID: &str = "file_type_dir";
const FILETYPEREGULARFILE_STOCKID: &str = "file_type_regular";

fn fav_app_menu_item(app: &App) -> gio::MenuItem {
    let item = gio::MenuItem::new(Some(&app.name()), None);
    item.set_action_and_target_value(Some("fl.open-with"), Some(&app.to_variant()));
    if let Some(icon) = app.icon() {
        item.set_icon(&icon);
    }
    item
}

fn fav_app_matches_files(app: &UserDefinedApp, files: &glib::List<File>) -> bool {
    return match app.target {
        AppTarget::AllFiles => files
            .iter()
            .all(|file| file.file_info().file_type() == gio::FileType::Regular),
        AppTarget::AllDirs => files
            .iter()
            .all(|file| file.file_info().file_type() == gio::FileType::Directory),
        AppTarget::AllDirsAndFiles => files.iter().all(|file| {
            matches!(
                file.file_info().file_type(),
                gio::FileType::Regular | gio::FileType::Directory
            )
        }),
        AppTarget::SomeFiles => {
            files.iter().map(|f| f.file_info()).all(|file_info| {
                if file_info.file_type() != gio::FileType::Regular {
                    return false;
                }
                // Check that the file matches at least one pattern
                let name = file_info.display_name();
                app.pattern_list()
                    .iter()
                    .any(|pattern| fnmatch(pattern, &name, false))
            })
        }
    };
}

/// Try to get the script info out of the script
fn extract_script_info(script_path: &Path) -> Option<(String, bool)> {
    let file = fs::File::open(script_path).ok()?;

    let mut script_name = None;
    let mut in_terminal = false;
    for line in io::BufReader::new(file).lines().flatten() {
        if let Some(name) = line.strip_prefix("#name:") {
            script_name = Some(name.trim().to_string());
        }
        if let Some(term) = line.trim().strip_prefix("#term:") {
            in_terminal = term.trim() == "true";
        }
    }
    Some((script_name?, in_terminal))
}

fn create_action_script_menu() -> Option<gio::Menu> {
    let scripts_dir = glib::user_config_dir().join(PACKAGE).join("scripts");

    let menu = gio::Menu::new();

    for entry in fs::read_dir(scripts_dir).ok()?.flatten() {
        let path = entry.path();
        if !path.is_file() {
            continue;
        }

        let mut script_info = Script {
            path,
            in_terminal: false,
        };
        let script_name;

        // Try to get the scriptname out of the script, otherwise take the filename
        if let Some((name, in_terminal)) = extract_script_info(&script_info.path) {
            script_info.in_terminal = in_terminal;
            script_name = name;
        } else {
            script_name = entry.file_name().to_string_lossy().to_string();
        }

        let item = gio::MenuItem::new(Some(&script_name), None);
        item.set_action_and_target_value(
            Some("fl.execute-script"),
            Some(&script_info.to_variant()),
        );

        menu.append_item(&item);
    }

    Some(menu)
}

/// This method adds all "open" popup entries
/// (for opening the selected files or folders with dedicated programs).
fn add_open_with_entries(menu: &gio::Menu, file_list: &FileList) {
    let files = file_list.selected_files();
    let first_file = files.front();

    let content_type = first_file.and_then(|f| f.content_type());

    // Only try to find a default application for the first file in the list of selected files
    let default_app_info = first_file.and_then(|f| f.app_info_for_content_type());
    if let Some(ref app_info) = default_app_info {
        // Add the default "Open" menu entry at the top of the popup
        let label = gettext("_Open with “{}”").replace("{}", &app_info.name().replace("_", "__"));
        let item = gio::MenuItem::new(Some(&label), Some("fl.open-with-default"));
        if let Some(icon) = app_info.icon() {
            item.set_icon(&icon);
        }

        menu.append_item(&item);
    } else {
        debug!(
            'u',
            "No default application found for the MIME type {:?} of {:?}",
            content_type,
            first_file.map(|f| f.get_name())
        );
        menu.append(Some(&gettext("Open Wit_h…")), Some("fl.open-with-other"));
    }

    // Add menu items in the "Open with" submenu
    let submenu = gio::Menu::new();

    let app_infos = content_type
        .map(|ct| gio::AppInfo::all_for_type(&ct))
        .unwrap_or_default()
        .into_iter()
        .filter(|app_info| {
            // Only add the entry to the submenu if its name different from the default app
            match default_app_info {
                Some(ref d) => !d.equal(app_info),
                None => true,
            }
        })
        .take(MAX_OPEN_WITH_APPS)
        .map(|app_info| App::Regular(RegularApp { app_info }));

    for app in app_infos {
        let item = gio::MenuItem::new(Some(&app.name()), None);
        item.set_action_and_target_value(Some("fl.open-with"), Some(&app.to_variant()));
        if let Some(icon) = app.icon() {
            item.set_icon(&icon);
        }

        submenu.append_item(&item);
    }

    // We haven't added an "Open with..." entry further above, so adding it here.
    if default_app_info.is_some() {
        submenu.append(Some(&gettext("Open Wit_h…")), Some("fl.open-with-other"));
    }

    if submenu.n_items() > 0 {
        menu.append_submenu(Some(&gettext("Open Wit_h")), &submenu);
    }
}

pub fn file_popup_menu(main_win: &MainWindow, file_list: &FileList) -> Option<gio::Menu> {
    let files = file_list.selected_files();

    let menu = gio::Menu::new();

    // Add execute menu entry
    let Some(first_file) = files.front() else {
        return None;
    };
    if first_file.is_executable() && files.len() == 1 {
        let item = gio::MenuItem::new(Some(&gettext("E_xecute")), Some("fl.execute"));
        item.set_icon(&gio::ThemedIcon::new("system-run"));

        let section = gio::Menu::new();
        section.append_item(&item);

        menu.append_section(None, &section);
    }

    // Add "Open With" popup entries
    add_open_with_entries(&menu, file_list);

    // Add plugin popup entries
    for (action_group_name, plugin) in main_win.plugin_manager().active_plugins() {
        if let Some(file_actions) = plugin.downcast_ref::<FileActions>() {
            if let Some(plugin_menu) = file_actions.create_popup_menu_items(&main_win.state()) {
                let plugin_menu = wrap_plugin_menu(&action_group_name, &plugin_menu);
                menu.append_section(None, &plugin_menu);
            }
        }
    }

    // Add favorite applications menu entries
    let options = GeneralOptions::new();
    let fav_menu = gio::Menu::new();
    for app in load_favorite_apps(&options) {
        if fav_app_matches_files(&app, &files) {
            fav_menu.append_item(&fav_app_menu_item(&&App::UserDefined(app)));
        }
    }
    menu.append_section(None, &fav_menu);

    if let Some(section) = create_action_script_menu() {
        menu.append_section(None, &section);
    }

    menu.append_section(None, &{
        gio::Menu::new()
            .item(gettext("Cut"), "win.edit-cap-cut")
            .item(gettext("Copy"), "win.edit-cap-copy")
            .item_icon(
                gettext("Copy file names"),
                "win.edit-copy-fnames",
                COPYFILENAMES_STOCKID,
            )
            .item(gettext("Delete"), "win.file-delete")
            .item(gettext("Rename"), "win.file-rename")
            .item_icon(
                gettext("Send files"),
                "win.file-sendto",
                GTK_MAILSEND_STOCKID,
            )
            .item_icon(
                gettext("Open _terminal here"),
                "win.command-open-terminal",
                GTK_TERMINAL_STOCKID,
            )
    });

    menu.append(Some(&gettext("_Properties…")), Some("win.file-properties"));

    Some(menu)
}

pub fn list_popup_menu() -> gio::Menu {
    gio::Menu::new()
        .submenu(gettext("New"), {
            gio::Menu::new()
                .item_icon(gettext("_Directory"), "win.file-mkdir", FILETYPEDIR_STOCKID)
                .item_icon(
                    gettext("_Text File"),
                    "win.file-edit-new-doc",
                    FILETYPEREGULARFILE_STOCKID,
                )
        })
        .item(gettext("_Paste"), "win.edit-cap-paste")
        .item_icon(
            gettext("Open _terminal here"),
            "win.command-open-terminal",
            GTK_TERMINAL_STOCKID,
        )
        .item(gettext("_Refresh"), "fl.refresh")
}
