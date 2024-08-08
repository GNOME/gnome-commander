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

use crate::{config::PREFIX, data::ProgramsOptionsRead, file::File};
use gettextrs::gettext;
use gtk::{gdk, glib, prelude::*};
use std::{
    ffi::{OsStr, OsString},
    sync::OnceLock,
};

pub fn temp_directory() -> &'static tempfile::TempDir {
    static TEMP_DIRECTORY: OnceLock<tempfile::TempDir> = OnceLock::new();
    TEMP_DIRECTORY
        .get_or_init(|| tempfile::tempdir().expect("Cannot create a temporary directory."))
}

pub fn temp_file(f: &File) -> Result<File, ErrorMessage> {
    let name = f.get_name();
    let name_parts = name.as_deref().and_then(|n| n.rsplit_once('.'));

    let temp_file = tempfile::Builder::new()
        .prefix(name_parts.map(|p| p.0).unwrap_or("tmp"))
        .suffix(name_parts.map(|p| p.0).unwrap_or(".tmp"))
        .tempfile_in(temp_directory().path())
        .map_err(|error| ErrorMessage {
            message: gettext("Cannot create a temporary file."),
            secondary_text: Some(error.to_string()),
        })?;

    let path = temp_file.into_temp_path();
    File::new_from_path(&path).ok_or_else(|| ErrorMessage {
        message: gettext!("Cannot create a file for a path {}.", path.display()),
        secondary_text: None,
    })
}

fn substitute_command_argument(command_template: &str, arg: &OsStr) -> OsString {
    let mut cmd = OsString::new();
    match command_template.split_once("%s") {
        Some((before, after)) => {
            cmd.push(before);
            cmd.push(arg);
            cmd.push(after);
        }
        None => {
            cmd.push(command_template);
            cmd.push(" ");
            cmd.push(arg);
        }
    }
    cmd
}

pub fn make_run_in_terminal_command(
    command: &OsStr,
    options: &dyn ProgramsOptionsRead,
) -> OsString {
    let arg = if options.use_gcmd_block() {
        glib::shell_quote(substitute_command_argument(
            &format!("bash -c \"%s; {PREFIX}/bin/gcmd-block\""),
            command,
        ))
    } else {
        glib::shell_quote(command)
    };
    substitute_command_argument(&options.terminal_exec_cmd(), &arg)
}

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

impl ErrorMessage {
    pub fn with_error(message: impl Into<String>, error: &dyn std::error::Error) -> Self {
        Self {
            message: message.into(),
            secondary_text: Some(error.to_string()),
        }
    }
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

pub fn show_error(parent: &gtk::Window, message: &str, error: &dyn std::error::Error) {
    show_message(parent, message, Some(&error.to_string()));
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
