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

use super::connection::{Connection, ConnectionExt};
use crate::{dir::Directory, path::GnomeCmdPath};
use ffi::GnomeCmdConRemote;
use gtk::{
    gio,
    glib::{
        self,
        translate::{from_glib_none, ToGlibPtr},
        Cast,
    },
};
use std::{ffi::c_char, path::Path};

pub mod ffi {
    use crate::connection::connection::ffi::GnomeCmdConClass;
    use glib::ffi::GType;
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdConRemote {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_remote_get_type() -> GType;
        pub fn gnome_cmd_con_remote_new(
            alias: *const c_char,
            uri: *const c_char,
        ) -> *mut GnomeCmdConRemote;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConRemoteClass {
        pub parent_class: GnomeCmdConClass,
    }
}

glib::wrapper! {
    pub struct ConnectionRemote(Object<ffi::GnomeCmdConRemote, ffi::GnomeCmdConRemoteClass>)
        @extends Connection;

    match fn {
        type_ => || ffi::gnome_cmd_con_remote_get_type(),
    }
}

impl Default for ConnectionRemote {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ConnectionRemote {
    pub fn new(alias: &str, uri: &str) -> Self {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_remote_new(
                alias.to_glib_none().0,
                uri.to_glib_none().0,
            ))
        }
    }

    pub fn method(&self) -> Option<ConnectionMethodID> {
        let uri = self.uri()?;
        match uri.scheme().as_str() {
            "file" => Some(ConnectionMethodID::CON_FILE),
            "ftp" if uri.user().as_deref() == Some("anonymous") => {
                Some(ConnectionMethodID::CON_ANON_FTP)
            }
            "ftp" => Some(ConnectionMethodID::CON_FTP),
            "sftp" => Some(ConnectionMethodID::CON_SFTP),
            "dav" => Some(ConnectionMethodID::CON_DAV),
            "davs" => Some(ConnectionMethodID::CON_DAVS),
            "smb" => Some(ConnectionMethodID::CON_SMB),
            _ => None,
        }
    }

    pub fn icon_name(&self) -> &'static str {
        match self.method() {
            Some(ConnectionMethodID::CON_FILE) => "folder",
            Some(ConnectionMethodID::CON_URI) => "network-workgroup",
            Some(_) => "folder-remote",
            _ => "network-workgroup",
        }
    }
}

#[derive(Clone, Copy, strum::FromRepr, PartialEq, PartialOrd, Eq, Ord)]
#[repr(i32)]
#[allow(non_camel_case_types)]
pub enum ConnectionMethodID {
    CON_SFTP = 0,
    CON_FTP,
    CON_ANON_FTP,
    CON_SMB,
    CON_DAV,
    CON_DAVS,
    CON_URI,
    CON_FILE,
}

impl ConnectionExt for ConnectionRemote {
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
    fn add_bookmark(&self, name: &str, path: &str) {
        self.upcast_ref::<Connection>().add_bookmark(name, path)
    }
    fn path_target_type(&self, path: &Path) -> Option<gio::FileType> {
        self.upcast_ref::<Connection>().path_target_type(path)
    }
    fn mkdir(&self, path: &Path) -> Result<(), glib::Error> {
        self.upcast_ref::<Connection>().mkdir(path)
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_remote_get_icon_name(
    con_ptr: *mut GnomeCmdConRemote,
) -> *mut c_char {
    let con: ConnectionRemote = unsafe { from_glib_none(con_ptr) };
    let icon_name = con.icon_name();
    icon_name.to_glib_full()
}
