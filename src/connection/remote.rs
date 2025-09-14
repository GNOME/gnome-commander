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
use crate::{debug::debug, path::GnomeCmdPath, utils::ErrorMessage};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{
    future::Future,
    path::{Path, PathBuf},
    pin::Pin,
};

mod imp {
    use super::*;
    use crate::connection::connection::ConnectionImpl;
    use std::cell::RefCell;

    #[derive(Default)]
    pub struct ConnectionRemote {
        pub uri: RefCell<Option<glib::Uri>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectionRemote {
        const NAME: &'static str = "GnomeCmdConRemote";
        type Type = super::ConnectionRemote;
        type ParentType = Connection;
    }

    impl ObjectImpl for ConnectionRemote {}
    impl ConnectionImpl for ConnectionRemote {}
}

glib::wrapper! {
    pub struct ConnectionRemote(ObjectSubclass<imp::ConnectionRemote>)
        @extends Connection;
}

pub trait ConnectionRemoteExt: IsA<ConnectionRemote> + 'static {
    fn uri(&self) -> Option<glib::Uri> {
        self.as_ref().imp().uri.borrow().clone()
    }

    fn set_uri(&self, uri: Option<&glib::Uri>) {
        self.as_ref().imp().uri.replace(uri.map(Clone::clone));
    }

    fn uri_string(&self) -> Option<String> {
        Some(self.uri()?.to_str().to_string())
    }
}

impl<O: IsA<ConnectionRemote>> ConnectionRemoteExt for O {}

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
    fn open_impl(
        &self,
        window: gtk::Window,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async move {
            debug!('m', "Opening remote connection");

            let Some(_uri) = self.uri() else {
                return Ok(());
            };

            if self.base_path().is_none() {
                self.set_base_path(Some(GnomeCmdPath::Plain(PathBuf::from("/"))));
            }

            let file = self.create_gfile(&GnomeCmdPath::Plain(PathBuf::from("/")));
            debug!('m', "Connecting to {}", file.uri());

            let mount_operation = gtk::MountOperation::new(Some(&window));

            let result = file
                .mount_enclosing_volume_future(gio::MountMountFlags::NONE, Some(&mount_operation))
                .await
                .or_else(|error| {
                    if error.matches(gio::IOErrorEnum::AlreadyMounted) {
                        Ok(())
                    } else {
                        Err(error)
                    }
                });

            match result {
                Ok(()) => {}
                Err(error) => {
                    debug!('m', "Unable to mount enclosing volume: {}", error);
                    self.set_base_file_info(None);
                    return Err(ErrorMessage::with_error(
                        gettext("Cannot connect to a remote location"),
                        &error,
                    ));
                }
            }

            let base_file = self.create_gfile(&GnomeCmdPath::Plain(PathBuf::from("/")));
            match base_file
                .query_info_future("*", gio::FileQueryInfoFlags::NONE, glib::Priority::DEFAULT)
                .await
            {
                Ok(base_file_info) => {
                    self.set_base_file_info(Some(&base_file_info));
                }
                Err(error) => {
                    self.set_base_file_info(None);
                    return Err(ErrorMessage::with_error(
                        gettext("Cannot query remote location information"),
                        &error,
                    ));
                }
            }

            Ok(())
        })
    }

    fn close_impl(
        &self,
        _window: Option<gtk::Window>,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async {
            self.set_default_dir(None);
            self.set_base_path(None);

            let Some(uri) = self.uri_string() else {
                return Ok(());
            };
            debug!('m', "Closing connection to {}", uri);

            let file = gio::File::for_uri(&uri);

            let mount = file
                .find_enclosing_mount(gio::Cancellable::NONE)
                .map_err(|error| {
                    ErrorMessage::with_error(gettext("Cannot find an enclosing mount"), &error)
                })?;

            mount
                .unmount_with_operation_future(
                    gio::MountUnmountFlags::NONE,
                    gio::MountOperation::NONE,
                )
                .await
                .or_else(|error| {
                    if error.matches(gio::IOErrorEnum::Closed) {
                        Ok(())
                    } else {
                        Err(error)
                    }
                })
                .map_err(|error| ErrorMessage::with_error(gettext("Disconnect error"), &error))?;

            Ok(())
        })
    }

    fn create_gfile(&self, path: &GnomeCmdPath) -> gio::File {
        let connection_uri = self.uri().unwrap();
        let uri = glib::Uri::build(
            glib::UriFlags::NONE,
            &connection_uri.scheme(),
            connection_uri.userinfo().as_deref(),
            connection_uri.host().as_deref(),
            connection_uri.port(),
            path.path().to_str().unwrap_or_default(),
            None,
            None,
        );
        gio::File::for_uri(&uri.to_str())
    }

    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        GnomeCmdPath::Plain(path.to_owned())
    }

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
