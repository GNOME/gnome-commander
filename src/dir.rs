// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    connection::{Connection, list::ConnectionList},
    debug::debug,
    dirlist::list_directory,
    file::{File, FileOps},
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{cell::Ref, path::Path};

const SIGNAL_FILE_CREATED: &str = "file-created";
const SIGNAL_FILE_DELETED: &str = "file-deleted";
const SIGNAL_FILE_CHANGED: &str = "file-changed";
const SIGNAL_FILE_RENAMED: &str = "file-renamed";
const SIGNAL_LIST_OK: &str = "list-ok";
const SIGNAL_LIST_FAILED: &str = "list-failed";
const SIGNAL_DIR_DELETED: &str = "dir-deleted";
const SIGNAL_DIR_RENAMED: &str = "dir-renamed";

mod imp {
    use super::*;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::Directory)]
    pub struct Directory {
        #[property(get)]
        pub connection: RefCell<Connection>,
        pub file: RefCell<gio::File>,
        pub state: Cell<DirectoryState>,
        pub files: RefCell<Vec<File>>,
        #[property(get, set)]
        pub mtime: RefCell<Option<glib::DateTime>>,
        #[property(get, set)]
        pub needs_mtime_update: Cell<bool>,
        pub file_monitor: RefCell<Option<gio::FileMonitor>>,
        pub monitor_users: Cell<u32>,
        pub lock: Cell<bool>,
    }

    thread_local! {
        static DUMMY_CONNECTION: RefCell<Connection> = RefCell::new(glib::Object::builder().build());
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Directory {
        const NAME: &'static str = "GnomeCmdDir";
        type Type = super::Directory;

        fn new() -> Self {
            Self {
                connection: DUMMY_CONNECTION.with(|con| con.clone()),
                file: RefCell::new(gio::File::for_path("/")),
                state: Cell::new(DirectoryState::Empty),
                files: RefCell::new(Vec::new()),
                mtime: Default::default(),
                needs_mtime_update: Default::default(),
                file_monitor: Default::default(),
                monitor_users: Default::default(),
                lock: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for Directory {
        fn constructed(&self) {
            self.parent_constructed();
        }

        fn dispose(&self) {
            if !self.file.borrow().has_uri_scheme("virtual") {
                self.connection.borrow().remove_from_cache(&self.obj());
            }
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder(SIGNAL_FILE_CREATED)
                        .param_types([File::static_type()])
                        .build(),
                    glib::subclass::Signal::builder(SIGNAL_FILE_DELETED)
                        .param_types([File::static_type()])
                        .build(),
                    glib::subclass::Signal::builder(SIGNAL_FILE_CHANGED)
                        .param_types([File::static_type()])
                        .build(),
                    glib::subclass::Signal::builder(SIGNAL_FILE_RENAMED)
                        .param_types([File::static_type()])
                        .build(),
                    glib::subclass::Signal::builder(SIGNAL_LIST_OK).build(),
                    glib::subclass::Signal::builder(SIGNAL_LIST_FAILED)
                        .param_types([glib::Error::static_type()])
                        .build(),
                    glib::subclass::Signal::builder(SIGNAL_DIR_DELETED).build(),
                    glib::subclass::Signal::builder(SIGNAL_DIR_RENAMED).build(),
                ]
            })
        }
    }
}

glib::wrapper! {
    pub struct Directory(ObjectSubclass<imp::Directory>);
}

#[derive(Clone, Copy, Default, PartialEq, Eq)]
pub enum DirectoryState {
    #[default]
    Empty,
    Listed,
    Listing,
    Canceling,
}

impl Directory {
    pub fn new(connection: &impl IsA<Connection>, uri: &str) -> Self {
        Self::new_from_file(connection, gio::File::for_uri(uri))
    }

    pub fn new_from_file(connection: &impl IsA<Connection>, file: impl AsRef<gio::File>) -> Self {
        let file = file.as_ref();
        let is_virtual = file.has_uri_scheme("virtual");
        if !is_virtual && let Some(directory) = connection.as_ref().cache_lookup(&file.uri()) {
            directory
        } else {
            let this: Self = glib::Object::builder().build();
            this.imp().connection.replace(connection.as_ref().clone());
            this.imp().file.replace(file.clone());
            if !is_virtual {
                connection.as_ref().add_to_cache(&this, &file.uri());
            }
            this
        }
    }

    /// Creates a "virtual" directory only meant to keep track of a bunch of files since files can
    /// currently only notify directories about changes. TODO: We should really allow files to be
    /// monitored by a generic trait instead of a directory.
    pub fn create_virtual() -> Self {
        Self::new(&ConnectionList::get().home(), "virtual:")
    }

    pub fn parent(&self) -> Option<Directory> {
        self.file()
            .parent()
            .map(|parent| Directory::new_from_file(&self.connection(), parent))
    }

    pub fn child(&self, name: impl AsRef<Path>) -> Directory {
        Directory::new_from_file(&self.connection(), self.file().child(name))
    }

    pub fn display_path(&self) -> String {
        self.path_from_root().to_string_lossy().to_string()
    }

    pub fn files(&self) -> Ref<'_, Vec<File>> {
        self.imp().files.borrow()
    }

    pub fn state(&self) -> DirectoryState {
        self.imp().state.get()
    }

    fn set_state(&self, state: DirectoryState) {
        self.imp().state.set(state);
    }

    fn set_files(&self, files: impl IntoIterator<Item = File>) {
        let mut store = self.imp().files.borrow_mut();
        store.clear();
        store.extend(files);
    }

    pub async fn relist_files(
        &self,
        parent_window: &gtk::Window,
        mut visual: bool,
    ) -> Result<(), ErrorMessage> {
        let Some(lock) = DirectoryLock::try_acquire(self) else {
            return Ok(());
        };

        visual &= self.connection().needs_list_visprog();
        let window = if visual { Some(parent_window) } else { None };
        let result = match list_directory(self, window).await {
            Ok(file_infos) => {
                self.set_state(DirectoryState::Listed);
                self.set_files(
                    file_infos
                        .into_iter()
                        .filter_map(|file_info| create_file_from_file_info(&file_info, self)),
                );
                self.emit_by_name::<()>(SIGNAL_LIST_OK, &[]);
                Ok(())
            }
            Err(error) => {
                self.set_state(DirectoryState::Empty);
                self.emit_by_name::<()>(SIGNAL_LIST_FAILED, &[&error]);
                Err(ErrorMessage::with_error(
                    gettext("Directory listing failed."),
                    &error,
                ))
            }
        };

        lock.release();

        result
    }

    pub async fn list_files(
        &self,
        parent_window: &gtk::Window,
        mut visual: bool,
    ) -> Result<(), ErrorMessage> {
        visual &= self.connection().needs_list_visprog();
        if self.files().is_empty() || self.is_local() {
            self.relist_files(parent_window, visual).await
        } else {
            self.emit_by_name::<()>(SIGNAL_LIST_OK, &[]);
            Ok(())
        }
    }

    pub fn clear_files(&self) {
        self.imp().files.borrow_mut().clear();
    }

    pub fn add_file(&self, file: &File) {
        self.imp().files.borrow_mut().push(file.clone());
    }

    pub fn get_child_gfile(&self, filename: &Path) -> gio::File {
        self.file().child(filename)
    }

    /// This function will search in the file tree to retrieve the first
    /// physically existing parent dir for a given dir.
    pub fn existing_parent(&self) -> Option<Directory> {
        let mut directory = self.parent()?;
        loop {
            if directory.file().query_exists(gio::Cancellable::NONE) {
                break Some(directory);
            }

            directory.connection().remove_from_cache(&directory);
            directory = directory.parent()?;
        }
    }

    fn find_file(&self, uri_str: &str) -> Option<(usize, File)> {
        for (i, f) in self.files().iter().enumerate() {
            if f.uri() == uri_str {
                return Some((i, f.clone()));
            }
        }
        None
    }

    pub fn file_created(&self, uri_str: &str) {
        if self.find_file(uri_str).is_some() {
            return;
        }

        let file = gio::File::for_uri(uri_str);
        let file_info = match file.query_info(
            File::DEFAULT_ATTRIBUTES,
            gio::FileQueryInfoFlags::NONE,
            gio::Cancellable::NONE,
        ) {
            Ok(file_info) => file_info,
            Err(error) => {
                debug!(
                    't',
                    "Could not retrieve file information for {}, error: {}", uri_str, error
                );
                return;
            }
        };

        let f = File::new_from_file(file, &file_info);
        f.set_parent_directory(Some(self));
        self.add_file(&f);
        self.set_needs_mtime_update(true);
        self.emit_by_name::<()>(SIGNAL_FILE_CREATED, &[&f]);
    }

    pub fn file_deleted(&self, uri_str: &str) {
        if let Some(directory) = self.connection().cache_lookup(uri_str) {
            self.connection().remove_from_cache_by_uri(uri_str);
            directory.emit_by_name::<()>(SIGNAL_DIR_DELETED, &[]);
        }

        if let Some((position, file)) = self.find_file(uri_str) {
            self.set_needs_mtime_update(true);
            self.emit_by_name::<()>(SIGNAL_FILE_DELETED, &[&file]);
            self.imp().files.borrow_mut().remove(position);
        }
    }

    pub fn file_changed(&self, uri_str: &str) {
        if let Some((_, file)) = self.find_file(uri_str) {
            self.set_needs_mtime_update(true);
            match file.refresh_file_info() {
                Ok(()) => {
                    self.emit_by_name::<()>(SIGNAL_FILE_CHANGED, &[&file]);
                }
                Err(error) => {
                    debug!(
                        't',
                        "Could not retrieve file information for changed file {:?}: {}",
                        file.file().basename(),
                        error
                    );
                }
            }
        }
    }

    pub fn file_renamed(&self, file: &File, old_uri_str: &str) {
        if file.is_directory()
            && let Some(directory) = self.connection().cache_lookup(old_uri_str)
        {
            // Reattach monitoring if necessary
            let monitor_users = directory.imp().monitor_users.get();
            for _ in 0..monitor_users {
                directory.cancel_monitoring();
            }
            directory.set_file(file.file().clone());
            for _ in 0..monitor_users {
                directory.start_monitoring();
            }

            // Update listed files
            for file in directory.files().iter() {
                file.set_file(directory.get_child_gfile(&file.path_name()));
            }

            self.connection().remove_from_cache_by_uri(old_uri_str);
            self.connection().add_to_cache(&directory, &file.uri());
            directory.emit_by_name::<()>(SIGNAL_DIR_RENAMED, &[]);
        }
        self.set_needs_mtime_update(true);
        self.emit_by_name::<()>(SIGNAL_FILE_RENAMED, &[file]);
    }

    /// This function also determines if cached dir is up-to-date (false=yes)
    pub fn update_mtime(&self) -> bool {
        let current_time = self
            .file()
            .query_info(
                gio::FILE_ATTRIBUTE_TIME_MODIFIED,
                gio::FileQueryInfoFlags::NONE,
                gio::Cancellable::NONE,
            )
            .ok()
            .and_then(|fi| fi.modification_date_time());

        let cached_time = self.mtime();

        let result = if let Some(cached_time) = cached_time
            && let Some(current_time) = current_time
            && current_time != cached_time
        {
            // cache is not up-to-date
            self.set_mtime(&current_time);
            true
        } else {
            false
        };

        // after this function we are sure dir's mtime is up-to-date
        self.set_needs_mtime_update(false);

        result
    }

    pub fn start_monitoring(&self) {
        if self.imp().monitor_users.get() != 0 {
            return;
        }

        //ToDo: We might want to activate G_FILE_MONITOR_WATCH_MOVES in the future
        match self
            .file()
            .monitor_directory(gio::FileMonitorFlags::NONE, gio::Cancellable::NONE)
        {
            Ok(file_monitor) => {
                file_monitor.connect_changed(glib::clone!(
                    #[weak(rename_to = this)]
                    self,
                    move |_, file, _, event| {
                        let uri = file.uri();
                        if uri == this.uri() {
                            // We don't care about changes to directory itself, only its contents
                            return;
                        }
                        debug!('n', "{:?} for {}", event, uri);
                        match event {
                            gio::FileMonitorEvent::Changed
                            | gio::FileMonitorEvent::AttributeChanged => this.file_changed(&uri),
                            gio::FileMonitorEvent::Deleted => this.file_deleted(&uri),
                            gio::FileMonitorEvent::Created => this.file_created(&uri),
                            _ => {}
                        }
                    }
                ));

                debug!('n', "Added monitor to {}", self.uri());

                self.imp().file_monitor.replace(Some(file_monitor));
                self.imp()
                    .monitor_users
                    .set(self.imp().monitor_users.get() + 1);
            }
            Err(error) => {
                debug!('n', "Failed to add monitor to {}: {}", self.uri(), error);
            }
        }
    }

    pub fn cancel_monitoring(&self) {
        let mut monitor_users = self.imp().monitor_users.get();
        if monitor_users < 1 {
            return;
        }
        monitor_users -= 1;
        self.imp().monitor_users.set(monitor_users);
        if monitor_users == 0
            && let Some(file_monitor) = self.imp().file_monitor.take()
        {
            file_monitor.cancel();
            debug!('n', "Removed monitor from {}", self.uri());
        }
    }

    pub fn is_monitored(&self) -> bool {
        self.imp().monitor_users.get() > 0
    }
}

impl FileOps for Directory {
    fn file(&self) -> Ref<'_, gio::File> {
        self.imp().file.borrow()
    }

    fn set_file(&self, file: gio::File) {
        self.imp().file.replace(file);
    }
}

struct DirectoryLock<'d>(&'d Directory);

impl<'d> DirectoryLock<'d> {
    fn try_acquire(dir: &'d Directory) -> Option<Self> {
        if dir.imp().lock.get() {
            None
        } else {
            dir.imp().lock.set(true);
            Some(Self(dir))
        }
    }

    fn release(self) {
        drop(self)
    }
}

impl<'d> Drop for DirectoryLock<'d> {
    fn drop(&mut self) {
        self.0.imp().lock.set(false);
    }
}

fn create_file_from_file_info(file_info: &gio::FileInfo, parent: &Directory) -> Option<File> {
    let name = file_info.display_name();
    if name == "." || name == ".." {
        None
    } else {
        let file = File::new_from_file(parent.get_child_gfile(&file_info.name()), file_info);
        file.set_parent_directory(Some(parent));
        Some(file)
    }
}
