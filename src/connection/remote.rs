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
use gettextrs::gettext;
use gtk::glib::{
    self,
    translate::{from_glib_none, ToGlibPtr},
};
use std::ffi::c_char;

pub mod ffi {
    use crate::connection::connection::ffi::GnomeCmdConClass;
    use glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdConRemote {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_remote_get_type() -> GType;
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

impl ConnectionRemote {
    pub fn new(alias: &str, uri: &glib::Uri) -> Self {
        let con: Self = glib::Object::builder().build();
        con.set_alias(Some(alias));
        con.set_uri(Some(uri));
        con.set_open_message(
            &gettext("Connecting to {hostname}")
                .replace("{hostname}", uri.host().as_deref().unwrap_or("<?>")),
        );
        con
    }

    pub fn try_from_string(alias: &str, uri: &str) -> Result<Self, glib::Error> {
        let uri = glib::Uri::parse(
            uri,
            glib::UriFlags::HAS_PASSWORD | glib::UriFlags::HAS_AUTH_PARAMS,
        )?;
        Ok(Self::new(alias, &uri))
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
#[repr(u32)]
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

#[no_mangle]
pub extern "C" fn gnome_cmd_con_remote_get_icon_name(
    con_ptr: *mut ffi::GnomeCmdConRemote,
) -> *mut c_char {
    let con: ConnectionRemote = unsafe { from_glib_none(con_ptr) };
    let icon_name = con.icon_name();
    icon_name.to_glib_full()
}
