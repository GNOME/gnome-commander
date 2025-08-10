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

use super::{
    bookmark::Bookmark, device::ConnectionDevice, history::History, home::ConnectionHome,
    remote::ConnectionRemote, smb::ConnectionSmb,
};
use crate::{
    data::{GeneralOptions, GeneralOptionsRead},
    debug::debug,
    dir::{ffi::GnomeCmdDir, Directory},
    file::File,
    path::GnomeCmdPath,
    utils::{sleep, ErrorMessage},
};
use gettextrs::gettext;
use gtk::{
    gio::{self, ffi::GFileInfo},
    glib::{
        self,
        ffi::{gboolean, GError, GUri},
        translate::*,
    },
    prelude::*,
};
use std::{collections::HashMap, ffi::c_char, ops::Deref, path::Path, sync::LazyLock};

pub mod ffi {
    use super::*;
    use gtk::{
        ffi::GtkWindow,
        gio::ffi::{GCancellable, GFile},
        glib::ffi::GType,
    };
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdCon {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_get_type() -> GType;

        pub fn gnome_cmd_con_create_path(
            con: *const GnomeCmdCon,
            path_str: *const c_char,
        ) -> *mut GnomeCmdPath;

        pub fn gnome_cmd_con_create_gfile(
            con: *const GnomeCmdCon,
            uri: *const c_char,
        ) -> *mut GFile;

        pub fn gnome_cmd_con_open(
            con: *mut GnomeCmdCon,
            parent_window: *mut GtkWindow,
            cancellable: *mut GCancellable,
        );

        pub fn gnome_cmd_con_close(con: *mut GnomeCmdCon, parent_window: *mut GtkWindow);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct Connection(Object<ffi::GnomeCmdCon, ffi::GnomeCmdConClass>);

    match fn {
        type_ => || ffi::gnome_cmd_con_get_type(),
    }
}

struct ConnectionPrivate {
    uuid: String,
    history: History<String>, // TODO: consider Rc<GnomeCmdPath>
    dir_cache: HashMap<String, Directory>,
    bookmarks: gio::ListStore,
    open_result: OpenResult,
    alias: Option<String>,
    state: ConnectionState,
    /// the start directory of this connection
    default_dir: Option<Directory>,
    uri: Option<glib::Uri>,
    base_file_info: Option<gio::FileInfo>,
    base_path: Option<GnomeCmdPath>,
}

impl ConnectionPrivate {
    fn new(connection: &Connection) -> Self {
        let bookmarks = gio::ListStore::new::<Bookmark>();
        bookmarks.connect_items_changed(glib::clone!(
            #[weak]
            connection,
            move |_, _, _, _| connection.emit_by_name::<()>("updated", &[])
        ));
        Self {
            uuid: glib::uuid_string_random().to_string(),
            history: History::new(20),
            dir_cache: Default::default(),
            bookmarks,
            open_result: OpenResult::NotStarted,
            alias: None,
            state: ConnectionState::Closed,
            default_dir: None,
            uri: None,
            base_file_info: None,
            base_path: None,
        }
    }
}

pub enum OpenResult {
    Ok,
    Failed(Option<glib::Error>, Option<String>),
    Cancelled,
    InProgress,
    NotStarted,
}

impl Connection {
    fn private(&self) -> &mut ConnectionPrivate {
        static QUARK: LazyLock<glib::Quark> =
            LazyLock::new(|| glib::Quark::from_str("connection-private"));

        unsafe {
            if let Some(mut private) = self.qdata::<ConnectionPrivate>(*QUARK) {
                private.as_mut()
            } else {
                self.set_qdata(*QUARK, ConnectionPrivate::new(self));
                self.qdata::<ConnectionPrivate>(*QUARK).unwrap().as_mut()
            }
        }
    }

    pub fn dir_history(&self) -> &History<String> {
        &self.private().history
    }

    pub fn find_mount(&self) -> Option<gio::Mount> {
        if let Some(device) = self.downcast_ref::<ConnectionDevice>() {
            if let Some(mount_path) = device.mountp_string() {
                let file = gio::File::for_path(mount_path);
                file.find_enclosing_mount(gio::Cancellable::NONE).ok()
            } else {
                device.mount()
            }
        } else if let Some(remote) = self.downcast_ref::<ConnectionRemote>() {
            let file = gio::File::for_uri(&remote.uri_string()?);
            file.find_enclosing_mount(gio::Cancellable::NONE).ok()
        } else {
            None
        }
    }

    pub fn add_to_cache(&self, directory: &Directory, uri: &str) {
        debug!('k', "ADDING {:?} {} to the cache", directory, uri);
        self.private()
            .dir_cache
            .insert(uri.to_owned(), directory.clone());
    }

    pub fn remove_from_cache(&self, directory: &Directory) {
        let uri = directory.upcast_ref::<File>().get_uri_str();
        debug!('k', "REMOVING {:?} {} from the cache", directory, uri);
        self.private().dir_cache.remove(&uri);
    }

    pub fn remove_from_cache_by_uri(&self, uri: &str) {
        debug!('k', "REMOVING {} from the cache", uri);
        self.private().dir_cache.remove(uri);
    }

    pub fn cache_lookup(&self, uri: &str) -> Option<Directory> {
        match self.private().dir_cache.get(uri) {
            Some(directory) => {
                debug!(
                    'k',
                    "FOUND {:?} {} in the cache, reusing it!", directory, uri
                );
                Some(directory.clone())
            }
            None => {
                debug!('k', "FAILED to find {} in the cache", uri);
                None
            }
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, strum::FromRepr)]
pub enum ConnectionState {
    Closed,
    Open,
    Opening,
}

pub trait ConnectionExt: IsA<Connection> + 'static {
    fn uuid(&self) -> String {
        self.as_ref().private().uuid.clone()
    }

    fn alias(&self) -> Option<String> {
        self.as_ref().private().alias.clone()
    }

    fn set_alias(&self, alias: Option<&str>) {
        self.as_ref().private().alias = alias.map(ToOwned::to_owned);
    }

    fn state(&self) -> ConnectionState {
        self.as_ref().private().state
    }

    fn set_state(&self, state: ConnectionState) {
        self.as_ref().private().state = state;
    }

    fn uri(&self) -> Option<glib::Uri> {
        self.as_ref().private().uri.clone()
    }

    fn set_uri(&self, uri: Option<&glib::Uri>) {
        self.as_ref().private().uri = uri.map(Clone::clone);
    }

    fn uri_string(&self) -> Option<String> {
        Some(self.uri()?.to_str().to_string())
    }

    fn set_uri_string(&self, uri: Option<&str>) {
        let uri = uri.and_then(|uri| glib::Uri::parse(uri, glib::UriFlags::NONE).ok());
        self.set_uri(uri.as_ref());
    }

    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        unsafe {
            GnomeCmdPath::from_raw(ffi::gnome_cmd_con_create_path(
                self.as_ref().to_glib_none().0,
                path.to_glib_none().0,
            ))
        }
    }

    fn create_gfile(&self, path: Option<&Path>) -> gio::File {
        unsafe {
            from_glib_full(ffi::gnome_cmd_con_create_gfile(
                self.as_ref().to_glib_none().0,
                path.to_glib_none().0,
            ))
        }
    }

    fn default_dir(&self) -> Option<Directory> {
        self.as_ref().private().default_dir.clone()
    }

    fn set_default_dir(&self, dir: Option<&Directory>) {
        self.as_ref().private().default_dir = dir.map(Clone::clone);
    }

    fn base_path(&self) -> Option<&GnomeCmdPath> {
        self.as_ref().private().base_path.as_ref()
    }

    fn set_base_path(&self, path: Option<GnomeCmdPath>) {
        self.as_ref().private().base_path = path;
    }

    fn base_file_info(&self) -> Option<gio::FileInfo> {
        self.as_ref().private().base_file_info.clone()
    }

    fn set_base_file_info(&self, file_info: Option<&gio::FileInfo>) {
        self.as_ref().private().base_file_info = file_info.map(Clone::clone);
    }

    fn is_open(&self) -> bool {
        self.state() == ConnectionState::Open
    }

    fn add_bookmark(&self, bookmark: &Bookmark) {
        self.as_ref().private().bookmarks.append(bookmark)
    }

    fn erase_bookmarks(&self) {
        self.as_ref().private().bookmarks.remove_all()
    }

    fn bookmarks(&self) -> gio::ListModel {
        self.as_ref().private().bookmarks.clone().upcast()
    }

    fn replace_bookmark(&self, old_bookmark: &Bookmark, new_bookmark: Bookmark) {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(old_bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return;
        };
        bookmarks.splice(position, 1, &[new_bookmark]);
    }

    fn move_bookmark_up(&self, bookmark: &Bookmark) -> Option<u32> {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return None;
        };
        if position > 0 {
            bookmarks.remove(position);
            bookmarks.insert(position - 1, bookmark);
            Some(position - 1)
        } else {
            None
        }
    }

    fn move_bookmark_down(&self, bookmark: &Bookmark) -> Option<u32> {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return None;
        };
        if position + 1 < bookmarks.n_items() {
            bookmarks.remove(position);
            bookmarks.insert(position + 1, bookmark);
            Some(position + 1)
        } else {
            None
        }
    }

    fn remove_bookmark(&self, bookmark: &Bookmark) {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return;
        };
        bookmarks.remove(position);
    }

    /// Get the type of the file at the specified path.
    fn path_target_type(&self, path: &Path) -> Option<gio::FileType> {
        let path = self.create_path(path).path();
        let file = self.create_gfile(Some(&path));
        if file.query_exists(gio::Cancellable::NONE) {
            let file_type =
                file.query_file_type(gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE);
            Some(file_type)
        } else {
            None
        }
    }

    fn mkdir(&self, path: &Path) -> Result<(), glib::Error> {
        let path = self.create_path(path).path();
        let file = self.create_gfile(Some(&path));
        file.make_directory(gio::Cancellable::NONE)
            .map_err(|error| {
                eprintln!("g_file_make_directory error: {error}");
                error
            })
    }

    fn connect_updated<F: Fn() + 'static>(&self, f: F) -> glib::SignalHandlerId {
        self.connect_local("updated", false, move |_| {
            (f)();
            None
        })
    }

    async fn open(
        &self,
        parent_window: &gtk::Window,
        cancellable: Option<&gio::Cancellable>,
    ) -> Result<(), ErrorMessage> {
        if self.is_open() {
            return Ok(());
        }

        debug!('m', "Opening connection");
        self.set_state(ConnectionState::Opening);
        self.set_open_state(OpenResult::InProgress);

        unsafe {
            ffi::gnome_cmd_con_open(
                self.as_ref().to_glib_none().0,
                parent_window.to_glib_none().0,
                cancellable.to_glib_none().0,
            )
        };
        let timeout = GeneralOptions::new().gui_update_rate().as_millis() as u64;
        loop {
            sleep(timeout).await;
            let open_result = std::mem::replace(
                &mut self.as_ref().private().open_result,
                OpenResult::InProgress,
            );
            match open_result {
                OpenResult::Ok => {
                    debug!('m', "OPEN_OK detected");
                    let dir = Directory::new_with_con(self.as_ref());
                    self.as_ref().set_default_dir(dir.as_ref());
                    self.set_state(ConnectionState::Open);
                    break Ok(());
                }
                OpenResult::Failed(error, message) => {
                    debug!('m', "OPEN_FAILED detected");
                    self.set_state(ConnectionState::Closed);
                    break Err(ErrorMessage {
                        message: message.unwrap_or_else(|| gettext("Failed to open a connection.")),
                        secondary_text: error.map(|e| e.message().to_owned()),
                    });
                }
                OpenResult::InProgress => {}
                OpenResult::Cancelled | OpenResult::NotStarted => {
                    self.set_state(ConnectionState::Closed);
                    break Ok(());
                }
            }
        }
    }

    fn close(&self, parent_window: Option<&gtk::Window>) {
        if self.as_ref().is_closeable() && self.is_open() {
            unsafe {
                ffi::gnome_cmd_con_close(
                    self.as_ref().to_glib_none().0,
                    parent_window.to_glib_none().0,
                )
            }
        }
    }

    fn set_open_state(&self, open_result: OpenResult) {
        self.as_ref().private().open_result = open_result;
    }
}

impl<O: IsA<Connection>> ConnectionExt for O {}

impl Deref for Connection {
    type Target = dyn ConnectionInterface;

    fn deref(&self) -> &Self::Target {
        if let Some(home) = self.downcast_ref::<ConnectionHome>() {
            home
        } else if let Some(device) = self.downcast_ref::<ConnectionDevice>() {
            device
        } else if let Some(smb) = self.downcast_ref::<ConnectionSmb>() {
            smb
        } else if let Some(remote) = self.downcast_ref::<ConnectionRemote>() {
            remote
        } else {
            unreachable!()
        }
    }
}

pub trait ConnectionInterface {
    fn is_local(&self) -> bool;

    fn open_is_needed(&self) -> bool;

    fn is_closeable(&self) -> bool;

    fn should_remember_dir(&self) -> bool {
        true
    }

    fn needs_open_visprog(&self) -> bool {
        false
    }

    /// Defines if a graphical progress bar should be drawn when opening a folder
    fn needs_list_visprog(&self) -> bool {
        false
    }

    fn can_show_free_space(&self) -> bool {
        false
    }

    fn open_message(&self) -> Option<String> {
        None
    }

    fn go_text(&self) -> Option<String> {
        None
    }

    fn open_text(&self) -> Option<String> {
        None
    }

    fn close_text(&self) -> Option<String> {
        None
    }

    fn go_tooltip(&self) -> Option<String> {
        None
    }

    fn open_tooltip(&self) -> Option<String> {
        None
    }

    fn close_tooltip(&self) -> Option<String> {
        None
    }

    fn go_icon(&self) -> Option<gio::Icon> {
        self.open_icon()
    }

    fn open_icon(&self) -> Option<gio::Icon>;

    fn close_icon(&self) -> Option<gio::Icon> {
        let icon = self.open_icon()?;
        let unmount = gio::ThemedIcon::new("overlay_umount");
        let emblem = gio::Emblem::new(&unmount);
        Some(gio::EmblemedIcon::new(&icon, Some(&emblem)).upcast())
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_is_local(con: *mut ffi::GnomeCmdCon) -> gboolean {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.is_local().into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_is_open(con: *mut ffi::GnomeCmdCon) -> gboolean {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.is_open().into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_open_state(
    con: *mut ffi::GnomeCmdCon,
    result: i32,
    error: *mut GError,
    msg: *const c_char,
) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.private().open_result = match result {
        0 => OpenResult::Ok,
        1 => OpenResult::Failed(unsafe { from_glib_none(error) }, unsafe {
            from_glib_none(msg)
        }),
        2 => OpenResult::Cancelled,
        3 => OpenResult::InProgress,
        _ => OpenResult::NotStarted,
    };
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_alias(con: *mut ffi::GnomeCmdCon, alias: *const c_char) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let alias: Option<String> = unsafe { from_glib_none(alias) };
    con.set_alias(alias.as_deref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_state(con: *mut ffi::GnomeCmdCon, state: i32) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.set_state(ConnectionState::from_repr(state as usize).unwrap());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_get_default_dir(
    con: *const ffi::GnomeCmdCon,
) -> *const GnomeCmdDir {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.default_dir().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_default_dir(
    con: *const ffi::GnomeCmdCon,
    dir: *mut GnomeCmdDir,
) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let dir: Option<Directory> = unsafe { from_glib_none(dir) };
    con.set_default_dir(dir.as_ref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_get_uri(con: *const ffi::GnomeCmdCon) -> *const GUri {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.uri().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_uri(con: *const ffi::GnomeCmdCon, uri: *const GUri) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let uri: Borrowed<Option<glib::Uri>> = unsafe { from_glib_borrow(uri) };
    con.set_uri((*uri).as_ref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_get_uri_string(con: *const ffi::GnomeCmdCon) -> *mut c_char {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.uri_string().to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_uri_string(con: *const ffi::GnomeCmdCon, uri: *const c_char) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let uri: Option<String> = unsafe { from_glib_none(uri) };
    con.set_uri_string(uri.as_deref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_get_base_file_info(con: *mut ffi::GnomeCmdCon) -> *mut GFileInfo {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    con.base_file_info().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_base_file_info(
    con: *mut ffi::GnomeCmdCon,
    file_info: *mut GFileInfo,
) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let file_info: Option<gio::FileInfo> = unsafe { from_glib_none(file_info) };
    con.set_base_file_info(file_info.as_ref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_get_base_path(con: *const ffi::GnomeCmdCon) -> *const GnomeCmdPath {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    match con.base_path() {
        Some(p) => std::ptr::from_ref(p),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_set_base_path(
    con: *const ffi::GnomeCmdCon,
    path: *mut GnomeCmdPath,
) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let path = if path.is_null() {
        None
    } else {
        Some(unsafe { GnomeCmdPath::from_raw(path) })
    };
    con.set_base_path(path);
}
