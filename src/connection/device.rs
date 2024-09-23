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

use super::connection::Connection;
use gtk::glib::{
    self,
    translate::{from_glib_none, ToGlibPtr},
};
use std::path::PathBuf;

pub mod ffi {
    use crate::connection::connection::ffi::GnomeCmdConClass;
    use gtk::glib::ffi::GType;
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdConDevice {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_device_get_type() -> GType;

        pub fn gnome_cmd_con_device_get_mountp_string(dev: *mut GnomeCmdConDevice)
            -> *const c_char;
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
    pub fn mountp_string(&self) -> Option<PathBuf> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_device_get_mountp_string(
                self.to_glib_none().0,
            ))
        }
    }
}
