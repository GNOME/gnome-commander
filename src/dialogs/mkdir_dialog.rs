// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    file::{File, FileOps},
    utils::{ErrorMessage, SenderExt, WindowExt, dialog_button_box, display_help},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};
use std::path::PathBuf;

fn make_path(parent: &gio::File, filename: &str) -> PathBuf {
    let path = if let Some(rel_path) = filename.strip_prefix("~/") {
        glib::home_dir().join(rel_path)
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
    dialog.add_css_class("dialog");

    let grid = gtk::Grid::builder().build();
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
        let mut value = file.name();
        if !file.is_directory()
            && let Some((p, _)) = value.rsplit_once('.')
        {
            value = p.to_string();
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
        .label(gettext("_Make Directory"))
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
    dialog.set_cancel_widget(&cancel_button);

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
