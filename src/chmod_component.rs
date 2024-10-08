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

use glib::translate::from_glib_none;
use gtk::glib::{self, translate::ToGlibPtr};

pub mod ffi {
    use super::*;
    use glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdChmodComponent {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_chmod_component_get_type() -> GType;

        pub fn gnome_cmd_chmod_component_new(perms: u32) -> *mut GnomeCmdChmodComponent;

        pub fn gnome_cmd_chmod_component_get_perms(c: *mut GnomeCmdChmodComponent) -> u32;
        pub fn gnome_cmd_chmod_component_set_perms(
            c: *mut GnomeCmdChmodComponent,
            permissions: u32,
        );
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdChmodComponentClass {
        pub parent_class: gtk::ffi::GtkBoxClass,
    }
}

glib::wrapper! {
    pub struct ChmodComponent(Object<ffi::GnomeCmdChmodComponent, ffi::GnomeCmdChmodComponentClass>)
        @extends gtk::Box, gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_chmod_component_get_type(),
    }
}

impl ChmodComponent {
    pub fn new(permissions: u32) -> Self {
        unsafe { from_glib_none(ffi::gnome_cmd_chmod_component_new(permissions)) }
    }

    pub fn permissions(&self) -> u32 {
        unsafe { ffi::gnome_cmd_chmod_component_get_perms(self.to_glib_none().0) }
    }

    pub fn set_permissions(&self, permissions: u32) {
        unsafe { ffi::gnome_cmd_chmod_component_set_perms(self.to_glib_none().0, permissions) }
    }
}
