// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    connection::{Connection, ConnectionExt},
    debug::debug,
    dirlist::list_directory,
    file::{File, FileOps},
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::{gio, glib, glib::object::WeakRef, prelude::*, subclass::prelude::*};
use std::{cell::Ref, collections::HashMap, path::Path, rc::Rc, time::Duration};

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
        pub file_monitor: RefCell<Option<gio::FileMonitor>>,
        pub monitor_users: Cell<u32>,
        pub(super) monitor_queue: RefCell<HashMap<String, (gio::File, gio::FileMonitorEvent)>>,
        pub lock: Cell<bool>,
        /// Weak reference to be set as "parent directory" on directory’s files. Sharing a single
        /// weak reference avoids hitting Glib's limits.
        pub(super) weak_ref: Rc<WeakRef<super::Directory>>,
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
                file_monitor: Default::default(),
                monitor_users: Default::default(),
                monitor_queue: Default::default(),
                lock: Default::default(),
                weak_ref: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for Directory {
        fn constructed(&self) {
            self.parent_constructed();
            self.weak_ref.set(Some(&*self.obj()));
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

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum DirectoryState {
    #[default]
    Empty,
    Listed,
    Listing,
}

impl Directory {
    pub fn new(connection: &impl IsA<Connection>, uri: &str) -> Self {
        Self::new_from_file(connection, gio::File::for_uri(uri))
    }

    pub fn new_from_file(connection: &impl IsA<Connection>, file: impl AsRef<gio::File>) -> Self {
        let file = file.as_ref();
        let is_virtual = file.has_uri_scheme("virtual");
        if !is_virtual && let Some(directory) = connection.as_ref().dir_cache().get(&file.uri()) {
            directory.clone()
        } else {
            let this: Self = glib::Object::builder().build();
            this.imp().connection.replace(connection.as_ref().clone());
            this.imp().file.replace(file.clone());
            if !is_virtual {
                connection.as_ref().dir_cache_mut().insert(&this);
            }
            this
        }
    }

    /// Makes sure a directory cannot be released from cache, typically because it is being
    /// displayed. Each call to `pin_to_cache()` has to be paired with a matching
    /// `unpin_from_cache()` call.
    pub fn pin_to_cache(&self) {
        if !self.is_virtual() {
            self.imp().connection.borrow().dir_cache_mut().pin(self);
        }
    }

    /// Allows the directory to be released from cache again, once called as many times as
    /// `pin_to_cache()` was called before.
    pub fn unpin_from_cache(&self) {
        if !self.is_virtual() {
            self.imp().connection.borrow().dir_cache_mut().unpin(self);
        }
    }

    /// Creates a "virtual" directory only meant to serve as a placeholder. Virtual directories are
    /// not cached.
    pub fn create_virtual(connection: &Connection) -> Self {
        Self::new(connection, "virtual:")
    }

    pub fn is_virtual(&self) -> bool {
        self.file().has_uri_scheme("virtual")
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

    pub fn invalidate(&self) {
        // This directory belongs to a closed connection, force a relist next time it is used.
        self.set_state(DirectoryState::Empty);
    }

    fn set_files(&self, files: impl IntoIterator<Item = File>) {
        let mut store = self.imp().files.borrow_mut();
        store.clear();
        store.extend(files);
    }

    pub async fn relist_files(
        &self,
        cancellable: Option<&gio::Cancellable>,
    ) -> Result<(), ErrorMessage> {
        if self.is_virtual() {
            return Ok(());
        }
        let Some(lock) = DirectoryLock::try_acquire(self) else {
            return Ok(());
        };

        let result = match list_directory(self, cancellable).await {
            Ok(file_infos) => {
                if !cancellable.is_some_and(|c| c.is_cancelled()) {
                    self.set_state(DirectoryState::Listed);
                    self.set_files(
                        file_infos
                            .into_iter()
                            .filter_map(|file_info| create_file_from_file_info(&file_info, self)),
                    );
                    self.emit_by_name::<()>(SIGNAL_LIST_OK, &[]);
                }
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

    pub async fn list_files(&self) -> Result<(), ErrorMessage> {
        if self.files().is_empty() || self.is_local() {
            self.relist_files(None).await
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

            directory
                .connection()
                .dir_cache_mut()
                .remove(&directory.uri());
            directory = directory.parent()?;
        }
    }

    pub fn emit_deleted(&self) {
        self.emit_by_name::<()>(SIGNAL_DIR_DELETED, &[]);
    }

    pub fn listen_to_file(&self, file: &File) {
        file.set_parent_directory(Some(self.imp().weak_ref.clone()));
    }

    fn files_created(&self, files: Vec<gio::File>) {
        let files = files
            .into_iter()
            .filter_map(|file| {
                let file_info = match file.query_info(
                    File::DEFAULT_ATTRIBUTES,
                    gio::FileQueryInfoFlags::NONE,
                    gio::Cancellable::NONE,
                ) {
                    Ok(file_info) => Some(file_info),
                    Err(_error) => {
                        debug!(
                            't',
                            "Could not retrieve file information for {}, error: {_error}",
                            file.uri(),
                        );
                        None
                    }
                }?;
                Some(File::new_from_file(file, &file_info))
            })
            .collect::<Vec<_>>();

        for file in &files {
            self.listen_to_file(file);
            self.emit_by_name::<()>(SIGNAL_FILE_CREATED, &[file]);
        }
        self.imp().files.borrow_mut().extend(files);
    }

    fn files_deleted(&self, positions: Vec<usize>) {
        {
            let files = self.imp().files.borrow();
            let connection = self.imp().connection.borrow();
            let mut dir_cache = connection.dir_cache_mut();
            for position in &positions {
                if let Some(file) = files.get(*position) {
                    dir_cache.remove_with_children(&file.uri());
                    self.emit_by_name::<()>(SIGNAL_FILE_DELETED, &[file]);
                }
            }
        }

        let mut files = self.imp().files.borrow_mut();
        for position in positions.into_iter().rev() {
            files.remove(position);
        }
    }

    fn file_changed_impl(&self, file: &File) {
        match file.refresh_file_info() {
            Ok(()) => {
                self.emit_by_name::<()>(SIGNAL_FILE_CHANGED, &[file]);
            }
            Err(_error) => {
                debug!(
                    't',
                    "Could not retrieve file information for changed file {:?}: {_error}",
                    file.file().basename(),
                );
            }
        }
    }

    pub fn file_renamed(&self, file: &File, old_uri_str: &str) {
        if file.is_directory()
            && let Some(directory) = self.connection().dir_cache().get(old_uri_str)
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

            self.connection()
                .dir_cache_mut()
                .rename(old_uri_str, &file.uri());
            directory.emit_by_name::<()>(SIGNAL_DIR_RENAMED, &[]);
        }
        self.emit_by_name::<()>(SIGNAL_FILE_RENAMED, &[file]);
    }

    fn add_to_monitor_queue(&self, file: &gio::File, event: gio::FileMonitorEvent) {
        // Normalize event types to simplify folding below.
        let event = match event {
            gio::FileMonitorEvent::AttributeChanged => {
                gio::FileMonitorEvent::Changed
            }
            gio::FileMonitorEvent::Changed
            | gio::FileMonitorEvent::Deleted
            | gio::FileMonitorEvent::Created => event,
            _ => return,
        };

        let mut queue = self.imp().monitor_queue.borrow_mut();
        let is_first = queue.is_empty();
        queue
            .entry(file.uri().to_string())
            .and_modify(|(_, existing_event)| {
                // Fold event types. We abuse unmounted event to mark a no-op here.
                if existing_event == &event {
                    return;
                }

                if existing_event == &gio::FileMonitorEvent::Changed
                    || existing_event == &gio::FileMonitorEvent::Unmounted
                {
                    // Other event types always take precedence over a change event.
                    *existing_event = event;
                } else if existing_event == &gio::FileMonitorEvent::Deleted
                    && event == gio::FileMonitorEvent::Created
                {
                    // Create on top of delete is a change event.
                    *existing_event = gio::FileMonitorEvent::Changed;
                } else if existing_event == &gio::FileMonitorEvent::Created
                    && event == gio::FileMonitorEvent::Deleted
                {
                    // Delete on top of create is a no-op.
                    *existing_event = gio::FileMonitorEvent::Unmounted;
                }
            })
            .or_insert_with(|| (file.clone(), event));

        if is_first {
            glib::timeout_add_local_once(
                Duration::from_millis(10),
                glib::clone!(
                    #[strong(rename_to = this)]
                    self,
                    move || this.process_monitor_queue(),
                ),
            );
        }
    }

    pub fn file_created(&self, uri: &str) {
        self.add_to_monitor_queue(&gio::File::for_uri(uri), gio::FileMonitorEvent::Created);
    }

    pub fn file_deleted(&self, uri: &str) {
        self.add_to_monitor_queue(&gio::File::for_uri(uri), gio::FileMonitorEvent::Deleted);
    }

    pub fn file_changed(&self, uri: &str) {
        self.add_to_monitor_queue(&gio::File::for_uri(uri), gio::FileMonitorEvent::Changed);
    }

    fn process_monitor_queue(&self) {
        let mut queue = std::mem::take(&mut *self.imp().monitor_queue.borrow_mut());
        // We don't care about changes to directory itself, only its contents
        queue.remove(&self.uri());

        // Do a single pass through the file list ot validate queue contents.
        let mut remove = Vec::new();
        for (index, file) in self.files().iter().enumerate() {
            if let Some((_, event)) = queue.remove(&file.uri()) {
                if event == gio::FileMonitorEvent::Deleted {
                    remove.push(index);
                } else if event == gio::FileMonitorEvent::Changed {
                    self.file_changed_impl(file);
                }
                // Ignore created for files we already know
            }
        }

        let add = queue
            .into_iter()
            .filter_map(|(_, (file, event))| {
                if event == gio::FileMonitorEvent::Created {
                    Some(file)
                } else {
                    None
                }
            })
            .collect::<Vec<_>>();

        if !remove.is_empty() {
            self.files_deleted(remove);
        }
        if !add.is_empty() {
            self.files_created(add);
        }
    }

    pub fn start_monitoring(&self) {
        let monitor_users = self.imp().monitor_users.get();
        if monitor_users != 0 {
            self.imp().monitor_users.set(monitor_users + 1);
            return;
        }

        // TODO: FileMonitorFlags::WATCH_MOVES
        match self
            .file()
            .monitor_directory(gio::FileMonitorFlags::NONE, gio::Cancellable::NONE)
        {
            Ok(file_monitor) => {
                file_monitor.connect_changed(glib::clone!(
                    #[weak(rename_to = this)]
                    self,
                    move |_, file, _, event| {
                        debug!('n', "{:?} for {}", event, file.uri());
                        this.add_to_monitor_queue(file, event);
                    }
                ));

                debug!('n', "Added monitor to {}", self.uri());

                self.imp().file_monitor.replace(Some(file_monitor));
                self.imp().monitor_users.set(monitor_users + 1);
            }
            Err(_error) => {
                debug!('n', "Failed to add monitor to {}: {_error}", self.uri());
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
            self.imp().monitor_queue.borrow_mut().clear();
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
        parent.listen_to_file(&file);
        Some(file)
    }
}
