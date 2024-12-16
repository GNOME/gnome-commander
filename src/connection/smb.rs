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

use super::{
    bookmark::Bookmark,
    connection::{Connection, ConnectionExt},
    remote::ConnectionRemote,
};
use crate::{dir::Directory, path::GnomeCmdPath};
use gtk::{gio, glib, prelude::*};
use std::path::Path;

pub mod ffi {
    use crate::connection::remote::ffi::GnomeCmdConRemoteClass;
    use gtk::glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdConSmb {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_smb_get_type() -> GType;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConSmbClass {
        pub parent_class: GnomeCmdConRemoteClass,
    }
}

glib::wrapper! {
    pub struct ConnectionSmb(Object<ffi::GnomeCmdConSmb, ffi::GnomeCmdConSmbClass>)
        @extends Connection, ConnectionRemote;

    match fn {
        type_ => || ffi::gnome_cmd_con_smb_get_type(),
    }
}

impl Default for ConnectionSmb {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ConnectionExt for ConnectionSmb {
    fn alias(&self) -> Option<String> {
        self.upcast_ref::<Connection>().alias()
    }
    fn set_alias(&self, alias: Option<&str>) {
        self.upcast_ref::<Connection>().set_alias(alias)
    }
    fn uri(&self) -> Option<glib::Uri> {
        self.upcast_ref::<Connection>().uri()
    }
    fn set_uri(&self, uri: Option<&glib::Uri>) {
        self.upcast_ref::<Connection>().set_uri(uri)
    }
    fn uri_string(&self) -> Option<String> {
        self.upcast_ref::<Connection>().uri_string()
    }
    fn set_uri_string(&self, uri: Option<&str>) {
        self.upcast_ref::<Connection>().set_uri_string(uri)
    }
    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        self.upcast_ref::<Connection>().create_path(path)
    }
    fn create_gfile(&self, path: Option<&str>) -> gio::File {
        self.upcast_ref::<Connection>().create_gfile(path)
    }
    fn default_dir(&self) -> Option<Directory> {
        self.upcast_ref::<Connection>().default_dir()
    }
    fn set_default_dir(&self, dir: Option<&Directory>) {
        self.upcast_ref::<Connection>().set_default_dir(dir)
    }
    fn set_base_path(&self, path: GnomeCmdPath) {
        self.upcast_ref::<Connection>().set_base_path(path)
    }
    fn is_local(&self) -> bool {
        self.upcast_ref::<Connection>().is_local()
    }
    fn is_open(&self) -> bool {
        self.upcast_ref::<Connection>().is_open()
    }
    fn add_bookmark(&self, bookmark: &Bookmark) {
        self.upcast_ref::<Connection>().add_bookmark(bookmark)
    }
    fn erase_bookmarks(&self) {
        self.upcast_ref::<Connection>().erase_bookmarks()
    }
    fn bookmarks(&self) -> gio::ListModel {
        self.upcast_ref::<Connection>().bookmarks()
    }
    fn path_target_type(&self, path: &Path) -> Option<gio::FileType> {
        self.upcast_ref::<Connection>().path_target_type(path)
    }
    fn mkdir(&self, path: &Path) -> Result<(), glib::Error> {
        self.upcast_ref::<Connection>().mkdir(path)
    }
    fn close(&self) -> bool {
        self.upcast_ref::<Connection>().close()
    }
}
