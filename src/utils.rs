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

use gtk::{gdk, glib, prelude::*};

pub async fn run_simple_dialog(
    parent: &gtk::Window,
    ignore_close_box: bool,
    msg_type: gtk::MessageType,
    text: &str,
    title: &str,
    def_response: Option<u16>,
    buttons: &[&str],
) -> gtk::ResponseType {
    let dialog = gtk::MessageDialog::builder()
        .transient_for(parent)
        .modal(true)
        .message_type(msg_type)
        .text(text)
        .use_markup(true)
        .title(title)
        .deletable(!ignore_close_box)
        .build();
    for (i, button) in buttons.into_iter().enumerate() {
        dialog.add_button(button, gtk::ResponseType::Other(i as u16));
    }

    if let Some(response_id) = def_response {
        dialog.set_default_response(gtk::ResponseType::Other(response_id));
    }

    if ignore_close_box {
        dialog.connect_delete_event(|_, _| glib::Propagation::Stop);
    } else {
        dialog.connect_key_press_event(|dialog, event| {
            if event.keyval() == gdk::keys::constants::Escape {
                dialog.response(gtk::ResponseType::None);
                glib::Propagation::Stop
            } else {
                glib::Propagation::Proceed
            }
        });
    }

    let result = dialog.run_future().await;
    dialog.hide();

    result
}

pub fn close_dialog_with_escape_key(dialog: &gtk::Dialog) {
    let key_controller = gtk::EventControllerKey::new(dialog);
    key_controller.connect_key_pressed(
        glib::clone!(@weak dialog => @default-return false, move |_, key, _, _| {
            if key == *gdk::keys::constants::Escape {
                dialog.response(gtk::ResponseType::Cancel);
                true
            } else {
                false
            }
        }),
    );
}

pub trait Gtk3to4BoxCompat {
    fn append(&self, child: &impl IsA<gtk::Widget>);
}

impl Gtk3to4BoxCompat for gtk::Box {
    fn append(&self, child: &impl IsA<gtk::Widget>) {
        self.add(child);
    }
}
