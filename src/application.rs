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
    gio,
    glib::{self, translate::from_glib_none},
};

pub mod ffi {
    use gtk::{ffi::GtkApplicationClass, glib::ffi::GType};

    #[repr(C)]
    pub struct GnomeCmdApplication {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_application_get_type() -> GType;
        pub fn gnome_cmd_application_new() -> *mut GnomeCmdApplication;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileClass {
        pub parent_class: GtkApplicationClass,
    }
}

glib::wrapper! {
    pub struct GnomeCmdApplication(Object<ffi::GnomeCmdApplication, ffi::GnomeCmdFileClass>)
        @extends gtk::Application, gio::Application;

    match fn {
        type_ => || ffi::gnome_cmd_application_get_type(),
    }
}

impl GnomeCmdApplication {
    pub fn new() -> Self {
        unsafe { from_glib_none(ffi::gnome_cmd_application_new()) }
    }
}
