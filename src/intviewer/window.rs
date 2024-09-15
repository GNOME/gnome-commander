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

use super::{
    search_progress_dialog::SearchProgressDialog,
    searcher::{SearchProgress, Searcher},
    text_render::TextRender,
};
use crate::{file::File, utils::pending};
use ffi::GViewerWindow;
use gettextrs::gettext;
use glib::ffi::gboolean;
use gtk::{
    glib::{self, translate::*, Cast},
    prelude::*,
};
use std::{ffi::c_char, ptr};

pub mod ffi {
    use crate::{
        file::ffi::GnomeCmdFile,
        intviewer::{searcher::ffi::GViewerSearcher, text_render::ffi::TextRender},
    };
    use gtk::{ffi::GtkWindowClass, glib::ffi::GType};
    use std::ffi::c_void;

    #[repr(C)]
    pub struct GViewerWindow {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gviewer_window_get_type() -> GType;

        pub fn gviewer_window_file_view(
            f: *mut GnomeCmdFile,
            initial_settings: /* GViewerWindowSettings */ *mut c_void,
        ) -> *mut GViewerWindow;

        pub fn gviewer_window_get_searcher(w: *mut GViewerWindow) -> *mut GViewerSearcher;
        pub fn gviewer_window_get_text_render(w: *mut GViewerWindow) -> *mut TextRender;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GViewerWindowClass {
        pub parent_class: GtkWindowClass,
    }
}

glib::wrapper! {
    pub struct ViewerWindow(Object<ffi::GViewerWindow, ffi::GViewerWindowClass>)
        @extends gtk::Window, gtk::Widget;

    match fn {
        type_ => || ffi::gviewer_window_get_type(),
    }
}

impl ViewerWindow {
    pub fn file_view(file: &File) -> Self {
        unsafe {
            from_glib_none(ffi::gviewer_window_file_view(
                file.to_glib_none().0,
                ptr::null_mut(),
            ))
        }
    }

    fn searcher(&self) -> Searcher {
        unsafe { from_glib_none(ffi::gviewer_window_get_searcher(self.to_glib_none().0)) }
    }

    fn text_render(&self) -> Option<TextRender> {
        unsafe { from_glib_none(ffi::gviewer_window_get_text_render(self.to_glib_none().0)) }
    }
}

async fn say_not_found(parent: &gtk::Window, search_pattern: &str) {
    let dlg = gtk::MessageDialog::builder()
        .transient_for(parent)
        .modal(true)
        .message_type(gtk::MessageType::Info)
        .buttons(gtk::ButtonsType::Ok)
        .text(gettext!("Pattern “{}” was not found", search_pattern))
        .build();
    dlg.run_future().await;
    dlg.close();
}

async fn start_search(
    window: &ViewerWindow,
    search_pattern: &str,
    search_pattern_len: i32,
    forward: bool,
) {
    let searcher = window.searcher();

    let (sender, receiver) = async_channel::bounded::<SearchProgress>(1);

    let thread = std::thread::spawn(glib::clone!(@strong searcher => move || {
        let sender1 = sender.clone();
        let found = searcher.search(forward, move |p| sender1.send_blocking(SearchProgress::Progress(p)).unwrap());
        sender.send_blocking(SearchProgress::Done(found)).unwrap();
    }));

    let progress_dlg = SearchProgressDialog::new(window.upcast_ref(), search_pattern);
    progress_dlg.connect_stop(glib::clone!(@weak searcher => move || searcher.abort()));

    progress_dlg.present();
    let found = loop {
        let message = receiver.recv().await.unwrap();
        match message {
            SearchProgress::Progress(progress) => progress_dlg.set_progress(progress),
            SearchProgress::Done(found) => break found,
        }
    };
    thread.join().unwrap();
    progress_dlg.close();

    pending().await;

    match found {
        Some(result) => {
            let text_render = window.text_render().unwrap();
            text_render.set_marker(
                result,
                if forward {
                    result + search_pattern_len as u64
                } else {
                    result - search_pattern_len as u64
                },
            );
            text_render.ensure_offset_visible(result);
        }
        None => {
            say_not_found(window.upcast_ref(), search_pattern).await;
        }
    }
}

#[no_mangle]
pub extern "C" fn start_search_r(
    window_ptr: *mut GViewerWindow,
    search_pattern_ptr: *const c_char,
    search_pattern_len: i32,
    forward: gboolean,
) {
    let window: ViewerWindow = unsafe { from_glib_none(window_ptr) };
    let search_pattern: String = unsafe { from_glib_none(search_pattern_ptr) };
    let forward: bool = forward != 0;

    glib::MainContext::default().spawn_local(async move {
        start_search(&window, &search_pattern, search_pattern_len, forward).await
    });
}
