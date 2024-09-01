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

use gtk::glib::{self, translate::ToGlibPtr};

pub mod ffi {
    use super::*;
    use glib::ffi::GType;

    #[repr(C)]
    pub struct TextRender {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn text_render_get_type() -> GType;

        pub fn text_render_ensure_offset_visible(w: *mut TextRender, offset: u64);
        pub fn text_render_set_marker(w: *mut TextRender, start: u64, end: u64);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct TextRenderClass {
        pub parent_class: gtk::ffi::GtkDrawingAreaClass,
    }
}

glib::wrapper! {
    pub struct TextRender(Object<ffi::TextRender, ffi::TextRenderClass>);

    match fn {
        type_ => || ffi::text_render_get_type(),
    }
}

impl TextRender {
    pub fn ensure_offset_visible(&self, offset: u64) {
        unsafe { ffi::text_render_ensure_offset_visible(self.to_glib_none().0, offset) }
    }

    pub fn set_marker(&self, start: u64, end: u64) {
        unsafe { ffi::text_render_set_marker(self.to_glib_none().0, start, end) }
    }
}
