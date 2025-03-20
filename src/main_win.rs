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
    connection::{
        bookmark::{Bookmark, BookmarkGoToVariant},
        connection::{Connection, ConnectionExt},
        list::ConnectionList,
        remote::ConnectionRemote,
    },
    file::File,
    file_list::list::FileList,
    file_selector::FileSelector,
    libgcmd::{
        file_actions::{FileActions, FileActionsExt},
        file_descriptor::FileDescriptorExt,
        state::{State, StateExt},
    },
    plugin_manager::{wrap_plugin_menu, PluginManager},
    shortcuts::Shortcuts,
    tags::tags::FileMetadataService,
    transfer::{gnome_cmd_copy_gfiles, gnome_cmd_move_gfiles},
    types::{FileSelectorID, GnomeCmdConfirmOverwriteMode},
    user_actions,
    utils::{extract_menu_shortcuts, MenuBuilderExt},
};
use gettextrs::gettext;
use gtk::{
    gio::{self, ffi::GMenu},
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_none, FromGlibPtrNone, ToGlibPtr},
    },
    prelude::*,
};
use std::sync::LazyLock;

pub mod ffi {
    use super::*;
    use crate::{file_selector::ffi::GnomeCmdFileSelector, tags::tags::FileMetadataService};
    use gtk::glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdMainWin {
        _data: [u8; 0],
        _phantom: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_main_win_get_type() -> GType;

        pub fn gnome_cmd_main_win_get_fs(
            main_win: *mut GnomeCmdMainWin,
            id: FileSelectorID,
        ) -> *mut GnomeCmdFileSelector;

        pub fn gnome_cmd_main_win_change_connection(
            main_win: *mut GnomeCmdMainWin,
            id: FileSelectorID,
        );

        pub fn gnome_cmd_main_win_focus_file_lists(main_win: *mut GnomeCmdMainWin);

        pub fn gnome_cmd_main_win_update_bookmarks(main_win: *mut GnomeCmdMainWin);

        pub fn gnome_cmd_main_win_shortcuts(main_win: *mut GnomeCmdMainWin) -> *mut Shortcuts;

        pub fn gnome_cmd_main_win_get_plugin_manager(
            main_win: *mut GnomeCmdMainWin,
        ) -> *mut <PluginManager as glib::object::ObjectType>::GlibType;

        pub fn gnome_cmd_main_win_get_file_metadata_service(
            main_win: *mut GnomeCmdMainWin,
        ) -> *mut <FileMetadataService as glib::object::ObjectType>::GlibType;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdMainWinClass {
        pub parent_class: gtk::ffi::GtkApplicationWindowClass,
    }
}

glib::wrapper! {
    pub struct MainWindow(Object<ffi::GnomeCmdMainWin, ffi::GnomeCmdMainWinClass>)
        @extends gtk::ApplicationWindow, gtk::Window, gtk::Widget,
        @implements gio::ActionMap, gtk::Root;

    match fn {
        type_ => || ffi::gnome_cmd_main_win_get_type(),
    }
}

#[derive(Default)]
struct MainWindowPrivate {
    cut_and_paste_state: Option<CutAndPasteState>,
}

impl MainWindow {
    fn private(&self) -> &mut MainWindowPrivate {
        static QUARK: LazyLock<glib::Quark> =
            LazyLock::new(|| glib::Quark::from_str("main-window-private"));

        unsafe {
            if let Some(mut private) = self.qdata::<MainWindowPrivate>(*QUARK) {
                private.as_mut()
            } else {
                self.set_qdata(*QUARK, MainWindowPrivate::default());
                self.qdata::<MainWindowPrivate>(*QUARK).unwrap().as_mut()
            }
        }
    }

    pub fn file_selector(&self, id: FileSelectorID) -> FileSelector {
        unsafe {
            FileSelector::from_glib_none(ffi::gnome_cmd_main_win_get_fs(self.to_glib_none().0, id))
        }
    }

    pub fn change_connection(&self, id: FileSelectorID) {
        unsafe { ffi::gnome_cmd_main_win_change_connection(self.to_glib_none().0, id) }
    }

    pub fn focus_file_lists(&self) {
        unsafe { ffi::gnome_cmd_main_win_focus_file_lists(self.to_glib_none().0) }
    }

    pub fn update_bookmarks(&self) {
        unsafe { ffi::gnome_cmd_main_win_update_bookmarks(self.to_glib_none().0) }
    }

    pub fn shortcuts(&self) -> &mut Shortcuts {
        unsafe { &mut *ffi::gnome_cmd_main_win_shortcuts(self.to_glib_none().0) }
    }

    pub fn state(&self) -> State {
        let fs1 = self.file_selector(FileSelectorID::ACTIVE);
        let fs2 = self.file_selector(FileSelectorID::INACTIVE);
        let dir1 = fs1.directory();
        let dir2 = fs2.directory();

        let state = State::new();
        state.set_active_dir(dir1.and_upcast_ref());
        state.set_inactive_dir(dir2.and_upcast_ref());
        state.set_active_dir_files(&fs1.file_list().visible_files());
        state.set_inactive_dir_files(&fs2.file_list().visible_files());
        state.set_active_dir_selected_files(&fs1.file_list().selected_files());
        state.set_inactive_dir_selected_files(&fs2.file_list().selected_files());

        state
    }

    pub fn plugin_manager(&self) -> PluginManager {
        unsafe {
            from_glib_none(ffi::gnome_cmd_main_win_get_plugin_manager(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn file_metadata_service(&self) -> FileMetadataService {
        unsafe {
            from_glib_none(ffi::gnome_cmd_main_win_get_file_metadata_service(
                self.to_glib_none().0,
            ))
        }
    }
}

enum CutAndPasteOperation {
    Cut,
    Copy,
}

struct CutAndPasteState {
    operation: CutAndPasteOperation,
    source_file_list: FileList,
    files: glib::List<File>,
}

impl MainWindow {
    fn set_cap_state(&self, state: bool) {
        if let Some(action) = self
            .lookup_action("edit-cap-paste")
            .and_downcast::<gio::SimpleAction>()
        {
            action.set_enabled(state);
        }
    }

    pub fn cut_files(&self) {
        let source_file_list = self.file_selector(FileSelectorID::ACTIVE).file_list();
        let files = source_file_list.selected_files();
        if files.is_empty() {
            return;
        }

        self.private().cut_and_paste_state = Some(CutAndPasteState {
            source_file_list,
            operation: CutAndPasteOperation::Cut,
            files,
        });
        self.set_cap_state(true);
    }

    pub fn copy_files(&self) {
        let source_file_list = self.file_selector(FileSelectorID::ACTIVE).file_list();
        let files = source_file_list.selected_files();
        if files.is_empty() {
            return;
        }

        self.private().cut_and_paste_state = Some(CutAndPasteState {
            source_file_list,
            operation: CutAndPasteOperation::Copy,
            files,
        });
        self.set_cap_state(true);
    }

    pub async fn paste_files(&self) {
        let destination_file_list = self.file_selector(FileSelectorID::ACTIVE).file_list();
        let Some(dir) = destination_file_list.directory() else {
            return;
        };

        self.set_cap_state(false);
        let Some(state) = self.private().cut_and_paste_state.take() else {
            return;
        };

        let files = state.files.iter().map(|f| f.file()).collect();
        let success = match state.operation {
            CutAndPasteOperation::Cut => {
                gnome_cmd_move_gfiles(
                    self.clone().upcast(),
                    files,
                    dir.clone(),
                    None,
                    gio::FileCopyFlags::NONE,
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                )
                .await
            }
            CutAndPasteOperation::Copy => {
                gnome_cmd_copy_gfiles(
                    self.clone().upcast(),
                    files,
                    dir.clone(),
                    None,
                    gio::FileCopyFlags::NONE,
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                )
                .await
            }
        };
        if !success {
            eprintln!("Transfer failed");
        }

        state.source_file_list.reload();
        dir.relist_files(self.upcast_ref(), false).await;
        self.focus_file_lists();
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_main_win_install_actions(mw_ptr: *mut ffi::GnomeCmdMainWin) {
    let mw: MainWindow = unsafe { from_glib_none(mw_ptr) };
    mw.add_action_entries(user_actions::USER_ACTIONS.iter().map(|a| a.action_entry()));
}

fn main_menu(main_win: &MainWindow) -> gio::Menu {
    let menu = gio::Menu::new();

    menu.append_submenu(Some(&gettext("_File")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .item(gettext("Change _Owner/Group"), "win.file-chown")
                    .item(gettext("Change Per_missions"), "win.file-chmod")
                    .item_accel(
                        gettext("Advanced _Rename Tool"),
                        "win.file-advrename",
                        "<Control>M",
                    )
                    .item_accel(
                        gettext("Create _Symbolic Link"),
                        "win.file-create-symlink",
                        "<Control><shift>F5",
                    )
                    .item_accel(
                        gettext("_Properties…"),
                        "win.file-properties",
                        "<Alt>KP_Enter",
                    )
            })
            .section({
                gio::Menu::new()
                    .item_accel(gettext("_Search…"), "win.file-search", "<Alt>F7")
                    .item(gettext("_Quick Search…"), "win.file-quick-search")
                    .item(gettext("_Enable Filter…"), "win.edit-filter")
            })
            .section({
                gio::Menu::new()
                    .item(gettext("_Diff"), "win.file-diff")
                    .item(gettext("S_ynchronize Directories"), "win.file-sync-dirs")
            })
    });

    menu.append_submenu(Some(&gettext("_Edit")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .item_accel(gettext("Cu_t"), "win.edit-cap-cut", "<Control>X")
                    .item_accel(gettext("_Copy"), "win.edit-cap-copy", "<Control>C")
                    .item_accel(gettext("_Paste"), "win.edit-cap-paste", "<Control>V")
                    .item_accel(gettext("_Delete"), "win.file-delete", "Delete")
            })
            .item(gettext("Copy _File Names"), "win.edit-copy-fnames")
    });

    menu.append_submenu(Some(&gettext("_Mark")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .item_accel(
                        gettext("_Select All"),
                        "win.mark-select-all",
                        "<Control>KP_Add",
                    )
                    .item_accel(
                        gettext("_Unselect All"),
                        "win.mark-unselect-all",
                        "<Control>KP_Subtract",
                    )
                    .item(gettext("Select all _Files"), "win.mark-select-all-files")
                    .item(
                        gettext("Unselect all Fi_les"),
                        "win.mark-unselect-all-files",
                    )
                    .item_accel(
                        gettext("Select with _Pattern"),
                        "win.mark-select-with-pattern",
                        "KP_Add",
                    )
                    .item_accel(
                        gettext("Unselect with P_attern"),
                        "win.mark-unselect-with-pattern",
                        "KP_Subtract",
                    )
                    .item(
                        gettext("Select with same _Extension"),
                        "win.mark-select-all-with-same-extension",
                    )
                    .item(
                        gettext("Unselect with same E_xtension"),
                        "win.mark-unselect-all-with-same-extension",
                    )
                    .item_accel(
                        gettext("_Invert Selection"),
                        "win.mark-invert-selection",
                        "KP_Multiply",
                    )
                    .item(gettext("_Restore Selection"), "win.mark-restore-selection")
            })
            .item_accel(
                gettext("_Compare Directories"),
                "win.mark-compare-directories",
                "<Shift>F2",
            )
    });

    menu.append_submenu(Some(&gettext("_View")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .item_accel(gettext("_Back"), "win.view-back", "<Alt>Pointer_Left")
                    .item_accel(
                        gettext("_Forward"),
                        "win.view-forward",
                        "<Alt>Pointer_Right",
                    )
                    .item_accel(gettext("_Refresh"), "win.view-refresh", "<Control>R")
            })
            .section({
                gio::Menu::new()
                    .item_accel(
                        gettext("Open in New _Tab"),
                        "win.view-new-tab",
                        "<Control>T",
                    )
                    .item_accel(gettext("_Close Tab"), "win.view-close-tab", "<Control>W")
                    .item_accel(
                        gettext("Close _All Tabs"),
                        "win.view-close-all-tabs",
                        "<Control><Shift>W",
                    )
            })
            .section({
                gio::Menu::new()
                    .item(gettext("Show Toolbar"), "win.view-toolbar")
                    .item(gettext("Show Device Buttons"), "win.view-conbuttons")
                    .item(gettext("Show Device List"), "win.view-devlist")
                    .item(gettext("Show Command Line"), "win.view-cmdline")
                    .item(gettext("Show Buttonbar"), "win.view-buttonbar")
            })
            .section({
                gio::Menu::new()
                    .item_accel(
                        gettext("Show Hidden Files"),
                        "win.view-hidden-files",
                        "<Control><Shift>H",
                    )
                    .item(gettext("Show Backup Files"), "win.view-backup-files")
            })
            .section({
                gio::Menu::new()
                    .item_accel(
                        gettext("_Equal Panel Size"),
                        "win.view-equal-panes",
                        "<Control><Shift>KP_Equal",
                    )
                    .item(gettext("Maximize Panel Size"), "win.view-maximize-pane")
            })
            .section({
                gio::Menu::new().item(
                    gettext("Horizontal Orientation"),
                    "win.view-horizontal-orientation",
                )
            })
    });

    menu.append_submenu(Some(&gettext("_Settings")), &{
        gio::Menu::new()
            .item_accel(gettext("_Options…"), "win.options-edit", "<Control>O")
            .item(
                gettext("_Keyboard Shortcuts…"),
                "win.options-edit-shortcuts",
            )
    });

    menu.append_submenu(Some(&gettext("_Connections")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .item_accel(
                        gettext("_Remote Server…"),
                        "win.connections-open",
                        "<Control>F",
                    )
                    .item_accel(
                        gettext("New Connection…"),
                        "win.connections-new",
                        "<Control>N",
                    )
            })
            .section(local_connections_menu())
            .section(connections_menu())
    });

    menu.append_submenu(Some(&gettext("_Bookmarks")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .item(
                        gettext("_Bookmark this Directory…"),
                        "win.bookmarks-add-current",
                    )
                    .item_accel(
                        gettext("_Manage Bookmarks…"),
                        "win.bookmarks-edit",
                        "<Control>D",
                    )
            })
            .section(create_bookmarks_menu())
    });

    menu.append_submenu(Some(&gettext("_Plugins")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new().item(gettext("_Configure Plugins…"), "win.plugins-configure")
            })
            .section(create_plugins_menu(main_win))
    });

    menu.append_submenu(Some(&gettext("_Settings")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .item_accel(gettext("_Documentation"), "win.help-help", "F1")
                    .item(gettext("_Keyboard Shortcuts"), "win.help-keyboard")
                    .item(gettext("GNOME Commander on the _Web"), "win.help-web")
                    .item(gettext("Report a _Problem"), "win.help-problem")
            })
            .item(gettext("_About"), "win.help-about")
    });

    menu
}

fn local_connections_menu() -> gio::Menu {
    let con_list = ConnectionList::get();
    let all_cons = con_list.all();

    let menu = gio::Menu::new();
    for con in all_cons.iter::<Connection>().flatten() {
        if con
            .downcast_ref::<ConnectionRemote>()
            .map(|c| c.is_open())
            .unwrap_or(true)
        {
            let item = gio::MenuItem::new(con.go_text().as_deref(), None);
            if let Some(icon) = con.go_icon() {
                item.set_icon(&icon);
            }
            item.set_action_and_target_value(
                Some("win.connections-set-current"),
                Some(&con.uuid().to_variant()),
            );

            menu.append_item(&item);
        }
    }
    menu
}

fn connections_menu() -> gio::Menu {
    let con_list = ConnectionList::get();
    let all_cons = con_list.all();

    let menu = gio::Menu::new();

    // Add all open connections that are not permanent
    for con in all_cons.iter::<Connection>().flatten() {
        if con.is_closeable() && con.is_open() {
            let item = gio::MenuItem::new(con.close_text().as_deref(), None);
            if let Some(icon) = con.close_icon() {
                item.set_icon(&icon);
            }
            item.set_action_and_target_value(
                Some("win.connections-close"),
                Some(&con.uuid().to_variant()),
            );
            menu.append_item(&item);
        }
    }
    menu
}

fn create_bookmarks_menu() -> gio::Menu {
    let menu = gio::Menu::new();

    let con_list = ConnectionList::get();
    let all_cons = con_list.all();

    for con in all_cons.iter::<Connection>().flatten() {
        let bookmarks = con.bookmarks();
        if bookmarks.n_items() > 0 {
            let con_uuid = con.uuid();

            // Add bookmarks for this group
            let group_items = gio::Menu::new();
            for bookmark in bookmarks.iter::<Bookmark>().flatten() {
                let name = bookmark.name();
                let item = gio::MenuItem::new(Some(&name), None);
                item.set_action_and_target_value(
                    Some("win.bookmarks-goto"),
                    Some(
                        &BookmarkGoToVariant {
                            connection_uuid: con_uuid.clone(),
                            bookmark_name: name.clone(),
                        }
                        .to_variant(),
                    ),
                );
                group_items.append_item(&item);
            }

            let group_item = gio::MenuItem::new_submenu(con.alias().as_deref(), &group_items);
            if let Some(icon) = con.go_icon() {
                group_item.set_icon(&icon);
            }
            menu.append_item(&group_item);
        }
    }

    menu
}

fn create_plugins_menu(main_win: &MainWindow) -> gio::Menu {
    let menu = gio::Menu::new();
    for (action_group_name, plugin) in main_win.plugin_manager().active_plugins() {
        if let Some(file_actions) = plugin.downcast_ref::<FileActions>() {
            if let Some(plugin_menu) = file_actions.create_main_menu() {
                let plugin_menu = wrap_plugin_menu(&action_group_name, &plugin_menu);
                menu.append_section(None, &plugin_menu);
            }
        }
    }
    menu
}

#[no_mangle]
pub extern "C" fn gnome_cmd_main_menu_new(
    mw_ptr: *mut ffi::GnomeCmdMainWin,
    initial: gboolean,
) -> *mut GMenu {
    let mw: MainWindow = unsafe { from_glib_none(mw_ptr) };
    let menu = main_menu(&mw);
    if initial != 0 {
        let shortcuts = extract_menu_shortcuts(menu.upcast_ref());
        let shortcuts_controller = gtk::ShortcutController::for_model(&shortcuts);
        shortcuts_controller.set_propagation_phase(gtk::PropagationPhase::Capture);
        mw.add_controller(shortcuts_controller);
    }
    menu.to_glib_full()
}
