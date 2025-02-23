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

use gettextrs::gettext;
use gtk::{
    glib::{
        ffi::gboolean,
        translate::{from_glib_full, ToGlibPtr},
    },
    prelude::*,
};
use std::fmt;

pub mod ffi {
    use super::*;
    use glib::ffi::GType;
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GViewerSearchDlg {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gviewer_search_dlg_get_type() -> GType;
        pub fn gviewer_search_dlg_get_search_mode(sdlg: *mut GViewerSearchDlg) -> i32;
        pub fn gviewer_search_dlg_get_search_text_string(
            sdlg: *mut GViewerSearchDlg,
        ) -> *mut c_char;
        pub fn gviewer_search_dlg_get_search_hex_buffer(
            sdlg: *mut GViewerSearchDlg,
            buflen: *mut u32,
        ) -> *mut u8;
        pub fn gviewer_search_dlg_get_case_sensitive(sdlg: *mut GViewerSearchDlg) -> gboolean;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GViewerSearchDlgClass {
        pub parent_class: gtk::ffi::GtkDialogClass,
    }
}

glib::wrapper! {
    pub struct SearchDialog(Object<ffi::GViewerSearchDlg, ffi::GViewerSearchDlgClass>)
    @extends gtk::Widget, gtk::Window, gtk::Dialog;

    match fn {
        type_ => || ffi::gviewer_search_dlg_get_type(),
    }
}

impl SearchDialog {
    pub fn new(parent: &gtk::Window) -> Self {
        glib::Object::builder()
            .property("title", gettext("Find"))
            .property("modal", true)
            .property("transient-for", parent)
            .property("resizable", false)
            .build()
    }

    pub fn hex_search_mode(&self) -> bool {
        unsafe { ffi::gviewer_search_dlg_get_search_mode(self.to_glib_none().0) != 0 }
    }

    pub fn search_text(&self) -> String {
        unsafe {
            from_glib_full(ffi::gviewer_search_dlg_get_search_text_string(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn search_hex(&self) -> Vec<u8> {
        unsafe {
            let mut len = 0;
            let data = ffi::gviewer_search_dlg_get_search_hex_buffer(
                self.to_glib_none().0,
                *&mut len as *mut _,
            );
            let slice: &[u8] = std::slice::from_raw_parts(data, len);
            let vec = slice.to_vec();
            glib::ffi::g_free(data as *mut _);
            vec
        }
    }

    pub fn case_sensitive(&self) -> bool {
        unsafe { ffi::gviewer_search_dlg_get_case_sensitive(self.to_glib_none().0) != 0 }
    }

    pub async fn show(parent: &gtk::Window) -> Option<SearchRequest> {
        let dialog = Self::new(parent);
        dialog.present();
        let response_id = dialog.run_future().await;

        if response_id != gtk::ResponseType::Ok {
            dialog.close();
            return None;
        }

        let search_pattern = dialog.search_text();

        let result = if dialog.hex_search_mode() {
            SearchRequest::Binary {
                pattern: dialog.search_hex(),
            }
        } else {
            SearchRequest::Text {
                pattern: search_pattern,
                case_sensitive: dialog.case_sensitive(),
            }
        };

        dialog.close();

        Some(result)
    }
}

#[derive(Clone)]
pub enum SearchRequest {
    Text {
        pattern: String,
        case_sensitive: bool,
    },
    Binary {
        pattern: Vec<u8>,
    },
}

impl SearchRequest {
    pub fn len(&self) -> u64 {
        match self {
            Self::Text { pattern, .. } => pattern.len() as u64,
            Self::Binary { pattern } => pattern.len() as u64,
        }
    }
}

impl fmt::Display for SearchRequest {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Text { pattern, .. } => {
                write!(f, "{pattern}")?;
            }
            Self::Binary { pattern } => {
                // TODO: hex
                write!(f, "{:?}", pattern)?;
            }
        }
        Ok(())
    }
}
