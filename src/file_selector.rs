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
        bookmark::Bookmark,
        connection::{Connection, ConnectionExt},
        list::ConnectionList,
        remote::ConnectionRemote,
    },
    dir::Directory,
    file::File,
    file_list::list::{ffi::GnomeCmdFileList, ColumnID, FileList},
    notebook_ext::{GnomeCmdNotebookExt, TabClick},
    tags::tags::FileMetadataService,
    utils::{ALT, CONTROL, CONTROL_SHIFT, NO_MOD},
};
use gettextrs::gettext;
use gtk::{
    ffi::{GtkDropDown, GtkNotebook, GtkWidget},
    gdk, gio,
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_borrow, from_glib_none, Borrowed, IntoGlib, ToGlibPtr},
    },
    graphene, pango,
    prelude::*,
    subclass::prelude::*,
};
use std::{
    collections::{BTreeSet, HashMap, HashSet},
    ffi::c_int,
    path::{Path, PathBuf},
};

pub mod ffi {
    use super::*;
    use crate::{
        dir::ffi::GnomeCmdDir, file::ffi::GnomeCmdFile, file_list::list::ffi::GnomeCmdFileList,
    };
    use gtk::{
        ffi::GtkWidget,
        glib::ffi::{gboolean, GType},
    };

    pub type GnomeCmdFileSelector = <super::FileSelector as glib::object::ObjectType>::GlibType;

    #[no_mangle]
    pub extern "C" fn gnome_cmd_file_selector_get_type() -> GType {
        super::FileSelector::static_type().into_glib()
    }

    extern "C" {
        pub fn gnome_cmd_file_selector_init(fs: *mut GnomeCmdFileSelector);

        pub fn gnome_cmd_file_selector_file_list(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GnomeCmdFileList;

        pub fn on_notebook_switch_page(fs: *mut GnomeCmdFileSelector, n: u32);

        pub fn gnome_cmd_file_selector_new_tab_full(
            fs: *mut GnomeCmdFileSelector,
            dir: *mut GnomeCmdDir,
            sort_col: c_int,
            sort_order: c_int,
            locked: gboolean,
            activate: gboolean,
            grab_focus: gboolean,
        ) -> *const GtkWidget;

        pub fn gnome_cmd_file_selector_is_tab_locked(
            fs: *mut GnomeCmdFileSelector,
            fl: *mut GnomeCmdFileList,
        ) -> gboolean;

        pub fn gnome_cmd_file_selector_set_tab_locked(
            fs: *mut GnomeCmdFileSelector,
            fl: *mut GnomeCmdFileList,
            lock: gboolean,
        );

        pub fn gnome_cmd_file_selector_update_connections(fs: *mut GnomeCmdFileSelector);

        pub fn gnome_cmd_file_selector_do_file_specific_action(
            fs: *mut GnomeCmdFileSelector,
            fl: *mut GnomeCmdFileList,
            f: *mut GnomeCmdFile,
        ) -> gboolean;
    }
}

mod imp {
    use super::*;
    use crate::{
        command_line::CommandLine,
        connection::{device::ConnectionDevice, smb::ConnectionSmb},
        connection_bar::ConnectionBar,
        data::{GeneralOptions, GeneralOptionsRead},
        dialogs::manage_bookmarks_dialog::bookmark_directory,
        directory_indicator::DirectoryIndicator,
        libgcmd::file_descriptor::FileDescriptorExt,
        tab_label::TabLabel,
        utils::get_modifiers_state,
    };
    use std::{
        cell::{Cell, OnceCell, RefCell},
        sync::OnceLock,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::FileSelector)]
    pub struct FileSelector {
        pub connection_list: ConnectionList,

        pub connection_bar: ConnectionBar,
        pub connection_dropdown: gtk::DropDown,
        pub volume_size_label: gtk::Label,
        pub directory_indicator_box: gtk::Box,
        pub directory_indicator: DirectoryIndicator,
        pub notebook: gtk::Notebook,
        pub info_label: gtk::Label,
        pub filter_box: gtk::Box,
        pub filter_entry: gtk::Entry,

        #[property(get, set, nullable)]
        pub command_line: RefCell<Option<CommandLine>>,

        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,

        #[property(get, set = Self::set_always_show_tabs)]
        always_show_tabs: Cell<bool>,

        #[property(get, set = Self::set_active)]
        active: Cell<bool>,
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
                    if let Some(path) = param.and_then(|p| String::from_variant(&p)) {
                        obj.imp().select_path(&path);
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
                        .and_then(|p| i32::from_variant(&p))
                        .and_then(|i| i.try_into().ok())
                    {
                        obj.imp().toggle_tab_lock(index);
                    }
                },
            );
            klass.install_action(
                "fs.refresh-tab",
                Some(&i32::static_variant_type()),
                |obj, _, param| {
                    if let Some(index) = param
                        .and_then(|p| i32::from_variant(&p))
                        .and_then(|i| i.try_into().ok())
                    {
                        obj.imp().refresh_tab(index);
                    }
                },
            );
        }

        fn new() -> Self {
            let connection_list = ConnectionList::get();

            let model = gtk::FilterListModel::new(
                Some(connection_list.all()),
                Some(gtk::CustomFilter::new(|item| {
                    item.downcast_ref::<Connection>().map_or(false, |con| {
                        con.is_open()
                            || con.downcast_ref::<ConnectionDevice>().is_some()
                            || con.downcast_ref::<ConnectionSmb>().is_some()
                    })
                })),
            );

            let (filter_box, filter_entry) = create_filter_box();

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

                filter_box,
                filter_entry,

                command_line: Default::default(),

                file_metadata_service: Default::default(),

                always_show_tabs: Default::default(),
                active: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileSelector {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_column_spacing(6);

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

            let bookmark_button = gtk::Button::builder()
                .icon_name("gnome-commander-bookmark-outline")
                .can_focus(false)
                .has_frame(false)
                .build();
            self.directory_indicator_box.append(&bookmark_button);

            let history_button = gtk::Button::builder()
                .icon_name("gnome-commander-down")
                .can_focus(false)
                .has_frame(false)
                .build();
            self.directory_indicator_box.append(&history_button);

            this.attach(&self.directory_indicator_box, 0, 2, 2, 1);

            bookmark_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| this.show_bookmarks()
            ));
            history_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| this.show_history()
            ));

            this.attach(&self.notebook, 0, 3, 2, 1);
            this.attach(&self.info_label, 0, 4, 1, 1);
            this.attach(&self.filter_box, 0, 5, 2, 1);

            unsafe {
                ffi::gnome_cmd_file_selector_init(self.obj().to_glib_none().0);
            }

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
                    unsafe {
                        ffi::on_notebook_switch_page(this.to_glib_none().0, n);
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

            let key_controller = gtk::EventControllerKey::builder()
                .propagation_phase(gtk::PropagationPhase::Capture)
                .build();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.key_pressed(key, state)
            ));
            this.add_controller(key_controller);

            let options = GeneralOptions::new();
            options
                .0
                .bind("always-show-tabs", &*this, "always-show-tabs")
                .build();
        }

        fn dispose(&self) {
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
            let Some(dir) = con.default_dir() else {
                eprintln!("Connection has no default directory");
                return;
            };

            if new_tab || self.obj().is_current_tab_locked() {
                self.obj().new_tab_with_dir(&dir, true, false);
            }
            self.obj().file_list().set_connection(con, Some(&dir));
            self.obj().emit_by_name::<()>("activate-request", &[]);
        }

        fn select_path(&self, path: &str) {
            let mask = self
                .obj()
                .root()
                .and_downcast::<gtk::Window>()
                .as_ref()
                .and_then(get_modifiers_state);
            let new_tab = mask.map_or(false, |m| m.contains(gdk::ModifierType::CONTROL_MASK));
            self.on_navigate(path, new_tab);
        }

        async fn add_bookmark(&self) {
            let Some(window) = self.obj().root().and_downcast::<gtk::Window>() else {
                return;
            };
            let Some(dir) = self.obj().file_list().directory() else {
                return;
            };
            let options = GeneralOptions::new();
            bookmark_directory(&window, &dir, &options).await;
        }

        fn toggle_tab_lock(&self, index: u32) {
            let fl = self.obj().file_list_nth(index);
            let locked = self.obj().is_tab_locked(&fl);
            self.obj().set_tab_locked(&fl, !locked);
            self.update_tab_label(&fl);
        }

        fn refresh_tab(&self, index: u32) {
            let list = self.obj().file_list_nth(index);
            list.reload();
        }

        fn on_navigate(&self, path: &str, new_tab: bool) {
            let fl = self.obj().file_list();
            if new_tab || self.obj().is_current_tab_locked() {
                let Some(con) = fl.connection() else {
                    eprintln!("Cannot navigate to {path}. No connection.");
                    return;
                };
                let dir = Directory::new(&con, con.create_path(&Path::new(path)));
                self.obj().new_tab_with_dir(&dir, true, true);
            } else {
                fl.goto_directory(&Path::new(path));
            }
            self.obj().emit_by_name::<()>("activate-request", &[]);
        }

        pub fn update_tab_label(&self, fl: &FileList) {
            let Some(tab_label) = self.notebook.tab_label(fl).and_downcast::<TabLabel>() else {
                return;
            };

            let options = GeneralOptions::new();

            tab_label.set_label(
                fl.directory()
                    .map(|d| d.upcast::<File>().get_name())
                    .unwrap_or_default(),
            );
            tab_label.set_locked(self.obj().is_tab_locked(fl));
            tab_label.set_indicator(options.tab_lock_indicator());
        }

        pub fn update_selected_files_label(&self) {
            let options = GeneralOptions::new();
            if let Some(file_list) = self.obj().current_file_list() {
                let stats = file_list.stats_str(options.size_display_mode());
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
        }

        fn key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match (key, state) {
                (gdk::Key::Tab | gdk::Key::ISO_Left_Tab, CONTROL) => {
                    self.obj().next_tab();
                    glib::Propagation::Stop
                }
                (gdk::Key::Tab | gdk::Key::ISO_Left_Tab, CONTROL_SHIFT) => {
                    self.obj().prev_tab();
                    glib::Propagation::Stop
                }
                (gdk::Key::Left | gdk::Key::KP_Left, ALT) => {
                    self.obj().back();
                    glib::Propagation::Stop
                }
                (gdk::Key::Right | gdk::Key::KP_Right, ALT) => {
                    self.obj().forward();
                    glib::Propagation::Stop
                }
                (gdk::Key::Left | gdk::Key::KP_Left | gdk::Key::BackSpace, NO_MOD) => {
                    let fl = self.obj().file_list();
                    if self.obj().is_tab_locked(&fl) {
                        if let Some(parent) = fl.directory().and_then(|d| d.parent()) {
                            self.obj().new_tab_with_dir(&parent, true, true);
                        }
                    } else {
                        fl.invalidate_tree_size();
                        fl.goto_directory(&Path::new(".."));
                    }
                    glib::Propagation::Stop
                }
                (gdk::Key::Right | gdk::Key::KP_Right, NO_MOD) => {
                    let fl = self.obj().file_list();
                    if let Some(directory) = fl
                        .selected_file()
                        .filter(|f| f.file_info().file_type() == gio::FileType::Directory)
                    {
                        self.obj().do_file_specific_action(&fl, &directory);
                    }
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
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
                    (connection.go_icon(), connection.alias())
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
        @extends gtk::Grid, gtk::Widget;
}

impl FileSelector {
    pub fn new(file_metadata_service: &FileMetadataService) -> Self {
        glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build()
    }

    fn current_file_list(&self) -> Option<FileList> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_file_list(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn file_list(&self) -> FileList {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_file_list(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn file_list_nth(&self, n: u32) -> FileList {
        let notebook = &self.imp().notebook;
        notebook
            .nth_page(Some(n))
            .and_downcast::<FileList>()
            .unwrap()
    }

    pub fn new_tab(&self) -> gtk::Widget {
        self.new_tab_full(
            None,
            ColumnID::COLUMN_NAME,
            gtk::SortType::Ascending,
            false,
            true,
            true,
        )
    }

    pub fn new_tab_with_dir(
        &self,
        dir: &Directory,
        activate: bool,
        grab_focus: bool,
    ) -> gtk::Widget {
        self.new_tab_full(
            Some(dir),
            self.file_list().sort_column(),
            self.file_list().sort_order(),
            false,
            activate,
            grab_focus,
        )
    }

    pub fn new_tab_full(
        &self,
        dir: Option<&Directory>,
        sort_column: ColumnID,
        sort_order: gtk::SortType,
        locked: bool,
        activate: bool,
        grab_focus: bool,
    ) -> gtk::Widget {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_new_tab_full(
                self.to_glib_none().0,
                dir.to_glib_none().0,
                sort_column as c_int,
                sort_order.into_glib(),
                locked as gboolean,
                activate as gboolean,
                grab_focus as gboolean,
            ))
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
        unsafe {
            ffi::gnome_cmd_file_selector_is_tab_locked(self.to_glib_none().0, fl.to_glib_none().0)
                != 0
        }
    }

    pub fn set_tab_locked(&self, fl: &FileList, lock: bool) {
        unsafe {
            ffi::gnome_cmd_file_selector_set_tab_locked(
                self.to_glib_none().0,
                fl.to_glib_none().0,
                lock as gboolean,
            )
        }
    }

    pub fn is_current_tab_locked(&self) -> bool {
        self.is_tab_locked(&self.file_list())
    }

    pub fn toggle_tab_lock(&self, file_list: &FileList) {
        let locked = self.is_tab_locked(&file_list);
        self.set_tab_locked(&file_list, !locked);
        self.imp().update_tab_label(file_list);
    }

    pub fn update_show_devlist(&self, visible: bool) {
        self.imp().connection_dropdown.set_visible(visible);

        self.layout_manager()
            .map(|lm| lm.layout_child(&self.imp().volume_size_label))
            .and_downcast::<gtk::GridLayoutChild>()
            .map(|lc| lc.set_row(if visible { 1 } else { 4 }));
    }

    pub fn update_connections(&self) {
        unsafe { ffi::gnome_cmd_file_selector_update_connections(self.to_glib_none().0) }
    }

    pub fn activate_connection_list(&self) {
        let connection_dropdown = &self.imp().connection_dropdown;
        if connection_dropdown.is_visible() {
            connection_dropdown.emit_by_name::<()>("activate", &[]);
            connection_dropdown.grab_focus();
        }
    }

    pub fn goto_directory(&self, con: &Connection, path: &Path) {
        if self.file_list().connection().as_ref() == Some(&con) {
            if self.is_current_tab_locked() {
                let dir = Directory::new(con, con.create_path(path));
                self.new_tab_with_dir(&dir, true, true);
            } else {
                self.file_list().goto_directory(path);
            }
        } else {
            if con.is_open() {
                let dir = Directory::new(con, con.create_path(path));

                if self.is_current_tab_locked() {
                    self.new_tab_with_dir(&dir, true, true);
                } else {
                    self.file_list().set_connection(con, Some(&dir));
                }
            } else {
                con.set_base_path(con.create_path(path));
                if self.is_current_tab_locked() {
                    self.new_tab_with_dir(&con.default_dir().unwrap(), true, true);
                } else {
                    self.file_list().set_connection(con, None);
                }
            }
        }
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
                if let Some(directory) = file_list.directory() {
                    dirs.entry(directory).or_default().insert(index);
                }
            }
        }

        let mut duplicates = BTreeSet::new();
        if let Some(mut indexes) = self.file_list().directory().and_then(|d| dirs.remove(&d)) {
            if let Some(current_index) = self.imp().notebook.current_page() {
                indexes.remove(&current_index);
            }
            duplicates.extend(indexes);
        }

        for indexes in dirs.values() {
            duplicates.extend(indexes.into_iter().skip(1));
        }

        for index in duplicates.into_iter().rev() {
            self.close_tab_nth(index);
        }
    }

    pub fn prev_tab(&self) {
        let notebook = &self.imp().notebook;
        if notebook.current_page().map_or(false, |p| p > 0) {
            notebook.prev_page();
        } else if notebook.n_pages() > 1 {
            notebook.set_current_page(None);
        }
    }

    pub fn next_tab(&self) {
        let notebook = &self.imp().notebook;
        let n = notebook.n_pages();
        if notebook.current_page().map_or(false, |p| p + 1 < n) {
            notebook.next_page();
        } else if n > 1 {
            notebook.set_current_page(Some(0));
        }
    }

    fn on_navigate(&self, path: &str, new_tab: bool) {
        if new_tab || self.is_current_tab_locked() {
            if let Some(connection) = self.current_file_list().and_then(|fl| fl.connection()) {
                let dir = Directory::new(&connection, connection.create_path(&Path::new(path)));
                self.new_tab_with_dir(&dir, true, true);
            }
        } else {
            self.file_list().goto_directory(&Path::new(path));
        }
        self.emit_by_name::<()>("activate-request", &[]);
    }

    fn goto(&self, connection: &Connection, dir: &str) {
        if self.is_current_tab_locked() {
            self.new_tab_with_dir(
                &Directory::new(connection, connection.create_path(&Path::new(&dir))),
                true,
                true,
            );
        } else {
            self.goto_directory(connection, &Path::new(&dir));
        }
    }

    pub fn first(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.first() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn back(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.back() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn forward(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.forward() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn last(&self) {
        let Some(connection) = self.file_list().connection() else {
            return;
        };
        let history = connection.dir_history();
        if let Some(dir) = history.last() {
            history.lock();
            self.goto(&connection, &dir);
            history.unlock();
        }
    }

    pub fn can_back(&self) -> bool {
        let Some(connection) = self.file_list().connection() else {
            return false;
        };
        connection.dir_history().can_back()
    }

    pub fn can_forward(&self) -> bool {
        let Some(connection) = self.file_list().connection() else {
            return false;
        };
        connection.dir_history().can_forward()
    }

    pub fn save_tabs(&self, save_all_tabs: bool, save_current: bool) -> Vec<TabVariant> {
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
            .filter_map(|file_list| {
                let directory = file_list.directory()?;
                let uri = directory.upcast_ref::<File>().get_uri_str()?;
                Some(TabVariant {
                    uri,
                    file_felector_id: 0,
                    sort_column: file_list.sort_column() as u8,
                    sort_order: file_list.sort_order() != gtk::SortType::Ascending,
                    locked: self.is_tab_locked(&file_list),
                })
            })
            .collect()
    }

    pub fn open_tabs(&self, tabs: Vec<TabVariant>) {
        let connection_list = ConnectionList::get();

        let mut visited: HashSet<TabVariant> = Default::default();
        for mut stored_tab in tabs {
            stored_tab.file_felector_id = 0;
            if visited.contains(&stored_tab) {
                continue;
            }
            if let Some(directory) = restore_directory(&connection_list, &stored_tab.uri) {
                self.new_tab_full(
                    Some(&directory),
                    stored_tab.sort_column_id(),
                    stored_tab.sort_type(),
                    stored_tab.locked,
                    true,
                    false,
                );
            }
            visited.insert(stored_tab);
        }

        if self.tab_count() == 0 {
            // Fallback to home directory
            let con = connection_list.home();
            let path = PathBuf::from(glib::home_dir());
            if let Some(directory) =
                Directory::new_startup(con.upcast_ref(), con.create_path(&path))
            {
                self.new_tab_full(
                    Some(&directory),
                    ColumnID::COLUMN_NAME,
                    gtk::SortType::Ascending,
                    false,
                    true,
                    false,
                );
            } else {
                eprintln!("Stored path {} is invalid. Skipping", path.display());
            }
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

        if let Some(connection) = self.file_list().connection() {
            let bookmarks_section = gio::Menu::new();
            for bookmark in connection.bookmarks().iter::<Bookmark>().flatten() {
                let item = gio::MenuItem::new(Some(&bookmark.name()), None);
                item.set_action_and_target_value(
                    Some("fs.select-path"),
                    Some(&bookmark.path().to_variant()),
                );
                bookmarks_section.append_item(&item);
            }
            menu.append_section(None, &bookmarks_section);
        }

        let manage_section = gio::Menu::new();
        manage_section.append(Some(&gettext("Add current dir")), Some("fs.add-bookmark"));
        manage_section.append(
            Some(&gettext("Manage bookmarks…")),
            Some("win.bookmarks-edit"),
        );
        menu.append_section(None, &manage_section);

        let popover = gtk::PopoverMenu::builder()
            .menu_model(&menu)
            .position(gtk::PositionType::Bottom)
            .build();
        popover.set_parent(&self.imp().directory_indicator_box);
        let width = self.width();
        if width > 100 {
            popover.set_width_request(width);
        }
        popover.popup();
    }

    pub fn show_history(&self) {
        let Some(dir_history) = self
            .file_list()
            .connection()
            .map(|connection| connection.dir_history().export())
            .filter(|history| !history.is_empty())
        else {
            return;
        };

        let menu = gio::Menu::new();
        for path in dir_history {
            let item = gio::MenuItem::new(Some(&path), None);
            item.set_action_and_target_value(Some("fs.select-path"), Some(&path.to_variant()));
            menu.append_item(&item);
        }

        let popover = gtk::PopoverMenu::builder()
            .menu_model(&menu)
            .position(gtk::PositionType::Bottom)
            .build();
        popover.set_parent(&self.imp().directory_indicator_box);
        let width = self.width();
        if width > 100 {
            popover.set_width_request(width);
        }
        popover.popup();
    }

    fn update_files(&self) {
        let Some(file_list) = self.current_file_list() else {
            return;
        };
        let Some(directory) = file_list.directory() else {
            return;
        };
        file_list.show_files(&directory);
        if self.is_realized() {
            self.imp().update_selected_files_label();
        }
        if self.active() {
            file_list.select_row(None);
        }
    }

    pub fn update_style(&self) {
        if self.is_realized() {
            self.update_files();
        }
        self.update_show_tabs();
        for i in 0..self.tab_count() {
            let fl = self.file_list_nth(i);
            fl.update_style();
            self.imp().update_tab_label(&fl);
        }
        self.update_connections();
    }

    pub fn update_volume_label(&self) {
        let Some(file_list) = self.current_file_list() else {
            return;
        };
        if !file_list
            .connection()
            .filter(|c| c.can_show_free_space())
            .is_some()
        {
            return;
        }
        let Some(directory) = file_list.directory() else {
            return;
        };
        match directory.upcast_ref::<File>().free_space() {
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

    pub fn do_file_specific_action(&self, fl: &FileList, f: &File) -> bool {
        unsafe {
            ffi::gnome_cmd_file_selector_do_file_specific_action(
                self.to_glib_none().0,
                fl.to_glib_none().0,
                f.to_glib_none().0,
            ) != 0
        }
    }

    pub fn show_filter(&self) {
        self.imp().filter_box.set_visible(true);
        self.imp().filter_entry.grab_focus();
    }
}

#[derive(PartialEq, Eq, Hash, glib::Variant)]
pub struct TabVariant {
    pub uri: String,
    pub file_felector_id: u8,
    pub sort_column: u8,
    pub sort_order: bool,
    pub locked: bool,
}

impl TabVariant {
    pub fn new(uri: impl Into<String>) -> Self {
        Self {
            uri: uri.into(),
            file_felector_id: 0,
            sort_column: ColumnID::COLUMN_NAME as u8,
            sort_order: false,
            locked: false,
        }
    }

    pub fn sort_column_id(&self) -> ColumnID {
        self.sort_column
            .try_into()
            .ok()
            .and_then(ColumnID::from_repr)
            .unwrap_or(ColumnID::COLUMN_NAME)
    }

    pub fn sort_type(&self) -> gtk::SortType {
        match self.sort_order {
            false => gtk::SortType::Ascending,
            true => gtk::SortType::Descending,
        }
    }
}

fn restore_directory(connection_list: &ConnectionList, stored_uri: &str) -> Option<Directory> {
    let (con, path) = match glib::Uri::parse(stored_uri, glib::UriFlags::NONE) {
        Ok(uri) => {
            let con: Connection = if uri.scheme() == "file" {
                connection_list.home().upcast()
            } else {
                // TODO: use connection_list to find or register a connection
                ConnectionRemote::new(stored_uri, &uri).upcast()
            };
            let path = PathBuf::from(uri.path());
            (con, path)
        }
        Err(error) => {
            eprintln!("Stored URI is invalid: {}", error.message());
            let con = connection_list.home().upcast();
            let path = PathBuf::from(stored_uri);
            (con, path)
        }
    };

    if let Some(directory) = Directory::new_startup(&con, con.create_path(&path)) {
        Some(directory)
    } else {
        eprintln!("Stored path {} is invalid. Skipping", path.display());
        None
    }
}

async fn ask_close_locked_tab(parent: &gtk::Window) -> bool {
    gtk::AlertDialog::builder()
        .modal(true)
        .message(gettext("The tab is locked, close anyway?"))
        .buttons([gettext("_Cancel"), gettext("_OK")])
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
            if !file_selector.is_tab_locked(&fl) {
                file_selector.close_tab_nth(index);
            } else if ask_close_locked_tab(&window).await {
                file_selector.close_tab_nth(index);
            }
        }
        (1, 3, TabClick::Tab(index)) => {
            // right-click
            // notebook.set_current_page(Some(index));    // switch to the page the mouse is over
            let fl = file_selector.file_list_nth(index);

            let menu = gio::Menu::new();
            menu.append(Some(&gettext("Open in New _Tab")), Some("win.view-new-tab"));

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
                Some(&gettext("Copy Tab to Other _Pane")),
                Some("win.view-in-inactive-tab"),
            );
            menu.append_section(None, &section);

            menu.append(Some(&gettext("_Close Tab")), Some("win.view-close-tab"));
            menu.append(
                Some(&gettext("Close _All Tabs")),
                Some("win.view-close-all-tabs"),
            );
            menu.append(
                Some(&gettext("Close _Duplicate Tabs")),
                Some("win.view-close-duplicate-tabs"),
            );

            let popover = gtk::PopoverMenu::from_model(Some(&menu));
            popover.set_parent(notebook);
            popover.set_position(gtk::PositionType::Bottom);
            popover.set_pointing_to(Some(&gdk::Rectangle::new(x as i32, y as i32, 0, 0)));
            popover.popup();
        }
        (2, 1, TabClick::Area) => {
            // double-click on a tabs area
            if let Some(dir) = file_selector.file_list().directory() {
                file_selector.new_tab_with_dir(&dir, true, true);
            }
        }
        _ => {}
    }
}

fn create_filter_box() -> (gtk::Box, gtk::Entry) {
    let filter_box = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .margin_start(6)
        .margin_end(6)
        .visible(false)
        .build();

    filter_box.append(&gtk::Label::builder().label(gettext("Filter:")).build());

    let entry = gtk::Entry::builder().hexpand(true).build();
    let key_controller = gtk::EventControllerKey::new();
    key_controller.connect_key_pressed(glib::clone!(
        #[weak]
        filter_box,
        #[upgrade_or]
        glib::Propagation::Proceed,
        move |_, key, _, state| {
            eprintln!("{:?} {:?}", key, state);
            if state == gdk::ModifierType::NO_MODIFIER_MASK && key == gdk::Key::Escape {
                filter_box.set_visible(false);
            }
            glib::Propagation::Proceed
        }
    ));
    entry.add_controller(key_controller);
    filter_box.append(&entry);

    let filter_close_button = gtk::Button::builder()
        .label("x")
        .has_frame(false)
        .focusable(false)
        .build();
    filter_close_button.connect_clicked(glib::clone!(
        #[weak]
        filter_box,
        move |_| filter_box.set_visible(false)
    ));
    filter_box.append(&filter_close_button);

    (filter_box, entry)
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_can_forward(
    fs: *mut ffi::GnomeCmdFileSelector,
) -> gboolean {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.can_forward() as gboolean
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_can_back(fs: *mut ffi::GnomeCmdFileSelector) -> gboolean {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.can_back() as gboolean
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_forward(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.forward()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_back(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.back()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_connection_dropdown(
    fs: *mut ffi::GnomeCmdFileSelector,
) -> *mut GtkDropDown {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.imp().connection_dropdown.to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_get_dir_indicator(
    fs: *mut ffi::GnomeCmdFileSelector,
) -> *mut GtkWidget {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.imp()
        .directory_indicator
        .upcast_ref::<gtk::Widget>()
        .to_glib_none()
        .0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_get_notebook(
    fs: *mut ffi::GnomeCmdFileSelector,
) -> *mut GtkNotebook {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.imp().notebook.to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_update_selected_files_label(
    fs: *mut ffi::GnomeCmdFileSelector,
) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.imp().update_selected_files_label();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_update_show_tabs(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.update_show_tabs();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_update_tab_label(
    fs: *mut ffi::GnomeCmdFileSelector,
    fl: *mut GnomeCmdFileList,
) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    fs.imp().update_tab_label(&fl);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_update_files(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.update_files();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_update_style(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.update_style();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_selector_update_vol_label(fs: *mut ffi::GnomeCmdFileSelector) {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.update_volume_label();
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_type() {
        assert_eq!(*TabVariant::static_variant_type(), "(syybb)");
    }
}
