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
    dir::Directory,
    file::File,
    file_list::list::FileList,
    file_selector::{FileSelector, TabVariant},
    libgcmd::{
        file_actions::{FileActions, FileActionsExt},
        state::{State, StateExt},
    },
    options::{
        options::{GeneralOptions, SearchConfig},
        types::WriteResult,
    },
    paned_ext::GnomeCmdPanedExt,
    plugin_manager::{PluginManager, wrap_plugin_menu},
    search::search_dialog::SearchDialog,
    shortcuts::Shortcuts,
    tags::tags::FileMetadataService,
    transfer::{copy_files, move_files},
    types::{ConfirmOverwriteMode, FileSelectorID},
    user_actions::UserAction,
    utils::{
        ALT_SHIFT, CONTROL, CONTROL_ALT, MenuBuilderExt, NO_MOD, SHIFT, extract_menu_shortcuts,
        sleep,
    },
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, graphene, prelude::*, subclass::prelude::*};
use std::{cell::RefCell, path::Path};

pub mod imp {
    use super::*;
    use crate::{
        command_line::CommandLine,
        dir::Directory,
        layout::{color_themes::ColorThemes, ls_colors_palette::LsColorPalettes},
        options::{
            options::{FiltersOptions, ProgramsOptions},
            utils::remember_window_state,
        },
        pwd::uid,
        shortcuts::Shortcut,
        spawn::{SpawnError, run_command_indir},
        types::QuickSearchShortcut,
    };
    use std::{
        cell::{Cell, RefCell},
        ffi::OsString,
        path::Path,
        time::Duration,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::MainWindow)]
    pub struct MainWindow {
        menubar: gtk::PopoverMenuBar,

        toolbar: gtk::Box,
        con_drop: gtk::Button,
        toolbar_sep: gtk::Separator,
        cmdline_paned: gtk::Paned,
        pub cmdline: CommandLine,
        buttonbar: gtk::Box,
        buttonbar_sep: gtk::Separator,
        view_btn: gtk::Button,
        edit_btn: gtk::Button,
        copy_btn: gtk::Button,
        move_btn: gtk::Button,
        mkdir_btn: gtk::Button,
        delete_btn: gtk::Button,
        find_btn: gtk::Button,

        pub paned: gtk::Paned,
        pub file_selector_left: RefCell<FileSelector>,
        pub file_selector_right: RefCell<FileSelector>,
        #[property(get, set = Self::set_current_panel)]
        current_panel: Cell<u32>,

        pub plugin_manager: PluginManager,
        pub file_metadata_service: FileMetadataService,
        pub cut_and_paste_state: RefCell<Option<CutAndPasteState>>,

        color_themes: ColorThemes,
        ls_color_palettes: LsColorPalettes,

        #[property(get, set)]
        menu_visible: Cell<bool>,
        #[property(get, set)]
        toolbar_visible: Cell<bool>,
        #[property(get, set = Self::set_horizontal_orientation)]
        horizontal_orientation: Cell<bool>,
        #[property(get, set)]
        command_line_visible: Cell<bool>,
        #[property(get, set)]
        buttonbar_visible: Cell<bool>,
        #[property(get, set)]
        connection_buttons_visible: Cell<bool>,
        #[property(get, set = Self::set_connection_list_visible)]
        connection_list_visible: Cell<bool>,
        #[property(get, set = Self::set_view_hidden_files)]
        view_hidden_files: Cell<bool>,
        #[property(get, set = Self::set_view_backup_files)]
        view_backup_files: Cell<bool>,
        #[property(get, set, builder(QuickSearchShortcut::default()))]
        quick_search_shortcut: Cell<QuickSearchShortcut>,

        pub search_dialog: glib::WeakRef<SearchDialog>,

        pub shortcuts: Shortcuts,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for MainWindow {
        const NAME: &'static str = "GnomeCmdMainWindow";
        type Type = super::MainWindow;
        type ParentType = gtk::ApplicationWindow;

        fn class_init(klass: &mut Self::Class) {
            for user_action in UserAction::all() {
                if let Some(property) = user_action.bound_property() {
                    klass.install_property_action(user_action.name(), property);
                } else {
                    klass.install_action(
                        user_action.name(),
                        user_action.parameter_type().as_deref(),
                        move |obj, _, param| user_action.activate(obj.clone(), param),
                    );
                }
            }
        }

        fn new() -> Self {
            let plugin_manager = PluginManager::new();
            let file_metadata_service = FileMetadataService::new(&plugin_manager);

            let cmdline = CommandLine::new();

            let file_selector_left = FileSelector::new(&file_metadata_service);
            file_selector_left.set_command_line(Some(&cmdline));

            let file_selector_right = FileSelector::new(&file_metadata_service);
            file_selector_right.set_command_line(Some(&cmdline));

            Self {
                menubar: gtk::PopoverMenuBar::builder().build(),

                toolbar: gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .css_classes(["main-toolbar"])
                    .build(),
                con_drop: toolbar_button(
                    UserAction::ConnectionsCloseCurrent,
                    REMOTE_DISCONNECT_ICON,
                ),
                toolbar_sep: gtk::Separator::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .build(),

                paned: gtk::Paned::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .wide_handle(true)
                    .hexpand(true)
                    .vexpand(true)
                    .build(),
                file_selector_left: RefCell::new(file_selector_left),
                file_selector_right: RefCell::new(file_selector_right),
                cmdline_paned: gtk::Paned::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .vexpand(true)
                    .shrink_start_child(false)
                    .shrink_end_child(false)
                    .build(),
                cmdline,
                buttonbar: gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .css_classes(["buttonbar"])
                    .build(),
                buttonbar_sep: gtk::Separator::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .build(),
                view_btn: buttonbar_button(&gettext("F3 View"), UserAction::FileView.name()),
                edit_btn: buttonbar_button(&gettext("F4 Edit"), UserAction::FileEdit.name()),
                copy_btn: buttonbar_button(&gettext("F5 Copy"), UserAction::FileCopy.name()),
                move_btn: buttonbar_button(&gettext("F6 Move"), UserAction::FileMove.name()),
                mkdir_btn: buttonbar_button(&gettext("F7 Mkdir"), UserAction::FileMkdir.name()),
                delete_btn: buttonbar_button(&gettext("F8 Delete"), UserAction::FileDelete.name()),
                find_btn: buttonbar_button(&gettext("F9 Search"), UserAction::FileSearch.name()),

                plugin_manager,
                file_metadata_service,
                cut_and_paste_state: Default::default(),

                color_themes: ColorThemes::new(),
                ls_color_palettes: LsColorPalettes::new(),

                current_panel: Cell::new(0),

                menu_visible: Cell::new(true),
                toolbar_visible: Cell::new(true),
                horizontal_orientation: Cell::new(false),
                command_line_visible: Cell::new(true),
                buttonbar_visible: Cell::new(true),
                connection_buttons_visible: Cell::new(true),
                connection_list_visible: Cell::new(true),
                view_hidden_files: Cell::new(true),
                view_backup_files: Cell::new(true),
                quick_search_shortcut: Default::default(),

                search_dialog: Default::default(),
                shortcuts: Shortcuts::new(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for MainWindow {
        fn constructed(&self) {
            self.parent_constructed();

            let mw = self.obj();

            mw.set_title(Some(&if uid() == 0 {
                gettext("GNOME Commander — ROOT PRIVILEGES")
            } else {
                gettext("GNOME Commander")
            }));
            mw.set_icon_name(Some("gnome-commander"));
            mw.set_resizable(true);

            self.plugin_manager.connect_plugins_changed(glib::clone!(
                #[weak]
                mw,
                move |_| mw.imp().update_menu()
            ));

            let vbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .css_classes(["main-box"])
                .build();
            mw.set_child(Some(&vbox));

            let menu = main_menu(&mw);

            self.menubar.set_menu_model(Some(&menu));
            mw.bind_property("menu-visible", &self.menubar, "visible")
                .sync_create()
                .build();
            vbox.append(&self.menubar);

            mw.connect_focus_widget_notify(|mw| {
                if gtk::prelude::GtkWindowExt::focus(mw)
                    .is_some_and(|widget| widget.widget_name() == "GtkPopoverMenuBarItem")
                {
                    // Individual menu bar items should never be focused, return focus to the file list
                    mw.focus_file_lists();
                }
            });

            self.create_toolbar();
            mw.bind_property("toolbar-visible", &self.toolbar, "visible")
                .sync_create()
                .build();
            mw.bind_property("toolbar-visible", &self.toolbar_sep, "visible")
                .sync_create()
                .build();
            vbox.append(&self.toolbar);
            vbox.append(&self.toolbar_sep);

            vbox.append(&self.cmdline_paned);

            self.paned
                .set_start_child(Some(&*self.file_selector_left.borrow()));
            self.paned
                .set_end_child(Some(&*self.file_selector_right.borrow()));

            let focus_controller = gtk::EventControllerFocus::new();
            focus_controller.connect_enter(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.obj().set_current_panel(0),
            ));
            self.file_selector_left
                .borrow()
                .add_controller(focus_controller);

            let focus_controller = gtk::EventControllerFocus::new();
            focus_controller.connect_enter(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.obj().set_current_panel(1),
            ));
            self.file_selector_right
                .borrow()
                .add_controller(focus_controller);

            mw.bind_property(
                "connection-buttons-visible",
                &mw.imp().file_selector_left.borrow().connection_bar(),
                "visible",
            )
            .build();
            mw.bind_property(
                "connection-buttons-visible",
                &mw.imp().file_selector_right.borrow().connection_bar(),
                "visible",
            )
            .build();

            let paned_click_gesture = gtk::GestureClick::builder().button(3).build();
            paned_click_gesture.connect_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, n_press, x, y| imp.on_slide_button_press(n_press, x, y)
            ));
            self.paned.add_controller(paned_click_gesture);

            self.cmdline.connect_lose_focus(glib::clone!(
                #[weak]
                mw,
                move |_| mw.focus_file_lists()
            ));
            self.cmdline.connect_change_directory(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, dir| imp.on_cmdline_change_directory(dir)
            ));
            self.cmdline.connect_execute(glib::clone!(
                #[weak]
                mw,
                move |_, command, in_terminal| {
                    let command = command.to_owned();
                    glib::spawn_future_local(async move {
                        mw.imp().on_cmdline_execute(&command, in_terminal).await;
                    });
                }
            ));

            mw.bind_property("command-line-visible", &self.cmdline, "visible")
                .sync_create()
                .build();
            self.cmdline_paned.set_start_child(Some(&self.paned));
            self.cmdline_paned.set_end_child(Some(&self.cmdline));

            self.create_buttonbar();
            mw.bind_property("buttonbar-visible", &self.buttonbar, "visible")
                .sync_create()
                .build();
            mw.bind_property("buttonbar-visible", &self.buttonbar_sep, "visible")
                .sync_create()
                .build();
            vbox.append(&self.buttonbar_sep);
            vbox.append(&self.buttonbar);

            let mut shortcuts = extract_menu_shortcuts(menu.upcast_ref());
            shortcuts.extend([
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("<Control>x"),
                    Some(gtk::NamedAction::new(UserAction::EditCapCut.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("<Control>c"),
                    Some(gtk::NamedAction::new(UserAction::EditCapCopy.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("<Control>v"),
                    Some(gtk::NamedAction::new(UserAction::EditCapPaste.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("<Control>s"),
                    Some(gtk::NamedAction::new(UserAction::ShowSlidePopup.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("<Control>u"),
                    Some(gtk::NamedAction::new(UserAction::SwapPanes.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("<Alt><Shift>p"),
                    Some(gtk::NamedAction::new(UserAction::PluginsConfigure.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("<Control><Shift>h"),
                    Some(gtk::NamedAction::new(UserAction::ViewHiddenFiles.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F1"),
                    Some(gtk::NamedAction::new(UserAction::HelpHelp.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F2"),
                    Some(gtk::NamedAction::new(UserAction::FileRename.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F3"),
                    Some(gtk::NamedAction::new(UserAction::FileView.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F4"),
                    Some(gtk::NamedAction::new(UserAction::FileEdit.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F5"),
                    Some(gtk::NamedAction::new(UserAction::FileCopy.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F6"),
                    Some(gtk::NamedAction::new(UserAction::FileMove.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F7"),
                    Some(gtk::NamedAction::new(UserAction::FileMkdir.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F8"),
                    Some(gtk::NamedAction::new(UserAction::FileDelete.name())),
                ),
                gtk::Shortcut::new(
                    gtk::ShortcutTrigger::parse_string("F9"),
                    Some(gtk::NamedAction::new(UserAction::FileSearch.name())),
                ),
            ]);
            let shortcuts_controller = gtk::ShortcutController::for_model(&shortcuts);
            mw.add_controller(shortcuts_controller);

            self.update_drop_con_button(None);

            self.file_selector_left
                .borrow()
                .connect_directory_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |fs, dir| imp.on_fs_dir_changed(fs, dir)
                ));
            self.file_selector_right
                .borrow()
                .connect_directory_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |fs, dir| imp.on_fs_dir_changed(fs, dir)
                ));

            self.file_selector_left
                .borrow()
                .connect_activate_request(glib::clone!(
                    #[weak]
                    mw,
                    move |fs| mw.switch_to_fs(fs)
                ));
            self.file_selector_right
                .borrow()
                .connect_activate_request(glib::clone!(
                    #[weak]
                    mw,
                    move |fs| mw.switch_to_fs(fs)
                ));

            self.file_selector_left.borrow().update_connections();
            self.file_selector_right.borrow().update_connections();

            let key_capture_controller = gtk::EventControllerKey::builder()
                .propagation_phase(gtk::PropagationPhase::Capture)
                .build();
            key_capture_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.on_key_pressed_capture(key, state)
            ));
            mw.add_controller(key_capture_controller);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.on_key_pressed(key, state)
            ));
            mw.add_controller(key_controller);

            self.file_selector_left
                .borrow()
                .connect_list_clicked(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move || imp.on_left_fs_select()
                ));
            self.file_selector_right
                .borrow()
                .connect_list_clicked(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move || imp.on_right_fs_select()
                ));

            ConnectionList::get().connect_list_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move || imp.on_con_list_list_changed()
            ));

            let options = GeneralOptions::new();
            remember_window_state(
                &*mw,
                &options.main_window_width,
                &options.main_window_height,
                &options.main_window_state,
            );

            options.menu_visible.bind(&*mw, "menu-visible").build();
            options
                .toolbar_visible
                .bind(&*mw, "toolbar-visible")
                .build();
            options
                .horizontal_orientation
                .bind(&*mw, "horizontal-orientation")
                .build();
            options
                .command_line_visible
                .bind(&*mw, "command-line-visible")
                .build();
            options
                .buttonbar_visible
                .bind(&*mw, "buttonbar-visible")
                .build();
            options
                .connection_buttons_visible
                .bind(&*mw, "connection-buttons-visible")
                .build();
            options
                .connection_list_visible
                .bind(&*mw, "connection-list-visible")
                .build();
            options
                .quick_search_shortcut
                .bind(&*mw, "quick-search-shortcut")
                .build();

            let command_line_split = options.command_line_split.get();
            if command_line_split > 0 {
                self.cmdline_paned.set_position(command_line_split);
            } else {
                // Allocate 10% of the overall height for the command line by default
                let handler_id = std::rc::Rc::new(Cell::new(None));
                let handler_id_cloned = handler_id.clone();
                handler_id.replace(Some(self.cmdline_paned.connect_position_notify(
                    move |this| {
                        if let Some(handler_id) = handler_id_cloned.take() {
                            this.disconnect(handler_id);
                        }
                        if let Some(rect) = this.compute_bounds(this) {
                            this.set_position((rect.height() * 0.9) as i32);
                        }
                    },
                )));
            }
            options
                .command_line_split
                .bind(&self.cmdline_paned, "position")
                .set_only()
                .build();

            self.shortcuts.load(options.keybindings.get());

            self.color_themes.connect_local(
                "theme-changed",
                false,
                glib::clone!(
                    #[weak(rename_to = this)]
                    self.obj(),
                    #[upgrade_or]
                    None,
                    move |_| {
                        this.update_view();
                        None
                    }
                ),
            );
            self.ls_color_palettes.connect_local(
                "palette-changed",
                false,
                glib::clone!(
                    #[weak(rename_to = this)]
                    self.obj(),
                    #[upgrade_or]
                    None,
                    move |_| {
                        this.update_view();
                        None
                    }
                ),
            );

            let filters_options = FiltersOptions::new();
            filters_options
                .hide_hidden
                .bind(&*mw, "view-hidden-files")
                .invert_boolean()
                .build();
            filters_options
                .hide_backup
                .bind(&*mw, "view-backup-files")
                .invert_boolean()
                .build();
        }

        fn dispose(&self) {
            if let Err(error) = self.obj().save_state() {
                eprintln!("Failed to save state: {error}");
            }
        }
    }

    impl WidgetImpl for MainWindow {
        fn realize(&self) {
            self.parent_realize();
            let _ =
                WidgetExt::activate_action(&*self.obj(), UserAction::ViewEqualPanes.name(), None);
        }
    }

    impl WindowImpl for MainWindow {}
    impl ApplicationWindowImpl for MainWindow {}

    impl MainWindow {
        pub fn update_menu(&self) {
            let menu = main_menu(&self.obj());
            self.menubar.set_menu_model(Some(&menu));
        }

        fn create_toolbar(&self) {
            self.toolbar
                .append(&toolbar_button(UserAction::ViewRefresh, "view-refresh"));
            self.toolbar
                .append(&toolbar_button(UserAction::ViewUp, "go-up"));
            self.toolbar
                .append(&toolbar_button(UserAction::ViewFirst, "go-first"));
            self.toolbar
                .append(&toolbar_button(UserAction::ViewBack, "go-previous"));
            self.toolbar
                .append(&toolbar_button(UserAction::ViewForward, "go-next"));
            self.toolbar
                .append(&toolbar_button(UserAction::ViewLast, "go-last"));
            self.toolbar
                .append(&gtk::Separator::new(gtk::Orientation::Vertical));
            self.toolbar.append(&toolbar_button(
                UserAction::EditCopyNames,
                COPY_FILE_NAMES_ICON,
            ));
            self.toolbar
                .append(&toolbar_button(UserAction::EditCapCut, "edit-cut"));
            self.toolbar
                .append(&toolbar_button(UserAction::EditCapCopy, "edit-copy"));
            self.toolbar
                .append(&toolbar_button(UserAction::EditCapPaste, "edit-paste"));
            self.toolbar
                .append(&toolbar_button(UserAction::FileDelete, DELETE_FILE_ICON));
            self.toolbar
                .append(&toolbar_button(UserAction::FileEdit, EDIT_FILE_ICON));
            self.toolbar.append(&toolbar_button(
                UserAction::FileSendto,
                GTK_MAILSEND_STOCKID,
            ));
            self.toolbar.append(&toolbar_button(
                UserAction::CommandOpenTerminal,
                GTK_TERMINAL_STOCKID,
            ));
            self.toolbar
                .append(&gtk::Separator::new(gtk::Orientation::Vertical));
            self.toolbar.append(&toolbar_button(
                UserAction::ConnectionsOpen,
                REMOTE_CONNECT_ICON,
            ));
            self.toolbar.append(&self.con_drop);
        }

        pub fn update_drop_con_button(&self, connection: Option<&Connection>) {
            if let Some(connection) = connection {
                let closeable = connection.is_closeable();
                self.obj()
                    .action_set_enabled(UserAction::ConnectionsCloseCurrent.name(), closeable);

                if closeable {
                    self.con_drop
                        .set_tooltip_text(connection.close_tooltip().as_deref());

                    if let Some(icon) = connection.close_icon() {
                        self.con_drop
                            .set_child(Some(&gtk::Image::from_gicon(&icon)));
                    } else {
                        self.con_drop.set_label(
                            &connection
                                .close_text()
                                .unwrap_or_else(|| gettext("Drop connection")),
                        );
                    }
                    return;
                }
            }

            self.obj()
                .action_set_enabled(UserAction::ConnectionsCloseCurrent.name(), false);
            self.con_drop.set_icon_name(REMOTE_DISCONNECT_ICON);
            self.con_drop.set_tooltip_text(None);
        }

        fn create_buttonbar(&self) {
            fn separator() -> gtk::Separator {
                gtk::Separator::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .build()
            }

            self.buttonbar.append(&self.view_btn);
            self.buttonbar.append(&separator());
            self.buttonbar.append(&self.edit_btn);
            self.buttonbar.append(&separator());
            self.buttonbar.append(&self.copy_btn);
            self.buttonbar.append(&separator());
            self.buttonbar.append(&self.move_btn);
            self.buttonbar.append(&separator());
            self.buttonbar.append(&self.mkdir_btn);
            self.buttonbar.append(&separator());
            self.buttonbar.append(&self.delete_btn);
            self.buttonbar.append(&separator());
            self.buttonbar.append(&self.find_btn);
        }

        fn on_slide_button_press(&self, n_press: i32, x: f64, y: f64) {
            if n_press != 1 {
                return;
            }

            let point = graphene::Point::new(x as f32, y as f32);
            if self
                .paned
                .handle_rect()
                .is_some_and(|r| r.contains_point(&point))
            {
                self.show_slide_popup_at(&point);
            }
        }

        pub fn show_slide_popup_at(&self, point: &graphene::Point) {
            let popover = gtk::PopoverMenu::builder()
                .menu_model(&create_slide_popup())
                .pointing_to(&gdk::Rectangle::new(
                    point.x() as i32,
                    point.y() as i32,
                    0,
                    0,
                ))
                .build();
            popover.set_parent(&self.paned);
            popover.connect_closed(glib::clone!(
                #[weak(rename_to = this)]
                self,
                move |popover| {
                    // Delay removing the menu to allow the selected action to be handled first.
                    let popover = popover.clone();
                    glib::spawn_future_local(async move {
                        popover.unparent();
                        this.obj().focus_file_lists();
                    });
                }
            ));
            popover.present();
            popover.popup();
        }

        fn set_horizontal_orientation(&self, horizontal_orientation: bool) {
            self.horizontal_orientation.set(horizontal_orientation);
            self.paned.set_orientation(if horizontal_orientation {
                gtk::Orientation::Vertical
            } else {
                gtk::Orientation::Horizontal
            });

            let _ =
                WidgetExt::activate_action(&*self.obj(), UserAction::ViewEqualPanes.name(), None);
            self.obj().focus_file_lists();
        }

        fn on_cmdline_change_directory(&self, dest_dir: &str) {
            let file_selector = self.obj().file_selector(FileSelectorID::Active);

            if dest_dir == "-"
                && !file_selector.file_list().directory().is_some_and(|d| {
                    d.get_child_gfile(Path::new(dest_dir))
                        .query_exists(gio::Cancellable::NONE)
                })
            {
                file_selector.back();
            } else {
                file_selector
                    .file_list()
                    .goto_directory(Path::new(dest_dir));
            }
        }

        async fn on_cmdline_execute(&self, command: &str, in_terminal: bool) {
            let file_list = self.obj().file_selector(FileSelectorID::Active).file_list();

            if file_list.connection().is_some_and(|c| c.is_local()) {
                let working_directory = file_list
                    .directory()
                    .and_then(|d| d.upcast_ref::<File>().get_real_path());

                let command: OsString = command.into();
                let options = ProgramsOptions::new();

                let result = run_command_indir(
                    working_directory.as_deref(),
                    &command,
                    in_terminal,
                    &options,
                )
                .map_err(SpawnError::into_message);

                if let Err(error) = result {
                    error.show(self.obj().upcast_ref()).await;
                }
            }
        }

        fn set_current_panel(&self, panel: u32) {
            self.current_panel.set(panel);
            let fs = match panel {
                0 => {
                    self.file_selector_left.borrow().set_active(true);
                    self.file_selector_right.borrow().set_active(false);
                    self.file_selector_left.borrow()
                }
                _ => {
                    self.file_selector_right.borrow().set_active(true);
                    self.file_selector_left.borrow().set_active(false);
                    self.file_selector_right.borrow()
                }
            };
            self.update_browse_buttons(&fs);
            self.update_drop_con_button(fs.file_list().connection().as_ref());
            self.update_cmdline();
            fs.file_list().grab_focus();
        }

        fn on_left_fs_select(&self) {
            self.obj().set_current_panel(0);
        }

        fn on_right_fs_select(&self) {
            self.obj().set_current_panel(1);
        }

        fn on_fs_dir_changed(&self, fs: &FileSelector, _dir: &Directory) {
            self.update_browse_buttons(fs);
            self.update_drop_con_button(fs.file_list().connection().as_ref());
            self.update_cmdline();
        }

        fn on_con_list_list_changed(&self) {
            self.update_menu();
        }

        fn set_connection_list_visible(&self, value: bool) {
            self.connection_list_visible.set(value);
            self.file_selector_left.borrow().update_show_devlist(value);
            self.file_selector_right.borrow().update_show_devlist(value);
        }

        fn set_view_hidden_files(&self, value: bool) {
            self.view_hidden_files.set(value);
            self.update_view_in_next_cycle();
        }

        fn set_view_backup_files(&self, value: bool) {
            self.view_backup_files.set(value);
            self.update_view_in_next_cycle();
        }

        fn update_view_in_next_cycle(&self) {
            glib::timeout_add_local_once(
                Duration::from_millis(0),
                glib::clone!(
                    #[strong(rename_to = this)]
                    self.obj(),
                    move || this.update_view()
                ),
            );
            self.obj().update_view();
        }

        fn update_browse_buttons(&self, fs: &FileSelector) {
            if *fs == self.obj().file_selector(FileSelectorID::Active) {
                let can_back = fs.can_back();
                let can_forward = fs.can_forward();
                self.obj()
                    .action_set_enabled(UserAction::ViewFirst.name(), can_back);
                self.obj()
                    .action_set_enabled(UserAction::ViewBack.name(), can_back);
                self.obj()
                    .action_set_enabled(UserAction::ViewForward.name(), can_forward);
                self.obj()
                    .action_set_enabled(UserAction::ViewLast.name(), can_forward);
            }
        }

        fn update_cmdline(&self) {
            let directory = self
                .obj()
                .file_selector(FileSelectorID::Active)
                .file_list()
                .directory()
                .map(|d| d.display_path())
                .unwrap_or_default();
            self.cmdline.set_directory(&directory);
        }

        fn on_key_pressed_capture(
            &self,
            key: gdk::Key,
            state: gdk::ModifierType,
        ) -> glib::Propagation {
            match (state, key) {
                (CONTROL, gdk::Key::e | gdk::Key::E | gdk::Key::Down) => {
                    if self.cmdline.is_visible() {
                        self.cmdline.show_history();
                    }
                    return glib::Propagation::Stop;
                }
                (ALT_SHIFT, gdk::Key::f | gdk::Key::F) => {
                    if let Some(remote_con) = ConnectionList::get()
                        .all()
                        .iter::<Connection>()
                        .flatten()
                        .find_map(|c| c.downcast::<ConnectionRemote>().ok())
                    {
                        self.obj()
                            .file_selector(FileSelectorID::Active)
                            .file_list()
                            .set_connection(&remote_con, None);
                    }
                    return glib::Propagation::Stop;
                }
                _ => {}
            }

            if self
                .shortcuts
                .handle_key_event(&self.obj(), Shortcut { key, state })
            {
                glib::Propagation::Stop
            } else {
                glib::Propagation::Proceed
            }
        }

        fn on_key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match (state, key) {
                (CONTROL_ALT, gdk::Key::c | gdk::Key::C) => {
                    if self.cmdline.is_visible()
                        && self.quick_search_shortcut.get() == QuickSearchShortcut::JustACharacter
                    {
                        self.cmdline.grab_focus();
                    }
                    glib::Propagation::Stop
                }
                (ALT_SHIFT, gdk::Key::f | gdk::Key::F) => {
                    if let Some(remote_con) = ConnectionList::get()
                        .all()
                        .iter::<Connection>()
                        .flatten()
                        .find_map(|c| c.downcast::<ConnectionRemote>().ok())
                    {
                        self.obj()
                            .file_selector(FileSelectorID::Active)
                            .file_list()
                            .set_connection(&remote_con, None);
                    }
                    glib::Propagation::Stop
                }
                (NO_MOD, gdk::Key::Tab | gdk::Key::ISO_Left_Tab) => {
                    self.obj().switch_to_opposite();
                    glib::Propagation::Stop
                }
                (SHIFT, gdk::Key::Tab | gdk::Key::ISO_Left_Tab) => glib::Propagation::Stop,
                (NO_MOD, gdk::Key::Escape) => {
                    if self.cmdline.is_visible() {
                        self.cmdline.set_text("");
                    }
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }
    }

    fn toolbar_button(action: UserAction, icon: &str) -> gtk::Button {
        let button = gtk::Button::builder()
            .icon_name(icon)
            .tooltip_text(&action.description())
            .action_name(action.name())
            .build();
        button.add_css_class("flat");
        button
    }

    fn buttonbar_button(label: &str, action_name: &str) -> gtk::Button {
        gtk::Button::builder()
            .label(label)
            .action_name(action_name)
            .has_frame(false)
            .can_focus(false)
            .hexpand(true)
            .build()
    }

    const COPY_FILE_NAMES_ICON: &str = "gnome-commander-copy-file-names-symbolic";
    const DELETE_FILE_ICON: &str = "gnome-commander-recycling-bin-symbolic";
    const EDIT_FILE_ICON: &str = "gnome-commander-edit-symbolic";
    const GTK_MAILSEND_STOCKID: &str = "mail-send";
    const GTK_TERMINAL_STOCKID: &str = "utilities-terminal";
    const REMOTE_CONNECT_ICON: &str = "gnome-commander-folder-remote-symbolic";
    const REMOTE_DISCONNECT_ICON: &str = "gnome-commander-folder-remote-disconnect-symbolic";

    fn create_slide_popup() -> gio::Menu {
        let menu = gio::Menu::new();

        for (label, value) in [
            ("100 - 0", 100),
            ("80 - 20", 80),
            ("60 - 40", 60),
            ("50 - 50", 50),
            ("40 - 60", 40),
            ("20 - 80", 20),
            ("0 - 100", 0),
        ] {
            let item = gio::MenuItem::new(Some(label), None);
            item.set_action_and_target_value(
                Some(UserAction::ViewSlide.name()),
                Some(&value.to_variant()),
            );
            menu.append_item(&item);
        }
        menu
    }
}

glib::wrapper! {
    pub struct MainWindow(ObjectSubclass<imp::MainWindow>)
        @extends gtk::ApplicationWindow, gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native,
        gio::ActionMap, gio::ActionGroup;
}

impl MainWindow {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn left_panel(&self) -> FileSelector {
        self.imp().file_selector_left.borrow().clone()
    }

    pub fn right_panel(&self) -> FileSelector {
        self.imp().file_selector_right.borrow().clone()
    }

    pub fn file_selectors(&self) -> (FileSelector, FileSelector) {
        match self.current_panel() {
            0 => (self.left_panel(), self.right_panel()),
            _ => (self.right_panel(), self.left_panel()),
        }
    }

    pub fn file_selector(&self, id: FileSelectorID) -> FileSelector {
        match id {
            FileSelectorID::Left => self.left_panel(),
            FileSelectorID::Right => self.right_panel(),
            FileSelectorID::Active => match self.current_panel() {
                0 => self.left_panel(),
                _ => self.right_panel(),
            },
            FileSelectorID::Inactive => match self.current_panel() {
                0 => self.right_panel(),
                _ => self.left_panel(),
            },
        }
    }

    pub fn swap_panels(&self) {
        let fs1 = self.left_panel();
        let fs2 = self.right_panel();

        // swap widgets
        self.imp().paned.set_start_child(gtk::Widget::NONE);
        self.imp().paned.set_end_child(gtk::Widget::NONE);
        self.imp().paned.set_start_child(Some(&fs2));
        self.imp().paned.set_end_child(Some(&fs1));

        RefCell::swap(
            &self.imp().file_selector_left,
            &self.imp().file_selector_right,
        );

        self.switch_to_opposite();
    }

    fn switch_to_fs(&self, file_selector: &FileSelector) {
        if file_selector != &self.file_selector(FileSelectorID::Active) {
            self.switch_to_opposite();
        }
    }

    fn switch_to_opposite(&self) {
        self.set_current_panel(1_u32.saturating_sub(self.current_panel()));
    }

    pub fn set_directory_to_opposite(&self, id: FileSelectorID) {
        let (dst, src) = match id {
            FileSelectorID::Left => (self.left_panel(), self.right_panel()),
            FileSelectorID::Right => (self.right_panel(), self.left_panel()),
            FileSelectorID::Active => self.file_selectors(),
            FileSelectorID::Inactive => {
                let (active, inactive) = self.file_selectors();
                (inactive, active)
            }
        };

        let Some(dir) = src
            .active()
            .then(|| {
                src.file_list()
                    .selected_file()
                    .filter(|f| f.file_info().file_type() == gio::FileType::Directory)
                    .and_downcast::<Directory>()
            })
            .flatten()
            .or_else(|| src.file_list().directory())
        else {
            return;
        };

        if dst.is_current_tab_locked() {
            dst.new_tab_with_dir(&dir, true, false);
        } else {
            dst.file_list()
                .set_connection(&dir.connection(), Some(&dir));
        }
    }

    pub fn load_tabs(&self, start_left_dir: Option<&Path>, start_right_dir: Option<&Path>) {
        let options = GeneralOptions::new();

        let (mut left_tabs, mut right_tabs): (Vec<_>, Vec<_>) = options
            .file_list_tabs
            .get()
            .into_iter()
            .partition(|t| t.file_felector_id == 0);

        if let Some(dir) = start_left_dir.and_then(|d| d.to_str()) {
            left_tabs.push(TabVariant::new(dir));
        }

        if let Some(dir) = start_right_dir.and_then(|d| d.to_str()) {
            right_tabs.push(TabVariant::new(dir));
        }

        self.file_selector(FileSelectorID::Left)
            .open_tabs(left_tabs);

        self.file_selector(FileSelectorID::Right)
            .open_tabs(right_tabs);
    }

    fn save_tabs(&self, save_all: bool, save_current: bool) -> WriteResult {
        let options = GeneralOptions::new();

        let mut tabs = Vec::<TabVariant>::new();
        tabs.extend(
            self.file_selector(FileSelectorID::Left)
                .save_tabs(save_all, save_current)
                .into_iter()
                .map(|tab| TabVariant {
                    file_felector_id: 0,
                    ..tab
                }),
        );
        tabs.extend(
            self.file_selector(FileSelectorID::Right)
                .save_tabs(save_all, save_current)
                .into_iter()
                .map(|tab| TabVariant {
                    file_felector_id: 1,
                    ..tab
                }),
        );

        options.file_list_tabs.set(tabs)
    }

    pub fn load_command_line_history(&self, options: &GeneralOptions) {
        self.imp()
            .cmdline
            .set_max_history_size(options.command_line_history_length.get() as usize);
        self.imp()
            .cmdline
            .set_history(&options.command_line_history.get());
    }

    pub fn save_command_line_history(&self, options: &GeneralOptions) -> WriteResult {
        if options.save_command_line_history_on_exit.get() {
            options
                .command_line_history
                .set(self.imp().cmdline.history())
        } else {
            options.command_line_history.set(&[])
        }
    }

    pub fn save_state(&self) -> WriteResult {
        let options = GeneralOptions::new();

        self.imp().plugin_manager.save();

        options.keybindings.set(self.imp().shortcuts.save())?;
        self.save_tabs(
            options.save_tabs_on_exit.get(),
            options.save_dirs_on_exit.get(),
        )?;
        self.save_command_line_history(&options)?;

        gio::Settings::sync();

        Ok(())
    }

    pub fn change_connection(&self, id: FileSelectorID) {
        let file_selector = self.file_selector(id);
        self.switch_to_fs(&file_selector);
        file_selector.activate_connection_list();
    }

    pub fn focus_file_lists(&self) {
        let (active, inactive) = self.file_selectors();
        active.set_active(true);
        inactive.set_active(false);
        active.grab_focus();
    }

    pub fn update_view(&self) {
        self.update_style();
    }

    pub fn shortcuts(&self) -> &Shortcuts {
        &self.imp().shortcuts
    }

    pub fn state(&self) -> State {
        let fl1 = self.file_selector(FileSelectorID::Active).file_list();
        let fl2 = self.file_selector(FileSelectorID::Inactive).file_list();
        let dir1 = fl1.directory();
        let dir2 = fl2.directory();

        let state = State::new();
        state.set_active_dir(dir1.and_upcast_ref());
        state.set_inactive_dir(dir2.and_upcast_ref());
        state.set_active_dir_files(&fl1.visible_files());
        state.set_inactive_dir_files(&fl2.visible_files());
        state.set_active_dir_selected_files(&fl1.selected_files());
        state.set_inactive_dir_selected_files(&fl2.selected_files());

        state
    }

    pub fn plugin_manager(&self) -> PluginManager {
        self.imp().plugin_manager.clone()
    }

    pub fn file_metadata_service(&self) -> FileMetadataService {
        self.imp().file_metadata_service.clone()
    }

    pub fn get_or_create_search_dialog(&self) -> SearchDialog {
        if let Some(dialog) = self.imp().search_dialog.upgrade() {
            dialog
        } else {
            let search_config = SearchConfig::get();
            let options = GeneralOptions::new();

            let dialog = SearchDialog::new(
                search_config,
                &self.file_metadata_service(),
                self,
                options.search_window_is_transient.get(),
            );
            self.imp().search_dialog.set(Some(&dialog));

            dialog
        }
    }

    pub fn update_style(&self) {
        self.imp().file_selector_left.borrow().update_style();
        self.imp().file_selector_right.borrow().update_style();
        self.imp().cmdline.update_style();
        if let Some(dialog) = self.imp().search_dialog.upgrade() {
            dialog.update_style();
        }
    }
}

enum CutAndPasteOperation {
    Cut,
    Copy,
}

pub struct CutAndPasteState {
    operation: CutAndPasteOperation,
    source_file_list: FileList,
    files: glib::List<File>,
}

impl MainWindow {
    fn set_cap_state(&self, state: bool) {
        self.action_set_enabled(UserAction::EditCapPaste.name(), state);
    }

    pub fn cut_files(&self) {
        let source_file_list = self.file_selector(FileSelectorID::Active).file_list();
        let files = source_file_list.selected_files();
        if files.is_empty() {
            return;
        }

        self.imp()
            .cut_and_paste_state
            .replace(Some(CutAndPasteState {
                source_file_list,
                operation: CutAndPasteOperation::Cut,
                files,
            }));
        self.set_cap_state(true);
    }

    pub fn copy_files(&self) {
        let source_file_list = self.file_selector(FileSelectorID::Active).file_list();
        let files = source_file_list.selected_files();
        if files.is_empty() {
            return;
        }

        self.imp()
            .cut_and_paste_state
            .replace(Some(CutAndPasteState {
                source_file_list,
                operation: CutAndPasteOperation::Copy,
                files,
            }));
        self.set_cap_state(true);
    }

    pub async fn paste_files(&self) {
        let destination_file_list = self.file_selector(FileSelectorID::Active).file_list();
        let Some(dir) = destination_file_list.directory() else {
            return;
        };

        self.set_cap_state(false);
        let Some(state) = self.imp().cut_and_paste_state.take() else {
            return;
        };

        let files = state.files.iter().map(|f| f.file()).collect();
        let success = match state.operation {
            CutAndPasteOperation::Cut => {
                move_files(
                    self.clone().upcast(),
                    files,
                    dir.clone(),
                    None,
                    gio::FileCopyFlags::NONE,
                    ConfirmOverwriteMode::Query,
                )
                .await
            }
            CutAndPasteOperation::Copy => {
                copy_files(
                    self.clone().upcast(),
                    files,
                    dir.clone(),
                    None,
                    gio::FileCopyFlags::NONE,
                    ConfirmOverwriteMode::Query,
                )
                .await
            }
        };
        if !success {
            eprintln!("Transfer failed");
        }

        state.source_file_list.reload().await;
        if let Err(error) = dir.relist_files(self.upcast_ref(), false).await {
            error.show(self.upcast_ref()).await;
        }
        self.focus_file_lists();
    }

    pub fn set_slide(&self, percentage: i32) -> bool {
        let paned = &self.imp().paned;
        let dimension = if self.horizontal_orientation() {
            paned.height()
        } else {
            paned.width()
        };
        let new_dimension = dimension * percentage / 100;

        if paned.is_position_set() && paned.position() == new_dimension {
            true
        } else {
            paned.set_position(new_dimension);
            false
        }
    }

    pub async fn ensure_slide_position(&self, percentage: i32) {
        loop {
            sleep(10).await;
            if self.set_slide(percentage) {
                break;
            }
        }
    }

    pub fn show_slide_popup(&self) {
        if let Some(handle_rect) = self.imp().paned.handle_rect() {
            self.imp().show_slide_popup_at(&handle_rect.center());
        }
    }
}

fn main_menu(main_win: &MainWindow) -> gio::Menu {
    let menu = gio::Menu::new();

    menu.append_submenu(Some(&gettext("_File")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::FileChown)
                    .action(UserAction::FileChmod)
                    .action_accel(UserAction::FileAdvrename, "<Control>M")
                    .action_accel(UserAction::FileCreateSymlink, "<Control><shift>F5")
                    .action_accel(UserAction::FileProperties, "<Alt>KP_Enter")
            })
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::FileSearch, "F9")
                    .action(UserAction::FileQuickSearch)
                    .action(UserAction::EditFilter)
            })
            .section({
                gio::Menu::new()
                    .action(UserAction::FileDiff)
                    .action(UserAction::FileSyncDirs)
            })
    });

    menu.append_submenu(Some(&gettext("_Edit")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::EditCapCut, "<Control>X")
                    .action_accel(UserAction::EditCapCopy, "<Control>C")
                    .action_accel(UserAction::EditCapPaste, "<Control>V")
                    .action_accel(UserAction::FileDelete, "Delete")
            })
            .action(UserAction::EditCopyNames)
    });

    menu.append_submenu(Some(&gettext("_Mark")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::MarkSelectAll, "<Control>KP_Add")
                    .action_accel(UserAction::MarkUnselectAll, "<Control>KP_Subtract")
                    .action(UserAction::MarkSelectAllFiles)
                    .action(UserAction::MarkUnselectAllFiles)
                    .action_accel(UserAction::MarkSelectWithPattern, "KP_Add")
                    .action_accel(UserAction::MarkUnselectWithPattern, "KP_Subtract")
                    .action(UserAction::MarkSelectAllWithSameExtension)
                    .action(UserAction::MarkUnselectAllWithSameExtension)
                    .action_accel(UserAction::MarkInvertSelection, "KP_Multiply")
                    .action(UserAction::MarkRestoreSelection)
            })
            .action_accel(UserAction::MarkCompareDirectories, "<Shift>F2")
    });

    menu.append_submenu(Some(&gettext("_View")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::ViewBack, "<Alt>Pointer_Left")
                    .action_accel(UserAction::ViewForward, "<Alt>Pointer_Right")
                    .action_accel(UserAction::ViewRefresh, "<Control>R")
            })
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::ViewNewTab, "<Control>T")
                    .action_accel(UserAction::ViewCloseTab, "<Control>W")
                    .action_accel(UserAction::ViewCloseAllTabs, "<Control><Shift>W")
            })
            .section({
                gio::Menu::new()
                    .action(UserAction::ViewToolbar)
                    .action(UserAction::ViewConbuttons)
                    .action(UserAction::ViewDevlist)
                    .action(UserAction::ViewCmdline)
                    .action(UserAction::ViewButtonbar)
            })
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::ViewHiddenFiles, "<Control><Shift>H")
                    .action(UserAction::ViewBackupFiles)
            })
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::ViewEqualPanes, "<Control><Shift>KP_Equal")
                    .action(UserAction::ViewMaximizePane)
            })
            .section(gio::Menu::new().action(UserAction::ViewHorizontalOrientation))
    });

    menu.append_submenu(Some(&gettext("_Settings")), &{
        gio::Menu::new()
            .action_accel(UserAction::OptionsEdit, "<Control>O")
            .action(UserAction::OptionsEditShortcuts)
    });

    menu.append_submenu(Some(&gettext("_Connections")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::ConnectionsOpen, "<Control>F")
                    .action_accel(UserAction::ConnectionsNew, "<Control>N")
            })
            .section(local_connections_menu())
            .section(connections_menu())
    });

    menu.append_submenu(Some(&gettext("_Bookmarks")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::BookmarksAddCurrent)
                    .action_accel(UserAction::BookmarksEdit, "<Control>D")
            })
            .section(create_bookmarks_menu())
    });

    menu.append_submenu(Some(&gettext("_Plugins")), &{
        gio::Menu::new()
            .section(gio::Menu::new().action(UserAction::PluginsConfigure))
            .section(create_plugins_menu(main_win))
    });

    menu.append_submenu(Some(&gettext("_Help")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action_accel(UserAction::HelpHelp, "F1")
                    .action(UserAction::HelpKeyboard)
                    .action(UserAction::HelpWeb)
                    .action(UserAction::HelpProblem)
            })
            .action(UserAction::HelpAbout)
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
            item.set_action_and_target_value(
                Some(UserAction::ConnectionsSetCurrent.name()),
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
            item.set_action_and_target_value(
                Some(UserAction::ConnectionsClose.name()),
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
                    Some(UserAction::BookmarksGoto.name()),
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

            menu.append_item(&gio::MenuItem::new_submenu(
                con.alias().as_deref(),
                &group_items,
            ));
        }
    }

    menu
}

fn create_plugins_menu(main_win: &MainWindow) -> gio::Menu {
    let menu = gio::Menu::new();
    for (action_group_name, plugin) in main_win.plugin_manager().active_plugins() {
        if let Some(file_actions) = plugin.downcast_ref::<FileActions>()
            && let Some(plugin_menu) = file_actions.create_main_menu()
        {
            let plugin_menu = wrap_plugin_menu(&action_group_name, &plugin_menu);
            menu.append_section(None, &plugin_menu);
        }
    }
    menu
}
