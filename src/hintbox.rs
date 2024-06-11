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
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdHintBox {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_hint_box_get_type() -> GType;
        pub fn gnome_cmd_hint_box_new(hint: *const c_char) -> *mut GnomeCmdHintBox;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdHintBoxClass {
        pub parent_class: gtk::ffi::GtkBoxClass,
    }
}

glib::wrapper! {
    pub struct HintBox(Object<ffi::GnomeCmdHintBox, ffi::GnomeCmdHintBoxClass>)
        @extends gtk::Box, gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_hint_box_get_type(),
    }
}

impl HintBox {
    pub fn new(hint: &str) -> Self {
        unsafe { from_glib_none(ffi::gnome_cmd_hint_box_new(hint.to_glib_none().0)) }
    }
}
