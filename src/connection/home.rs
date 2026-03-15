// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{Connection, ConnectionInterface};
use crate::utils::ErrorMessage;
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{self, prelude::*, subclass::prelude::*},
};
use std::{future::Future, path::Path, pin::Pin};

mod imp {
    use super::*;
    use crate::{
        connection::{ConnectionExt, ConnectionImpl, ConnectionState},
        dir::Directory,
    };

    #[derive(Default)]
    pub struct ConnectionHome {}

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectionHome {
        const NAME: &'static str = "GnomeCmdConHome";
        type Type = super::ConnectionHome;
        type ParentType = Connection;
    }

    impl ObjectImpl for ConnectionHome {
        fn constructed(&self) {
            self.parent_constructed();
            let home = self.obj();

            home.set_state(ConnectionState::Open);
            home.set_alias(Some(&gettext("Home")));

            let dir = Directory::new(&*home, &home.create_uri(&glib::home_dir()));
            home.set_default_dir(Some(&dir));
        }
    }

    impl ConnectionImpl for ConnectionHome {}
}

glib::wrapper! {
    pub struct ConnectionHome(ObjectSubclass<imp::ConnectionHome>)
        @extends Connection;
}

impl Default for ConnectionHome {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ConnectionInterface for ConnectionHome {
    fn open_impl(
        &self,
        _window: gtk::Window,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async { Ok(()) })
    }

    fn close_impl(
        &self,
        _window: Option<gtk::Window>,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async { Ok(()) })
    }

    fn create_uri(&self, path: &Path) -> String {
        glib::filename_to_uri(path, None)
            .map(|uri| uri.into())
            .unwrap_or_else(|_| "file:///".to_string())
    }

    fn is_local(&self) -> bool {
        true
    }

    fn open_is_needed(&self) -> bool {
        false
    }

    fn is_closeable(&self) -> bool {
        false
    }

    fn can_show_free_space(&self) -> bool {
        true
    }

    fn go_text(&self) -> Option<String> {
        Some(gettext("Go to: Home"))
    }

    fn open_icon(&self) -> Option<gio::Icon> {
        Some(gio::ThemedIcon::new("user-home").upcast())
    }
}
