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
    bookmark::Bookmark,
    connection::{Connection, ConnectionExt},
    device::ConnectionDevice,
    home::ConnectionHome,
    remote::{ConnectionRemote, ConnectionRemoteExt},
    smb::ConnectionSmb,
};
use crate::{debug::debug, options::options::GeneralOptions, options::types::WriteResult};
use gtk::{
    gio,
    glib::{self, subclass::prelude::*},
    prelude::*,
};
use std::{path::Path, sync::OnceLock};

mod imp {
    use super::*;
    use crate::connection::{home::ConnectionHome, smb::ConnectionSmb};
    use std::{
        cell::{Cell, OnceCell, RefCell},
        collections::HashMap,
        sync::OnceLock,
    };

    pub struct ConnectionList {
        pub changed: Cell<bool>,
        pub locked: Cell<bool>,
        pub home: ConnectionHome,
        pub smb: OnceCell<ConnectionSmb>,
        pub connections: gio::ListStore,
        pub connection_handlers: RefCell<HashMap<Connection, glib::SignalHandlerId>>,
        pub volume_monitor: gio::VolumeMonitor,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectionList {
        const NAME: &'static str = "GnomeCmdConnectionList";
        type Type = super::ConnectionList;

        fn new() -> Self {
            Self {
                changed: Default::default(),
                locked: Default::default(),
                home: Default::default(),
                smb: Default::default(),
                connections: gio::ListStore::new::<Connection>(),
                connection_handlers: Default::default(),
                volume_monitor: gio::VolumeMonitor::get(),
            }
        }
    }

    impl ObjectImpl for ConnectionList {
        fn constructed(&self) {
            self.parent_constructed();

            self.connections.append(&self.home);

            self.connections.connect_items_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _, _, _| imp.obj().emit_changed()
            ));
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| vec![glib::subclass::Signal::builder("list-changed").build()])
        }
    }
}

glib::wrapper! {
    pub struct ConnectionList(ObjectSubclass<imp::ConnectionList>);
}

static LIST: OnceLock<glib::thread_guard::ThreadGuard<ConnectionList>> = OnceLock::new();

impl ConnectionList {
    pub fn create(show_samba_workgroups_button: bool) {
        LIST.get_or_init(|| {
            glib::thread_guard::ThreadGuard::new(ConnectionList::new(show_samba_workgroups_button))
        });
    }

    pub fn get() -> &'static Self {
        LIST.get()
            .expect("Connection list isn't created yet")
            .get_ref()
    }

    pub fn new(show_samba_workgroups_button: bool) -> Self {
        let this: Self = glib::Object::builder().build();
        if show_samba_workgroups_button {
            let smb = ConnectionSmb::default();
            this.imp().connections.append(&smb);
            this.imp().smb.set(smb).unwrap();
        }
        this
    }

    pub fn all(&self) -> gio::ListModel {
        self.imp().connections.clone().upcast()
    }

    fn iter(&self) -> impl Iterator<Item = Connection> + '_ {
        self.imp().connections.iter::<Connection>().flatten()
    }

    fn lock(&self) {
        self.imp().locked.set(true);
        self.imp().changed.set(false);
    }

    fn unlock(&self) {
        if self.imp().changed.get() {
            self.emit_changed();
        }
        self.imp().locked.set(false);
    }

    fn emit_changed(&self) {
        self.emit_by_name::<()>("list-changed", &[]);
    }

    fn mark_changed(&self) {
        if self.imp().locked.get() {
            self.imp().changed.set(true);
        } else {
            self.emit_by_name::<()>("list-changed", &[]);
        }
    }

    pub fn add(&self, connection: &impl IsA<Connection>) {
        let connection = connection.as_ref();
        self.imp().connections.append(connection);
        let handler_id = connection.connect_updated(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move || this.mark_changed()
        ));
        self.imp()
            .connection_handlers
            .borrow_mut()
            .insert(connection.clone(), handler_id);
        self.mark_changed();
    }

    pub fn remove(&self, connection: &impl IsA<Connection>) {
        let store = &self.imp().connections;
        let connection = connection.as_ref();
        if let Some(position) = store.find(connection) {
            store.remove(position);
        }
        if let Some(handler_id) = self
            .imp()
            .connection_handlers
            .borrow_mut()
            .remove(connection)
        {
            connection.disconnect(handler_id);
        }
        self.mark_changed();
    }

    pub fn replace(&self, old: &impl IsA<Connection>, new: &impl IsA<Connection>) {
        let store = &self.imp().connections;
        if let Some(position) = store.find(old.as_ref()) {
            store.splice(position, 1, &[new.as_ref().clone()]);
        } else {
            store.append(new.as_ref());
        }
    }

    #[deprecated(note = "This is a hack. Prefer to use immutable objects instead.")]
    pub fn refresh(&self, connection: &impl IsA<Connection>) {
        let store = &self.imp().connections;
        let connection = connection.as_ref();
        if let Some(position) = store.find(connection) {
            store.remove(position);
            store.insert(position, connection);
        }
    }

    pub fn find_by_uuid(&self, uuid: &str) -> Option<Connection> {
        self.iter().find(|c| c.uuid() == uuid)
    }

    pub fn find_by_alias(&self, alias: &str) -> Option<Connection> {
        self.iter().find(|c| c.alias().as_deref() == Some(alias))
    }

    pub fn home(&self) -> ConnectionHome {
        self.imp().home.clone()
    }

    pub fn smb(&self) -> Option<ConnectionSmb> {
        self.imp().smb.get().cloned()
    }

    pub fn get_remote_con_for_file(&self, file: &gio::File) -> Option<ConnectionRemote> {
        let file_uri = file.uri();
        self.iter()
            .filter_map(|c| c.downcast::<ConnectionRemote>().ok())
            .find(|c| {
                if let Some(uri) = c.uri_string() {
                    file_uri.starts_with(&uri)
                } else {
                    false
                }
            })
    }

    pub fn load(&self, options: &GeneralOptions) {
        self.lock();

        self.load_devices(&options.device_list.get());
        self.load_connections(&options.connections.get());
        self.load_bookmarks(&options.bookmarks.get());

        let home = self.home();
        let dir_history = home.upcast_ref::<Connection>().dir_history();
        for item in options.directory_history.get().into_iter().rev() {
            dir_history.add(item);
        }

        self.unlock();
    }

    pub fn save(&self, options: &GeneralOptions) -> WriteResult {
        options.device_list.set(self.save_devices())?;
        options
            .directory_history
            .set(if options.save_directory_history_on_exit.get() {
                self.home()
                    .upcast_ref::<Connection>()
                    .dir_history()
                    .export()
            } else {
                Vec::new()
            })?;
        options.connections.set(self.save_connections())?;
        options.bookmarks.set(self.save_bookmarks())?;
        Ok(())
    }

    pub fn load_bookmarks(&self, bookmarks: &[BookmarkVariant]) {
        for con in self.iter() {
            con.erase_bookmarks();
        }

        for bookmark in bookmarks {
            let con = if bookmark.is_remote {
                self.find_by_alias(&bookmark.group_name)
            } else if bookmark.group_name == "Home" {
                Some(self.home().upcast())
            } else if bookmark.group_name == "SMB" {
                self.smb().and_upcast()
            } else {
                None
            };

            if let Some(con) = con {
                con.add_bookmark(&Bookmark::new(&bookmark.name, &bookmark.path));
            } else {
                eprintln!(
                    "<Bookmarks> unknown connection: '{}' - ignored",
                    bookmark.group_name
                );
            }
        }
    }

    pub fn save_bookmarks(&self) -> Vec<BookmarkVariant> {
        let mut bookmarks = Vec::<BookmarkVariant>::new();

        for bookmark in self.home().bookmarks().iter::<Bookmark>().flatten() {
            bookmarks.push(BookmarkVariant {
                is_remote: false,
                group_name: "Home".to_owned(),
                name: bookmark.name(),
                path: bookmark.path(),
            });
        }

        if let Some(smb) = self.smb() {
            for bookmark in smb.bookmarks().iter::<Bookmark>().flatten() {
                bookmarks.push(BookmarkVariant {
                    is_remote: false,
                    group_name: "SMB".to_owned(),
                    name: bookmark.name(),
                    path: bookmark.path(),
                });
            }
        }

        for con in self
            .iter()
            .filter_map(|c| c.downcast::<ConnectionRemote>().ok())
        {
            let alias = con.alias().unwrap_or_default();
            for bookmark in con.bookmarks().iter::<Bookmark>().flatten() {
                bookmarks.push(BookmarkVariant {
                    is_remote: true,
                    group_name: alias.clone(),
                    name: bookmark.name(),
                    path: bookmark.path(),
                });
            }
        }

        bookmarks
    }

    pub fn load_devices(&self, variants: &[CustomDeviceVariant]) {
        for v in variants {
            self.add(&ConnectionDevice::new(
                &v.alias,
                &v.device_fn,
                &Path::new(&v.mount_point),
                gio::Icon::deserialize(&v.icon).as_ref(),
            ));
        }
        self.load_available_volumes();
    }

    fn save_devices(&self) -> Vec<CustomDeviceVariant> {
        self.iter()
            .filter_map(|c| c.downcast::<ConnectionDevice>().ok())
            .filter(|d| !d.autovol())
            .map(|device| CustomDeviceVariant {
                alias: device.alias().unwrap_or_default(),
                device_fn: device.device_fn().unwrap_or_default(),
                mount_point: device
                    .mountp_string()
                    .as_ref()
                    .and_then(|p| p.to_str())
                    .unwrap_or_default()
                    .to_owned(),
                icon: device
                    .icon()
                    .and_then(|i| i.serialize())
                    .unwrap_or_else(|| "".to_variant()),
            })
            .collect()
    }

    pub fn load_connections(&self, variants: &[ConnectionVariant]) {
        for v in variants {
            match ConnectionRemote::try_from_string(&v.alias, &v.uri) {
                Ok(remote) => self.add(&remote),
                Err(error) => eprintln!(
                    "<Connection> invalid URI: '{}' - ignored\n{}",
                    v.uri,
                    error.message()
                ),
            }
        }
    }

    fn save_connections(&self) -> Vec<ConnectionVariant> {
        self.iter()
            .filter_map(|c| c.downcast::<ConnectionRemote>().ok())
            .filter_map(|device| {
                Some(ConnectionVariant {
                    alias: device.alias()?,
                    uri: device.uri_string()?,
                })
            })
            .collect()
    }

    fn find_remote_by_root(&self, root_file: &gio::File) -> Option<ConnectionRemote> {
        self.all()
            .iter::<Connection>()
            .flatten()
            .filter_map(|c| c.downcast::<ConnectionRemote>().ok())
            .find(|c| {
                c.uri_string()
                    .map(|uri| gio::File::for_uri(&uri).equal(root_file))
                    .unwrap_or(false)
            })
    }

    /// This function will add mounts which are not for the 'file' scheme
    /// to the list of stored connections. This might be usefull when opening a remote
    /// connection with an external programm. This connection can be opened / used at
    /// a later time then also in Gnome Commander as it will be selectable from
    /// the connection list.
    fn add_mount(&self, mount: &gio::Mount) {
        let file = mount.root();
        //Don't care about 'local' mounts (for the moment)
        if file.uri_scheme().as_deref() == Some("file") {
            return;
        }
        if self.find_remote_by_root(&file).is_none() {
            if let Ok(connection) = ConnectionRemote::try_from_string(&mount.name(), &file.uri()) {
                self.add(&connection);
            }
        }
    }

    async fn remove_mount(&self, mount: &gio::Mount) {
        let file = mount.root();
        if let Some(con) = self.find_remote_by_root(&file) {
            con.close(None).await;
        }
    }

    fn find_device_by_mount_point(&self, mount_point: &Path) -> Option<ConnectionDevice> {
        self.all()
            .iter::<Connection>()
            .flatten()
            .filter_map(|c| c.downcast::<ConnectionDevice>().ok())
            .filter(|d| !d.autovol())
            .find(|d| d.mountp_string().as_deref() == Some(mount_point))
    }

    fn add_volume(&self, volume: &gio::Volume) {
        debug!('m', "volume name = {}", volume.name());
        debug!('m', "volume uuid = {:?}", volume.uuid());

        if let Some(unix_device_string) = volume.identifier(gio::VOLUME_IDENTIFIER_KIND_UNIX_DEVICE)
        {
            debug!('m', "volume unix device = {:?}", unix_device_string);

            // Only create a new device connection if it does not already exist.
            // This can happen if the user manually added the same device in "Options|Devices" menu
            // We have to compare each connection in con_list with the `unix_device_string` for this.
            if self
                .find_device_by_mount_point(Path::new(&unix_device_string))
                .is_some()
            {
                debug!(
                    'm',
                    "Device for mountpoint({:?}) already exists. AutoVolume not added",
                    unix_device_string
                );
                return;
            }
        }
        // If it does not exist already and a UUID is available, create the new device connection
        if let Some(dev) = ConnectionDevice::new_auto_volume(&volume) {
            self.add(&dev);
        } else {
            debug!('m', "Device does not have a UUID. Skipping");
        }
    }

    fn remove_volume(&self, volume: &gio::Volume) {
        let Some(uuid) = volume.identifier(gio::VOLUME_IDENTIFIER_KIND_UUID) else {
            return;
        };
        let Some(device) = self
            .all()
            .iter::<Connection>()
            .flatten()
            .filter_map(|c| c.downcast::<ConnectionDevice>().ok())
            .filter(|d| d.autovol())
            .find(|d| d.device_fn().as_deref() == Some(&uuid))
        else {
            return;
        };
        debug!('m', "Remove Volume: {:?}", device.device_fn());
        self.remove(&device);
    }

    pub fn set_volume_monitor(&self) {
        let monitor = &self.imp().volume_monitor;
        monitor.connect_mount_added(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, mount| this.add_mount(mount)
        ));
        monitor.connect_mount_removed(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, mount| {
                let mount = mount.clone();
                glib::spawn_future_local(async move {
                    this.remove_mount(&mount).await;
                });
            }
        ));
        monitor.connect_volume_added(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, volume| this.add_volume(&volume)
        ));
        monitor.connect_volume_removed(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, volume| this.remove_volume(&volume)
        ));
    }

    fn load_available_volumes(&self) {
        let monitor = &self.imp().volume_monitor;
        for volume in monitor.volumes() {
            self.add_volume(&volume);
        }
    }

    pub fn connect_list_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn() + 'static,
    {
        self.connect_local("list-changed", false, move |_| {
            (callback)();
            None
        })
    }
}

#[derive(glib::Variant)]
pub struct BookmarkVariant {
    pub is_remote: bool,
    pub group_name: String,
    pub name: String,
    pub path: String,
}

#[derive(glib::Variant)]
pub struct CustomDeviceVariant {
    pub alias: String,
    pub device_fn: String,
    pub mount_point: String,
    pub icon: glib::Variant,
}

#[derive(glib::Variant)]
pub struct ConnectionVariant {
    pub alias: String,
    pub uri: String,
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_types() {
        assert_eq!(*BookmarkVariant::static_variant_type(), "(bsss)");
        assert_eq!(*Vec::<BookmarkVariant>::static_variant_type(), "a(bsss)");

        assert_eq!(*CustomDeviceVariant::static_variant_type(), "(sssv)");
        assert_eq!(
            *Vec::<CustomDeviceVariant>::static_variant_type(),
            "a(sssv)"
        );

        assert_eq!(*ConnectionVariant::static_variant_type(), "(ss)");
        assert_eq!(*Vec::<ConnectionVariant>::static_variant_type(), "a(ss)");
    }
}
