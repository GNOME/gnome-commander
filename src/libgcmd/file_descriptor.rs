/*
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

use gtk::{
    gio::{
        self,
        ffi::{GFile, GFileInfo},
    },
    glib::{self, prelude::*, subclass::prelude::*, translate::*},
};

pub mod ffi {
    use gtk::{
        gio::ffi::{GFile, GFileInfo},
        glib::{ffi::GType, gobject_ffi},
    };

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileDescriptor {
        pub g_iface: gobject_ffi::GTypeInterface,

        pub file: Option<unsafe extern "C" fn(*mut GnomeCmdFileDescriptor) -> *mut GFile>,
        pub file_info: Option<unsafe extern "C" fn(*mut GnomeCmdFileDescriptor) -> *mut GFileInfo>,
    }

    unsafe extern "C" {
        pub fn gnome_cmd_file_descriptor_get_type() -> GType;

        pub fn gnome_cmd_file_descriptor_get_file(fb: *mut GnomeCmdFileDescriptor) -> *const GFile;
        pub fn gnome_cmd_file_descriptor_get_file_info(
            fb: *mut GnomeCmdFileDescriptor,
        ) -> *const GFileInfo;
    }
}

glib::wrapper! {
    pub struct FileDescriptor(Interface<ffi::GnomeCmdFileDescriptor, ffi::GnomeCmdFileDescriptor>);

    match fn {
        type_ => || ffi::gnome_cmd_file_descriptor_get_type(),
    }
}

pub trait FileDescriptorImpl: ObjectImpl + ObjectSubclass<Type: IsA<FileDescriptor>> {
    fn file(&self) -> gio::File;
    fn file_info(&self) -> gio::FileInfo;
}

unsafe impl<T: FileDescriptorImpl> IsImplementable<T> for FileDescriptor {
    fn interface_init(iface: &mut glib::Interface<Self>) {
        let iface = iface.as_mut();
        iface.file = Some(file_descriptor_file::<T>);
        iface.file_info = Some(file_descriptor_file_info::<T>);
    }
}

unsafe extern "C" fn file_descriptor_file<T: FileDescriptorImpl>(
    fd: *mut ffi::GnomeCmdFileDescriptor,
) -> *mut GFile {
    let instance = unsafe { &*(fd as *mut T::Instance) };
    let imp = instance.imp();
    imp.file().to_glib_none().0
}

unsafe extern "C" fn file_descriptor_file_info<T: FileDescriptorImpl>(
    fd: *mut ffi::GnomeCmdFileDescriptor,
) -> *mut GFileInfo {
    let instance = unsafe { &*(fd as *mut T::Instance) };
    let imp = instance.imp();
    imp.file_info().to_glib_none().0
}

pub trait FileDescriptorExt: IsA<FileDescriptor> + 'static {
    fn file(&self) -> gio::File {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_descriptor_get_file(
                self.as_ref().to_glib_none().0,
            ))
        }
    }

    fn file_info(&self) -> gio::FileInfo {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_descriptor_get_file_info(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
}

impl<O: IsA<FileDescriptor>> FileDescriptorExt for O {}
