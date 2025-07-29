/*
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

use crate::connection::smb::{ffi::GnomeCmdConSmb, ConnectionSmb, SmbResource};
use gtk::{
    gio,
    glib::translate::{from_glib_borrow, from_glib_none, Borrowed, ToGlibPtr},
    prelude::*,
};
use std::{
    ffi::c_char,
    fmt,
    path::{Path, PathBuf},
};

#[derive(Clone)]
pub enum GnomeCmdPath {
    Plain(PathBuf),
    Smb(SmbResource),
}

impl GnomeCmdPath {
    pub fn path(&self) -> PathBuf {
        match self {
            Self::Plain(path) => path.clone(),
            Self::Smb(resource) => resource.path(),
        }
    }

    pub fn parent(&self) -> Option<Self> {
        match self {
            Self::Plain(path) => {
                let absolute_path = if path.is_absolute() {
                    path.clone()
                } else {
                    PathBuf::from("/").join(path)
                };
                let parent_path = gio::File::for_path(&absolute_path).parent()?.path()?;
                Some(Self::Plain(parent_path))
            }
            Self::Smb(resource) => Some(Self::Smb(resource.parent()?)),
        }
    }

    pub fn child(&self, child: &Path) -> Self {
        match self {
            Self::Plain(path) => Self::Plain(path.join(child)),
            Self::Smb(resource) => Self::Smb(resource.child(child)),
        }
    }

    pub fn into_raw(self) -> *mut Self {
        let this = Box::new(self);
        Box::leak(this) as *mut _
    }

    pub unsafe fn from_raw(ptr: *mut Self) -> Self {
        *Box::from_raw(ptr)
    }
}

impl fmt::Display for GnomeCmdPath {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Plain(path) => write!(f, "{}", path.to_string_lossy()),
            Self::Smb(resource) => write!(f, "{resource}"),
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_plain_path_new(path: *const c_char) -> *mut GnomeCmdPath {
    let path: PathBuf = unsafe { from_glib_none(path) };
    GnomeCmdPath::Plain(path).into_raw()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_smb_path_new(
    path: *const c_char,
    smb_con: *mut GnomeCmdConSmb,
) -> *mut GnomeCmdPath {
    if path.is_null() {
        return GnomeCmdPath::Smb(SmbResource::Root).into_raw();
    }
    let con: Borrowed<ConnectionSmb> = unsafe { from_glib_borrow(smb_con) };
    let path: String = unsafe { from_glib_none(path) };
    if let Some(smb_resource) = SmbResource::from_str(&path, con.smb_discovery()) {
        GnomeCmdPath::Smb(smb_resource).into_raw()
    } else {
        eprintln!("Can't find a host or workgroup for path {}", path);
        std::ptr::null_mut()
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_path_get_path(p: *mut GnomeCmdPath) -> *mut c_char {
    let path: &GnomeCmdPath = unsafe { &*p };
    path.path().to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_path_get_display_path(p: *mut GnomeCmdPath) -> *mut c_char {
    let path: &GnomeCmdPath = unsafe { &*p };
    path.to_string().to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_path_get_child(
    p: *mut GnomeCmdPath,
    child: *const c_char,
) -> *mut GnomeCmdPath {
    let path: &GnomeCmdPath = unsafe { &*p };
    let child: PathBuf = unsafe { from_glib_none(child) };
    path.child(&child).into_raw()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_path_get_parent(p: *mut GnomeCmdPath) -> *mut GnomeCmdPath {
    let path: &GnomeCmdPath = unsafe { &*p };
    if let Some(parent) = path.parent() {
        parent.into_raw()
    } else {
        std::ptr::null_mut()
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_path_clone(p: *mut GnomeCmdPath) -> *mut GnomeCmdPath {
    let path: &GnomeCmdPath = unsafe { &*p };
    path.clone().into_raw()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_path_free(p: *mut GnomeCmdPath) {
    if !p.is_null() {
        let _: Box<GnomeCmdPath> = unsafe { Box::from_raw(p) };
    }
}
