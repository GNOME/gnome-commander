/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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

use gtk::glib::{self, prelude::*, translate::*};

pub mod ffi {
    use gtk::{ffi::GtkWindow, glib::ffi::GType};

    #[repr(C)]
    pub struct GnomeCmdConfigurable {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    unsafe extern "C" {
        pub fn gnome_cmd_configurable_get_type() -> GType;

        pub fn gnome_cmd_configurable_configure(
            cfgbl: *mut GnomeCmdConfigurable,
            parent_window: *const GtkWindow,
        );
    }
}

glib::wrapper! {
    pub struct Configurable(Interface<ffi::GnomeCmdConfigurable, ffi::GnomeCmdConfigurable>);

    match fn {
        type_ => || ffi::gnome_cmd_configurable_get_type(),
    }
}

pub trait ConfigurableExt: IsA<Configurable> + 'static {
    fn configure(&self, parent_window: &gtk::Window) {
        unsafe {
            ffi::gnome_cmd_configurable_configure(
                self.as_ref().to_glib_none().0,
                parent_window.to_glib_none().0,
            )
        }
    }
}

impl<O: IsA<Configurable>> ConfigurableExt for O {}
