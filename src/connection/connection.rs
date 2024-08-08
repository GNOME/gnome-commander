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

use crate::dir::Directory;
use gtk::{
    gio,
    glib::{self, translate::*},
};
use std::{ffi::c_void, path::Path};

pub mod ffi {
    use super::*;
    use crate::dir::ffi::GnomeCmdDir;
    use gtk::{gio::ffi::GFile, glib::ffi::GType};
    use std::ffi::{c_char, c_void};

    #[repr(C)]
    pub struct GnomeCmdCon {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_get_type() -> GType;
        pub fn gnome_cmd_con_get_uuid(con: *const GnomeCmdCon) -> *const c_char;

        pub fn gnome_cmd_con_get_alias(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_set_alias(con: *const GnomeCmdCon, alias: *const c_char);

        pub fn gnome_cmd_con_get_uri(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_set_uri(con: *const GnomeCmdCon, uri: *const c_char);

        pub fn gnome_cmd_con_create_path(
            con: *const GnomeCmdCon,
            path_str: *const c_char,
        ) -> *mut c_void;

        pub fn gnome_cmd_con_create_gfile(
            con: *const GnomeCmdCon,
            uri: *const c_char,
        ) -> *mut GFile;

        pub fn gnome_cmd_con_get_default_dir(con: *const GnomeCmdCon) -> *const GnomeCmdDir;
        pub fn gnome_cmd_con_set_default_dir(con: *const GnomeCmdCon, dir: *mut GnomeCmdDir);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct Connection(Object<ffi::GnomeCmdCon, ffi::GnomeCmdConClass>);

    match fn {
        type_ => || ffi::gnome_cmd_con_get_type(),
    }
}

impl Connection {
    pub fn uuid(&self) -> String {
        unsafe { from_glib_none(ffi::gnome_cmd_con_get_uuid(self.to_glib_none().0)) }
    }
}

pub struct GnomeCmdPath(pub *mut c_void);

pub trait ConnectionExt {
    fn alias(&self) -> Option<String>;
    fn set_alias(&self, alias: Option<&str>);

    fn uri(&self) -> Option<String>;
    fn set_uri(&self, uri: Option<&str>);

    fn create_path(&self, path: &Path) -> GnomeCmdPath;

    fn create_gfile(&self, path: Option<&str>) -> gio::File;

    fn default_dir(&self) -> Option<Directory>;
    fn set_default_dir(&self, dir: Option<&Directory>);
}

impl ConnectionExt for Connection {
    fn alias(&self) -> Option<String> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_get_alias(self.to_glib_none().0)) }
    }

    fn set_alias(&self, alias: Option<&str>) {
        unsafe { ffi::gnome_cmd_con_set_alias(self.to_glib_none().0, alias.to_glib_none().0) }
    }

    fn uri(&self) -> Option<String> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_get_uri(self.to_glib_none().0)) }
    }

    fn set_uri(&self, uri: Option<&str>) {
        unsafe { ffi::gnome_cmd_con_set_uri(self.to_glib_none().0, uri.to_glib_none().0) }
    }

    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        unsafe {
            GnomeCmdPath(ffi::gnome_cmd_con_create_path(
                self.to_glib_none().0,
                path.to_glib_none().0,
            ))
        }
    }

    fn create_gfile(&self, path: Option<&str>) -> gio::File {
        unsafe {
            from_glib_full(ffi::gnome_cmd_con_create_gfile(
                self.to_glib_none().0,
                path.to_glib_none().0,
            ))
        }
    }

    fn default_dir(&self) -> Option<Directory> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_get_default_dir(self.to_glib_none().0)) }
    }
    fn set_default_dir(&self, dir: Option<&Directory>) {
        unsafe { ffi::gnome_cmd_con_set_default_dir(self.to_glib_none().0, dir.to_glib_none().0) }
    }
}
