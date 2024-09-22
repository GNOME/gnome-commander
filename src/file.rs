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

use crate::{
    connection::{
        connection::{Connection, ConnectionExt},
        list::ConnectionList,
    },
    dir::Directory,
    libgcmd::file_base::{FileBase, FileBaseExt},
};
use gtk::{
    gio::{self, prelude::*},
    glib::{self, translate::*, Cast},
};
use std::{
    ffi::CString,
    path::{Path, PathBuf},
    ptr,
};

pub mod ffi {
    use crate::{
        connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir,
        libgcmd::file_base::ffi::GnomeCmdFileBaseClass,
    };
    use gtk::{
        gio::ffi::{GFile, GFileInfo},
        glib::ffi::{gboolean, GError, GType},
    };
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdFile {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_file_get_type() -> GType;

        pub fn gnome_cmd_file_new(
            file_info: *mut GFileInfo,
            dir: *mut GnomeCmdDir,
        ) -> *mut GnomeCmdFile;

        pub fn gnome_cmd_file_new_full(
            file_info: *mut GFileInfo,
            file: *mut GFile,
            dir: *mut GnomeCmdDir,
        ) -> *mut GnomeCmdFile;

        pub fn gnome_cmd_file_get_real_path(f: *const GnomeCmdFile) -> *mut c_char;
        pub fn gnome_cmd_file_get_path_through_parent(f: *const GnomeCmdFile) -> *mut c_char;

        pub fn gnome_cmd_file_get_uri_str(f: *const GnomeCmdFile) -> *mut c_char;
        pub fn gnome_cmd_file_is_local(f: *const GnomeCmdFile) -> gboolean;

        pub fn gnome_cmd_file_is_executable(f: *const GnomeCmdFile) -> gboolean;
        pub fn gnome_cmd_file_execute(f: *const GnomeCmdFile);

        pub fn gnome_cmd_file_chmod(
            f: *mut GnomeCmdFile,
            permissions: u32,
            error: *mut *mut GError,
        ) -> gboolean;

        pub fn gnome_cmd_file_get_connection(f: *mut GnomeCmdFile) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_file_is_dotdot(f: *mut GnomeCmdFile) -> gboolean;

        pub fn gnome_cmd_file_set_deleted(f: *mut GnomeCmdFile);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileClass {
        pub parent_class: GnomeCmdFileBaseClass,
    }
}

glib::wrapper! {
    pub struct File(Object<ffi::GnomeCmdFile, ffi::GnomeCmdFileClass>)
        @extends FileBase;

    match fn {
        type_ => || ffi::gnome_cmd_file_get_type(),
    }
}

impl FileBaseExt for File {
    fn file(&self) -> gio::File {
        self.upcast_ref::<FileBase>().file()
    }

    fn file_info(&self) -> gio::FileInfo {
        self.upcast_ref::<FileBase>().file_info()
    }
}

impl File {
    pub fn new(file_info: &gio::FileInfo, dir: &Directory) -> Option<Self> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_new(
                file_info.to_glib_full(),
                dir.to_glib_none().0,
            ))
        }
    }

    pub fn new_full(file_info: &gio::FileInfo, file: &gio::File, dir: &Directory) -> Option<Self> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_new_full(
                file_info.to_glib_full(),
                file.to_glib_full(),
                dir.to_glib_none().0,
            ))
        }
    }

    pub fn new_from_path(path: &Path) -> Result<Self, glib::Error> {
        let file = gio::File::for_path(path);

        let file_info =
            file.query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)?;

        let parent_path = file
            .parent()
            .and_then(|p| p.path())
            .unwrap_or_else(|| PathBuf::from("/"));

        let home = ConnectionList::get().home();
        let dir_path = home.create_path(&parent_path);
        let dir = Directory::new(&home, dir_path);

        Self::new_full(&file_info, &file, &dir).ok_or_else(|| {
            glib::Error::new(glib::FileError::Failed, "Failed to create File object")
        })
    }

    pub fn get_name(&self) -> String {
        self.file_info().display_name().into()
    }

    pub fn get_real_path(&self) -> PathBuf {
        unsafe { from_glib_full(ffi::gnome_cmd_file_get_real_path(self.to_glib_none().0)) }
    }

    pub fn get_path_through_parent(&self) -> PathBuf {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_get_path_through_parent(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn get_uri_str(&self) -> Option<String> {
        let ptr =
            unsafe { CString::from_raw(ffi::gnome_cmd_file_get_uri_str(self.to_glib_none().0)) };
        Some(ptr.to_str().ok()?.to_string())
    }

    pub fn is_local(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_is_local(self.to_glib_none().0) != 0 }
    }

    pub fn content_type(&self) -> Option<glib::GString> {
        let file_info = self.file_info();
        file_info
            .attribute_string(gio::FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)
            .or_else(|| file_info.attribute_string(gio::FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE))
    }

    pub fn app_info_for_content_type(&self) -> Option<gio::AppInfo> {
        let content_type = self.content_type()?;
        let must_support_uris = !self.file().has_uri_scheme("file");
        gio::AppInfo::default_for_type(&content_type, must_support_uris)
    }

    pub fn is_executable(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_is_executable(self.to_glib_none().0) != 0 }
    }

    pub fn execute(&self) {
        unsafe { ffi::gnome_cmd_file_execute(self.to_glib_none().0) }
    }

    pub fn chmod(&self, permissions: u32) -> Result<(), glib::Error> {
        unsafe {
            let mut error = ptr::null_mut();
            let _is_ok = ffi::gnome_cmd_file_chmod(self.to_glib_none().0, permissions, &mut error);
            if error.is_null() {
                Ok(())
            } else {
                Err(from_glib_full(error))
            }
        }
    }

    pub fn is_dotdot(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_is_dotdot(self.to_glib_none().0) != 0 }
    }

    pub fn set_deleted(&self) {
        unsafe { ffi::gnome_cmd_file_set_deleted(self.to_glib_none().0) }
    }
}

pub trait GnomeCmdFileExt {
    fn connection(&self) -> Connection;
}

impl GnomeCmdFileExt for File {
    fn connection(&self) -> Connection {
        unsafe { from_glib_none(ffi::gnome_cmd_file_get_connection(self.to_glib_none().0)) }
    }
}
