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
use libc::{gid_t, uid_t};

pub mod ffi {
    use super::*;
    use glib::ffi::GType;

    #[repr(C)]
    pub struct GnomeCmdChownComponent {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_chown_component_get_type() -> GType;

        pub fn gnome_cmd_chown_component_new() -> *mut GnomeCmdChownComponent;

        pub fn gnome_cmd_chown_component_set(
            c: *mut GnomeCmdChownComponent,
            owner: uid_t,
            group: gid_t,
        );
        pub fn gnome_cmd_chown_component_get_owner(c: *mut GnomeCmdChownComponent) -> uid_t;
        pub fn gnome_cmd_chown_component_get_group(c: *mut GnomeCmdChownComponent) -> gid_t;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdChownComponentClass {
        pub parent_class: gtk::ffi::GtkGridClass,
    }
}

glib::wrapper! {
    pub struct ChownComponent(Object<ffi::GnomeCmdChownComponent, ffi::GnomeCmdChownComponentClass>)
        @extends gtk::Grid, gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_chown_component_get_type(),
    }
}

impl ChownComponent {
    pub fn new() -> Self {
        unsafe { from_glib_none(ffi::gnome_cmd_chown_component_new()) }
    }

    pub fn ownership(&self) -> (uid_t, gid_t) {
        let owner = unsafe { ffi::gnome_cmd_chown_component_get_owner(self.to_glib_none().0) };
        let group = unsafe { ffi::gnome_cmd_chown_component_get_group(self.to_glib_none().0) };
        (owner, group)
    }

    pub fn set_ownership(&self, owner: uid_t, group: gid_t) {
        unsafe { ffi::gnome_cmd_chown_component_set(self.to_glib_none().0, owner, group) }
    }
}
