// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    bookmark::Bookmark, device::ConnectionDevice, dir_cache::DirCache, home::ConnectionHome,
    remote::ConnectionRemote, smb::ConnectionSmb,
};
use crate::{debug::debug, dir::Directory, utils::ErrorMessage};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{
    cell::{Ref, RefMut},
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
        pub(super) dir_cache: RefCell<DirCache>,
        pub bookmarks: RefCell<Vec<Bookmark>>,
        pub alias: RefCell<Option<String>>,
        pub state: Cell<ConnectionState>,
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
                base_path: Default::default(),
            }
        }
    }

    impl ObjectImpl for Connection {
        fn constructed(&self) {
            self.parent_constructed();
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
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum ConnectionState {
    Closed,
    Open,
    Opening,
}

pub trait ConnectionExt: IsA<Connection> + 'static {
    fn dir_cache(&self) -> Ref<'_, DirCache> {
        self.as_ref().imp().dir_cache.borrow()
    }

    fn dir_cache_mut(&self) -> RefMut<'_, DirCache> {
        self.as_ref().imp().dir_cache.borrow_mut()
    }

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
        if state == ConnectionState::Closed {
            self.dir_cache_mut().invalidate_all();
        }
        self.emit_by_name::<()>("updated", &[]);
    }

    fn default_dir(&self) -> Directory {
        Directory::new(
            self,
            &self
                .as_ref()
                .create_uri(&self.base_path().unwrap_or(PathBuf::from("/"))),
        )
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

    /// Moves bookmarks from old connection instance to the new one.
    fn bookmarks_from(&self, other: impl AsRef<Connection>) {
        self.as_ref()
            .imp()
            .bookmarks
            .replace(other.as_ref().imp().bookmarks.take());
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
