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

use super::input_modes::ffi::GVInputModesData;
use gtk::glib::{
    self,
    ffi::{gboolean, gpointer},
    translate::ToGlibPtr,
};
use std::mem::transmute;

pub mod ffi {
    use super::*;
    use crate::intviewer::input_modes::ffi::GVInputModesData;
    use glib::{ffi::GType, gobject_ffi::GCallback};
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GViewerSearcher {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn g_viewer_searcher_get_type() -> GType;

        pub fn g_viewer_searcher_setup_new_text_search(
            src: *mut GViewerSearcher,
            imd: *mut GVInputModesData,
            start_offset: u64,
            max_offset: u64,
            text: *const c_char,
            case_sensitive: gboolean,
        );

        pub fn g_viewer_searcher_setup_new_hex_search(
            src: *mut GViewerSearcher,
            imd: *mut GVInputModesData,
            start_offset: u64,
            max_offset: u64,
            buffer: *const u8,
            buflen: u32,
        );

        pub fn g_viewer_searcher_search(
            src: *mut GViewerSearcher,
            forward: gboolean,
            progress: GCallback,
            user_data: gpointer,
        ) -> gboolean;
        pub fn g_viewer_searcher_get_search_result(src: *mut GViewerSearcher) -> u64;

        pub fn g_viewer_searcher_abort(src: *mut GViewerSearcher);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GViewerSearcherClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct Searcher(Object<ffi::GViewerSearcher, ffi::GViewerSearcherClass>);

    match fn {
        type_ => || ffi::g_viewer_searcher_get_type(),
    }
}

unsafe impl Send for Searcher {}

#[derive(Clone, Copy)]
pub enum SearchProgress {
    Progress(u32),
    Done(Option<u64>),
}

impl Searcher {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn setup_new_text_search(
        &self,
        imd: *mut GVInputModesData,
        start_offset: u64,
        max_offset: u64,
        text: &str,
        case_sensitive: bool,
    ) {
        unsafe {
            ffi::g_viewer_searcher_setup_new_text_search(
                self.to_glib_none().0,
                imd,
                start_offset,
                max_offset,
                text.to_glib_none().0,
                case_sensitive as gboolean,
            )
        }
    }

    pub fn setup_new_hex_search(
        &self,
        imd: *mut GVInputModesData,
        start_offset: u64,
        max_offset: u64,
        buffer: &[u8],
    ) {
        unsafe {
            ffi::g_viewer_searcher_setup_new_hex_search(
                self.to_glib_none().0,
                imd,
                start_offset,
                max_offset,
                buffer.as_ptr(),
                buffer.len() as u32,
            )
        }
    }

    pub fn search<F: Fn(u32) + 'static>(&self, forward: bool, f: F) -> Option<u64> {
        unsafe extern "C" fn response_trampoline<F: Fn(u32) + 'static>(p: u32, f: gpointer) {
            let f: &F = &*(f as *const F);
            f(p)
        }
        let found = unsafe {
            let f: Box<F> = Box::new(f);
            ffi::g_viewer_searcher_search(
                self.to_glib_none().0,
                forward as i32,
                Some(transmute::<_, unsafe extern "C" fn()>(
                    response_trampoline::<F> as *const (),
                )),
                Box::into_raw(f) as gpointer,
            ) != 0
        };
        if found {
            Some(unsafe { ffi::g_viewer_searcher_get_search_result(self.to_glib_none().0) })
        } else {
            None
        }
    }

    pub fn abort(&self) {
        unsafe { ffi::g_viewer_searcher_abort(self.to_glib_none().0) }
    }
}
