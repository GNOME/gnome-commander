// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    connection::{
        Connection, ConnectionExt, ConnectionInterface, home::ConnectionHome, list::ConnectionList,
        remote::ConnectionRemote,
    },
    dir::Directory,
    file::{File, FileOps},
    file_list::list::{ColumnID, ColumnOptions, FileList},
    main_win::{ExecutionTarget, MainWindow},
    notebook_ext::{GnomeCmdNotebookExt, TabClick},
    open_file::mime_exec_single,
    shortcuts::{Area, Shortcuts},
    tab_label::TabLabel,
    types::MiddleMouseButtonMode,
    user_actions::UserAction,
    utils::u32_enum,
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, graphene, pango, prelude::*, subclass::prelude::*};
use std::{
    collections::{BTreeMap, BTreeSet, HashMap},
    path::{Path, PathBuf},
};

mod imp {
    use super::*;
    use crate::{
        command_line::CommandLine,
        connection::{device::ConnectionDevice, smb::ConnectionSmb},
        connection_bar::ConnectionBar,
        dialogs::manage_bookmarks_dialog::bookmark_directory,
        directory_indicator::DirectoryIndicator,
        options::GeneralOptions,
        tab_label::TabLabel,
        utils::get_modifiers_state,
        weak_set::WeakSet,
    };
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::FileSelector)]
    pub struct FileSelector {
        pub connection_list: ConnectionList,

        pub connection_bar: ConnectionBar,
        pub connection_dropdown: gtk::DropDown,
        pub volume_size_label: gtk::Label,
        pub bookmark_button: gtk::ToggleButton,
        pub history_button: gtk::ToggleButton,
        pub directory_indicator_box: gtk::Box,
        pub directory_indicator: DirectoryIndicator,
        pub notebook: gtk::Notebook,
        pub info_label: gtk::Label,
        pub(super) quick_search_box: gtk::Box,

        #[property(get, set, nullable)]
        pub command_line: RefCell<Option<CommandLine>>,

        #[property(get, set = Self::set_always_show_tabs)]
        always_show_tabs: Cell<bool>,

        #[property(get, set = Self::set_active)]
        active: Cell<bool>,

        #[property(get, set = Self::set_list, nullable)]
        list: RefCell<Option<FileList>>,

        select_connection_in_progress: Cell<bool>,
        pub locked_tabs: WeakSet<FileList>,

        pub tab_popover: gtk::PopoverMenu,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileSelector {
        const NAME: &'static str = "GnomeCmdFileSelector";
        type Type = super::FileSelector;
        type ParentType = gtk::Grid;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action(
                "fs.select-path",
                Some(&String::static_variant_type()),
                |obj, _, param| {
                    if let Some(path) = param.and_then(String::from_variant) {
                        obj.on_navigate(&path, obj.imp().is_control_pressed());
                    }
                },
            );
            klass.install_action(
                "fs.select-history-entry",
                Some(&u32::static_variant_type()),
                |obj, _, param| {
                    let file_list = obj.file_list();
                    if let Some(index) = param.and_then(u32::from_variant)
                        && let Some((connection, path)) =
                            file_list.dir_history().get(index as usize)
                    {
                        file_list.dir_history().set_current_position(index as usize);
                        obj.goto(
                            &file_list,
                            &Self::Type::directory_for_path(&connection, &path),
                        );
                    }
                },
            );
            klass.install_action_async("fs.add-bookmark", None, |obj, _, _| async move {
                obj.imp().add_bookmark().await
            });
            klass.install_action(
                "fs.toggle-tab-lock",
                Some(&i32::static_variant_type()),
                |obj, _, param| {
                    if let Some(index) = param
                        .and_then(i32::from_variant)
                        .and_then(|i| i.try_into().ok())
                    {
                        obj.imp().toggle_tab_lock(index);
                    }
                },
            );
            klass.install_action_async(
                "fs.refresh-tab",
                Some(&i32::static_variant_type()),
                |obj, _, param| async move {
                    if let Some(index) = param
                        .and_then(|p| i32::from_variant(&p))
                        .and_then(|i| i.try_into().ok())
                    {
                        obj.imp().refresh_tab(index).await;
                    }
                },
            );
        }

        fn new() -> Self {
            let connection_list = ConnectionList::get();

            let model = gtk::FilterListModel::new(
                Some(connection_list.all()),
                Some(gtk::CustomFilter::new(|item| {
                    item.downcast_ref::<Connection>().is_some_and(|con| {
                        con.is_open()
                            || con.downcast_ref::<ConnectionDevice>().is_some()
                            || con.downcast_ref::<ConnectionSmb>().is_some()
                    })
                })),
            );

            Self {
                connection_bar: ConnectionBar::new(connection_list),
                connection_list: connection_list.clone(),
                connection_dropdown: gtk::DropDown::builder()
                    .model(&model)
                    .halign(gtk::Align::Start)
                    .factory(&connection_dropdown_factory())
                    .build(),
                volume_size_label: gtk::Label::builder()
                    .hexpand(true)
                    .halign(gtk::Align::End)
                    .valign(gtk::Align::Center)
                    .margin_start(6)
                    .margin_end(6)
                    .build(),

                bookmark_button: gtk::ToggleButton::builder()
                    .child(&gtk::Image::from_gicon(&gio::ThemedIcon::from_names(&[
                        "bookmarks",
                        "gnome-commander-bookmark-outline",
                    ])))
                    .can_focus(false)
                    .has_frame(false)
                    .build(),
                history_button: gtk::ToggleButton::builder()
                    .icon_name("go-down")
                    .can_focus(false)
                    .has_frame(false)
                    .build(),

                directory_indicator_box: gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .build(),
                directory_indicator: DirectoryIndicator::default(),

                notebook: gtk::Notebook::builder()
                    .show_tabs(false)
                    .show_border(false)
                    .scrollable(true)
                    .hexpand(true)
                    .vexpand(true)
                    .build(),
                info_label: gtk::Label::builder()
                    .ellipsize(pango::EllipsizeMode::End)
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .margin_start(6)
                    .margin_end(6)
                    .build(),

                quick_search_box: gtk::Box::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .build(),

                command_line: Default::default(),

                always_show_tabs: Default::default(),
                active: Default::default(),
                list: Default::default(),

                select_connection_in_progress: Default::default(),
                locked_tabs: Default::default(),

                tab_popover: gtk::PopoverMenu::builder()
                    .position(gtk::PositionType::Bottom)
                    .build(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileSelector {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_column_spacing(6);
            this.add_css_class("gnome-cmd-file-selector");

            self.connection_bar.set_hexpand(true);
            this.attach(&self.connection_bar, 0, 0, 2, 1);

            this.attach(&self.connection_dropdown, 0, 1, 1, 1);
            this.attach(&self.volume_size_label, 1, 1, 1, 1);

            self.connection_bar.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, connection, new_tab| imp.open_connection(connection, new_tab)
            ));

            let scrolled_window = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Never)
                .hexpand(true)
                .margin_start(6)
                .margin_end(6)
                .child(&self.directory_indicator)
                .build();
            self.directory_indicator_box.append(&scrolled_window);

            self.directory_indicator_box.append(&self.bookmark_button);

            self.directory_indicator_box.append(&self.history_button);

            this.attach(&self.directory_indicator_box, 0, 2, 2, 1);

            self.bookmark_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| this.show_bookmarks()
            ));
            self.history_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| this.show_history()
            ));

            this.attach(&self.notebook, 0, 3, 2, 1);
            this.attach(&self.quick_search_box, 0, 4, 2, 1);
            this.attach(&self.info_label, 0, 5, 1, 1);

            self.tab_popover.set_parent(&self.notebook);

            self.connection_dropdown
                .connect_selected_item_notify(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.on_connection_dropdown_item_selected()
                ));

            self.directory_indicator.connect_navigate(glib::clone!(
                #[weak]
                this,
                move |_, path, new_tab| this.on_navigate(path, new_tab)
            ));

            self.connection_list.connect_list_changed(glib::clone!(
                #[weak]
                this,
                move || this.update_connections()
            ));

            self.notebook.connect_switch_page(glib::clone!(
                #[weak]
                this,
                move |_, _, n| {
                    let file_list = this.file_list_nth(n);
                    let directory = file_list.directory();

                    this.set_list(Some(&file_list));
                    this.imp()
                        .directory_indicator
                        .set_directory(Some(&directory));
                    this.imp().update_selected_files_label();
                    this.update_volume_label();

                    this.emit_by_name::<()>("dir-changed", &[&directory]);
                    this.imp().select_connection(&file_list.connection());
                    file_list.grab_focus();
                    if let Some(pos) = file_list.focused_file_position() {
                        // Make sure that selected item is still focused
                        let view = file_list.view();
                        glib::spawn_future_local(async move {
                            view.scroll_to(pos, None, gtk::ListScrollFlags::FOCUS, None);
                        });
                    }
                }
            ));

            let notebook_click = gtk::GestureClick::builder()
                .propagation_phase(gtk::PropagationPhase::Capture)
                .button(0)
                .build();
            notebook_click.connect_pressed(glib::clone!(
                #[weak]
                this,
                move |gesture, n_press, x, y| {
                    let button = gesture.current_button();
                    glib::spawn_future_local(async move {
                        on_notebook_button_pressed(&this, n_press, button, x, y).await;
                    });
                }
            ));
            self.notebook.add_controller(notebook_click);

            GeneralOptions::instance()
                .always_show_tabs
                .bind(&*this, "always-show-tabs")
                .build();
        }

        fn dispose(&self) {
            self.tab_popover.unparent();
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("dir-changed")
                        .param_types([Directory::static_type()])
                        .build(),
                    glib::subclass::Signal::builder("list-clicked").build(),
                    glib::subclass::Signal::builder("activate-request").build(),
                ]
            })
        }
    }

    impl WidgetImpl for FileSelector {
        fn grab_focus(&self) -> bool {
            if let Some(list) = self.obj().current_file_list() {
                list.grab_focus()
            } else {
                self.parent_grab_focus()
            }
        }
    }

    impl GridImpl for FileSelector {}

    impl FileSelector {
        fn open_connection(&self, con: &Connection, new_tab: bool) {
            let file_list = self.obj().file_list();
            if new_tab {
                self.obj()
                    .new_tab(&con.default_dir(), TabOptions::from(&file_list));
            } else {
                self.obj().goto(&file_list, &con.default_dir());
            }
            self.obj().emit_by_name::<()>("activate-request", &[]);
        }

        pub fn is_control_pressed(&self) -> bool {
            let mask = self
                .obj()
                .root()
                .and_downcast::<gtk::Window>()
                .as_ref()
                .and_then(get_modifiers_state);
            mask.is_some_and(|m| m.contains(gdk::ModifierType::CONTROL_MASK))
        }

        pub fn set_quick_search(&self, widget: Option<gtk::Widget>) {
            while let Some(child) = self.quick_search_box.first_child() {
                child.unparent();
            }
            if let Some(widget) = widget {
                self.quick_search_box.append(&widget);
            }
        }

        async fn add_bookmark(&self) {
            let Some(window) = self.obj().root().and_downcast::<gtk::Window>() else {
                return;
            };
            let dir = self.obj().file_list().directory();
            bookmark_directory(&window, &dir).await;
        }

        fn toggle_tab_lock(&self, index: u32) {
            let fl = self.obj().file_list_nth(index);
            let locked = self.obj().is_tab_locked(&fl);
            self.obj().set_tab_locked(&fl, !locked);
            self.update_tab_label(&fl);
        }

        async fn refresh_tab(&self, index: u32) {
            let list = self.obj().file_list_nth(index);
            list.reload().await;
        }

        fn find_connection_in_dropdown(&self, con: &Connection) -> Option<u32> {
            let model = self.connection_dropdown.model()?;
            (0..model.n_items()).find(|position| {
                model.item(*position).and_downcast::<Connection>().as_ref() == Some(con)
            })
        }

        pub fn select_connection(&self, con: &Connection) {
            if let Some(position) = self.find_connection_in_dropdown(con) {
                self.select_connection_in_progress.set(true);
                self.connection_dropdown.set_selected(position);
                self.select_connection_in_progress.set(false);
            }
        }

        fn on_connection_dropdown_item_selected(&self) {
            if self.select_connection_in_progress.get() {
                return;
            }
            if self
                .find_connection_in_dropdown(&self.obj().file_list().connection())
                .is_none()
            {
                // Current connection is not in dropdown, this selection is probably due to it
                // being closed right now. Ignore this event.
                return;
            }
            if let Some(connection) = self
                .connection_dropdown
                .selected_item()
                .and_downcast::<Connection>()
            {
                self.open_connection(&connection, self.is_control_pressed());
            }
        }

        pub fn update_tab_label(&self, fl: &FileList) {
            let Some(tab_label) = self.notebook.tab_label(fl).and_downcast::<TabLabel>() else {
                return;
            };

            tab_label.set_label(fl.directory().name());
            tab_label.set_locked(self.obj().is_tab_locked(fl));
            tab_label.set_indicator(GeneralOptions::instance().tab_lock_indicator.get());
        }

        pub fn update_selected_files_label(&self) {
            if let Some(file_list) = self.obj().current_file_list() {
                let stats = file_list.stats_str(GeneralOptions::instance().size_display_mode.get());
                self.info_label.set_text(&stats);
            }
        }

        fn set_always_show_tabs(&self, value: bool) {
            self.always_show_tabs.set(value);
            self.obj().update_show_tabs();
        }

        fn set_active(&self, active: bool) {
            self.active.set(active);
            self.directory_indicator.set_active(active);
            if active {
                self.obj().add_css_class("active");
            } else {
                self.obj().remove_css_class("active");
            }
        }

        fn set_list(&self, list: Option<FileList>) {
            self.list.replace(list);
            self.set_quick_search(None);
        }
    }

    fn connection_dropdown_factory() -> gtk::ListItemFactory {
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(|_, item| {
            let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let hbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .build();
            let image = gtk::Image::new();
            hbox.append(&image);
            let label = gtk::Label::new(None);
            hbox.append(&label);
            list_item.set_child(Some(&hbox));
        });
        factory.connect_bind(|_, item| {
            let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(hbox) = list_item.child() else {
                return;
            };
            let Some(image) = hbox.first_child().and_downcast::<gtk::Image>() else {
                return;
            };
            let Some(label) = image.next_sibling().and_downcast::<gtk::Label>() else {
                return;
            };

            let (icon, text) =
                if let Some(connection) = list_item.item().and_downcast::<Connection>() {
                    (connection.go_icon(), Some(connection.display_name()))
                } else {
                    (None, None)
                };

            match icon {
                Some(icon) => image.set_from_gicon(&icon),
                None => image.set_icon_name(None),
            }
            label.set_text(text.as_deref().unwrap_or_default());
        });
        factory.upcast()
    }
}

glib::wrapper! {
    pub struct FileSelector(ObjectSubclass<imp::FileSelector>)
        @extends gtk::Grid, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::Orientable;
}

impl FileSelector {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    fn current_file_list(&self) -> Option<FileList> {
        self.list()
    }

    pub fn file_list(&self) -> FileList {
        self.current_file_list().unwrap()
    }

    pub fn file_list_nth(&self, n: u32) -> FileList {
        let notebook = &self.imp().notebook;
        notebook
            .nth_page(Some(n))
            .and_downcast::<FileList>()
            .unwrap()
    }

    pub fn new_tab(&self, dir: &Directory, options: TabOptions) -> FileList {
        let fl = FileList::new();
        fl.set_sorting(options.sort_column, options.sort_direction);
        fl.set_column_options(&options.column_options);

        self.set_tab_locked(&fl, options.locked);
        fl.show_column(ColumnID::Dir, false);

        if let Some((history_entries, (current_connection, current_uri))) = options.history {
            let connection_list = ConnectionList::get();
            let mut selected_entry = None;
            for (connection_alias, stored_uri) in history_entries.into_iter().rev() {
                let Ok(uri) = glib::Uri::parse(&stored_uri, glib::UriFlags::NONE) else {
                    continue;
                };

                let connection: Connection =
                    if connection_alias.is_empty() && uri.scheme() == "file" {
                        connection_list.home().upcast()
                    } else {
                        if !connection_alias.is_empty() {
                            connection_list.find_by_alias(&connection_alias)
                        } else {
                            None
                        }
                        .unwrap_or_else(|| ConnectionRemote::new("", &uri).upcast())
                    };

                let path = Directory::new(&connection, &stored_uri).path_from_root();
                if connection_alias == current_connection && stored_uri == current_uri {
                    selected_entry = Some((connection.clone(), path.clone()));
                }
                fl.dir_history().add((connection, path));
            }

            if let Some(selected_entry) = selected_entry {
                fl.dir_history().set_current(selected_entry);
            }
        }

        let n = self
            .imp()
            .notebook
            .append_page(&fl, Some(&TabLabel::default()));
        self.update_show_tabs();
        self.imp().notebook.set_tab_reorderable(&fl, true);
        fl.update_style(self.root().and_downcast::<MainWindow>().as_ref());

        fl.connect_con_changed(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, con| {
                this.imp().select_connection(con);
            }
        ));
        fl.connect_dir_changed(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |fl, dir| this.on_list_dir_changed(fl, dir)
        ));
        fl.connect_files_changed(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |fl| {
                if this.current_file_list().as_ref() == Some(fl) {
                    this.imp().update_selected_files_label();
                }
            }
        ));
        fl.connect_file_activated(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |fl, file| {
                this.do_file_specific_action(fl, file);
            }
        ));
        fl.connect_cmdline_append(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, text| {
                if let Some(command_line) = this.command_line().filter(|cl| cl.is_visible()) {
                    command_line.append_text(text);
                    command_line.grab_focus();
                }
            }
        ));
        fl.connect_cmdline_execute(glib::clone!(
            #[weak(rename_to = this)]
            self,
            #[upgrade_or]
            false,
            move |_| {
                if let Some(command_line) = this
                    .command_line()
                    .filter(|cl| !cl.is_empty() && cl.terminal_available())
                {
                    command_line.process_command(ExecutionTarget::EmbeddedTerminal);
                    true
                } else {
                    false
                }
            }
        ));
        fl.connect_list_clicked(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |fl, button, file| this.on_list_list_clicked(fl, button, file)
        ));
        fl.connect_show_quick_search(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |fl, widget| {
                if this.current_file_list().as_ref() == Some(fl) {
                    this.imp().set_quick_search(Some(widget));
                }
            }
        ));

        fl.set_directory(dir);
        self.imp().update_tab_label(&fl);

        if let Some(selected_file) = options.selected_file {
            fl.focus_file(&selected_file, true);
        }

        if options.activate {
            let focused = self.root().and_then(|root| root.focus());
            self.set_list(Some(&fl));
            self.imp().notebook.set_current_page(Some(n));
            if options.grab_focus {
                fl.grab_focus();
            } else if let Some(focused) = focused {
                // Changing notebook page grabs focus, restore it
                focused.grab_focus();
            }
        }

        fl
    }

    fn on_list_dir_changed(&self, fl: &FileList, dir: &Directory) {
        self.imp().update_tab_label(fl);

        if self.current_file_list().as_ref() != Some(fl) {
            return;
        }

        self.imp().directory_indicator.set_directory(Some(dir));
        self.update_volume_label();

        if &fl.directory() != dir {
            return;
        }

        self.imp().update_selected_files_label();

        self.emit_by_name::<()>("dir-changed", &[dir]);
    }

    fn on_list_list_clicked(&self, fl: &FileList, button: u32, file: Option<File>) {
        match button {
            1 | 3 => self.emit_by_name::<()>("list-clicked", &[]),

            2 => {
                if fl.middle_mouse_button_mode() == MiddleMouseButtonMode::GoesUpDir {
                    self.goto_parent(fl);
                } else if let Some(file) = file
                    && file.is_directory()
                {
                    if file.is_dotdot() {
                        if let Some(parent) = fl.directory().parent() {
                            self.new_tab(&parent, TabOptions::from(fl));
                        }
                    } else {
                        self.new_tab(
                            &fl.directory().child(file.path_name()),
                            TabOptions::from(fl),
                        );
                    }
                }
            }
            6 | 8 => self.back(),
            7 | 9 => self.forward(),
            _ => {}
        }
    }

    pub fn close_tab(&self) {
        let notebook = &self.imp().notebook;
        if let Some(n) = notebook.current_page() {
            self.close_tab_nth(n);
        }
    }

    pub fn close_tab_nth(&self, n: u32) {
        let notebook = &self.imp().notebook;
        let count = notebook.n_pages();
        if count > 1 && n < count {
            notebook.remove_page(Some(n));
            self.update_show_tabs();
        }
    }

    fn update_show_tabs(&self) {
        let notebook = &self.imp().notebook;
        notebook.set_show_tabs(self.always_show_tabs() || notebook.n_pages() > 1);
    }

    pub fn tab_count(&self) -> u32 {
        self.imp().notebook.n_pages()
    }

    pub fn is_tab_locked(&self, fl: &FileList) -> bool {
        self.imp().locked_tabs.contains(fl)
    }

    pub fn set_tab_locked(&self, fl: &FileList, lock: bool) {
        if lock {
            self.imp().locked_tabs.insert(fl);
        } else {
            self.imp().locked_tabs.remove(fl);
        }
    }

    pub fn is_current_tab_locked(&self) -> bool {
        self.is_tab_locked(&self.file_list())
    }

    pub fn toggle_tab_lock(&self, file_list: &FileList) {
        let locked = self.is_tab_locked(file_list);
        self.set_tab_locked(file_list, !locked);
        self.imp().update_tab_label(file_list);
    }

    pub fn update_show_devlist(&self, visible: bool) {
        self.imp().connection_dropdown.set_visible(visible);

        self.layout_manager()
            .map(|lm| lm.layout_child(&self.imp().volume_size_label))
            .and_downcast::<gtk::GridLayoutChild>()
            .inspect(|lc| lc.set_row(if visible { 1 } else { 5 }));
    }

    pub fn update_connections(&self) {
        if self.is_realized()
            && let Some(list) = self.current_file_list()
        {
            self.imp().select_connection(&list.connection());
        }
    }

    pub fn activate_connection_list(&self) {
        let connection_dropdown = &self.imp().connection_dropdown;
        if connection_dropdown.is_visible() {
            connection_dropdown.emit_by_name::<()>("activate", &[]);
            connection_dropdown.grab_focus();
        }
    }

    pub fn goto(&self, file_list: &FileList, directory: &Directory) {
        if self.is_tab_locked(file_list) {
            self.new_tab(directory, TabOptions::from(file_list));
        } else {
            file_list.set_directory(directory);
        }
    }

    pub fn goto_parent(&self, file_list: &FileList) {
        let directory = file_list.directory();
        if let Some(parent) = directory.parent() {
            self.goto(file_list, &parent);
            file_list.focus_file(&directory.path_name(), false);
        }
    }

    pub fn goto_child(&self, file_list: &FileList, child_name: &Path) {
        self.goto(file_list, &file_list.directory().child(child_name));
    }

    pub fn directory_for_path(connection: &Connection, path: &Path) -> Directory {
        Directory::new(connection, &connection.create_uri(path))
    }

    pub fn goto_path(&self, file_list: &FileList, path: &Path) {
        self.goto(
            file_list,
            &Self::directory_for_path(&file_list.connection(), path),
        );
    }

    pub fn go_to_file(&self, file: &File, connection: Connection) {
        let Some(parent_file) = file.file().parent() else {
            eprintln!(
                "Cannot go to a file {}. It has no parent directory.",
                file.name()
            );
            return;
        };
        let file_list = self.file_list();
        self.goto(
            &file_list,
            &Directory::new_from_file(&connection, &parent_file),
        );
        file_list.focus_file(&file.path_name(), true);
    }

    pub fn close_all_tabs(&self) {
        let current = self.imp().notebook.current_page();
        for index in (0..self.tab_count())
            .rev()
            .filter(|i| Some(*i) != current)
            .filter(|i| !self.is_tab_locked(&self.file_list_nth(*i)))
        {
            self.close_tab_nth(index);
        }
    }

    pub fn close_duplicate_tabs(&self) {
        let mut dirs = HashMap::<Directory, BTreeSet<u32>>::new();
        for index in 0..self.tab_count() {
            let file_list = self.file_list_nth(index);
            if !self.is_tab_locked(&file_list) {
                dirs.entry(file_list.directory()).or_default().insert(index);
            }
        }

        let mut duplicates = BTreeSet::new();
        if let Some(mut indexes) = dirs.remove(&self.file_list().directory()) {
            if let Some(current_index) = self.imp().notebook.current_page() {
                indexes.remove(&current_index);
            }
            duplicates.extend(indexes);
        }

        for indexes in dirs.values() {
            duplicates.extend(indexes.iter().skip(1));
        }

        for index in duplicates.into_iter().rev() {
            self.close_tab_nth(index);
        }
    }

    pub fn prev_tab(&self) {
        let notebook = &self.imp().notebook;
        if notebook.current_page().is_some_and(|p| p > 0) {
            notebook.prev_page();
        } else if notebook.n_pages() > 1 {
            notebook.set_current_page(None);
        }
    }

    pub fn next_tab(&self) {
        let notebook = &self.imp().notebook;
        let n = notebook.n_pages();
        if notebook.current_page().is_some_and(|p| p + 1 < n) {
            notebook.next_page();
        } else if n > 1 {
            notebook.set_current_page(Some(0));
        }
    }

    fn on_navigate(&self, path: &str, new_tab: bool) {
        let file_list = self.file_list();
        if new_tab {
            self.new_tab(
                &Self::directory_for_path(&file_list.connection(), Path::new(path)),
                TabOptions::from(&file_list),
            );
        } else {
            self.goto_path(&file_list, Path::new(path));
        }
        self.emit_by_name::<()>("activate-request", &[]);
    }

    pub fn first(&self) {
        let file_list = self.file_list();
        let history = file_list.dir_history();
        if let Some((connection, dir)) = history.first() {
            self.goto(&file_list, &Self::directory_for_path(&connection, &dir));
        }
    }

    pub fn back(&self) {
        let file_list = self.file_list();
        let history = file_list.dir_history();
        if let Some((connection, dir)) = history.back() {
            self.goto(&file_list, &Self::directory_for_path(&connection, &dir));
        }
    }

    pub fn forward(&self) {
        let file_list = self.file_list();
        let history = file_list.dir_history();
        if let Some((connection, dir)) = history.forward() {
            self.goto(&file_list, &Self::directory_for_path(&connection, &dir));
        }
    }

    pub fn last(&self) {
        let file_list = self.file_list();
        let history = file_list.dir_history();
        if let Some((connection, dir)) = history.last() {
            self.goto(&file_list, &Self::directory_for_path(&connection, &dir));
        }
    }

    pub fn can_back(&self) -> bool {
        self.file_list().dir_history().can_back()
    }

    pub fn can_forward(&self) -> bool {
        self.file_list().dir_history().can_forward()
    }

    pub fn save_tabs(
        &self,
        position: TabPosition,
        save_all_tabs: bool,
        save_current: bool,
        save_history: bool,
    ) -> Vec<TabVariant> {
        fn get_alias(connection: &Connection) -> String {
            if connection.downcast_ref::<ConnectionHome>().is_some() {
                String::new()
            } else {
                connection.alias().unwrap_or_default()
            }
        }

        self.imp()
            .notebook
            .pages()
            .iter::<gtk::NotebookPage>()
            .flatten()
            .map(|p| p.child())
            .filter_map(|c| c.downcast::<FileList>().ok())
            .filter(|file_list| {
                save_all_tabs
                    || (save_current && *file_list == self.file_list())
                    || self.is_tab_locked(file_list)
            })
            .map(|file_list| {
                let directory = file_list.directory();
                let connection = get_alias(&directory.connection());
                let uri = directory.uri();
                let (column, order) = file_list.sorting();

                let mut variant = TabVariant::new(connection, uri);
                variant.set_position(position);
                variant.set_sort_column(column);
                variant.set_sort_direction(order);
                variant.set_locked(self.is_tab_locked(&file_list));
                variant.set_state(if file_list != self.file_list() {
                    TabState::Background
                } else if self.active() {
                    TabState::Active
                } else {
                    TabState::Selected
                });
                if let Some(file) = file_list.selected_file() {
                    variant.set_selected_file(&file.path_name());
                }
                variant.set_column_options(file_list.column_options());
                if save_history {
                    variant.set_history(
                        file_list
                            .dir_history()
                            .export()
                            .into_iter()
                            .map(|(connection, path)| {
                                (get_alias(&connection), connection.create_uri(&path))
                            })
                            .collect(),
                    );
                }
                variant
            })
            .collect()
    }

    pub fn open_tabs(&self, tabs: Vec<TabVariant>) {
        let connection_list = ConnectionList::get();

        for tab in tabs {
            let directory = restore_directory(connection_list, tab.current_location());
            self.new_tab(&directory, tab.tab_options());
        }

        if self.tab_count() == 0 {
            // Fallback to home directory
            let con = connection_list.home();
            let path = glib::home_dir();
            let directory = Directory::new(&con, &con.create_uri(&path));
            self.new_tab(&directory, TabOptions::new().grab_focus(false));
        }
    }

    pub fn connect_list_clicked<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn() + 'static,
    {
        self.connect_local("list-clicked", false, move |_| {
            (callback)();
            None
        })
    }

    pub fn connect_directory_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &Directory) + 'static,
    {
        self.connect_closure(
            "dir-changed",
            false,
            glib::closure_local!(move |this: &Self, d: Directory| { (callback)(this, &d) }),
        )
    }

    pub fn connect_activate_request<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self) + 'static,
    {
        self.connect_closure(
            "activate-request",
            false,
            glib::closure_local!(move |this: &Self| { (callback)(this) }),
        )
    }

    pub fn connection_bar(&self) -> gtk::Widget {
        self.imp().connection_bar.clone().upcast()
    }

    pub fn show_bookmarks(&self) {
        let menu = gio::Menu::new();

        let bookmarks_section = gio::Menu::new();
        for bookmark in &*self.file_list().connection().bookmarks() {
            let item = gio::MenuItem::new(Some(bookmark.name()), None);
            item.set_action_and_target_value(
                Some("fs.select-path"),
                Some(&bookmark.path().to_variant()),
            );
            bookmarks_section.append_item(&item);
        }
        menu.append_section(None, &bookmarks_section);

        let manage_section = gio::Menu::new();
        manage_section.append(Some(&gettext("Add current dir")), Some("fs.add-bookmark"));
        manage_section.append(
            Some(&gettext("Manage bookmarks…")),
            Some(UserAction::BookmarksEdit.name()),
        );
        menu.append_section(None, &manage_section);

        let popover = gtk::PopoverMenu::builder()
            .menu_model(&menu)
            .position(gtk::PositionType::Bottom)
            .build();
        popover.set_parent(&self.imp().directory_indicator_box);
        popover.connect_closed(glib::clone!(
            #[weak(rename_to = button)]
            self.imp().bookmark_button,
            move |this| {
                let this = this.clone();
                glib::spawn_future_local(async move {
                    this.unparent();
                });
                button.set_active(false);
            }
        ));
        popover.set_pointing_to(
            self.imp()
                .bookmark_button
                .compute_bounds(&self.imp().directory_indicator_box)
                .map(|rect| {
                    gdk::Rectangle::new(
                        rect.x() as i32,
                        rect.y() as i32,
                        rect.width() as i32,
                        rect.height() as i32,
                    )
                })
                .as_ref(),
        );
        popover.popup();
        self.imp().bookmark_button.set_active(true);
    }

    pub fn show_history(&self) {
        let dir_history = self.file_list().dir_history().export();
        if dir_history.is_empty() {
            return;
        }

        let menu = gio::Menu::new();
        for (index, (connection, path)) in dir_history.into_iter().enumerate() {
            let alias = if connection.is::<ConnectionHome>() {
                String::new()
            } else {
                connection.display_name()
            };
            let path = if alias.is_empty() {
                path.to_string_lossy()
            } else {
                std::borrow::Cow::Owned(format!("{alias}: {}", path.display()))
            };

            let item = gio::MenuItem::new(Some(&path), None);
            item.set_action_and_target_value(
                Some("fs.select-history-entry"),
                Some(&(index as u32).to_variant()),
            );
            menu.append_item(&item);
        }

        let popover = gtk::PopoverMenu::builder()
            .menu_model(&menu)
            .position(gtk::PositionType::Bottom)
            .build();
        popover.set_parent(&self.imp().directory_indicator_box);
        popover.connect_closed(glib::clone!(
            #[weak(rename_to = button)]
            self.imp().history_button,
            move |this| {
                let this = this.clone();
                glib::spawn_future_local(async move {
                    this.unparent();
                });
                button.set_active(false);
            }
        ));
        popover.set_pointing_to(
            self.imp()
                .history_button
                .compute_bounds(&self.imp().directory_indicator_box)
                .map(|rect| {
                    gdk::Rectangle::new(
                        rect.x() as i32,
                        rect.y() as i32,
                        rect.width() as i32,
                        rect.height() as i32,
                    )
                })
                .as_ref(),
        );
        popover.popup();
        self.imp().history_button.set_active(true);
    }

    pub fn update_style(&self) {
        self.update_show_tabs();
        for i in 0..self.tab_count() {
            let fl = self.file_list_nth(i);
            fl.update_style(self.root().and_downcast::<MainWindow>().as_ref());
            self.imp().update_tab_label(&fl);
        }
        self.update_connections();
    }

    pub fn update_volume_label(&self) {
        let Some(file_list) = self.current_file_list() else {
            return;
        };
        if !file_list.connection().can_show_free_space() || file_list.directory().is_virtual() {
            self.imp().volume_size_label.set_visible(false);
            return;
        }
        self.imp().volume_size_label.set_visible(true);
        match file_list.directory().free_space() {
            Ok(free_space) => {
                self.imp()
                    .volume_size_label
                    .set_label(&gettext("%s free").replace("%s", &glib::format_size(free_space)));
            }
            Err(error) => {
                eprintln!("Failed to get free space: {error}");
                self.imp()
                    .volume_size_label
                    .set_label(&gettext("Unknown disk usage"));
            }
        }
    }

    pub fn do_file_specific_action(&self, fl: &FileList, file: &File) {
        match file.file_type() {
            gio::FileType::Directory => {
                if file.is_dotdot() {
                    self.goto_parent(fl);
                } else {
                    self.goto_child(fl, &file.path_name());
                }
            }
            gio::FileType::Regular => {
                if let Some(parent_window) = self.root().and_downcast::<gtk::Window>() {
                    let file = file.clone();
                    glib::spawn_future_local(async move {
                        if let Err(error) = mime_exec_single(&parent_window, &file).await {
                            error.show(&parent_window).await;
                        }
                    });
                }
            }
            _ => {}
        }
    }

    pub fn add_shortcuts(&self, shortcuts: &Shortcuts) {
        shortcuts.add_controller(&self.imp().notebook, Area::Panel);
    }
}

u32_enum! {
    pub enum TabPosition {
        #[default]
        LeftOrTop,
        RightOrBottom,
    }
}

u32_enum! {
    pub enum TabState {
        #[default]
        Background,
        Selected,
        Active,
    }
}

pub struct TabOptions {
    pub sort_column: ColumnID,
    pub sort_direction: gtk::SortType,
    pub locked: bool,
    pub history: Option<(Vec<StoredHistoryEntry>, StoredHistoryEntry)>,
    pub activate: bool,
    pub grab_focus: bool,
    pub selected_file: Option<PathBuf>,
    pub column_options: Vec<ColumnOptions>,
}

impl TabOptions {
    pub fn new() -> Self {
        Self {
            sort_column: ColumnID::Name,
            sort_direction: gtk::SortType::Ascending,
            locked: false,
            history: None,
            activate: true,
            grab_focus: true,
            selected_file: None,
            column_options: Vec::new(),
        }
    }

    pub fn from(file_list: &FileList) -> Self {
        let (sort_column, sort_direction) = file_list.sorting();
        Self::new()
            .sort_column(sort_column)
            .sort_direction(sort_direction)
            .column_options(file_list.column_options())
    }

    pub fn sort_column(mut self, sort_column: ColumnID) -> Self {
        self.sort_column = sort_column;
        self
    }

    pub fn sort_direction(mut self, sort_direction: gtk::SortType) -> Self {
        self.sort_direction = sort_direction;
        self
    }

    pub fn locked(mut self, locked: bool) -> Self {
        self.locked = locked;
        self
    }

    pub fn history(
        mut self,
        entries: Vec<StoredHistoryEntry>,
        current: StoredHistoryEntry,
    ) -> Self {
        self.history = Some((entries, current));
        self
    }

    pub fn activate(mut self, activate: bool) -> Self {
        self.activate = activate;
        self
    }

    pub fn grab_focus(mut self, grab_focus: bool) -> Self {
        self.grab_focus = grab_focus;
        self
    }

    pub fn selected_file(mut self, selected_file: PathBuf) -> Self {
        self.selected_file = Some(selected_file);
        self
    }

    pub fn column_options(mut self, column_options: Vec<ColumnOptions>) -> Self {
        self.column_options = column_options;
        self
    }
}

pub struct TabVariant(BTreeMap<String, glib::Variant>);

impl ToVariant for TabVariant {
    fn to_variant(&self) -> glib::Variant {
        self.0.to_variant()
    }
}

impl FromVariant for TabVariant {
    fn from_variant(variant: &glib::Variant) -> Option<Self> {
        BTreeMap::from_variant(variant).map(Self)
    }
}

impl StaticVariantType for TabVariant {
    fn static_variant_type() -> std::borrow::Cow<'static, glib::VariantTy> {
        BTreeMap::<String, glib::Variant>::static_variant_type()
    }
}

impl TabVariant {
    const SETTING_CURRENT_LOCATION: &str = "current-location";
    const SETTING_POSITION: &str = "position";
    const SETTING_SORT_COLUMN: &str = "sort-column";
    const SETTING_SORT_DIRECTION: &str = "sort-direction";
    const SETTING_LOCKED: &str = "locked";
    const SETTING_HISTORY: &str = "history";
    const SETTING_STATE: &str = "state";
    const SETTING_SELECTED_FILE: &str = "selected-file";
    const SETTING_COLUMN_OPTIONS: &str = "column-options";

    const SORT_DIRECTION_ASCENDING: &str = "asc";
    const SORT_DIRECTION_DESCENDING: &str = "desc";

    pub fn new(connection: String, uri: String) -> Self {
        let mut inner = BTreeMap::new();
        inner.insert(
            Self::SETTING_CURRENT_LOCATION.to_string(),
            (connection, uri).into(),
        );
        Self(inner)
    }

    pub fn tab_options(&self) -> TabOptions {
        let options = TabOptions::new()
            .sort_column(self.sort_column())
            .sort_direction(self.sort_direction())
            .locked(self.locked())
            .history(self.history(), self.current_location())
            .activate(self.state() != TabState::Background)
            .grab_focus(self.state() == TabState::Active)
            .column_options(self.column_options());
        if let Some(selected_file) = self.selected_file() {
            options.selected_file(selected_file)
        } else {
            options
        }
    }

    pub fn current_location(&self) -> StoredHistoryEntry {
        self.0
            .get(Self::SETTING_CURRENT_LOCATION)
            .and_then(StoredHistoryEntry::from_variant)
            .unwrap_or_default()
    }

    pub fn position(&self) -> TabPosition {
        self.0
            .get(Self::SETTING_POSITION)
            .and_then(TabPosition::from_variant)
            .unwrap_or_default()
    }

    pub fn set_position(&mut self, position: TabPosition) {
        self.0
            .insert(Self::SETTING_POSITION.to_string(), position.into());
    }

    pub fn sort_column(&self) -> ColumnID {
        self.0
            .get(Self::SETTING_SORT_COLUMN)
            .and_then(String::from_variant)
            .as_deref()
            .and_then(ColumnID::from_name)
            .unwrap_or(ColumnID::Name)
    }

    pub fn set_sort_column(&mut self, column: ColumnID) {
        self.0
            .insert(Self::SETTING_SORT_COLUMN.to_string(), column.name().into());
    }

    pub fn sort_direction(&self) -> gtk::SortType {
        self.0
            .get(Self::SETTING_SORT_DIRECTION)
            .and_then(String::from_variant)
            .map(|dir| {
                if dir == Self::SORT_DIRECTION_DESCENDING {
                    gtk::SortType::Descending
                } else {
                    gtk::SortType::Ascending
                }
            })
            .unwrap_or(gtk::SortType::Ascending)
    }

    pub fn set_sort_direction(&mut self, direction: gtk::SortType) {
        self.0.insert(
            Self::SETTING_SORT_DIRECTION.to_string(),
            if direction == gtk::SortType::Descending {
                Self::SORT_DIRECTION_DESCENDING
            } else {
                Self::SORT_DIRECTION_ASCENDING
            }
            .into(),
        );
    }

    pub fn locked(&self) -> bool {
        self.0
            .get(Self::SETTING_LOCKED)
            .and_then(bool::from_variant)
            .unwrap_or_default()
    }

    pub fn set_locked(&mut self, locked: bool) {
        self.0
            .insert(Self::SETTING_LOCKED.to_string(), locked.into());
    }

    pub fn history(&self) -> Vec<StoredHistoryEntry> {
        self.0
            .get(Self::SETTING_HISTORY)
            .and_then(Vec::from_variant)
            .unwrap_or_default()
    }

    pub fn set_history(&mut self, history: Vec<StoredHistoryEntry>) {
        self.0
            .insert(Self::SETTING_HISTORY.to_string(), history.into());
    }

    pub fn state(&self) -> TabState {
        self.0
            .get(Self::SETTING_STATE)
            .and_then(TabState::from_variant)
            .unwrap_or_default()
    }

    pub fn set_state(&mut self, state: TabState) {
        self.0.insert(Self::SETTING_STATE.to_string(), state.into());
    }

    pub fn selected_file(&self) -> Option<PathBuf> {
        self.0
            .get(Self::SETTING_SELECTED_FILE)
            .and_then(String::from_variant)
            .map(|s| s.into())
    }

    pub fn set_selected_file(&mut self, path: &Path) {
        if let Some(path) = path.as_os_str().to_str() {
            self.0
                .insert(Self::SETTING_SELECTED_FILE.to_string(), path.into());
        }
    }

    pub fn column_options(&self) -> Vec<ColumnOptions> {
        self.0
            .get(Self::SETTING_COLUMN_OPTIONS)
            .and_then(Vec::<ColumnOptions>::from_variant)
            .unwrap_or_default()
    }

    pub fn set_column_options(&mut self, options: Vec<ColumnOptions>) {
        self.0.insert(
            Self::SETTING_COLUMN_OPTIONS.to_string(),
            options.to_variant(),
        );
    }
}

type StoredHistoryEntry = (String, String);

#[derive(PartialEq, Eq, Hash, glib::Variant)]
pub struct LegacyTabVariant {
    pub uri: String,
    pub file_felector_id: u8,
    pub sort_column: u8,
    pub sort_order: bool,
    pub locked: bool,
}

impl From<LegacyTabVariant> for TabVariant {
    fn from(legacy: LegacyTabVariant) -> Self {
        let mut result = Self::new(Default::default(), legacy.uri);
        result.set_position((legacy.file_felector_id as u32).into());
        result.set_sort_column(u32::from(legacy.sort_column).into());
        result.set_sort_direction(match legacy.sort_order {
            false => gtk::SortType::Ascending,
            true => gtk::SortType::Descending,
        });
        result.set_locked(legacy.locked);
        result
    }
}

fn restore_directory(connection_list: &ConnectionList, location: StoredHistoryEntry) -> Directory {
    let (connection_alias, stored_uri) = location;
    let con: Connection = match glib::Uri::parse(&stored_uri, glib::UriFlags::NONE) {
        Ok(uri) => {
            if uri.scheme() == "file" {
                connection_list.home().upcast()
            } else {
                if !connection_alias.is_empty() {
                    connection_list.find_by_alias(&connection_alias)
                } else {
                    None
                }
                .unwrap_or_else(|| ConnectionRemote::new("", &uri).upcast())
            }
        }
        Err(error) => {
            eprintln!("Stored URI is invalid: {}", error.message());
            connection_list.home().upcast()
        }
    };

    Directory::new(&con, &stored_uri)
}

async fn ask_close_locked_tab(parent: &gtk::Window) -> bool {
    gtk::AlertDialog::builder()
        .modal(true)
        .message(gettext("The tab is locked, close anyway?"))
        .buttons([gettext("_Cancel"), gettext("C_lose Tab")])
        .cancel_button(0)
        .default_button(1)
        .build()
        .choose_future(Some(parent))
        .await
        == Ok(1)
}

async fn on_notebook_button_pressed(
    file_selector: &FileSelector,
    n_press: i32,
    button: u32,
    x: f64,
    y: f64,
) {
    let Some(window) = file_selector.root().and_downcast::<gtk::Window>() else {
        eprintln!("No root window.");
        return;
    };

    let notebook = &file_selector.imp().notebook;
    let Some(tab_clicked) = notebook.find_tab_num_at_pos(&graphene::Point::new(x as f32, y as f32))
    else {
        return;
    };
    match (n_press, button, tab_clicked) {
        (1, 2, TabClick::Tab(index)) | (2, 1, TabClick::Tab(index)) => {
            // mid-click or double-click on a label
            let fl = file_selector.file_list_nth(index);
            if !file_selector.is_tab_locked(&fl) || ask_close_locked_tab(&window).await {
                file_selector.close_tab_nth(index);
            }
        }
        (1, 3, TabClick::Tab(index)) => {
            // right-click
            // notebook.set_current_page(Some(index));    // switch to the page the mouse is over
            let fl = file_selector.file_list_nth(index);

            let menu = gio::Menu::new();
            menu.append(
                Some(&gettext("Open in New _Tab")),
                Some(UserAction::ViewNewTab.name()),
            );

            let section = gio::Menu::new();

            {
                let menuitem = gio::MenuItem::new(
                    Some(&if file_selector.is_tab_locked(&fl) {
                        gettext("_Unlock Tab")
                    } else {
                        gettext("_Lock Tab")
                    }),
                    None,
                );
                menuitem.set_action_and_target_value(
                    Some("fs.toggle-tab-lock"),
                    Some(&(index as i32).to_variant()),
                );
                section.append_item(&menuitem);
            }

            {
                let menuitem = gio::MenuItem::new(Some(&gettext("_Refresh Tab")), None);
                menuitem.set_action_and_target_value(
                    Some("fs.refresh-tab"),
                    Some(&(index as i32).to_variant()),
                );
                section.append_item(&menuitem);
            }

            section.append(
                Some(&gettext("Copy Tab to Other _Panel")),
                Some(UserAction::ViewInInactiveTab.name()),
            );
            menu.append_section(None, &section);

            menu.append(
                Some(&gettext("_Close Tab")),
                Some(UserAction::ViewCloseTab.name()),
            );
            menu.append(
                Some(&gettext("Close _All Tabs")),
                Some(UserAction::ViewCloseAllTabs.name()),
            );
            menu.append(
                Some(&gettext("Close _Duplicate Tabs")),
                Some(UserAction::ViewCloseDuplicateTabs.name()),
            );

            let tab_popover = &file_selector.imp().tab_popover;
            tab_popover.set_menu_model(Some(&menu));
            tab_popover.set_pointing_to(Some(&gdk::Rectangle::new(x as i32, y as i32, 0, 0)));
            tab_popover.popup();
        }
        (2, 1, TabClick::Area) => {
            // double-click on a tabs area
            let file_list = file_selector.file_list();
            let mut options = TabOptions::from(&file_list);
            if let Some(selected_file) = file_list.selected_file() {
                options = options.selected_file(selected_file.path_name());
            }
            file_selector.new_tab(&file_list.directory(), options);
        }
        _ => {}
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_type() {
        assert_eq!(*TabVariant::static_variant_type(), "a{sv}");
        assert_eq!(*LegacyTabVariant::static_variant_type(), "(syybb)");
    }
}
