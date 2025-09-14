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
        remote::{ConnectionRemote, ConnectionRemoteExt},
    },
    debug::debug,
    dirlist::list_directory,
    file::File,
    libgcmd::file_descriptor::{FileDescriptor, FileDescriptorExt},
    path::GnomeCmdPath,
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::path::{Path, PathBuf};

mod imp {
    use super::*;
    use crate::file::FileImpl;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    pub const SIGNAL_FILE_CREATED: &str = "file-created";
    pub const SIGNAL_FILE_DELETED: &str = "file-deleted";
    pub const SIGNAL_FILE_CHANGED: &str = "file-changed";
    pub const SIGNAL_FILE_RENAMED: &str = "file-renamed";
    pub const SIGNAL_LIST_OK: &str = "list-ok";
    pub const SIGNAL_LIST_FAILED: &str = "list-failed";
    pub const SIGNAL_DIR_DELETED: &str = "dir-deleted";

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::Directory)]
    pub struct Directory {
        pub connection: RefCell<Option<Connection>>,
        pub path: RefCell<Option<GnomeCmdPath>>,
        pub state: Cell<DirectoryState>,
        pub files: gio::ListStore,
        #[property(get, set)]
        pub needs_mtime_update: Cell<bool>,
        pub file_monitor: RefCell<Option<gio::FileMonitor>>,
        pub monitor_users: Cell<u32>,
        pub lock: Cell<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Directory {
        const NAME: &'static str = "GnomeCmdDir";
        type Type = super::Directory;
        type ParentType = File;

        fn new() -> Self {
            Self {
                connection: Default::default(),
                path: Default::default(),
                state: Cell::new(DirectoryState::Empty),
                files: gio::ListStore::new::<File>(),
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
            if let Some(connection) = self.connection.take() {
                connection.remove_from_cache(&*self.obj());
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
                ]
            })
        }
    }

    impl FileImpl for Directory {}
}

glib::wrapper! {
    pub struct Directory(ObjectSubclass<imp::Directory>)
        @extends File,
        @implements FileDescriptor;
}

#[repr(C)]
#[derive(Clone, Copy, strum::FromRepr, Default, PartialEq, Eq)]
pub enum DirectoryState {
    #[default]
    Empty = 0,
    Listed,
    Listing,
    Canceling,
}

impl Directory {
    pub fn new_from_file_info(file_info: &gio::FileInfo, parent: &Directory) -> Option<Self> {
        let connection = parent.connection();
        let dir_name = file_info.name();

        let (file, path) = if let Some(file) = file_for_connection_and_filename(parent, &dir_name) {
            let uri = glib::Uri::parse(&file.uri(), glib::UriFlags::NONE).ok()?;
            let path = connection.create_path(Path::new(&uri.path()));
            (file, path)
        } else {
            let parent_path = parent.path();
            let path = parent_path.child(&dir_name);
            let file = connection.create_gfile(&path);
            (file, path)
        };
        Some(Self::find_or_create(&connection, &file, file_info, path))
    }

    #[deprecated(
        note = "This function panics when file info query fails. Prefer Self::try_new instead."
    )]
    pub fn new(connection: &impl IsA<Connection>, path: GnomeCmdPath) -> Self {
        Self::try_new(connection, path).expect("directory file info")
    }

    pub fn try_new(
        connection: &impl IsA<Connection>,
        path: GnomeCmdPath,
    ) -> Result<Self, ErrorMessage> {
        let connection = connection.as_ref();
        let file = connection.create_gfile(&path);
        let file_info = file
            .query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)
            .map_err(|error| {
                ErrorMessage::with_error(
                    gettext("Failed to get directory info for {path}.")
                        .replace("{path}", &path.to_string()),
                    &error,
                )
            })?;
        Ok(Self::find_or_create(connection, &file, &file_info, path))
    }

    pub fn new_with_con(connection: &Connection) -> Option<Self> {
        let base_file_info = connection.base_file_info()?;
        let base_path = connection.base_path()?;
        let file = connection.create_gfile(&base_path);
        Some(Self::find_or_create(
            &connection,
            &file,
            &base_file_info,
            base_path.clone(),
        ))
    }

    fn new_full(
        connection: &Connection,
        file: &gio::File,
        file_info: &gio::FileInfo,
        path: GnomeCmdPath,
    ) -> Self {
        let this: Self = glib::Object::builder()
            .property("file", file)
            .property("file-info", file_info)
            .build();
        this.imp().connection.replace(Some(connection.clone()));
        this.imp().path.replace(Some(path));
        this
    }

    pub fn find_or_create(
        connection: &Connection,
        file: &gio::File,
        file_info: &gio::FileInfo,
        path: GnomeCmdPath,
    ) -> Self {
        let uri = file.uri();
        if let Some(directory) = connection.cache_lookup(&uri) {
            directory.upcast_ref::<File>().set_file_info(file_info);
            directory
        } else {
            let directory = Self::new_full(connection, file, file_info, path);
            connection.add_to_cache(&directory, &uri);
            directory
        }
    }

    pub fn connection(&self) -> Connection {
        self.imp().connection.borrow().clone().unwrap()
    }

    pub fn is_local(&self) -> bool {
        self.connection().is_local()
    }

    pub fn parent(&self) -> Option<Directory> {
        if let Some(path) = self.path().parent() {
            Some(Directory::new(&self.connection(), path))
        } else {
            None
        }
    }

    pub fn child(&self, name: &Path) -> Result<Directory, ErrorMessage> {
        let connection = self.connection();
        let path = self.path().child(name);
        Directory::try_new(&connection, path)
    }

    pub fn display_path(&self) -> String {
        self.path().to_string()
    }

    pub fn files(&self) -> gio::ListStore {
        self.imp().files.clone()
    }

    pub fn state(&self) -> DirectoryState {
        self.imp().state.get()
    }

    fn set_state(&self, state: DirectoryState) {
        self.imp().state.set(state);
    }

    fn set_files(&self, files: impl IntoIterator<Item = File>) {
        let store = self.files();
        store.remove_all();
        for file in files {
            store.append(&file);
        }
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
                self.emit_by_name::<()>("list-ok", &[]);
                Ok(())
            }
            Err(error) => {
                self.set_state(DirectoryState::Empty);
                self.emit_by_name::<()>("list-failed", &[&error]);
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
        let files = self.files();
        if files.n_items() == 0 || self.is_local() {
            self.relist_files(parent_window, visual).await
        } else {
            self.emit_by_name::<()>("list-ok", &[]);
            Ok(())
        }
    }

    pub fn get_child_gfile(&self, filename: &Path) -> gio::File {
        let connection = self.connection();
        file_for_connection_and_filename(self, filename)
            .unwrap_or_else(|| connection.create_gfile(&self.path().child(filename)))
    }

    pub fn path(&self) -> GnomeCmdPath {
        self.imp().path.borrow().clone().unwrap()
    }

    pub fn update_path(&self) {
        if let Some(parent) = self.parent() {
            let path = parent
                .path()
                .child(&self.upcast_ref::<File>().file_info().name());
            self.imp().path.replace(Some(path));
        }
    }

    /// This function will search in the file tree to retrieve the first
    /// physically existing parent dir for a given dir.
    pub fn existing_parent(&self) -> Option<Directory> {
        let mut directory = self.parent()?;
        loop {
            if directory
                .upcast_ref::<File>()
                .file()
                .query_exists(gio::Cancellable::NONE)
            {
                break Some(directory);
            }

            directory.connection().remove_from_cache(&directory);
            directory = directory.parent()?;
        }
    }

    fn find_file(&self, uri_str: &str) -> Option<(u32, File)> {
        let files = self.files();
        for (i, f) in files.iter::<File>().flatten().enumerate() {
            if f.get_uri_str() == uri_str {
                return Some((i as u32, f));
            }
        }
        None
    }

    pub fn file_created(&self, uri_str: &str) {
        if self.find_file(uri_str).is_some() {
            return;
        }

        let file = gio::File::for_uri(uri_str);
        let file_info =
            match file.query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE) {
                Ok(file_info) => file_info,
                Err(error) => {
                    debug!(
                        't',
                        "Could not retrieve file information for {}, error: {}", uri_str, error
                    );
                    return;
                }
            };

        let f = if file_info.file_type() == gio::FileType::Directory {
            Directory::new_from_file_info(&file_info, self).and_upcast()
        } else {
            Some(File::new(&file_info, self))
        };

        if let Some(f) = f {
            self.files().append(&f);
            self.set_needs_mtime_update(true);
            self.emit_by_name::<()>("file-created", &[&f]);
        }
    }

    pub fn file_deleted(&self, uri_str: &str) {
        if uri_str == self.upcast_ref::<File>().get_uri_str() {
            self.connection().remove_from_cache(self);
            self.emit_by_name::<()>("dir-deleted", &[]);
        }

        if let Some((position, file)) = self.find_file(uri_str) {
            self.set_needs_mtime_update(true);
            self.emit_by_name::<()>("file-deleted", &[&file]);
            self.files().remove(position);
        }
    }

    pub fn file_changed(&self, uri_str: &str) {
        if let Some((_, file)) = self.find_file(uri_str) {
            self.set_needs_mtime_update(true);
            match file.refresh_file_info() {
                Ok(()) => {
                    self.emit_by_name::<()>("file-changed", &[&file]);
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
        if let Some(_dir) = file.downcast_ref::<Directory>() {
            self.connection().remove_from_cache_by_uri(old_uri_str);
        }

        self.set_needs_mtime_update(true);
        self.emit_by_name::<()>("file-renamed", &[file]);
    }

    /// This function also determines if cached dir is up-to-date (false=yes)
    pub fn update_mtime(&self) -> bool {
        let file_info = self.file_info();

        let current_time = self
            .file()
            .query_info(
                gio::FILE_ATTRIBUTE_TIME_MODIFIED,
                gio::FileQueryInfoFlags::NONE,
                gio::Cancellable::NONE,
            )
            .ok()
            .and_then(|fi| fi.modification_date_time());

        let cached_time = file_info.modification_date_time();

        let result = match (cached_time, current_time) {
            (Some(cached_time), Some(current_time)) if current_time != cached_time => {
                // cache is not up-to-date
                self.file_info().set_modification_date_time(&current_time);
                true
            }
            _ => false,
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

                debug!(
                    'n',
                    "Added monitor to {}",
                    self.upcast_ref::<File>().get_uri_str()
                );

                self.imp().file_monitor.replace(Some(file_monitor));
                self.imp()
                    .monitor_users
                    .set(self.imp().monitor_users.get() + 1);
            }
            Err(error) => {
                debug!(
                    'n',
                    "Failed to add monitor to {}: {}",
                    self.upcast_ref::<File>().get_uri_str(),
                    error
                );
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
        if monitor_users == 0 {
            if let Some(file_monitor) = self.imp().file_monitor.take() {
                file_monitor.cancel();
                debug!(
                    'n',
                    "Removed monitor from {}",
                    self.upcast_ref::<File>().get_uri_str()
                );
            }
        }
    }

    pub fn is_monitored(&self) -> bool {
        self.imp().monitor_users.get() > 0
    }
}

/// Get the relative directory path string for the given base path
///
/// For example, let childPath be "/tmp/abc" and basePath is the path "/tmp/"
/// (from the original URI smb://localhost/tmp/). Then the return would be
/// the the address pointing to "/" (which is the second slash from "/tmp/").
/// If childPath is pointing to "/tmp/abc" and the basePath points
/// to "/xyz", then the return points to "/" because /xzy is relative
/// to the /tmp on the same directory level.
fn get_relative_path_string(child: Option<&Path>, base: &Path) -> PathBuf {
    let Some(child) = child else {
        return base.to_path_buf();
    };

    if base == Path::new("/") {
        return child.to_path_buf();
    }

    if let Ok(tail) = child.strip_prefix(base) {
        tail.to_path_buf()
    } else {
        PathBuf::from("/")
    }
}

fn remote_connection_mount(con: &ConnectionRemote) -> Option<gio::File> {
    let mut file = gio::File::for_uri(&con.uri_string()?);
    loop {
        if let Some(parent) = file.parent() {
            file = parent;
        } else {
            break;
        }
        if file.find_enclosing_mount(gio::Cancellable::NONE).is_err() {
            break;
        }
    }
    Some(file)
}

/// This function returns a GFile object which is the result of the URI construction of
/// two URI's and a path: the connection URI, which is the private member of GnomeCmdDir,
/// the directory path inside of that connection, and the given filename string.
fn file_for_connection_and_filename(dir: &Directory, filename: &Path) -> Option<gio::File> {
    let Ok(con) = dir.connection().downcast::<ConnectionRemote>() else {
        return None;
    };

    // Get the Uri for the mount which belongs to the GnomeCmdCon object
    let mount_file = remote_connection_mount(&con)?;
    let mut mount_uri = mount_file.uri().to_string();

    // Always let the connection URI to end with '/' because the last entry should be a directory
    if mount_uri.ends_with('/') {
        mount_uri.push('/')
    }

    // Create the merged URI out of the connection URI, the directory path and the filename
    let dir_path = dir.path().path();

    let rel_dir_to_uri_path =
        get_relative_path_string(Some(&dir_path), &mount_file.path().unwrap());
    let merged_dir_and_filename = Path::new(".").join(rel_dir_to_uri_path).join(filename);

    match glib::Uri::resolve_relative(
        Some(&mount_uri),
        &merged_dir_and_filename.to_str().unwrap_or_default(),
        glib::UriFlags::NONE,
    ) {
        Ok(uri) => Some(gio::File::for_uri(&uri)),
        Err(error) => {
            eprintln!("Could not resolve relative URI: {error}");
            None
        }
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
        return None;
    }

    if file_info.file_type() == gio::FileType::Directory {
        Directory::new_from_file_info(&file_info, parent).and_upcast::<File>()
    } else {
        Some(File::new(&file_info, parent))
    }
}
