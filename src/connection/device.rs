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

use super::connection::{Connection, ConnectionExt, ConnectionInterface};
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_full, from_glib_none, ToGlibPtr},
    },
    prelude::*,
};
use std::path::{Path, PathBuf};

pub mod ffi {
    use super::*;
    use crate::connection::connection::ffi::GnomeCmdConClass;
    use gtk::{
        gio::ffi::{GIcon, GMount, GVolume},
        glib::ffi::GType,
    };
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdConDevice {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_device_get_type() -> GType;

        pub fn gnome_cmd_con_device_new(
            alias: *const c_char,
            device_fn: *const c_char,
            mountp: *const c_char,
            icon: *const GIcon,
        ) -> *mut GnomeCmdConDevice;

        pub fn gnome_cmd_con_device_get_device_fn(dev: *mut GnomeCmdConDevice) -> *const c_char;
        pub fn gnome_cmd_con_device_set_device_fn(
            dev: *mut GnomeCmdConDevice,
            device_fn: *const c_char,
        );

        pub fn gnome_cmd_con_device_get_mountp_string(dev: *mut GnomeCmdConDevice)
            -> *const c_char;
        pub fn gnome_cmd_con_device_set_mountp(dev: *mut GnomeCmdConDevice, mountp: *const c_char);

        pub fn gnome_cmd_con_device_get_icon(dev: *mut GnomeCmdConDevice) -> *const GIcon;
        pub fn gnome_cmd_con_device_set_icon(dev: *mut GnomeCmdConDevice, icon: *const GIcon);

        pub fn gnome_cmd_con_device_get_autovol(dev: *mut GnomeCmdConDevice) -> gboolean;
        pub fn gnome_cmd_con_device_set_autovol(dev: *mut GnomeCmdConDevice, autovol: gboolean);

        pub fn gnome_cmd_con_device_get_gmount(dev: *mut GnomeCmdConDevice) -> *mut GMount;
        pub fn gnome_cmd_con_device_set_gmount(dev: *mut GnomeCmdConDevice, mount: *mut GMount);

        pub fn gnome_cmd_con_device_get_gvolume(dev: *mut GnomeCmdConDevice) -> *mut GVolume;
        pub fn gnome_cmd_con_device_set_gvolume(dev: *mut GnomeCmdConDevice, volume: *mut GVolume);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConDeviceClass {
        pub parent_class: GnomeCmdConClass,
    }
}

glib::wrapper! {
    pub struct ConnectionDevice(Object<ffi::GnomeCmdConDevice, ffi::GnomeCmdConDeviceClass>)
        @extends Connection;

    match fn {
        type_ => || ffi::gnome_cmd_con_device_get_type(),
    }
}

impl ConnectionDevice {
    pub fn new(alias: &str, device_fn: &str, mountp: &Path, icon: Option<&gio::Icon>) -> Self {
        unsafe {
            from_glib_full(ffi::gnome_cmd_con_device_new(
                alias.to_glib_none().0,
                device_fn.to_glib_none().0,
                mountp.to_glib_none().0,
                icon.to_glib_full(),
            ))
        }
    }

    pub fn new_auto_volume(volume: &gio::Volume) -> Option<Self> {
        let uuid = volume.identifier(gio::VOLUME_IDENTIFIER_KIND_UUID)?;
        let this: Self = glib::Object::builder().build();
        this.set_device_fn(Some(&uuid));
        this.set_mountp(None);
        this.set_icon(Some(&volume.icon()));
        this.set_autovol(false);
        this.set_alias(Some(&volume.name()));
        this.set_autovol(true);
        this.set_volume(Some(volume));
        Some(this)
    }

    pub fn device_fn(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_device_get_device_fn(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn set_device_fn(&self, device_fn: Option<&str>) {
        unsafe {
            ffi::gnome_cmd_con_device_set_device_fn(
                self.to_glib_none().0,
                device_fn.to_glib_none().0,
            )
        }
    }

    pub fn mountp_string(&self) -> Option<PathBuf> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_device_get_mountp_string(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn set_mountp(&self, mount_point: Option<&Path>) {
        unsafe {
            ffi::gnome_cmd_con_device_set_mountp(
                self.to_glib_none().0,
                mount_point.to_glib_none().0,
            )
        }
    }

    pub fn icon(&self) -> Option<gio::Icon> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_device_get_icon(self.to_glib_none().0)) }
    }

    pub fn set_icon(&self, icon: Option<&gio::Icon>) {
        unsafe { ffi::gnome_cmd_con_device_set_icon(self.to_glib_none().0, icon.to_glib_full()) }
    }

    pub fn autovol(&self) -> bool {
        unsafe { ffi::gnome_cmd_con_device_get_autovol(self.to_glib_none().0) != 0 }
    }

    pub fn set_autovol(&self, autovol: bool) {
        unsafe { ffi::gnome_cmd_con_device_set_autovol(self.to_glib_none().0, autovol as gboolean) }
    }

    pub fn mount(&self) -> Option<gio::Mount> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_device_get_gmount(self.to_glib_none().0)) }
    }

    pub fn set_mount(&self, mount: Option<&gio::Mount>) {
        unsafe {
            ffi::gnome_cmd_con_device_set_gmount(self.to_glib_none().0, mount.to_glib_none().0)
        }
    }

    pub fn volume(&self) -> Option<gio::Volume> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_device_get_gvolume(self.to_glib_none().0)) }
    }

    pub fn set_volume(&self, volume: Option<&gio::Volume>) {
        unsafe {
            ffi::gnome_cmd_con_device_set_gvolume(self.to_glib_none().0, volume.to_glib_none().0);
        }
    }
}

impl ConnectionInterface for ConnectionDevice {
    fn is_local(&self) -> bool {
        true
    }

    fn open_is_needed(&self) -> bool {
        true
    }

    fn is_closeable(&self) -> bool {
        true
    }

    fn can_show_free_space(&self) -> bool {
        true
    }

    fn open_message(&self) -> Option<String> {
        let alias = self.alias()?;
        Some(gettext("Mounting %s").replace("%s", &alias))
    }

    fn go_text(&self) -> Option<String> {
        let alias = self.alias()?;
        Some(match self.mountp_string() {
            Some(mount_point) => gettext("Go to: {device_name} ({mount_point})")
                .replace("{device_name}", &alias)
                .replace("{mount_point}", &mount_point.display().to_string()),
            None => gettext("Go to: {device_name}").replace("{device_name}", &alias),
        })
    }

    fn open_text(&self) -> Option<String> {
        let alias = self.alias()?;
        Some(gettext("Mount: {device_name}").replace("{device_name}", &alias))
    }

    fn close_text(&self) -> Option<String> {
        let alias = self.alias()?;
        Some(gettext("Unmount: {device_name}").replace("{device_name}", &alias))
    }

    fn open_icon(&self) -> Option<gio::Icon> {
        self.icon()
    }
}
