// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{list::ColumnID, list::FileList};
use crate::{
    app::{App, AppTarget, RegularApp, UserDefinedApp, load_favorite_apps},
    debug::debug,
    file::{File, FileOps},
    filter::fnmatch,
    main_win::MainWindow,
    options::GeneralOptions,
    plugins::{
        ApiRequestToPlugin, ApiResponseFromPlugin, MessageFromPluginHost, MessageToPluginHost,
    },
    user_actions::{PluginActionVariant, UserAction},
    utils::MenuBuilderExt,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

const MAX_OPEN_WITH_APPS: usize = 20;

fn fav_app_menu_item(app: &App) -> gio::MenuItem {
    let item = gio::MenuItem::new(Some(&app.name()), None);
    item.set_action_and_target_value(Some("fl.open-with"), Some(&app.to_variant()));
    item
}

fn fav_app_matches_files(app: &UserDefinedApp, files: &[File]) -> bool {
    match app.target {
        AppTarget::AllFiles => files.iter().all(|file| file.is_regular()),
        AppTarget::AllDirs => files.iter().all(|file| file.is_directory()),
        AppTarget::AllDirsAndFiles => files
            .iter()
            .all(|file| file.is_regular() || file.is_directory()),
        AppTarget::SomeFiles => {
            files.iter().all(|file| {
                if !file.is_regular() {
                    return false;
                }
                // Check that the file matches at least one pattern
                let name = file.name();
                app.pattern_list()
                    .iter()
                    .any(|pattern| fnmatch(pattern, &name, false))
            })
        }
    }
}

/// This method adds all "open" popup entries
/// (for opening the selected files or folders with dedicated programs).
fn add_open_with_entries(menu: &gio::Menu, file_list: &FileList) {
    let files = file_list.selected_files();
    let first_file = files.first();

    let content_type = first_file.and_then(|f| f.content_type());

    // Only try to find a default application for the first file in the list of selected files
    let default_app_info = first_file.and_then(|f| f.app_info_for_content_type());
    if let Some(ref app_info) = default_app_info {
        // Add the default "Open" menu entry at the top of the popup
        let label = gettext("_Open with “{}”").replace("{}", &app_info.name().replace("_", "__"));
        menu.append(Some(&label), Some("fl.open-with-default"));
    } else {
        debug!(
            'u',
            "No default application found for the MIME type {:?} of {:?}",
            content_type,
            first_file.map(|f| f.name())
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
    let first_file = files.first()?;
    if first_file.is_executable() && files.len() == 1 {
        let section = gio::Menu::new();
        section.append_item(&gio::MenuItem::new(
            Some(&gettext("E_xecute")),
            Some("fl.execute"),
        ));

        menu.append_section(None, &section);
    }

    // Add "Open With" popup entries
    add_open_with_entries(&menu, file_list);

    // Add plugin popup entries
    let section = gio::Menu::new();
    menu.append_section(None, &section);
    let mut channel = main_win.plugin_channel();
    let id = channel.new_id();
    channel.send(MessageToPluginHost::ApiRequest {
        id,
        plugin_name: None,
        request: ApiRequestToPlugin::ContextMenuItems(main_win.state()),
    });

    glib::spawn_future_local(async move {
        let mut received = Vec::new();
        loop {
            if let MessageFromPluginHost::ApiResponse {
                id: resp_id,
                plugin_name,
                plugin_display_name,
                response,
                last,
            } = channel.receive().await
                && resp_id == id
            {
                if let Some(ApiResponseFromPlugin::ContextMenuItems(items)) = response {
                    received.extend(
                        items
                            .into_iter()
                            .map(|item| (plugin_name.clone(), plugin_display_name.clone(), item)),
                    );
                }
                if last {
                    break;
                }
            }
        }

        received.sort_by_cached_key(|(plugin_name, _, _)| plugin_name.clone());

        for (plugin_name, plugin_display_name, item) in received {
            let label = if plugin_display_name.is_empty() {
                item.label
            } else {
                format!("{} | {plugin_display_name}", item.label)
            };
            let menuitem = gio::MenuItem::new(Some(&label), None);
            menuitem.set_action_and_target_value(
                Some(UserAction::PluginAction.name()),
                Some(
                    &PluginActionVariant {
                        plugin_name,
                        action: item.action,
                        parameter: item.parameter,
                    }
                    .to_variant(),
                ),
            );
            section.append_item(&menuitem);
        }
    });

    // Add favorite applications menu entries
    let fav_menu = gio::Menu::new();
    for app in load_favorite_apps(&GeneralOptions::instance()) {
        if fav_app_matches_files(&app, &files) {
            fav_menu.append_item(&fav_app_menu_item(&App::UserDefined(app)));
        }
    }
    menu.append_section(None, &fav_menu);

    menu.append_section(None, &{
        gio::Menu::new()
            .action(UserAction::EditCapCut)
            .action(UserAction::EditCapCopy)
            .action(UserAction::EditCopyNames)
            .action(UserAction::FileDelete)
            .action(UserAction::FileRename)
            .action(UserAction::FileSendto)
            .action(UserAction::CommandOpenTerminal)
    });

    let menu = menu.action(UserAction::FileProperties);

    Some(menu)
}

pub fn list_popup_menu() -> gio::Menu {
    let mut columns_menu = gio::Menu::new();
    for column in ColumnID::all() {
        if column != ColumnID::Dir
            && let Some(title) = column.title()
        {
            columns_menu = columns_menu.item(title, format!("fl.toggle-column-{}", column.name()));
        }
    }

    gio::Menu::new().section(columns_menu).section(
        gio::Menu::new()
            .submenu(gettext("New"), {
                gio::Menu::new()
                    .action(UserAction::FileMkdir)
                    .action(UserAction::FileEditNewDoc)
            })
            .action(UserAction::EditCapPaste)
            .action(UserAction::CommandOpenTerminal)
            .action(UserAction::ViewRefresh),
    )
}
