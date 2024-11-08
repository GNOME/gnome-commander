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
    connection::connection::ConnectionExt,
    dir::Directory,
    file::{File, GnomeCmdFileExt},
    file_list::{ffi::GnomeCmdFileList, FileList},
    libgcmd::file_base::FileBaseExt,
    utils::{dialog_button_box, ErrorMessage, NO_BUTTONS},
};
use gettextrs::gettext;
use gtk::{ffi::GtkWindow, gio, glib::translate::from_glib_none, prelude::*};
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
        con.create_gfile(Some(&path.path()))
    } else {
        dir.get_child_gfile(&name)
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
    let dialog = gtk::Dialog::builder()
        .transient_for(parent_window)
        .title(gettext("New Text File"))
        .build();

    let content_area = dialog.content_area();

    content_area.set_margin_top(12);
    content_area.set_margin_bottom(12);
    content_area.set_margin_start(12);
    content_area.set_margin_end(12);
    content_area.set_spacing(12);

    let entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    let label = gtk::Label::builder()
        .label(gettext("File name:"))
        .mnemonic_widget(&entry)
        .build();

    let hbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .build();
    hbox.append(&label);
    hbox.append(&entry);
    content_area.append(&hbox);

    let Some(dir) = file_list.cwd() else {
        eprintln!("No directory");
        return;
    };

    if let Some(f) = file_list.selected_file() {
        entry.set_text(&f.file_info().display_name());
    }

    let cancel_btn = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_btn.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| dialog.response(gtk::ResponseType::Cancel)
    ));

    let ok_btn = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .receives_default(true)
        .build();
    ok_btn.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| dialog.response(gtk::ResponseType::Ok)
    ));

    content_area.append(&dialog_button_box(NO_BUTTONS, &[&cancel_btn, &ok_btn]));

    dialog.set_default_widget(Some(&ok_btn));
    dialog.present();

    let file = loop {
        let response = dialog.run_future().await;
        if response == gtk::ResponseType::Ok {
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
                file_list.focus_file(&focus_filename, true);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_show_new_textfile_dialog(
    parent_window_ptr: *mut GtkWindow,
    file_list_ptr: *mut GnomeCmdFileList,
) {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };
    let file_list: FileList = unsafe { from_glib_none(file_list_ptr) };
    glib::spawn_future_local(async move {
        show_new_textfile_dialog(&parent_window, &file_list).await;
    });
}
