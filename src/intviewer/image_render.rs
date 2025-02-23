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

use gtk::{
    gdk_pixbuf,
    glib::{
        ffi::gboolean,
        object::ObjectType,
        translate::{from_glib_borrow, from_glib_none, ToGlibPtr},
    },
};
use std::path::Path;

pub mod ffi {
    use super::*;
    use gtk::{gdk_pixbuf::ffi::GdkPixbuf, glib::ffi::GType};
    use std::ffi::c_char;

    #[repr(C)]
    pub struct ImageRender {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn image_render_get_type() -> GType;

        pub fn image_render_get_origin_pixbuf(r: *mut ImageRender) -> *mut GdkPixbuf;

        pub fn image_render_get_best_fit(r: *mut ImageRender) -> gboolean;
        pub fn image_render_set_best_fit(r: *mut ImageRender, best_fit: gboolean);

        pub fn image_render_set_scale_factor(r: *mut ImageRender, factor: f64);
        pub fn image_render_get_scale_factor(r: *mut ImageRender) -> f64;

        pub fn image_render_operation(r: *mut ImageRender, op: i32);

        pub fn image_render_load_file(r: *mut ImageRender, filename: *const c_char);

        pub fn image_render_notify_status_changed(r: *mut ImageRender);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct ImageRenderClass {
        pub parent_class: gtk::ffi::GtkWidgetClass,
    }
}

glib::wrapper! {
    pub struct ImageRender(Object<ffi::ImageRender, ffi::ImageRenderClass>)
    @extends gtk::Widget;

    match fn {
        type_ => || ffi::image_render_get_type(),
    }
}

impl ImageRender {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn origin_pixbuf(&self) -> Option<gdk_pixbuf::Pixbuf> {
        unsafe { from_glib_none(ffi::image_render_get_origin_pixbuf(self.to_glib_none().0)) }
    }

    pub fn best_fit(&self) -> bool {
        unsafe { ffi::image_render_get_best_fit(self.to_glib_none().0) != 0 }
    }

    pub fn set_best_fit(&self, best_fit: bool) {
        unsafe { ffi::image_render_set_best_fit(self.to_glib_none().0, best_fit as gboolean) }
    }

    pub fn scale_factor(&self) -> f64 {
        unsafe { ffi::image_render_get_scale_factor(self.to_glib_none().0) }
    }

    pub fn set_scale_factor(&self, factor: f64) {
        unsafe { ffi::image_render_set_scale_factor(self.to_glib_none().0, factor) }
    }

    pub fn operation(&self, operation: i32) {
        unsafe { ffi::image_render_operation(self.to_glib_none().0, operation) }
    }

    pub fn load_file(&self, filename: &Path) {
        unsafe { ffi::image_render_load_file(self.to_glib_none().0, filename.to_glib_none().0) }
    }

    pub fn notify_status_changed(&self) {
        unsafe { ffi::image_render_notify_status_changed(self.to_glib_none().0) }
    }

    pub fn connect_image_status_changed<F: Fn(&Self) + 'static>(
        &self,
        f: F,
    ) -> glib::SignalHandlerId {
        unsafe extern "C" fn pressed_trampoline<F: Fn(&ImageRender) + 'static>(
            this: *mut ffi::ImageRender,
            _status: glib::ffi::gpointer,
            f: glib::ffi::gpointer,
        ) {
            let f: &F = &*(f as *const F);
            f(&from_glib_borrow(this))
        }
        unsafe {
            let f: Box<F> = Box::new(f);
            glib::signal::connect_raw(
                self.as_ptr() as *mut _,
                c"image-status-changed".as_ptr() as *const _,
                Some(std::mem::transmute::<*const (), unsafe extern "C" fn()>(
                    pressed_trampoline::<F> as *const (),
                )),
                Box::into_raw(f),
            )
        }
    }
}
