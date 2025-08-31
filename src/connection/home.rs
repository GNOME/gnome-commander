/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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

use super::connection::{Connection, ConnectionInterface};
use crate::{path::GnomeCmdPath, utils::ErrorMessage};
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{self, prelude::*, subclass::prelude::*},
};
use std::{future::Future, path::Path, pin::Pin};

mod imp {
    use super::*;
    use crate::{
        connection::connection::{ConnectionExt, ConnectionImpl, ConnectionState},
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

            let dir = Directory::new(&*home, GnomeCmdPath::Plain(glib::home_dir()));
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

    fn create_gfile(&self, path: &GnomeCmdPath) -> gio::File {
        gio::File::for_path(path.path())
    }

    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        GnomeCmdPath::Plain(path.to_owned())
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
