// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    config::{PACKAGE, PLUGIN_DIR},
    connection::{
        Connection, ConnectionExt, bookmark::BookmarkGoToVariant, list::ConnectionList,
        remote::ConnectionRemote,
    },
    dir::Directory,
    file::{File, FileOps},
    file_selector::{FileSelector, TabPosition, TabState, TabVariant},
    layout::color_themes::ColorThemes,
    options::{GeneralOptions, types::WriteResult},
    paned_ext::GnomeCmdPanedExt,
    plugins::{
        ApiRequestToPlugin, ApiResponseFromPlugin, InactivePluginHostChannel,
        MessageFromPluginHost, MessageToPluginHost, PanelsState, PluginHost, PluginHostChannel,
    },
    search::search_dialog::SearchDialog,
    shortcuts::{Area, LegacyShortcutVariant, Shortcuts},
    spawn::{SpawnError, app_needs_terminal, run_command_indir},
    tags::FileMetadataService,
    types::FileSelectorID,
    user_actions::UserAction,
    utils::{ErrorMessage, MenuBuilderExt, sleep},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, graphene, prelude::*, subclass::prelude::*};
use std::{cell::RefCell, ffi::OsString, path::Path, rc::Rc};

#[derive(Debug, Default, Copy, Clone, PartialEq, Eq, glib::ValueDelegate)]
#[value_delegate(from = u32)]
pub enum ExecutionTarget {
    #[default]
    Background,
    AnyTerminal,
    EmbeddedTerminal,
    ExternalTerminal,
}

impl From<ExecutionTarget> for u32 {
    fn from(value: ExecutionTarget) -> Self {
        value as Self
    }
}

impl From<&ExecutionTarget> for u32 {
    fn from(value: &ExecutionTarget) -> Self {
        *value as Self
    }
}

impl From<u32> for ExecutionTarget {
    fn from(value: u32) -> Self {
        match value {
            v if v == u32::from(ExecutionTarget::Background) => ExecutionTarget::Background,
            v if v == u32::from(ExecutionTarget::AnyTerminal) => ExecutionTarget::AnyTerminal,
            v if v == u32::from(ExecutionTarget::EmbeddedTerminal) => {
                ExecutionTarget::EmbeddedTerminal
            }
            v if v == u32::from(ExecutionTarget::ExternalTerminal) => {
                ExecutionTarget::ExternalTerminal
            }
            _ => Default::default(),
        }
    }
}

pub mod imp {
    use super::*;
    use crate::{
        command_line::CommandLine,
        config::PIXMAPS_DIR,
        dir::Directory,
        layout::ls_colors_palette::LsColorPalettes,
        options::{FiltersOptions, utils::remember_window_state},
        pwd::uid,
    };
    use std::{
        cell::{Cell, RefCell},
        collections::HashMap,
        path::PathBuf,
        time::Duration,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::MainWindow)]
    pub struct MainWindow {
        menubar: gtk::PopoverMenuBar,

        toolbar: gtk::Box,
        con_drop: gtk::Button,
        con_drop_image: gtk::Image,
        toolbar_sep: gtk::Separator,
        cmdline_paned: gtk::Paned,
        #[property(get)]
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
        pub focus_controller_left: gtk::EventControllerFocus,
        pub focus_controller_right: gtk::EventControllerFocus,
        #[property(get, set = Self::set_current_panel)]
        current_panel: Cell<u32>,

        pub plugin_channel: InactivePluginHostChannel,
        pub file_metadata_service: FileMetadataService,

        pub color_themes: Rc<ColorThemes>,
        ls_color_palettes: Rc<LsColorPalettes>,

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
        #[property(get, set)]
        cmdline_autohide_output: Cell<bool>,

        pub active_dialogs: RefCell<HashMap<String, glib::WeakRef<gtk::Window>>>,

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
            let system_plugins_dir = Path::new(PLUGIN_DIR);
            let user_plugins_dir = glib::user_config_dir().join(PACKAGE).join("plugins");
            let (plugin_host, plugin_channel) =
                PluginHost::new(system_plugins_dir, &user_plugins_dir);
            glib::spawn_future_local(plugin_host);

            let file_metadata_service = FileMetadataService::new(plugin_channel.clone());

            let (con_drop, con_drop_image) = build_con_drop_button();

            let cmdline = CommandLine::new();

            let file_selector_left = FileSelector::new();
            file_selector_left.set_command_line(Some(&cmdline));

            let file_selector_right = FileSelector::new();
            file_selector_right.set_command_line(Some(&cmdline));

            Self {
                menubar: gtk::PopoverMenuBar::builder().build(),

                toolbar: gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .css_classes(["main-toolbar"])
                    .build(),
                con_drop,
                con_drop_image,
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
                focus_controller_left: gtk::EventControllerFocus::new(),
                focus_controller_right: gtk::EventControllerFocus::new(),
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

                plugin_channel,
                file_metadata_service,

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
                cmdline_autohide_output: Cell::new(true),

                active_dialogs: Default::default(),
                shortcuts: Shortcuts::new(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for MainWindow {
        fn constructed(&self) {
            self.parent_constructed();

            let mw = self.obj();

            self.handle_plugin_host_requests(&self.plugin_channel);

            mw.set_title(Some(&if uid() == 0 {
                gettext("Gnome Commander — ROOT PRIVILEGES")
            } else {
                gettext("Gnome Commander")
            }));
            mw.set_icon_name(Some("gnome-commander"));
            mw.set_resizable(true);

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
                    // Individual menu bar items should never be focused, return focus to the
                    // content area. This is quite a hack to compensate for Gtk's shortcomings but
                    // worst-case scenario is that this won't work in some future Gtk version and
                    // focus simply remains on the menu requiring the user to click.
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
            vbox.append(&self.cmdline);

            self.paned
                .set_start_child(Some(&*self.file_selector_left.borrow()));
            self.paned
                .set_end_child(Some(&*self.file_selector_right.borrow()));

            self.focus_controller_left.connect_enter(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.obj().set_current_panel(0),
            ));
            self.file_selector_left
                .borrow()
                .add_controller(self.focus_controller_left.clone());

            self.focus_controller_right.connect_enter(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.obj().set_current_panel(1),
            ));
            self.file_selector_right
                .borrow()
                .add_controller(self.focus_controller_right.clone());

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
                move |_, command, target| {
                    let command = command.to_owned();
                    glib::spawn_future_local(async move {
                        mw.execute_command_with_message(&command, target).await;
                    });
                }
            ));

            mw.bind_property("command-line-visible", &self.cmdline, "visible")
                .sync_create()
                .build();
            self.cmdline_paned.set_start_child(Some(&self.paned));
            self.cmdline_paned
                .set_end_child(Some(self.cmdline.output()));

            self.create_buttonbar();
            mw.bind_property("buttonbar-visible", &self.buttonbar, "visible")
                .sync_create()
                .build();
            mw.bind_property("buttonbar-visible", &self.buttonbar_sep, "visible")
                .sync_create()
                .build();
            vbox.append(&self.buttonbar_sep);
            vbox.append(&self.buttonbar);

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

            let options = GeneralOptions::instance();
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
                .command_line_autohide_output
                .bind(&*mw, "cmdline-autohide-output")
                .build();

            let command_line_split = options.command_line_split.get();
            if command_line_split > 0 {
                self.cmdline_paned.set_position(command_line_split);
            } else {
                // Allocate 10% of the overall height for the command line by default
                let handler_id = Rc::new(Cell::new(None));
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

            self.shortcuts
                .load(&options.keybindings.get(), options.legacy_keybindings.get());
            self.shortcuts.attach(&*mw);
            self.shortcuts.add_controller(&*mw, Area::MainWindow);
            self.file_selector_left
                .borrow()
                .add_shortcuts(&self.shortcuts);
            self.file_selector_right
                .borrow()
                .add_shortcuts(&self.shortcuts);
            self.cmdline.add_shortcuts(&self.shortcuts);

            self.color_themes.set_update_callback(glib::clone!(
                #[weak(rename_to = this)]
                self.obj(),
                move |_| this.update_view()
            ));
            self.ls_color_palettes.set_update_callback(glib::clone!(
                #[weak(rename_to = this)]
                self.obj(),
                move |_| this.update_view()
            ));

            let filters_options = FiltersOptions::instance();
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
        fn handle_plugin_host_requests(&self, channel: &InactivePluginHostChannel) {
            let mut channel = channel.activate_cloned();
            let obj = self.obj().clone();
            glib::spawn_future_local(async move {
                loop {
                    if let MessageFromPluginHost::RunCommand {
                        id,
                        plugin_name,
                        command,
                        target,
                    } = channel.receive().await
                    {
                        let result = obj.execute_command(&command, target).await;
                        channel.send(MessageToPluginHost::RunCommandResult {
                            id,
                            plugin_name,
                            result,
                        });
                    }
                }
            });
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

                    if let Some(icon) = connection.open_icon() {
                        self.con_drop_image.set_from_gicon(&icon);
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
            self.con_drop_image
                .set_icon_name(Some(REMOTE_DISCONNECT_ICON));
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
            let file_list = file_selector.file_list();

            if dest_dir == "-"
                && !file_list
                    .directory()
                    .get_child_gfile(Path::new(dest_dir))
                    .query_exists(gio::Cancellable::NONE)
            {
                file_selector.back();
            } else {
                if let Some(relative) = dest_dir.strip_prefix("~") {
                    let mut dir = ConnectionList::get().home().default_dir();
                    for part in relative.split(std::path::MAIN_SEPARATOR) {
                        if !part.is_empty() {
                            dir = dir.child(part);
                        }
                    }
                    file_selector.goto(&file_list, &dir);
                } else {
                    let path = PathBuf::from(
                        glib::shell_unquote(dest_dir).unwrap_or_else(|_| dest_dir.into()),
                    );
                    if path.is_absolute() {
                        file_selector.goto_path(&file_list, &path);
                    } else {
                        file_selector.goto(&file_list, &file_list.directory().child(&path));
                    }
                };
            }
        }

        fn set_current_panel(&self, panel: u32) {
            self.current_panel.set(panel);
            let (fs, focus_controller) = match panel {
                0 => {
                    self.file_selector_left.borrow().set_active(true);
                    self.file_selector_right.borrow().set_active(false);
                    (
                        self.file_selector_left.borrow(),
                        &self.focus_controller_left,
                    )
                }
                _ => {
                    self.file_selector_right.borrow().set_active(true);
                    self.file_selector_left.borrow().set_active(false);
                    (
                        self.file_selector_right.borrow(),
                        &self.focus_controller_right,
                    )
                }
            };
            self.update_browse_buttons(&fs);
            self.update_drop_con_button(Some(&fs.file_list().connection()));
            self.update_cmdline();

            // We might get here because main window got focus. Do not grab focus unnecessarily,
            // this will move focus away from filter box for example.
            if !focus_controller.contains_focus() {
                fs.file_list().grab_focus();
            }
        }

        fn on_left_fs_select(&self) {
            self.obj().set_current_panel(0);
        }

        fn on_right_fs_select(&self) {
            self.obj().set_current_panel(1);
        }

        fn on_fs_dir_changed(&self, fs: &FileSelector, _dir: &Directory) {
            self.update_browse_buttons(fs);
            self.update_drop_con_button(Some(&fs.file_list().connection()));
            self.update_cmdline();
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
                .display_path();
            self.cmdline.set_directory(&directory);
        }
    }

    fn toolbar_button(action: UserAction, icon: &str) -> gtk::Button {
        let button = gtk::Button::builder()
            .icon_name(icon)
            .tooltip_text(action.description())
            .action_name(action.name())
            .build();
        button.add_css_class("flat");
        button
    }

    fn build_con_drop_button() -> (gtk::Button, gtk::Image) {
        let button = toolbar_button(UserAction::ConnectionsCloseCurrent, REMOTE_DISCONNECT_ICON);

        let image = gtk::Image::new();
        let overlay = gtk::Overlay::builder().child(&image).build();
        let overlay_image = gtk::Image::builder()
            .pixel_size(9)
            .halign(gtk::Align::End)
            .valign(gtk::Align::End)
            .build();
        overlay_image.set_from_file(Some(Path::new(PIXMAPS_DIR).join("overlay_umount.png")));
        overlay.add_overlay(&overlay_image);

        button.set_child(Some(&overlay));
        (button, image)
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

    pub fn plugin_channel(&self) -> PluginHostChannel {
        self.imp().plugin_channel.activate_cloned()
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

    pub fn switch_to_opposite(&self) {
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

        let dir = src
            .active()
            .then(|| {
                src.file_list()
                    .selected_file()
                    .filter(|file| file.is_directory())
                    .map(|file| {
                        Directory::new_from_file(&src.file_list().connection(), &*file.file())
                    })
            })
            .flatten()
            .unwrap_or_else(|| src.file_list().directory());

        dst.goto(&dst.file_list(), &dir);
    }

    pub fn load_tabs(&self, start_left_dir: Option<&Path>, start_right_dir: Option<&Path>) {
        let options = GeneralOptions::instance();
        let mut tabs = options.file_list_tabs.get();
        if tabs.is_empty() {
            tabs = options
                .legacy_file_list_tabs
                .get()
                .into_iter()
                .map(TabVariant::from)
                .collect();
        }

        let (mut left_tabs, mut right_tabs): (Vec<_>, Vec<_>) = tabs
            .into_iter()
            .partition(|t| t.position() == TabPosition::LeftOrTop);

        if let Some(dir) = start_left_dir
            .and_then(|dir| std::path::absolute(dir).ok())
            .and_then(|dir| glib::filename_to_uri(dir, None).ok())
        {
            let mut tab = TabVariant::new(Default::default(), dir.to_string());
            tab.set_state(TabState::Selected);
            left_tabs.push(tab);
        }

        if let Some(dir) = start_right_dir
            .and_then(|dir| std::path::absolute(dir).ok())
            .and_then(|dir| glib::filename_to_uri(dir, None).ok())
        {
            let mut tab = TabVariant::new(Default::default(), dir.to_string());
            tab.set_state(TabState::Selected);
            right_tabs.push(tab);
        }

        let current_panel = if right_tabs.iter().any(|tab| tab.state() == TabState::Active) {
            1
        } else {
            0
        };
        self.file_selector(FileSelectorID::Left)
            .open_tabs(left_tabs);
        self.file_selector(FileSelectorID::Right)
            .open_tabs(right_tabs);
        self.set_current_panel(current_panel);
    }

    fn save_tabs(&self, save_all: bool, save_current: bool, save_history: bool) -> WriteResult {
        let mut tabs = Vec::<TabVariant>::new();
        tabs.extend(self.file_selector(FileSelectorID::Left).save_tabs(
            TabPosition::LeftOrTop,
            save_all,
            save_current,
            save_history,
        ));
        tabs.extend(self.file_selector(FileSelectorID::Right).save_tabs(
            TabPosition::RightOrBottom,
            save_all,
            save_current,
            save_history,
        ));

        let options = GeneralOptions::instance();

        // Reset legacy option, making sure we don't import it more than once
        let _ = options.legacy_file_list_tabs.set(Vec::new());

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
        let options = GeneralOptions::instance();

        options.keybindings.set(self.imp().shortcuts.save())?;

        // Reset legacy option, making sure we don't import it more than once
        options
            .legacy_keybindings
            .set(glib::Variant::array_from_iter::<LegacyShortcutVariant>(
                [].into_iter(),
            ))?;

        self.save_tabs(
            options.save_tabs_on_exit.get(),
            options.save_dirs_on_exit.get(),
            options.save_directory_history_on_exit.get(),
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

    pub fn state(&self) -> PanelsState {
        fn get_file_name(file: impl FileOps) -> Option<String> {
            file.path_name().to_str().map(str::to_owned)
        }

        let fl1 = self.file_selector(FileSelectorID::Active).file_list();
        let fl2 = self.file_selector(FileSelectorID::Inactive).file_list();
        let dir1 = fl1.directory();
        let dir2 = fl2.directory();

        PanelsState {
            active_directory_path: dir1
                .local_path()
                .and_then(|path| path.to_str().map(str::to_owned)),
            active_directory_uri: dir1.uri(),
            active_focused_file: fl1.selected_file().and_then(get_file_name),
            active_selected_files: fl1
                .selected_files()
                .into_iter()
                .filter_map(get_file_name)
                .collect(),
            inactive_directory_path: dir2
                .local_path()
                .and_then(|path| path.to_str().map(str::to_owned)),
            inactive_directory_uri: dir2.uri(),
            inactive_focused_file: fl2.selected_file().and_then(get_file_name),
            inactive_selected_files: fl2
                .selected_files()
                .into_iter()
                .filter_map(get_file_name)
                .collect(),
        }
    }

    pub fn file_metadata_service(&self) -> &FileMetadataService {
        &self.imp().file_metadata_service
    }

    pub fn get_dialog<T: IsA<gtk::Window>>(&self, handle: &str) -> Option<T> {
        self.imp()
            .active_dialogs
            .borrow()
            .get(handle)
            .and_then(|weak_ref| weak_ref.upgrade())
            .and_downcast::<T>()
    }

    pub fn set_dialog<T: IsA<gtk::Window>>(&self, handle: &str, dialog: T) -> T {
        self.imp()
            .active_dialogs
            .borrow_mut()
            .insert(handle.to_owned(), dialog.as_ref().downgrade());
        dialog
    }

    pub fn get_or_create_dialog<F, T>(&self, handle: &str, initializer: F) -> T
    where
        F: FnOnce() -> T,
        T: IsA<gtk::Window>,
    {
        self.get_dialog(handle)
            .unwrap_or_else(|| self.set_dialog(handle, initializer()))
    }

    pub fn update_style(&self) {
        self.imp().file_selector_left.borrow().update_style();
        self.imp().file_selector_right.borrow().update_style();
        self.imp().cmdline.update_style();
        if let Some(dialog) = self.get_dialog::<SearchDialog>("search") {
            dialog.update_style();
        }
    }

    pub async fn execute_command(
        &self,
        command: &str,
        mut target: ExecutionTarget,
    ) -> Result<(), ErrorMessage> {
        let file_list = self.file_selector(FileSelectorID::Active).file_list();

        let working_directory = file_list.directory().local_path();

        if target == ExecutionTarget::AnyTerminal {
            target = if self.imp().cmdline.terminal_available() {
                ExecutionTarget::EmbeddedTerminal
            } else {
                ExecutionTarget::ExternalTerminal
            };
        }

        if target == ExecutionTarget::EmbeddedTerminal {
            let cmdline = &self.imp().cmdline;
            if !cmdline.terminal_available() {
                return Err(ErrorMessage::brief(gettext("Embedded terminal is busy")));
            }
            cmdline
                .exec(working_directory.as_deref(), command)
                .await
                .map_err(|error| ErrorMessage::with_error(gettext("Failed starting shell"), &error))
        } else {
            run_command_indir(
                working_directory.as_deref(),
                &OsString::from(command),
                target == ExecutionTarget::ExternalTerminal,
            )
            .map_err(SpawnError::into_message)
        }
    }

    pub async fn execute_command_with_message(
        &self,
        command: &str,
        target: ExecutionTarget,
    ) -> bool {
        if let Err(error) = self.execute_command(command, target).await {
            error.show(self.upcast_ref()).await;
            false
        } else {
            true
        }
    }

    pub async fn execute_file(&self, file: &File) -> bool {
        let mut command = String::from("./");
        command.push_str(&glib::shell_quote(file.file_info().display_name()).to_string_lossy());
        self.execute_command_with_message(
            &command,
            if app_needs_terminal(file) {
                ExecutionTarget::AnyTerminal
            } else {
                ExecutionTarget::Background
            },
        )
        .await
    }
}

impl MainWindow {
    pub fn set_slide(&self, percentage: i32) -> bool {
        let paned = &self.imp().paned;
        let dimension = if self.horizontal_orientation() {
            paned.height()
        } else {
            paned.width()
        };
        if dimension == 0 {
            return false;
        }
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

    pub fn color_themes(&self) -> &Rc<ColorThemes> {
        &self.imp().color_themes
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
                    .action(UserAction::FileAdvrename)
                    .action(UserAction::FileCreateSymlink)
                    .action(UserAction::FileProperties)
            })
            .section({
                gio::Menu::new()
                    .action(UserAction::FileSearch)
                    .action(UserAction::FileQuickSearch)
                    .action(UserAction::FileQuickFilter)
            })
            .section({
                gio::Menu::new()
                    .action(UserAction::FileDiff)
                    .action(UserAction::FileSyncDirs)
            })
            .section(gio::Menu::new().action(UserAction::FileExit))
    });

    menu.append_submenu(Some(&gettext("_Edit")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::EditCapCut)
                    .action(UserAction::EditCapCopy)
                    .action(UserAction::EditCapPaste)
                    .action(UserAction::FileDelete)
            })
            .action(UserAction::EditCopyNames)
    });

    menu.append_submenu(Some(&gettext("_Mark")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::MarkSelectAll)
                    .action(UserAction::MarkUnselectAll)
                    .action(UserAction::MarkSelectAllFiles)
                    .action(UserAction::MarkUnselectAllFiles)
                    .action(UserAction::MarkSelectWithPattern)
                    .action(UserAction::MarkUnselectWithPattern)
                    .action(UserAction::MarkSelectAllWithSameExtension)
                    .action(UserAction::MarkUnselectAllWithSameExtension)
                    .action(UserAction::MarkInvertSelection)
            })
            .action(UserAction::MarkCompareDirectories)
    });

    menu.append_submenu(Some(&gettext("_View")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::ViewBack)
                    .action(UserAction::ViewForward)
                    .action(UserAction::ViewRefresh)
            })
            .section({
                gio::Menu::new()
                    .action(UserAction::ViewNewTab)
                    .action(UserAction::ViewCloseTab)
                    .action(UserAction::ViewCloseAllTabs)
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
                    .action(UserAction::ViewHiddenFiles)
                    .action(UserAction::ViewBackupFiles)
                    .action(UserAction::CmdlineAutohideOutput)
            })
            .section({
                gio::Menu::new()
                    .action(UserAction::ViewEqualPanes)
                    .action(UserAction::ViewMaximizePane)
            })
            .section(gio::Menu::new().action(UserAction::ViewHorizontalOrientation))
    });

    menu.append_submenu(Some(&gettext("_Settings")), &{
        gio::Menu::new()
            .action(UserAction::OptionsEdit)
            .action(UserAction::OptionsEditShortcuts)
    });

    let connections_goto = gio::Menu::new();
    let connections_disconnect = gio::Menu::new();
    menu.append_submenu(Some(&gettext("_Connections")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::ConnectionsOpen)
                    .action(UserAction::ConnectionsNew)
            })
            .section(connections_goto.clone())
            .section(connections_disconnect.clone())
    });

    local_connections_menu(&connections_goto);
    connections_menu(&connections_disconnect);
    ConnectionList::get().connect_list_changed(move || {
        local_connections_menu(&connections_goto);
        connections_menu(&connections_disconnect);
    });

    menu.append_submenu(Some(&gettext("_Bookmarks")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::BookmarksAddCurrent)
                    .action(UserAction::BookmarksEdit)
            })
            .section(create_bookmarks_menu())
    });

    let plugins = gio::Menu::new();
    menu.append_submenu(Some(&gettext("_Plugins")), &{
        gio::Menu::new()
            .section(gio::Menu::new().action(UserAction::PluginsConfigure))
            .section(plugins.clone())
    });

    let mut channel = main_win.plugin_channel();
    glib::spawn_future_local(async move {
        plugins_menu(&mut channel, &plugins).await;
        loop {
            if let MessageFromPluginHost::PluginUpdated(..) = channel.receive().await {
                plugins_menu(&mut channel, &plugins).await;
            }
        }
    });

    menu.append_submenu(Some(&gettext("_Help")), &{
        gio::Menu::new()
            .section({
                gio::Menu::new()
                    .action(UserAction::HelpHelp)
                    .action(UserAction::HelpKeyboard)
                    .action(UserAction::HelpWeb)
                    .action(UserAction::HelpProblem)
            })
            .action(UserAction::HelpAbout)
    });

    menu
}

fn local_connections_menu(menu: &gio::Menu) {
    menu.remove_all();
    for con in ConnectionList::get().iter() {
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
}

fn connections_menu(menu: &gio::Menu) {
    menu.remove_all();

    // Add all open connections that are not permanent
    for con in ConnectionList::get().iter() {
        if con.is_closeable() && con.is_open() {
            let item = gio::MenuItem::new(con.close_text().as_deref(), None);
            item.set_action_and_target_value(
                Some(UserAction::ConnectionsClose.name()),
                Some(&con.uuid().to_variant()),
            );
            menu.append_item(&item);
        }
    }
}

fn create_bookmarks_menu() -> gio::Menu {
    let menu = gio::Menu::new();

    for con in ConnectionList::get().iter() {
        let bookmarks = con.bookmarks();
        if !bookmarks.is_empty() {
            let con_uuid = con.uuid();

            // Add bookmarks for this group
            let group_items = gio::Menu::new();
            for bookmark in &*bookmarks {
                let item = gio::MenuItem::new(Some(bookmark.name()), None);
                item.set_action_and_target_value(
                    Some(UserAction::BookmarksGoto.name()),
                    Some(
                        &BookmarkGoToVariant {
                            connection_uuid: con_uuid.clone(),
                            bookmark_name: bookmark.name().to_owned(),
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

async fn plugins_menu(channel: &mut PluginHostChannel, menu: &gio::Menu) {
    menu.remove_all();

    let id = channel.new_id();
    channel.send(MessageToPluginHost::ApiRequest {
        id,
        plugin_name: None,
        request: ApiRequestToPlugin::MainMenuItems,
    });

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
            if let Some(ApiResponseFromPlugin::MainMenuItems(items)) = response {
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
            Some(&(&plugin_name, &item.action, &item.parameter).to_variant()),
        );
        menu.append_item(&menuitem);
    }
}
