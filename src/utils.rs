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

use gettextrs::gettext;
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

pub async fn prompt_message(
    parent: &gtk::Window,
    message_type: gtk::MessageType,
    buttons: gtk::ButtonsType,
    message: &str,
    secondary_text: Option<&str>,
) -> gtk::ResponseType {
    let dlg = gtk::MessageDialog::builder()
        .parent(parent)
        .destroy_with_parent(true)
        .message_type(message_type)
        .buttons(buttons)
        .text(message)
        .build();
    dlg.set_secondary_text(secondary_text);
    let result = dlg.run_future().await;
    dlg.close();
    result
}

fn create_error_dialog(parent: &gtk::Window, message: &str) -> gtk::MessageDialog {
    gtk::MessageDialog::builder()
        .parent(parent)
        .destroy_with_parent(true)
        .message_type(gtk::MessageType::Error)
        .buttons(gtk::ButtonsType::Ok)
        .text(message)
        .build()
}

pub struct ErrorMessage {
    pub message: String,
    pub secondary_text: Option<String>,
}

pub fn show_error_message(parent: &gtk::Window, message: &ErrorMessage) {
    let dlg = create_error_dialog(parent, &message.message);
    dlg.set_secondary_text(message.secondary_text.as_deref());
    dlg.present();
}

pub fn show_message(parent: &gtk::Window, message: &str, secondary_text: Option<&str>) {
    let dlg = create_error_dialog(parent, message);
    dlg.set_secondary_text(secondary_text);
    dlg.present();
}

pub fn show_error(parent: &gtk::Window, message: &str, error: &glib::Error) {
    show_message(parent, message, Some(error.message()));
}

pub fn display_help(parent_window: &gtk::Window, link_id: Option<&str>) {
    let mut help_uri = format!("help:{}", crate::config::PACKAGE);
    if let Some(link_id) = link_id {
        help_uri.push('/');
        help_uri.push_str(link_id);
    }

    if let Err(error) =
        gtk::show_uri_on_window(Some(parent_window), &help_uri, gtk::current_event_time())
    {
        show_error(
            parent_window,
            &gettext("There was an error displaying help."),
            &error,
        );
    }
}

pub trait Gtk3to4BoxCompat {
    fn append(&self, child: &impl IsA<gtk::Widget>);
}

impl Gtk3to4BoxCompat for gtk::Box {
    fn append(&self, child: &impl IsA<gtk::Widget>) {
        self.add(child);
    }
}
