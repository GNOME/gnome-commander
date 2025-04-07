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
    device::ConnectionDevice,
    remote::ConnectionRemote,
};
use gtk::{
    gio,
    glib::{
        self,
        ffi::GVariant,
        translate::{
            from_glib_borrow, from_glib_full, from_glib_none, Borrowed, IntoGlibPtr, ToGlibPtr,
        },
    },
    prelude::*,
};
use std::path::Path;

pub mod ffi {
    use super::*;
    use crate::connection::connection::ffi::GnomeCmdCon;
    use gtk::{gio::ffi::GListModel, glib::ffi::GType};
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdConList {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_list_get_type() -> GType;

        pub fn gnome_cmd_con_list_get_all(list: *mut GnomeCmdConList) -> *const GListModel;

        pub fn gnome_cmd_con_list_add(list: *mut GnomeCmdConList, con: *mut GnomeCmdCon);
        pub fn gnome_cmd_con_list_remove(list: *mut GnomeCmdConList, con: *mut GnomeCmdCon);

        pub fn gnome_cmd_con_list_find_by_uuid(
            list: *mut GnomeCmdConList,
            uuid: *const c_char,
        ) -> *mut GnomeCmdCon;
        pub fn gnome_cmd_con_list_find_by_alias(
            list: *mut GnomeCmdConList,
            uuid: *const c_char,
        ) -> *mut GnomeCmdCon;
        pub fn gnome_cmd_con_list_get_home(list: *mut GnomeCmdConList) -> *mut GnomeCmdCon;
        pub fn gnome_cmd_con_list_get_smb(list: *mut GnomeCmdConList) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_con_list_get() -> *mut GnomeCmdConList;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConListClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct ConnectionList(Object<ffi::GnomeCmdConList, ffi::GnomeCmdConListClass>);

    match fn {
        type_ => || ffi::gnome_cmd_con_list_get_type(),
    }
}

impl ConnectionList {
    pub fn all(&self) -> gio::ListModel {
        unsafe { from_glib_none(ffi::gnome_cmd_con_list_get_all(self.to_glib_none().0)) }
    }

    pub fn add(&self, con: &impl IsA<Connection>) {
        unsafe { ffi::gnome_cmd_con_list_add(self.to_glib_none().0, con.as_ref().to_glib_full()) }
    }

    pub fn remove(&self, con: &impl IsA<Connection>) {
        unsafe {
            ffi::gnome_cmd_con_list_remove(self.to_glib_none().0, con.as_ref().to_glib_none().0)
        }
    }

    pub fn replace(&self, old: &impl IsA<Connection>, new: &impl IsA<Connection>) {
        if let Ok(store) = self.all().downcast::<gio::ListStore>() {
            if let Some(position) = store.find(old.as_ref()) {
                store.splice(position, 1, &[new.as_ref().clone()]);
            } else {
                store.append(new.as_ref());
            }
        }
    }

    #[deprecated(note = "This is a hack. Prefer to use immutable objects instead.")]
    pub fn refresh(&self, connection: &impl IsA<Connection>) {
        if let Ok(store) = self.all().downcast::<gio::ListStore>() {
            let connection = connection.as_ref();
            if let Some(position) = store.find(connection) {
                store.remove(position);
                store.insert(position, connection);
            }
        }
    }

    pub fn find_by_uuid(&self, uuid: &str) -> Option<Connection> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_list_find_by_uuid(
                self.to_glib_none().0,
                uuid.to_glib_none().0,
            ))
        }
    }

    pub fn find_by_alias(&self, alias: &str) -> Option<Connection> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_list_find_by_alias(
                self.to_glib_none().0,
                alias.to_glib_none().0,
            ))
        }
    }

    pub fn home(&self) -> Connection {
        unsafe { from_glib_none(ffi::gnome_cmd_con_list_get_home(self.to_glib_none().0)) }
    }

    pub fn smb(&self) -> Option<Connection> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_list_get_smb(self.to_glib_none().0)) }
    }

    pub fn get() -> ConnectionList {
        unsafe { from_glib_none(ffi::gnome_cmd_con_list_get()) }
    }

    pub fn load_bookmarks(&self, variant: glib::Variant) {
        debug_assert_eq!(*BookmarkVariant::static_variant_type(), "(bsss)");
        debug_assert_eq!(*Vec::<BookmarkVariant>::static_variant_type(), "a(bsss)");

        for con in self.all().iter() {
            let con: Connection = con.unwrap();
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
                Some(self.home())
            } else if bookmark.group_name == "SMB" {
                self.smb()
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

        for bookmark in self
            .home()
            .bookmarks()
            .into_iter()
            .filter_map(|i| i.ok().and_downcast::<Bookmark>())
        {
            bookmarks.push(BookmarkVariant {
                is_remote: false,
                group_name: "Home".to_owned(),
                name: bookmark.name(),
                path: bookmark.path(),
            });
        }

        if let Some(con) = self.smb() {
            for bookmark in con
                .bookmarks()
                .into_iter()
                .filter_map(|i| i.ok().and_downcast::<Bookmark>())
            {
                bookmarks.push(BookmarkVariant {
                    is_remote: true,
                    group_name: "SMB".to_owned(),
                    name: bookmark.name(),
                    path: bookmark.path(),
                });
            }
        }

        for con in self
            .all()
            .iter::<Connection>()
            .flatten()
            .filter_map(|c| c.downcast::<ConnectionRemote>().ok())
        {
            let alias = con.alias().unwrap_or_default();
            for bookmark in con
                .bookmarks()
                .into_iter()
                .filter_map(|i| i.ok().and_downcast::<Bookmark>())
            {
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
            self.add(&ConnectionRemote::new(&mount.name(), &file.uri()));
        }
    }

    fn remove_mount(&self, mount: &gio::Mount) {
        let file = mount.root();
        if let Some(con) = self.find_remote_by_root(&file) {
            con.close();
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
        let monitor = gio::VolumeMonitor::get();
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
        // TODO: make it a property
        unsafe { self.set_data("volume-monitor", monitor); }
    }

    fn load_available_volumes(&self) {
        let monitor = gio::VolumeMonitor::get();
        for volume in monitor.volumes() {
            self.add_volume(&volume);
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_load_bookmarks(
    list_ptr: *mut ffi::GnomeCmdConList,
    variant_ptr: *mut GVariant,
) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    let variant: glib::Variant = unsafe { from_glib_full(variant_ptr) };
    list.load_bookmarks(variant);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_save_bookmarks(
    list_ptr: *mut ffi::GnomeCmdConList,
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

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_set_volume_monitor(list_ptr: *mut ffi::GnomeCmdConList) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.set_volume_monitor();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_list_load_available_volumes(list_ptr: *mut ffi::GnomeCmdConList) {
    let list: Borrowed<ConnectionList> = unsafe { from_glib_borrow(list_ptr) };
    list.load_available_volumes();
}
