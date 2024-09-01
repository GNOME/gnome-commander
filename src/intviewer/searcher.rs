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

use std::mem::transmute;

use glib::ffi::gpointer;
use gtk::glib::{self, translate::ToGlibPtr};

pub mod ffi {
    use super::*;
    use glib::{
        ffi::{gboolean, gpointer, GType},
        gobject_ffi::GCallback,
    };

    #[repr(C)]
    pub struct GViewerSearcher {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn g_viewer_searcher_get_type() -> GType;

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
