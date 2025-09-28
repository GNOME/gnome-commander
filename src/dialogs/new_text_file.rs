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
    dir::Directory,
    file::File,
    file_list::list::FileList,
    utils::{ErrorMessage, NO_BUTTONS, SenderExt, dialog_button_box},
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
        let path = con.create_path(Path::new(&name));
        con.create_gfile(&path)
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

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .column_spacing(12)
        .row_spacing(6)
        .build();
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
        .label(gettext("_OK"))
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

    if let Some(f) = file_list.selected_file() {
        entry.set_text(&f.file_info().display_name());
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
    if let Some(file) = file {
        if file
            .parent()
            .map_or(false, |d| dir.upcast_ref::<File>().file().equal(&d))
        {
            dir.file_created(&file.uri());
            if let Some(focus_filename) = file.basename() {
                file_list.grab_focus();
                file_list.focus_file(&focus_filename, true);
            }
        }
    }
}
