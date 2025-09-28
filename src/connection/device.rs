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

use super::connection::{Connection, ConnectionExt, ConnectionInterface, ConnectionState};
use crate::{
    debug::debug,
    path::GnomeCmdPath,
    utils::{ErrorMessage, SenderExt},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{
    future::Future,
    path::{MAIN_SEPARATOR_STR, Path, PathBuf},
    pin::Pin,
    process::Command,
};

mod imp {
    use super::*;
    use crate::connection::connection::ConnectionImpl;
    use std::cell::{Cell, RefCell};

    #[derive(Default)]
    pub struct ConnectionDevice {
        pub auto_volume: Cell<bool>,
        /// The device identifier (either a linux device string or a uuid)
        pub device_fn: RefCell<Option<String>>,
        pub mount_point: RefCell<Option<PathBuf>>,
        pub icon: RefCell<Option<gio::Icon>>,
        pub mount: RefCell<Option<gio::Mount>>,
        pub volume: RefCell<Option<gio::Volume>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectionDevice {
        const NAME: &'static str = "GnomeCmdConDevice";
        type Type = super::ConnectionDevice;
        type ParentType = Connection;
    }

    impl ObjectImpl for ConnectionDevice {}
    impl ConnectionImpl for ConnectionDevice {}
}

glib::wrapper! {
    pub struct ConnectionDevice(ObjectSubclass<imp::ConnectionDevice>)
        @extends Connection;
}

impl ConnectionDevice {
    pub fn new(alias: &str, device_fn: &str, mountp: &Path, icon: Option<&gio::Icon>) -> Self {
        let this: Self = glib::Object::builder().build();
        this.set_device_fn(Some(device_fn));
        this.set_mountp(Some(mountp));
        this.set_icon(icon);
        this.set_autovol(false);
        this.set_mount(None);
        this.set_volume(None);
        this.set_alias(Some(alias));
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
        self.imp().device_fn.borrow().clone()
    }

    pub fn set_device_fn(&self, device_fn: Option<&str>) {
        self.imp()
            .device_fn
            .replace(device_fn.map(ToOwned::to_owned));
    }

    pub fn mountp_string(&self) -> Option<PathBuf> {
        self.imp().mount_point.borrow().clone()
    }

    pub fn set_mountp(&self, mount_point: Option<&Path>) {
        self.imp()
            .mount_point
            .replace(mount_point.map(ToOwned::to_owned));
    }

    pub fn icon(&self) -> Option<gio::Icon> {
        self.imp().icon.borrow().clone()
    }

    pub fn set_icon(&self, icon: Option<&gio::Icon>) {
        self.imp().icon.replace(icon.map(Clone::clone));
    }

    pub fn autovol(&self) -> bool {
        self.imp().auto_volume.get()
    }

    pub fn set_autovol(&self, autovol: bool) {
        self.imp().auto_volume.set(autovol);
    }

    pub fn mount(&self) -> Option<gio::Mount> {
        self.imp().mount.borrow().clone()
    }

    pub fn set_mount(&self, mount: Option<&gio::Mount>) {
        self.imp().mount.replace(mount.map(Clone::clone));
    }

    pub fn volume(&self) -> Option<gio::Volume> {
        self.imp().volume.borrow().clone()
    }

    pub fn set_volume(&self, volume: Option<&gio::Volume>) {
        self.imp().volume.replace(volume.map(Clone::clone));
    }

    async fn legacy_mount(&self) -> Result<(), ErrorMessage> {
        let Some(device) = self.device_fn() else {
            return Ok(());
        };
        let Some(mount_point) = self.mountp_string() else {
            return Ok(());
        };

        if self.base_path().is_none() {
            self.set_base_path(Some(GnomeCmdPath::Plain(mount_point.clone())));
        }

        let (sender, receiver) = async_channel::bounded(1);
        let _join_handle = std::thread::spawn({
            let device = device.clone();
            let mount_point = mount_point.clone();
            move || {
                let result = legacy_mount(&device, &mount_point);
                sender.toss(result);
            }
        });

        match receiver.recv().await {
            Ok(Ok(())) => {
                match gio::File::for_path(&mount_point)
                    .query_info_future("*", gio::FileQueryInfoFlags::NONE, glib::Priority::DEFAULT)
                    .await
                {
                    Ok(file_info) => {
                        self.set_base_file_info(Some(&file_info));
                        self.set_state(ConnectionState::Open);
                        Ok(())
                    }
                    Err(error) => {
                        self.set_base_file_info(None);
                        self.set_state(ConnectionState::Closed);
                        Err(ErrorMessage::with_error(
                            gettext("Unable to mount the volume {}").replace("{}", &device),
                            &error,
                        ))
                    }
                }
            }
            Ok(Err(error)) => {
                self.set_base_file_info(None);
                self.set_state(ConnectionState::Closed);
                Err(error)
            }
            Err(error) => {
                self.set_base_file_info(None);
                self.set_state(ConnectionState::Closed);
                eprintln!("Channel error: {error}");
                Err(ErrorMessage::brief(
                    gettext("Unable to mount the volume {}").replace("{}", &device),
                ))
            }
        }
    }
}

impl ConnectionInterface for ConnectionDevice {
    fn open_impl(
        &self,
        window: gtk::Window,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async move {
            debug!('m', "Mounting device");
            let is_legacy = self.mountp_string().is_some();
            if is_legacy {
                // This is a legacy-mount: If mount point is given, we mount the device with system calls ('mount')
                self.legacy_mount().await?;
            } else if let Some(volume) = self.volume() {
                // Check if the volume is already mounted:
                let mount = if let Some(mount) = volume.get_mount() {
                    mount
                } else {
                    let mount_operation = gtk::MountOperation::new(Some(&window));
                    volume
                        .mount_future(gio::MountMountFlags::NONE, Some(&mount_operation))
                        .await
                        .map_err(|error| {
                            eprintln!("Unable to mount the volume, error: {error}");
                            self.set_base_file_info(None);
                            ErrorMessage::with_error(
                                gettext("Unable to mount the volume {}")
                                    .replace("{}", &self.alias().unwrap_or_default()),
                                &error,
                            )
                        })?;
                    self.set_mount(volume.get_mount().as_ref());
                    self.mount().unwrap()
                };

                let file = mount.default_location();
                let path = file.path().unwrap();
                self.set_base_path(Some(GnomeCmdPath::Plain(path)));

                match file
                    .query_info_future("*", gio::FileQueryInfoFlags::NONE, glib::Priority::DEFAULT)
                    .await
                {
                    Ok(file_info) => {
                        self.set_base_file_info(Some(&file_info));
                    }
                    Err(error) => {
                        self.set_base_file_info(None);
                        eprintln!("Unable to mount the volume: error: {error}");
                        Err(ErrorMessage::with_error(
                            gettext("Unable to mount the volume {}")
                                .replace("{}", &self.alias().unwrap_or_default()),
                            &error,
                        ))?;
                    }
                }

                // if let Some(base_path) = self.base_path()                                 {
                //     let path = base_path.path();
                //     let uri_string = glib::filename_to_uri(path, None);
                //     self.set_uri_string(uri_string);
                // }
            }
            Ok(())
        })
    }

    fn close_impl(
        &self,
        window: Option<gtk::Window>,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async {
            self.set_default_dir(None);

            if let Err(error) = std::env::set_current_dir(glib::home_dir()) {
                debug!(
                    'm',
                    "Could not go back to home directory before unmounting: {error}"
                );
            }

            if self.autovol() {
                if let Some(mount) = self.mount().filter(|m| m.can_unmount()) {
                    debug!('m', "umounting GIO mount \"{}\"", mount.name());

                    match mount
                        .unmount_with_operation_future(
                            gio::MountUnmountFlags::NONE,
                            gio::MountOperation::NONE,
                        )
                        .await
                    {
                        Ok(()) => {
                            self.set_state(ConnectionState::Closed);
                            show_message_dialog_volume_unmounted(window);
                            Ok(())
                        }
                        Err(error) => Err(ErrorMessage::with_error(
                            gettext("Cannot unmount the volume"),
                            &error,
                        )),
                    }
                } else {
                    Ok(())
                }
            } else {
                if let Some(mount_point) = self.mountp_string() {
                    legacy_umount(&mount_point)
                } else {
                    Ok(())
                }
            }
        })
    }

    fn create_gfile(&self, path: &GnomeCmdPath) -> gio::File {
        if let Some(mount) = self.mount() {
            mount.default_location().resolve_relative_path(path.path())
        } else {
            gio::File::for_path(path.path())
        }
    }

    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        GnomeCmdPath::Plain(path.to_owned())
    }

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

fn show_message_dialog_volume_unmounted(window: Option<gtk::Window>) {
    debug!('m', "unmount succeeded");
    let message = gettext("Volume successfully unmounted");
    match window {
        Some(window) => {
            gtk::AlertDialog::builder()
                .modal(true)
                .message(message)
                .buttons([gettext("OK")])
                .cancel_button(0)
                .default_button(0)
                .build()
                .show(Some(&window));
        }
        None => {
            eprintln!("{message}");
        }
    }
}

#[cfg(target_os = "linux")]
fn is_mounted(mount_point: &Path) -> bool {
    use libc::{endmntent, getmntent, setmntent};
    use std::ffi::CStr;
    unsafe {
        let f = setmntent(c"/etc/mtab".as_ptr(), c"r".as_ptr());
        if f.is_null() {
            eprintln!("Failed to read /etc/mtab");
            return false;
        }
        let found = loop {
            let entry = getmntent(f);
            if entry.is_null() {
                break false;
            }
            if CStr::from_ptr((*entry).mnt_dir).to_str().ok() == mount_point.to_str() {
                break true;
            }
        };
        endmntent(f);
        found
    }
}

#[cfg(not(target_os = "linux"))]
fn is_mounted(mount_point: &Path) -> bool {
    fn str_compress(value: &str) -> String {
        unsafe { from_glib_full(glib::ffi::g_strcompress(value.to_glib_none().0)) }
    }

    fn parse_mtab(mtab: &str) -> impl Iterator<Item = Vec<String>> + use<'_> {
        mtab.lines()
            .map(|line| {
                if let Some((before_comment, _)) = line.split_once('#') {
                    before_comment
                } else {
                    line
                }
            })
            .map(|line| {
                line.split_ascii_whitespace()
                    .map(|path| str_compress(path))
                    .collect::<Vec<_>>()
            })
    }

    match std::fs::read_to_string("/etc/mtab") {
        Ok(mtab) => parse_mtab(&mtab)
            .filter_map(|line| line.get(1).cloned())
            .any(|path| Path::new(&path) == mount_point),
        Err(error) => {
            eprintln!("Failed to read /etc/mtab: {error}");
            return false;
        }
    }
}

fn legacy_mount(device: &str, mount_point: &Path) -> Result<(), ErrorMessage> {
    if is_mounted(mount_point) {
        debug!('m', "The device was already mounted at");
        return Ok(());
    }
    if device.is_empty() {
        return Ok(());
    }

    debug!('m', "mounting {device}");
    let mut command = Command::new("mount");
    if !device.starts_with(MAIN_SEPARATOR_STR) {
        command.arg("-L");
    }
    command.arg(device);
    command.arg(mount_point);
    debug!('m', "Mount command: {:?}", command);
    let status = command.status().map_err(|e| {
        ErrorMessage::new(
            gettext("Failed to execute the mount command"),
            Some(e.to_string()),
        )
    })?;

    debug!('m', "mount returned {:?}", status.code());

    match status.code() {
        Some(0) => Ok(()),
        Some(1) => Err(ErrorMessage::brief(gettext(
            "Mount failed: permission denied",
        ))),
        Some(32) => Err(ErrorMessage::brief(gettext(
            "Mount failed: no medium found",
        ))),
        Some(code) => Err(ErrorMessage::brief(
            gettext("Mount failed: mount exited with exitstatus {}")
                .replace("{}", &code.to_string()),
        )),
        _ => Err(ErrorMessage::brief(gettext(
            "Mount failed: mount exited without an exitstatus",
        ))),
    }
}

fn legacy_umount(mount_point: &Path) -> Result<(), ErrorMessage> {
    debug!('m', "umounting {}", mount_point.display());
    let ret = Command::new("umount").arg(mount_point).status();
    debug!('m', "umount returned {ret:?}");
    match ret {
        Ok(status) if status.success() => Ok(()),
        Ok(status) => Err(ErrorMessage::brief(
            gettext("Unmount failed: umount exited with exitstatus {}")
                .replace("{}", &status.to_string()),
        )),
        Err(error) => Err(ErrorMessage::new(
            gettext("Failed to execute the umount command"),
            Some(error.to_string()),
        )),
    }
}
