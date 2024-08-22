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

use crate::utils::Gtk3to4BoxCompat;
use gettextrs::gettext;
use gtk::{
    ffi::GtkWindow,
    glib::{
        self,
        ffi::gpointer,
        translate::{from_glib_none, ToGlibPtr},
    },
    prelude::*,
};
use std::{
    ffi::{c_char, CStr},
    os::raw::c_void,
};

async fn get_selected_range(
    parent_window: &gtk::Window,
    placeholder: &str,
    filename: Option<&str>,
) -> Option<String> {
    let filename = filename.unwrap_or(concat!("Lorem ipsum dolor sit amet, consectetur adipisici elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ",
                   "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. ",
                   "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. ",
                   "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."));

    let dialog = gtk::Dialog::builder()
        .title(gettext("Range Selection"))
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .width_request(480)
        .build();

    let content_area = dialog.content_area();

    // HIG defaults
    content_area.set_margin_top(12);
    content_area.set_margin_bottom(12);
    content_area.set_margin_start(12);
    content_area.set_margin_end(12);
    content_area.set_spacing(6);

    let hbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .build();
    content_area.append(&hbox);

    let entry = gtk::Entry::builder()
        .text(filename)
        .activates_default(true)
        .hexpand(true)
        .build();
    let label = gtk::Label::builder()
        .label(gettext("_Select range:"))
        .use_underline(true)
        .mnemonic_widget(&entry)
        .build();

    hbox.append(&label);
    hbox.append(&entry);

    let option = gtk::CheckButton::builder()
        .label(gettext("_Inverse selection"))
        .use_underline(true)
        .build();
    content_area.append(&option);

    let bbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .build();
    let bbox_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Both);
    content_area.append(&bbox);

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::End)
        .build();
    cancel_button.connect_clicked(glib::clone!(@strong sender => move |_|
        if let Err(error) = sender.send_blocking(false) {
            eprintln!("Failed to send a 'cancel' message: {error}.")
        }
    ));
    bbox.append(&cancel_button);
    bbox_size_group.add_widget(&cancel_button);

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    ok_button.connect_clicked(glib::clone!(@strong sender => move |_|
        if let Err(error) = sender.send_blocking(true) {
            eprintln!("Failed to send an 'ok' message: {error}.")
        }
    ));
    bbox.append(&ok_button);
    bbox_size_group.add_widget(&ok_button);

    content_area.show_all();
    dialog.set_default_response(gtk::ResponseType::Ok);

    dialog.present();
    let result = receiver.recv().await.unwrap_or_default();
    dialog.close();

    let range = if result {
        let inversed = option.is_active();

        if let Some((begin, end)) = entry.selection_bounds() {
            let len = i32::from(entry.text_length());
            if !inversed {
                if end == len {
                    Some(format!("{placeholder}({begin}:)"))
                } else {
                    Some(format!("{placeholder}({begin}:{end})"))
                }
            } else {
                let b = (begin != 0).then(|| format!("{placeholder}(:{begin})"));
                let e = (end != len).then(|| format!("{placeholder}({end}:)"));
                Some(format!(
                    "{}{}",
                    b.unwrap_or_default(),
                    e.unwrap_or_default()
                ))
            }
        } else {
            if !inversed {
                None
            } else {
                Some(placeholder.to_owned())
            }
        }
    } else {
        None
    };

    range
}

extern "C" {
    fn get_selected_range_done(rtag: *const c_char, user_data: gpointer);
}

#[no_mangle]
pub extern "C" fn get_selected_range_r(
    parent_window_ptr: *const GtkWindow,
    placeholder_ptr: *const c_char,
    filename_ptr: *const c_char,
    user_data: gpointer,
) {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };
    let placeholder = unsafe { CStr::from_ptr(placeholder_ptr).to_string_lossy() };

    let filename = if filename_ptr.is_null() {
        None
    } else {
        unsafe { CStr::from_ptr(filename_ptr).to_str().ok() }
    }
    .map(|s| s.to_owned());
    unsafe {
        glib::ffi::g_free(filename_ptr as *mut c_void);
    }

    glib::MainContext::default().spawn_local(async move {
        if let Some(rtag) =
            get_selected_range(&parent_window, placeholder.as_ref(), filename.as_deref()).await
        {
            unsafe {
                get_selected_range_done(rtag.to_glib_none().0, user_data);
            }
        }
    });
}
