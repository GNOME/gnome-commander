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
    connection::connection::{ffi::GnomeCmdCon, Connection, ConnectionExt},
    debug::debug,
    dirlist::list_directory,
    file::{ffi::GnomeCmdFile, File},
    libgcmd::file_descriptor::{FileDescriptor, FileDescriptorExt},
    path::GnomeCmdPath,
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::{
    gio::{
        self,
        ffi::{GFile, GFileInfo},
    },
    glib::{
        self,
        ffi::{gboolean, GError, GType},
        translate::{from_glib_borrow, from_glib_full, Borrowed, IntoGlib, ToGlibPtr},
    },
    prelude::*,
    subclass::prelude::*,
};
use std::path::Path;

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

pub mod ffi {
    use crate::{connection::connection::ffi::GnomeCmdCon, path::GnomeCmdPath};
    use gtk::{
        gio::ffi::{GFile, GFileInfo},
        glib::ffi::gboolean,
    };
    use std::ffi::c_char;

    pub type GnomeCmdDir = <super::Directory as glib::object::ObjectType>::GlibType;

    extern "C" {
        pub fn gnome_cmd_dir_new_from_gfileinfo(
            file_info: *mut GFileInfo,
            parent: *mut GnomeCmdDir,
        ) -> *mut GnomeCmdDir;
        pub fn gnome_cmd_dir_new(
            dir: *mut GnomeCmdCon,
            path: *const GnomeCmdPath,
            is_startup: gboolean,
        ) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_dir_get_child_gfile(
            dir: *mut GnomeCmdDir,
            filename: *const c_char,
        ) -> *mut GFile;
    }
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
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_new_from_gfileinfo(
                file_info.to_glib_full(),
                parent.to_glib_none().0,
            ))
        }
    }

    pub fn new(connection: &impl IsA<Connection>, path: GnomeCmdPath) -> Self {
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_new(
                connection.as_ref().to_glib_none().0,
                path.into_raw(),
                false as gboolean,
            ))
        }
    }

    pub fn new_startup(connection: &Connection, path: GnomeCmdPath) -> Option<Self> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_new(
                connection.to_glib_none().0,
                path.into_raw(),
                true as gboolean,
            ))
        }
    }

    pub fn new_with_con(connection: &Connection) -> Option<Self> {
        let base_file_info = connection.base_file_info()?;
        let base_path = connection.base_path()?;
        let file = connection.create_gfile(&base_path);
        match Self::find_or_create(&connection, &file, Some(&base_file_info), base_path.clone()) {
            Ok(directory) => Some(directory),
            Err(error) => {
                eprintln!("Directory::new_with_con error on {}: {}", file.uri(), error);
                None
            }
        }
    }

    fn new_impl(
        connection: &Connection,
        file: &gio::File,
        _file_info: Option<&gio::FileInfo>,
        path: GnomeCmdPath,
    ) -> Result<Self, glib::Error> {
        let this: Self = glib::Object::builder().build();
        this.upcast_ref::<File>().setup(file)?;
        this.imp().connection.replace(Some(connection.clone()));
        this.imp().path.replace(Some(path));
        Ok(this)
    }

    fn find_or_create(
        connection: &Connection,
        file: &gio::File,
        file_info: Option<&gio::FileInfo>,
        path: GnomeCmdPath,
    ) -> Result<Self, glib::Error> {
        let uri = file.uri();
        if let Some(directory) = connection.cache_lookup(&uri) {
            if let Some(file_info) = file_info {
                directory.upcast_ref::<File>().set_file_info(file_info);
            }
            Ok(directory)
        } else {
            let directory = Self::new_impl(connection, file, file_info, path)?;
            connection.add_to_cache(&directory, &uri);
            Ok(directory)
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

    pub fn child(&self, name: &Path) -> Option<Directory> {
        let connection = self.connection();
        let path = self.path().child(name);
        Some(Directory::new(&connection, path))
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
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_get_child_gfile(
                self.to_glib_none().0,
                filename.to_glib_none().0,
            ))
        }
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

struct DirectoryLock<'d>(&'d Directory);

impl<'d> DirectoryLock<'d> {
    fn try_acquire(dir: &'d Directory) -> Option<Self> {
        if unsafe { dir.data::<bool>("lock") }.is_some() {
            None
        } else {
            unsafe { dir.set_data::<bool>("lock", true) }
            Some(Self(dir))
        }
    }

    fn release(self) {
        drop(self)
    }
}

impl<'d> Drop for DirectoryLock<'d> {
    fn drop(&mut self) {
        unsafe {
            self.0.steal_data::<bool>("lock");
        }
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

#[no_mangle]
pub extern "C" fn gnome_cmd_dir_get_type() -> GType {
    Directory::static_type().into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_dir_find_or_create(
    con: *mut GnomeCmdCon,
    file: *mut GFile,
    file_info: *mut GFileInfo,
    path: *mut GnomeCmdPath,
    error: *mut *mut GError,
) -> *mut ffi::GnomeCmdDir {
    let connection: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let file: gio::File = unsafe { from_glib_full(file) };
    let file_info: Option<gio::FileInfo> = unsafe { from_glib_full(file_info) };
    let path = unsafe { GnomeCmdPath::from_raw(path) };
    match Directory::find_or_create(&connection, &file, file_info.as_ref(), path) {
        Ok(directory) => directory.to_glib_full(),
        Err(e) => {
            unsafe {
                *error = e.to_glib_full();
            }
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_dir_get_connection(file: *mut GnomeCmdFile) -> *mut GnomeCmdCon {
    let file: Borrowed<File> = unsafe { from_glib_borrow(file) };
    file.downcast_ref::<Directory>()
        .unwrap()
        .connection()
        .to_glib_none()
        .0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_dir_get_path(dir: *mut ffi::GnomeCmdDir) -> *mut GnomeCmdPath {
    let dir: Borrowed<Directory> = unsafe { from_glib_borrow(dir) };
    dir.path().into_raw()
}
