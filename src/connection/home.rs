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
    glib::{self, prelude::*},
};
use std::{future::Future, path::Path, pin::Pin};

pub mod ffi {
    use crate::connection::connection::ffi::GnomeCmdConClass;
    use gtk::glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdConHome {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_home_get_type() -> GType;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConHomeClass {
        pub parent_class: GnomeCmdConClass,
    }
}

glib::wrapper! {
    pub struct ConnectionHome(Object<ffi::GnomeCmdConHome, ffi::GnomeCmdConHomeClass>)
        @extends Connection;

    match fn {
        type_ => || ffi::gnome_cmd_con_home_get_type(),
    }
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
