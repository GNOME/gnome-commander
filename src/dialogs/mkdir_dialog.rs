/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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
    dir::Directory,
    file::File,
    utils::{
        ErrorMessage, SenderExt, channel_send_action, dialog_button_box, display_help,
        handle_escape_key,
    },
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};
use std::path::PathBuf;

fn make_path(parent: &gio::File, filename: &str) -> PathBuf {
    let path = if filename.starts_with("~/") {
        glib::home_dir().join(&filename[2..])
    } else {
        PathBuf::from(filename)
    };
    if path.is_absolute() {
        path
    } else if let Some(parent) = parent.path() {
        parent.join(path)
    } else {
        path
    }
}

pub async fn show_mkdir_dialog(
    parent_window: &gtk::Window,
    parent_dir: &gio::File,
    selected_file: Option<&File>,
) -> Option<gio::File> {
    let dialog = gtk::Window::builder()
        .title(gettext("Make Directory"))
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .build();

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
    dialog.set_child(Some(&grid));

    let entry = gtk::Entry::builder()
        .activates_default(true)
        .hexpand(true)
        .build();
    let label = gtk::Label::builder()
        .label(gettext("Directory name:"))
        .use_underline(true)
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Center)
        .mnemonic_widget(&entry)
        .build();

    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&entry, 1, 0, 1, 1);

    if let Some(file) = selected_file {
        let mut value = file.get_name();
        if !file.is::<Directory>() {
            if let Some((p, _)) = value.rsplit_once('.') {
                value = p.to_string();
            }
        }
        entry.set_text(&value);
    }

    let (sender, receiver) = async_channel::bounded::<gtk::ResponseType>(1);

    let help_button = gtk::Button::builder()
        .label(gettext("_Help"))
        .use_underline(true)
        .build();
    help_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(gtk::ResponseType::Help)
    ));

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(gtk::ResponseType::Cancel)
    ));

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    ok_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(gtk::ResponseType::Ok)
    ));

    grid.attach(
        &dialog_button_box(&[&help_button], &[&cancel_button, &ok_button]),
        0,
        1,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));
    handle_escape_key(
        &dialog,
        &channel_send_action(&sender, gtk::ResponseType::Cancel),
    );

    dialog.present();

    let file = loop {
        let response = receiver.recv().await;
        match response {
            Ok(gtk::ResponseType::Help) => {
                display_help(dialog.upcast_ref(), Some("gnome-commander-create-folder")).await;
            }
            Ok(gtk::ResponseType::Ok) => {
                let filename = entry.text();
                if filename.is_empty() {
                    ErrorMessage::brief(gettext("A directory name must be entered"))
                        .show(dialog.upcast_ref())
                        .await;
                } else {
                    let path = make_path(parent_dir, &filename);
                    let file = gio::File::for_path(&path);
                    match file.make_directory_with_parents(gio::Cancellable::NONE) {
                        Ok(()) => {
                            break Some(file);
                        }
                        Err(error) => {
                            ErrorMessage::with_error(gettext("Make directory failed"), &error)
                                .show(dialog.upcast_ref())
                                .await;
                            break None;
                        }
                    }
                }
            }
            _ => break None,
        }
    };
    dialog.close();

    file
}
