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
    home::{ffi::GnomeCmdConHome, ConnectionHome},
    remote::{ffi::GnomeCmdConRemote, ConnectionRemote},
    smb::{ffi::GnomeCmdConSmb, ConnectionSmb},
};
use gtk::{
    gio::{
        self,
        ffi::{GFile, GListModel},
    },
    glib::{
        self,
        ffi::GVariant,
        subclass::prelude::*,
        translate::{
            from_glib_borrow, from_glib_full, from_glib_none, Borrowed, IntoGlibPtr, ToGlibPtr,
        },
    },
    prelude::*,
};
use std::path::Path;

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

impl ConnectionList {
    pub fn get() -> Self {
        extern "C" {
            fn gnome_cmd_con_list_get(
            ) -> *mut <ConnectionList as glib::object::ObjectType>::GlibType;
        }

        unsafe { from_glib_none(gnome_cmd_con_list_get()) }
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

    pub fn load_bookmarks(&self, variant: glib::Variant) {
        debug_assert_eq!(*BookmarkVariant::static_variant_type(), "(bsss)");
        debug_assert_eq!(*Vec::<BookmarkVariant>::static_variant_type(), "a(bsss)");

        for con in self.iter() {
            con.erase_bookmarks();
        }

        let Some(bookmarks) = Vec::<BookmarkVariant>::from_variant(&variant) else {
            eprintln!("Cannot load bookmarks: bad variant format.");
            return;
        };

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

    pub fn save_bookmarks(&self) -> Option<glib::Variant> {
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

        if bookmarks.is_empty() {
            None
        } else {
            Some(bookmarks.to_variant())
        }
    }

    pub fn load_devices(&self, variant: glib::Variant) {
        CustomDeviceVariant::assert_variant_type();

        for v in Vec::<CustomDeviceVariant>::from_variant(&variant)
            .into_iter()
            .flatten()
        {
            self.add(&ConnectionDevice::new(
                &v.alias,
                &v.device_fn,
                &Path::new(&v.mount_point),
                gio::Icon::deserialize(&v.icon).as_ref(),
            ));
        }
        self.load_available_volumes();
    }

    pub fn save_devices(&self) -> Option<glib::Variant> {
        CustomDeviceVariant::assert_variant_type();

        let devices: Vec<CustomDeviceVariant> = self
            .iter()
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
            .collect();

        if devices.is_empty() {
            None
        } else {
            Some(devices.to_variant())
        }
    }

    pub fn load_connections(&self, variant: glib::Variant) {
        ConnectionVariant::assert_variant_type();

        for v in Vec::<ConnectionVariant>::from_variant(&variant)
            .into_iter()
            .flatten()
        {
            match ConnectionRemote::new(&v.alias, &v.uri) {
                Some(remote) => self.add(&remote),
                None => eprintln!("<Connection> invalid URI: '{}' - ignored", v.uri),
            }
        }
    }

    pub fn save_connections(&self) -> Option<glib::Variant> {
        ConnectionVariant::assert_variant_type();

        let connections: Vec<ConnectionVariant> = self
            .iter()
            .filter_map(|c| c.downcast::<ConnectionRemote>().ok())
            .filter_map(|device| {
                Some(ConnectionVariant {
                    alias: device.alias()?,
                    uri: device.uri_string()?,
                })
            })
            .collect();

        if connections.is_empty() {
            None
        } else {
            Some(connections.to_variant())
        }
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
            if let Some(connection) = ConnectionRemote::new(&mount.name(), &file.uri()) {
                self.add(&connection);
            }
        }
    }

    fn remove_mount(&self, mount: &gio::Mount) {
        let file = mount.root();
        if let Some(con) = self.find_remote_by_root(&file) {
            con.close(None);
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
        if let Some(unix_device_string) = volume.identifier(gio::VOLUME_IDENTIFIER_KIND_UNIX_DEVICE)
        {
            // Only create a new device connection if it does not already exist.
            // This can happen if the user manually added the same device in "Options|Devices" menu
            // We have to compare each connection in con_list with the `unix_device_string` for this.
            if self
                .find_device_by_mount_point(Path::new(&unix_device_string))
                .is_some()
            {
                eprintln!(
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
            eprintln!("Device does not have a UUID. Skipping");
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
        self.remove(&device);
    }

    fn set_volume_monitor(&self) {
        let monitor = &self.imp().volume_monitor;
        monitor.connect_mount_added(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, mount| this.add_mount(mount)
        ));
        monitor.connect_mount_removed(glib::clone!(
            #[weak(rename_to = this)]
            self,
            move |_, mount| this.remove_mount(mount)
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
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_new(
    show_samba_workgroups_button: bool,
) -> *mut <ConnectionList as glib::object::ObjectType>::GlibType {
    ConnectionList::new(show_samba_workgroups_button).to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_get_all(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) -> *mut GListModel {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.all().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_get_home(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) -> *mut GnomeCmdConHome {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.home().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_get_smb(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) -> *mut GnomeCmdConSmb {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.smb().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn get_remote_con_for_gfile(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
    file_ptr: *mut GFile,
) -> *mut GnomeCmdConRemote {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    let file: Borrowed<gio::File> = unsafe { from_glib_borrow(file_ptr) };
    list.get_remote_con_for_file(&*file).to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_lock(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.lock();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_unlock(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.unlock();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_load_bookmarks(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
    variant_ptr: *mut GVariant,
) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    let variant: glib::Variant = unsafe { from_glib_full(variant_ptr) };
    list.load_bookmarks(variant);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_save_bookmarks(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) -> *mut GVariant {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    let variant = list.save_bookmarks();
    unsafe { variant.into_glib_ptr() }
}

#[derive(glib::Variant)]
struct BookmarkVariant {
    is_remote: bool,
    group_name: String,
    name: String,
    path: String,
}

#[derive(glib::Variant)]
struct CustomDeviceVariant {
    alias: String,
    device_fn: String,
    mount_point: String,
    icon: glib::Variant,
}

impl CustomDeviceVariant {
    fn assert_variant_type() {
        debug_assert_eq!(*Self::static_variant_type(), "(sssv)");
        debug_assert_eq!(*Vec::<Self>::static_variant_type(), "a(sssv)");
    }
}

#[derive(glib::Variant)]
struct ConnectionVariant {
    alias: String,
    uri: String,
}

impl ConnectionVariant {
    fn assert_variant_type() {
        debug_assert_eq!(*ConnectionVariant::static_variant_type(), "(ss)");
        debug_assert_eq!(*Vec::<ConnectionVariant>::static_variant_type(), "a(ss)");
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_set_volume_monitor(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.set_volume_monitor();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_load_devices(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
    variant_ptr: *mut GVariant,
) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    let variant: glib::Variant = unsafe { from_glib_full(variant_ptr) };
    list.load_devices(variant);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_save_devices(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) -> *mut GVariant {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.save_devices().to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_load_connections(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
    variant_ptr: *mut GVariant,
) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    let variant: glib::Variant = unsafe { from_glib_full(variant_ptr) };
    list.load_connections(variant);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_save_connections(
    list_ptr: *mut <ConnectionList as glib::object::ObjectType>::GlibType,
) -> *mut GVariant {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.save_connections().to_glib_full()
}
