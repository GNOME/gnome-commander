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
    gio::{
        self,
        ffi::{GMount, GVolume},
    },
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_borrow, Borrowed, IntoGlib, ToGlibPtr},
    },
    prelude::*,
};
use std::{
    ffi::c_char,
    path::{Path, PathBuf},
    sync::LazyLock,
};

pub mod ffi {
    use crate::connection::connection::ffi::GnomeCmdConClass;
    use gtk::glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdConDevice {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_device_get_type() -> GType;
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

#[derive(Default)]
struct ConnectionDevicePrivate {
    auto_volume: bool,
    /// The device identifier (either a linux device string or a uuid)
    device_fn: Option<String>,
    mount_point: Option<PathBuf>,
    icon: Option<gio::Icon>,
    mount: Option<gio::Mount>,
    volume: Option<gio::Volume>,
}

impl ConnectionDevice {
    fn private(&self) -> &mut ConnectionDevicePrivate {
        static QUARK: LazyLock<glib::Quark> =
            LazyLock::new(|| glib::Quark::from_str("connection-device-private"));

        unsafe {
            if let Some(mut private) = self.qdata::<ConnectionDevicePrivate>(*QUARK) {
                private.as_mut()
            } else {
                self.set_qdata(*QUARK, ConnectionDevicePrivate::default());
                self.qdata::<ConnectionDevicePrivate>(*QUARK)
                    .unwrap()
                    .as_mut()
            }
        }
    }

    pub fn new(alias: &str, device_fn: &str, mountp: &Path, icon: Option<&gio::Icon>) -> Self {
        let this: Self = glib::Object::builder().build();
        this.set_device_fn(Some(device_fn));
        this.set_mountp(Some(mountp));
        this.set_icon(icon);
        this.set_autovol(false);
        this.set_mount(None);
        this.set_volume(None);
        this.set_alias(Some(alias));
        if let Ok(uri_string) = glib::filename_to_uri(mountp, None) {
            this.set_uri_string(Some(&uri_string));
        }
        this
    }

    pub fn new_auto_volume(volume: &gio::Volume) -> Option<Self> {
        let uuid = volume.identifier(gio::VOLUME_IDENTIFIER_KIND_UUID)?;
        let this: Self = glib::Object::builder().build();
        this.set_device_fn(Some(&uuid));
        this.set_mountp(None);
        this.set_icon(Some(&volume.icon()));
        this.set_alias(Some(&volume.name()));
        this.set_autovol(true);
        this.set_volume(Some(volume));
        Some(this)
    }

    pub fn device_fn(&self) -> Option<String> {
        self.private().device_fn.clone()
    }

    pub fn set_device_fn(&self, device_fn: Option<&str>) {
        self.private().device_fn = device_fn.map(ToOwned::to_owned);
    }

    pub fn mountp_string(&self) -> Option<PathBuf> {
        self.private().mount_point.clone()
    }

    pub fn set_mountp(&self, mount_point: Option<&Path>) {
        self.private().mount_point = mount_point.map(ToOwned::to_owned);
    }

    pub fn icon(&self) -> Option<gio::Icon> {
        self.private().icon.clone()
    }

    pub fn set_icon(&self, icon: Option<&gio::Icon>) {
        self.private().icon = icon.map(Clone::clone);
    }

    pub fn autovol(&self) -> bool {
        self.private().auto_volume
    }

    pub fn set_autovol(&self, autovol: bool) {
        self.private().auto_volume = autovol;
    }

    pub fn mount(&self) -> Option<gio::Mount> {
        self.private().mount.clone()
    }

    pub fn set_mount(&self, mount: Option<&gio::Mount>) {
        self.private().mount = mount.map(Clone::clone);
    }

    pub fn volume(&self) -> Option<gio::Volume> {
        self.private().volume.clone()
    }

    pub fn set_volume(&self, volume: Option<&gio::Volume>) {
        self.private().volume = volume.map(Clone::clone);
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

#[no_mangle]
pub extern "C" fn gnome_cmd_con_device_get_autovol(dev: *mut ffi::GnomeCmdConDevice) -> gboolean {
    let con: Borrowed<ConnectionDevice> = unsafe { from_glib_borrow(dev) };
    con.autovol().into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_device_get_device_fn(
    dev: *mut ffi::GnomeCmdConDevice,
) -> *mut c_char {
    let con: Borrowed<ConnectionDevice> = unsafe { from_glib_borrow(dev) };
    con.device_fn().to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_device_get_mountp_string(
    dev: *mut ffi::GnomeCmdConDevice,
) -> *mut c_char {
    let con: Borrowed<ConnectionDevice> = unsafe { from_glib_borrow(dev) };
    con.mountp_string().to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_device_get_gmount(dev: *mut ffi::GnomeCmdConDevice) -> *mut GMount {
    let con: Borrowed<ConnectionDevice> = unsafe { from_glib_borrow(dev) };
    con.mount().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_device_set_gmount(
    dev: *mut ffi::GnomeCmdConDevice,
    mount: *mut GMount,
) {
    let con: Borrowed<ConnectionDevice> = unsafe { from_glib_borrow(dev) };
    let mount: Borrowed<Option<gio::Mount>> = unsafe { from_glib_borrow(mount) };
    con.set_mount((*mount).as_ref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_device_get_gvolume(
    dev: *mut ffi::GnomeCmdConDevice,
) -> *mut GVolume {
    let con: Borrowed<ConnectionDevice> = unsafe { from_glib_borrow(dev) };
    con.volume().to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_device_set_gvolume(
    dev: *mut ffi::GnomeCmdConDevice,
    volume: *mut GVolume,
) {
    let con: Borrowed<ConnectionDevice> = unsafe { from_glib_borrow(dev) };
    let volume: Borrowed<Option<gio::Volume>> = unsafe { from_glib_borrow(volume) };
    con.set_volume((*volume).as_ref());
}
