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

use crate::{
    data::ProgramsOptionsRead,
    dir::Directory,
    file::File,
    spawn::run_command_indir,
    utils::{close_dialog_with_escape_key, ErrorMessage, Gtk3to4BoxCompat},
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::{ffi::OsString, rc::Rc};

pub async fn show_open_with_other_dialog(
    parent_window: &gtk::Window,
    files: &glib::List<File>,
    working_directory: Option<Directory>,
    options: Rc<dyn ProgramsOptionsRead>,
) {
    let dialog = gtk::Dialog::builder()
        .transient_for(parent_window)
        .title(gettext("Open with other…"))
        .resizable(false)
        .build();

    let content_area = dialog.content_area();
    content_area.set_margin_top(12);
    content_area.set_margin_bottom(12);
    content_area.set_margin_start(12);
    content_area.set_margin_end(12);
    content_area.set_spacing(12);

    let hbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .vexpand(true)
        .build();
    let command_entry = gtk::Entry::builder().activates_default(true).build();
    let label = gtk::Label::builder()
        .label(gettext("Application:"))
        .mnemonic_widget(&command_entry)
        .build();
    hbox.append(&label);
    hbox.append(&command_entry);
    content_area.append(&hbox);

    let needs_terminal = gtk::CheckButton::with_label("Needs terminal");
    content_area.append(&needs_terminal);

    let button_box = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .build();
    let button_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Both);

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::End)
        .build();
    button_box.append(&cancel_button);
    button_size_group.add_widget(&cancel_button);

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    button_box.append(&ok_button);
    button_size_group.add_widget(&ok_button);

    content_area.append(&button_box);

    cancel_button.connect_clicked(glib::clone!(@weak dialog => move |_| {
        dialog.response(gtk::ResponseType::Cancel);
    }));
    ok_button.connect_clicked(glib::clone!(@weak dialog, @weak command_entry, @strong files, @strong working_directory => move |_| {
        let command = command_entry.text();
        let needs_terminal = needs_terminal.is_active();
        let files = files.clone();
        let working_directory = working_directory.clone();
        let options = options.clone();
        glib::spawn_future_local(async move {
            if command.trim().is_empty() {
                ErrorMessage::new(gettext("Invalid command"), None::<String>)
                    .show(dialog.upcast_ref())
                    .await;
                return;
            }

            let mut full_command = OsString::from(&command);
            for file in &files {
                full_command.push(" ");
                full_command.push(&glib::shell_quote(file.get_real_path()));
            }

            let working_directory = working_directory.as_ref().map(|w| w.upcast_ref::<File>().get_real_path());
            match run_command_indir(working_directory.as_deref(), &full_command, needs_terminal, &*options) {
                Ok(_) => {dialog.response(gtk::ResponseType::Ok)}
                Err(error) => {
                    error.into_message().show(dialog.upcast_ref()).await;
                }
            }
        });
    }));
    close_dialog_with_escape_key(&dialog);

    dialog.show_all();
    dialog.present();
    dialog.run_future().await;
    dialog.close();
}
