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
    remote::ConnectionRemote, remote::ConnectionRemoteExt, smb::ConnectionSmb,
};
use crate::{debug::debug, dir::Directory, file::File, path::GnomeCmdPath, utils::ErrorMessage};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{collections::HashMap, future::Future, ops::Deref, path::Path, pin::Pin};

mod imp {
    use super::*;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    pub struct Connection {
        pub uuid: String,
        pub history: History<String>, // TODO: consider Rc<GnomeCmdPath>
        pub dir_cache: RefCell<HashMap<String, Directory>>,
        pub bookmarks: gio::ListStore,
        pub alias: RefCell<Option<String>>,
        pub state: Cell<ConnectionState>,
        /// the start directory of this connection
        pub default_dir: RefCell<Option<Directory>>,
        pub base_file_info: RefCell<Option<gio::FileInfo>>,
        pub base_path: RefCell<Option<GnomeCmdPath>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Connection {
        const NAME: &'static str = "GnomeCmdCon";
        type Type = super::Connection;

        fn new() -> Self {
            Self {
                uuid: glib::uuid_string_random().to_string(),
                history: History::new(20),
                dir_cache: Default::default(),
                bookmarks: gio::ListStore::new::<Bookmark>(),
                alias: Default::default(),
                state: Cell::new(ConnectionState::Closed),
                default_dir: Default::default(),
                base_file_info: Default::default(),
                base_path: Default::default(),
            }
        }
    }

    impl ObjectImpl for Connection {
        fn constructed(&self) {
            self.parent_constructed();

            let connection = self.obj();
            self.bookmarks.connect_items_changed(glib::clone!(
                #[weak]
                connection,
                move |_, _, _, _| connection.emit_by_name::<()>("updated", &[])
            ));
        }

        fn dispose(&self) {
            self.obj().set_default_dir(None);
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("updated").build(),
                    glib::subclass::Signal::builder("close").build(),
                ]
            })
        }
    }
}

pub trait ConnectionImpl: ObjectImpl {}

unsafe impl<T: ConnectionImpl> IsSubclassable<T> for Connection {}

glib::wrapper! {
    pub struct Connection(ObjectSubclass<imp::Connection>);
}

impl Connection {
    pub fn dir_history(&self) -> &History<String> {
        &self.imp().history
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
        self.imp()
            .dir_cache
            .borrow_mut()
            .insert(uri.to_owned(), directory.clone());
    }

    pub fn remove_from_cache(&self, directory: &Directory) {
        let uri = directory.upcast_ref::<File>().get_uri_str();
        debug!('k', "REMOVING {:?} {} from the cache", directory, uri);
        self.imp().dir_cache.borrow_mut().remove(&uri);
    }

    pub fn remove_from_cache_by_uri(&self, uri: &str) {
        debug!('k', "REMOVING {} from the cache", uri);
        self.imp().dir_cache.borrow_mut().remove(uri);
    }

    pub fn cache_lookup(&self, uri: &str) -> Option<Directory> {
        match self.imp().dir_cache.borrow().get(uri) {
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
        self.as_ref().imp().uuid.clone()
    }

    fn alias(&self) -> Option<String> {
        self.as_ref().imp().alias.borrow().to_owned()
    }

    fn set_alias(&self, alias: Option<&str>) {
        self.as_ref()
            .imp()
            .alias
            .replace(alias.map(ToOwned::to_owned));
    }

    fn state(&self) -> ConnectionState {
        self.as_ref().imp().state.get()
    }

    fn set_state(&self, state: ConnectionState) {
        self.as_ref().imp().state.set(state);
    }

    fn default_dir(&self) -> Option<Directory> {
        self.as_ref().imp().default_dir.borrow().clone()
    }

    fn set_default_dir(&self, dir: Option<&Directory>) {
        self.as_ref()
            .imp()
            .default_dir
            .replace(dir.map(Clone::clone));
    }

    fn base_path(&self) -> Option<GnomeCmdPath> {
        self.as_ref().imp().base_path.borrow().clone()
    }

    fn set_base_path(&self, path: Option<GnomeCmdPath>) {
        self.as_ref().imp().base_path.replace(path);
    }

    fn base_file_info(&self) -> Option<gio::FileInfo> {
        self.as_ref().imp().base_file_info.borrow().clone()
    }

    fn set_base_file_info(&self, file_info: Option<&gio::FileInfo>) {
        self.as_ref()
            .imp()
            .base_file_info
            .replace(file_info.map(Clone::clone));
    }

    fn is_open(&self) -> bool {
        self.state() == ConnectionState::Open
    }

    fn add_bookmark(&self, bookmark: &Bookmark) {
        self.as_ref().imp().bookmarks.append(bookmark)
    }

    fn erase_bookmarks(&self) {
        self.as_ref().imp().bookmarks.remove_all()
    }

    fn bookmarks(&self) -> gio::ListModel {
        self.as_ref().imp().bookmarks.clone().upcast()
    }

    fn replace_bookmark(&self, old_bookmark: &Bookmark, new_bookmark: Bookmark) {
        let bookmarks = &self.as_ref().imp().bookmarks;
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
        let bookmarks = &self.as_ref().imp().bookmarks;
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
        let bookmarks = &self.as_ref().imp().bookmarks;
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
        let bookmarks = &self.as_ref().imp().bookmarks;
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return;
        };
        bookmarks.remove(position);
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

        let open_future = self.as_ref().open_impl(parent_window.clone());

        let open_result = if let Some(cancellable) = cancellable {
            gio::CancellableFuture::new(open_future, cancellable.clone()).await
        } else {
            Ok(open_future.await)
        };

        match open_result {
            Ok(Ok(())) => {
                debug!('m', "OPEN_OK detected");
                let dir = Directory::new_with_con(self.as_ref());
                self.as_ref().set_default_dir(dir.as_ref());
                self.set_state(ConnectionState::Open);
                Ok(())
            }
            Ok(Err(error_message)) => {
                debug!('m', "OPEN_FAILED detected");
                self.set_state(ConnectionState::Closed);
                Err(error_message)
            }
            Err(gio::Cancelled) => {
                debug!('m', "OPEN_CANCELLED detected");
                self.set_state(ConnectionState::Closed);
                Ok(())
            }
        }
    }

    async fn close(&self, window: Option<&gtk::Window>) {
        if self.as_ref().is_closeable() && self.is_open() {
            if let Err(error_message) = self.as_ref().close_impl(window.cloned()).await {
                debug!('m', "unmount failed {error_message}");
                match window {
                    Some(window) => error_message.show(&window).await,
                    None => eprintln!("{error_message}"),
                }
            }
            self.emit_by_name::<()>("close", &[]);
            self.emit_by_name::<()>("updated", &[]);
        }
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
    fn open_impl(
        &self,
        window: gtk::Window,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>>;

    fn close_impl(
        &self,
        window: Option<gtk::Window>,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>>;

    fn create_gfile(&self, path: &GnomeCmdPath) -> gio::File;

    fn create_path(&self, path: &Path) -> GnomeCmdPath;

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

    /// Get the type of the file at the specified path.
    fn path_target_type(&self, path: &Path) -> Option<gio::FileType> {
        let path = self.create_path(path);
        let file = self.create_gfile(&path);
        if file.query_exists(gio::Cancellable::NONE) {
            let file_type =
                file.query_file_type(gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE);
            Some(file_type)
        } else {
            None
        }
    }

    fn mkdir(&self, path: &Path) -> Result<(), glib::Error> {
        let path = self.create_path(path);
        let file = self.create_gfile(&path);
        file.make_directory(gio::Cancellable::NONE)
            .map_err(|error| {
                eprintln!("g_file_make_directory error: {error}");
                error
            })
    }
}
