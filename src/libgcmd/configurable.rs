// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
