// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    dir::Directory,
    file::FileOps,
    file_list::list::FileList,
    utils::{ErrorMessage, NO_BUTTONS, SenderExt, WindowExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{gio, prelude::*};
use std::path::Path;

fn create_empty_file(name: &str, dir: &Directory) -> Result<gio::File, ErrorMessage> {
    if name.is_empty() {
        return Err(ErrorMessage::new(
            gettext("No file name entered"),
            None::<String>,
        ));
    }

    let file = if name.starts_with("/") {
        let con = dir.connection();
        gio::File::for_uri(&con.create_uri(Path::new(&name)))
    } else {
        dir.get_child_gfile(Path::new(&name))
    };

    if let Err(error) = file.create(gio::FileCreateFlags::NONE, gio::Cancellable::NONE) {
        return Err(ErrorMessage::with_error(
            gettext("Cannot create a file"),
            &error,
        ));
    }

    Ok(file)
}

pub async fn show_new_textfile_dialog(parent_window: &gtk::Window, file_list: &FileList) {
    let Some(dir) = file_list.directory() else {
        eprintln!("No directory");
        return;
    };

    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .title(gettext("New Text File"))
        .build();
    dialog.add_css_class("dialog");
    dialog.set_modal(true);

    let grid = gtk::Grid::builder().build();
    dialog.set_child(Some(&grid));

    let entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    let label = gtk::Label::builder()
        .label(gettext("File name:"))
        .mnemonic_widget(&entry)
        .build();

    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&entry, 1, 0, 1, 1);

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    let cancel_btn = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(false)
    ));

    let ok_btn = gtk::Button::builder()
        .label(gettext("Create _File"))
        .use_underline(true)
        .receives_default(true)
        .build();
    ok_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_btn, &ok_btn]),
        -0,
        1,
        2,
        1,
    );

    entry.connect_changed(glib::clone!(
        #[weak]
        ok_btn,
        move |entry| ok_btn.set_sensitive(!entry.text().is_empty())
    ));

    dialog.set_default_widget(Some(&ok_btn));
    dialog.set_cancel_widget(&cancel_btn);

    if let Some(f) = file_list.selected_file() {
        entry.set_text(&f.name());
    }

    dialog.present();

    let file = loop {
        let response = receiver.recv().await;
        if response == Ok(true) {
            let name = entry.text();
            match create_empty_file(&name, &dir) {
                Ok(f) => {
                    break Some(f);
                }
                Err(error_message) => {
                    error_message.show(dialog.upcast_ref()).await;
                }
            }
        } else {
            break None;
        }
    };

    dialog.close();

    // focus the created file (if possible)
    if let Some(file) = file
        && file.parent().is_some_and(|d| dir.file().equal(&d))
    {
        dir.file_created(&file.uri());
        if let Some(focus_filename) = file.basename() {
            file_list.grab_focus();
            file_list.focus_file(&focus_filename, true);
        }
    }
}
