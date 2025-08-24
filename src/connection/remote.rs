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

use super::connection::{Connection, ConnectionExt, ConnectionInterface};
use gettextrs::gettext;
use glib::object::Cast;
use gtk::{
    gio,
    glib::{
        self,
        translate::{from_glib_none, ToGlibPtr},
    },
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
        ConnectionMethodID::from_uri(&uri)
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

impl ConnectionInterface for ConnectionRemote {
    fn is_local(&self) -> bool {
        false
    }

    fn open_is_needed(&self) -> bool {
        true
    }

    fn is_closeable(&self) -> bool {
        true
    }

    fn needs_open_visprog(&self) -> bool {
        true
    }

    fn needs_list_visprog(&self) -> bool {
        true
    }

    fn open_message(&self) -> Option<String> {
        let host = self.uri().and_then(|u| u.host());
        Some(
            gettext("Connecting to {hostname}")
                .replace("{hostname}", host.as_deref().unwrap_or("<?>")),
        )
    }

    fn go_text(&self) -> Option<String> {
        Some(gettext("Go to: {connection}").replace("{connection}", &self.alias()?))
    }

    fn open_text(&self) -> Option<String> {
        Some(gettext("Connect to: {connection}").replace("{connection}", &self.alias()?))
    }

    fn close_text(&self) -> Option<String> {
        Some(gettext("Disconnect from: {connection}").replace("{connection}", &self.alias()?))
    }

    fn open_tooltip(&self) -> Option<String> {
        let host = self.uri()?.host()?;
        Some(gettext("Opens remote connection to %s").replace("%s", &host))
    }

    fn close_tooltip(&self) -> Option<String> {
        let host = self.uri()?.host()?;
        Some(gettext("Closes remote connection to %s").replace("%s", &host))
    }

    fn open_icon(&self) -> Option<gio::Icon> {
        Some(gio::ThemedIcon::new(&self.icon_name()).upcast())
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

impl ConnectionMethodID {
    pub fn from_uri(uri: &glib::Uri) -> Option<ConnectionMethodID> {
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
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_remote_get_icon_name(
    con_ptr: *mut ffi::GnomeCmdConRemote,
) -> *mut c_char {
    let con: ConnectionRemote = unsafe { from_glib_none(con_ptr) };
    let icon_name = con.icon_name();
    icon_name.to_glib_full()
}
