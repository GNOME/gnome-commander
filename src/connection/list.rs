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

use super::{connection::Connection, device::ConnectionDevice, remote::ConnectionRemote};
use gtk::glib::{
    self,
    translate::{from_glib_none, ToGlibPtr},
};

pub mod ffi {
    use super::*;
    use crate::connection::{
        connection::ffi::GnomeCmdCon, device::ffi::GnomeCmdConDevice,
        remote::ffi::GnomeCmdConRemote,
    };
    use glib::ffi::{GList, GType, GVariant};
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdConList {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_list_get_type() -> GType;

        pub fn gnome_cmd_con_list_get_all(list: *mut GnomeCmdConList) -> *const GList;
        pub fn gnome_cmd_con_list_get_all_remote(list: *mut GnomeCmdConList) -> *const GList;
        pub fn gnome_cmd_con_list_get_all_dev(list: *mut GnomeCmdConList) -> *const GList;

        pub fn gnome_cmd_con_list_add_remote(
            list: *mut GnomeCmdConList,
            con: *mut GnomeCmdConRemote,
        );
        pub fn gnome_cmd_con_list_add_dev(list: *mut GnomeCmdConList, con: *mut GnomeCmdConDevice);
        pub fn gnome_cmd_con_list_remove_remote(
            list: *mut GnomeCmdConList,
            con: *mut GnomeCmdConRemote,
        );
        pub fn gnome_cmd_con_list_remove_dev(
            list: *mut GnomeCmdConList,
            con: *mut GnomeCmdConDevice,
        );

        pub fn gnome_cmd_con_list_find_by_uuid(
            list: *mut GnomeCmdConList,
            uuid: *const c_char,
        ) -> *mut GnomeCmdCon;
        pub fn gnome_cmd_con_list_get_home(list: *mut GnomeCmdConList) -> *mut GnomeCmdCon;
        pub fn gnome_cmd_con_list_get_smb(list: *mut GnomeCmdConList) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_con_list_get() -> *mut GnomeCmdConList;

        pub fn gnome_cmd_con_list_load_bookmarks(
            list: *mut GnomeCmdConList,
            variant: *mut GVariant,
        );
        pub fn gnome_cmd_con_list_save_bookmarks(list: *mut GnomeCmdConList) -> *mut GVariant;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConListClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct ConnectionList(Object<ffi::GnomeCmdConList, ffi::GnomeCmdConListClass>);

    match fn {
        type_ => || ffi::gnome_cmd_con_list_get_type(),
    }
}

impl ConnectionList {
    pub fn all(&self) -> glib::List<Connection> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_con_list_get_all(self.to_glib_none().0))
        }
    }

    pub fn all_remote(&self) -> glib::List<Connection> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_con_list_get_all_remote(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn all_dev(&self) -> glib::List<Connection> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_con_list_get_all_dev(self.to_glib_none().0))
        }
    }

    pub fn add_remote(&self, con: &ConnectionRemote) {
        unsafe { ffi::gnome_cmd_con_list_add_remote(self.to_glib_none().0, con.to_glib_full()) }
    }

    pub fn add_dev(&self, con: &ConnectionDevice) {
        unsafe { ffi::gnome_cmd_con_list_add_dev(self.to_glib_none().0, con.to_glib_full()) }
    }

    pub fn remove_remote(&self, con: &ConnectionRemote) {
        unsafe {
            ffi::gnome_cmd_con_list_remove_remote(self.to_glib_none().0, con.to_glib_none().0)
        }
    }

    pub fn remove_dev(&self, con: &ConnectionDevice) {
        unsafe { ffi::gnome_cmd_con_list_remove_dev(self.to_glib_none().0, con.to_glib_none().0) }
    }

    pub fn find_by_uuid(&self, uuid: &str) -> Option<Connection> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_list_find_by_uuid(
                self.to_glib_none().0,
                uuid.to_glib_none().0,
            ))
        }
    }

    pub fn home(&self) -> Connection {
        unsafe { from_glib_none(ffi::gnome_cmd_con_list_get_home(self.to_glib_none().0)) }
    }

    pub fn smb(&self) -> Option<Connection> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_list_get_smb(self.to_glib_none().0)) }
    }

    pub fn get() -> ConnectionList {
        unsafe { from_glib_none(ffi::gnome_cmd_con_list_get()) }
    }

    pub fn load_bookmarks(&self, variant: glib::Variant) {
        unsafe {
            ffi::gnome_cmd_con_list_load_bookmarks(self.to_glib_none().0, variant.to_glib_none().0)
        }
    }

    pub fn save_bookmarks(&self) -> Option<glib::Variant> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_list_save_bookmarks(
                self.to_glib_none().0,
            ))
        }
    }
}
