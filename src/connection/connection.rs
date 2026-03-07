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
    bookmark::Bookmark, device::ConnectionDevice, home::ConnectionHome, remote::ConnectionRemote,
    smb::ConnectionSmb,
};
use crate::{debug::debug, dir::Directory, file::FileOps, utils::ErrorMessage};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{
    cell::Ref,
    collections::HashMap,
    future::Future,
    ops::Deref,
    path::{Path, PathBuf},
    pin::Pin,
};

mod imp {
    use super::*;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    pub struct Connection {
        pub uuid: String,
        pub dir_cache: RefCell<HashMap<String, Directory>>,
        pub bookmarks: RefCell<Vec<Bookmark>>,
        pub alias: RefCell<Option<String>>,
        pub state: Cell<ConnectionState>,
        /// the start directory of this connection
        pub default_dir: RefCell<Option<Directory>>,
        pub base_path: RefCell<Option<PathBuf>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Connection {
        const NAME: &'static str = "GnomeCmdCon";
        type Type = super::Connection;

        fn new() -> Self {
            Self {
                uuid: glib::uuid_string_random().to_string(),
                dir_cache: Default::default(),
                bookmarks: RefCell::new(Vec::new()),
                alias: Default::default(),
                state: Cell::new(ConnectionState::Closed),
                default_dir: Default::default(),
                base_path: Default::default(),
            }
        }
    }

    impl ObjectImpl for Connection {
        fn constructed(&self) {
            self.parent_constructed();
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
    fn emit_updated(&self) {
        self.emit_by_name::<()>("updated", &[]);
    }

    pub fn add_to_cache(&self, directory: &Directory, uri: &str) {
        debug!('k', "ADDING {:?} {} to the cache", directory, uri);
        self.imp()
            .dir_cache
            .borrow_mut()
            .insert(uri.to_owned(), directory.clone());
    }

    pub fn remove_from_cache(&self, directory: &Directory) {
        let uri = directory.uri();
        debug!('k', "REMOVING {:?} {} from the cache", directory, uri);
        self.imp().dir_cache.borrow_mut().remove(&uri);
    }

    pub fn remove_from_cache_by_uri(&self, uri: &str) {
        let removed: Vec<String> = self
            .imp()
            .dir_cache
            .borrow_mut()
            .extract_if(|u, _| u.starts_with(uri))
            .map(|(uri, _dir)| uri)
            .collect();
        debug!(
            'k',
            "REMOVING {} from the cache together with {:?}", uri, removed
        );
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
        self.emit_by_name::<()>("updated", &[]);
    }

    fn default_dir(&self) -> Option<Directory> {
        self.as_ref().imp().default_dir.borrow().clone()
    }

    fn set_default_dir(&self, dir: Option<&Directory>) {
        self.as_ref().imp().default_dir.replace(dir.cloned());
    }

    fn base_path(&self) -> Option<PathBuf> {
        self.as_ref().imp().base_path.borrow().clone()
    }

    fn set_base_path(&self, path: Option<PathBuf>) {
        self.as_ref().imp().base_path.replace(path);
    }

    fn is_open(&self) -> bool {
        self.state() == ConnectionState::Open
    }

    fn add_bookmark(&self, bookmark: Bookmark) {
        self.as_ref().imp().bookmarks.borrow_mut().push(bookmark);
        self.as_ref().emit_updated();
    }

    fn erase_bookmarks(&self) {
        self.as_ref().imp().bookmarks.borrow_mut().clear();
        self.as_ref().emit_updated();
    }

    fn bookmarks(&self) -> Ref<'_, Vec<Bookmark>> {
        self.as_ref().imp().bookmarks.borrow()
    }

    fn replace_bookmark(&self, old_bookmark: &Bookmark, new_bookmark: Bookmark) -> bool {
        {
            let mut bookmarks = self.as_ref().imp().bookmarks.borrow_mut();
            if let Some(position) = bookmarks.iter().position(|b| b == old_bookmark) {
                bookmarks.splice(position..=position, [new_bookmark]);
            } else {
                return false;
            }
        }

        // Important: trigger signal outside the block with our mutable bookmarks reference. Signal
        // handlers might want to borrow bookmarks, causing panic if it is already mutably borrowed.
        self.as_ref().emit_updated();
        true
    }

    fn move_bookmark_up(&self, bookmark: &Bookmark) -> bool {
        {
            let mut bookmarks = self.as_ref().imp().bookmarks.borrow_mut();
            if let Some(position) = bookmarks.iter().position(|b| b == bookmark)
                && position > 0
            {
                let bookmark = bookmarks.remove(position);
                bookmarks.insert(position - 1, bookmark);
            } else {
                return false;
            }
        }

        self.as_ref().emit_updated();
        true
    }

    fn move_bookmark_down(&self, bookmark: &Bookmark) -> bool {
        {
            let mut bookmarks = self.as_ref().imp().bookmarks.borrow_mut();
            if let Some(position) = bookmarks.iter().position(|b| b == bookmark)
                && position + 1 < bookmarks.len()
            {
                let bookmark = bookmarks.remove(position);
                bookmarks.insert(position + 1, bookmark);
            } else {
                return false;
            }
        }

        self.as_ref().emit_updated();
        true
    }

    fn remove_bookmark(&self, bookmark: &Bookmark) -> bool {
        {
            let mut bookmarks = self.as_ref().imp().bookmarks.borrow_mut();
            if let Some(position) = bookmarks.iter().position(|b| b == bookmark) {
                bookmarks.remove(position);
            } else {
                return false;
            }
        }

        self.as_ref().emit_updated();
        true
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
                let dir = self
                    .base_path()
                    .map(|base_path| Directory::new(self, &self.as_ref().create_uri(&base_path)));
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
                    Some(window) => error_message.show(window).await,
                    None => eprintln!("{error_message}"),
                }
            }
            self.set_state(ConnectionState::Closed);
            self.emit_by_name::<()>("close", &[]);
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

    fn create_uri(&self, path: &Path) -> String;

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
}
