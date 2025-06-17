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
        connection::{Connection, ConnectionExt},
        list::ConnectionList,
        remote::ConnectionRemote,
    },
    dir::Directory,
    file::File,
    file_list::list::{ffi::GnomeCmdFileList, ColumnID, FileList},
    notebook_ext::{GnomeCmdNotebookExt, TabClick},
    tags::tags::FileMetadataService,
};
use gettextrs::gettext;
use gtk::{
    ffi::GtkNotebook,
    gdk, gio,
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_borrow, from_glib_none, Borrowed, IntoGlib, ToGlibPtr},
    },
    graphene,
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
    use crate::{dir::ffi::GnomeCmdDir, file_list::list::ffi::GnomeCmdFileList};
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

        pub fn gnome_cmd_file_selector_file_list_nth(
            fs: *mut GnomeCmdFileSelector,
            n: u32,
        ) -> *mut GnomeCmdFileList;

        pub fn gnome_cmd_file_selector_connection_bar(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GtkWidget;

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

        pub fn gnome_cmd_file_selector_is_active(fs: *mut GnomeCmdFileSelector) -> gboolean;
        pub fn gnome_cmd_file_selector_set_active(fs: *mut GnomeCmdFileSelector, active: gboolean);

        pub fn gnome_cmd_file_selector_is_tab_locked(
            fs: *mut GnomeCmdFileSelector,
            fl: *mut GnomeCmdFileList,
        ) -> gboolean;

        pub fn gnome_cmd_file_selector_set_tab_locked(
            fs: *mut GnomeCmdFileSelector,
            fl: *mut GnomeCmdFileList,
            lock: gboolean,
        );

        pub fn gnome_cmd_file_selector_update_show_devlist(
            fs: *mut GnomeCmdFileSelector,
            value: gboolean,
        );

        pub fn gnome_cmd_file_selector_update_style(fs: *mut GnomeCmdFileSelector);

        pub fn gnome_cmd_file_selector_activate_connection_list(fs: *mut GnomeCmdFileSelector);
    }
}

mod imp {
    use super::*;
    use crate::{
        command_line::CommandLine,
        data::{GeneralOptions, GeneralOptionsRead},
        dialogs::manage_bookmarks_dialog::bookmark_directory,
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
        pub notebook: gtk::Notebook,

        #[property(get, set, nullable)]
        pub command_line: RefCell<Option<CommandLine>>,

        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,

        #[property(get, set = Self::set_always_show_tabs)]
        always_show_tabs: Cell<bool>,
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
            Self {
                notebook: gtk::Notebook::builder()
                    .show_tabs(false)
                    .show_border(false)
                    .scrollable(true)
                    .hexpand(true)
                    .vexpand(true)
                    .build(),
                command_line: Default::default(),

                file_metadata_service: Default::default(),

                always_show_tabs: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileSelector {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();

            unsafe {
                ffi::gnome_cmd_file_selector_init(self.obj().to_glib_none().0);
            }

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

    impl WidgetImpl for FileSelector {}
    impl GridImpl for FileSelector {}

    impl FileSelector {
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
                self.obj().new_tab_with_dir(&dir, true);
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

        fn set_always_show_tabs(&self, value: bool) {
            self.always_show_tabs.set(value);
            self.obj().update_show_tabs();
        }
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

    pub fn file_list(&self) -> FileList {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_file_list(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn file_list_nth(&self, n: u32) -> FileList {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_file_list_nth(
                self.to_glib_none().0,
                n,
            ))
        }
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

    pub fn new_tab_with_dir(&self, dir: &Directory, activate: bool) -> gtk::Widget {
        self.new_tab_full(
            Some(dir),
            self.file_list().sort_column(),
            self.file_list().sort_order(),
            false,
            activate,
            activate,
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

    pub fn is_active(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_selector_is_active(self.to_glib_none().0) != 0 }
    }

    pub fn set_active(&self, active: bool) {
        unsafe {
            ffi::gnome_cmd_file_selector_set_active(self.to_glib_none().0, active as gboolean);
        }
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

    pub fn update_show_devlist(&self, value: bool) {
        unsafe {
            ffi::gnome_cmd_file_selector_update_show_devlist(
                self.to_glib_none().0,
                value as gboolean,
            )
        }
    }

    pub fn activate_connection_list(&self) {
        unsafe { ffi::gnome_cmd_file_selector_activate_connection_list(self.to_glib_none().0) }
    }

    pub fn goto_directory(&self, con: &Connection, path: &Path) {
        if self.file_list().connection().as_ref() == Some(&con) {
            if self.is_current_tab_locked() {
                let dir = Directory::new(con, con.create_path(path));
                self.new_tab_with_dir(&dir, true);
            } else {
                self.file_list().goto_directory(path);
            }
        } else {
            if con.is_open() {
                let dir = Directory::new(con, con.create_path(path));

                if self.is_current_tab_locked() {
                    self.new_tab_with_dir(&dir, true);
                } else {
                    self.file_list().set_connection(con, Some(&dir));
                }
            } else {
                con.set_base_path(con.create_path(path));
                if self.is_current_tab_locked() {
                    self.new_tab_with_dir(&con.default_dir().unwrap(), true);
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

    fn goto(&self, connection: &Connection, dir: &str) {
        if self.is_current_tab_locked() {
            self.new_tab_with_dir(
                &Directory::new(connection, connection.create_path(&Path::new(&dir))),
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
                    true,
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
                    true,
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

    pub fn connection_bar(&self) -> gtk::Widget {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_connection_bar(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn update_style(&self) {
        unsafe { ffi::gnome_cmd_file_selector_update_style(self.to_glib_none().0) }
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
                file_selector.new_tab_with_dir(&dir, true);
            }
        }
        _ => {}
    }
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
pub extern "C" fn gnome_cmd_file_selector_get_notebook(
    fs: *mut ffi::GnomeCmdFileSelector,
) -> *mut GtkNotebook {
    let fs: Borrowed<FileSelector> = unsafe { from_glib_borrow(fs) };
    fs.imp().notebook.to_glib_none().0
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

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_type() {
        assert_eq!(*TabVariant::static_variant_type(), "(syybb)");
    }
}
