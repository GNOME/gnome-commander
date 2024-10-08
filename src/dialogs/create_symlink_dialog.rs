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
    libgcmd::file_base::FileBaseExt,
    utils::show_message,
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, prelude::*};
use std::path::Path;

pub async fn show_create_symlink_dialog(
    parent_window: &gtk::Window,
    file: &File,
    directory: &Directory,
    link_name: &str,
) {
    let dialog = gtk::Dialog::builder()
        .transient_for(parent_window)
        .title(gettext("Create Symbolic Link"))
        .resizable(false)
        .modal(true)
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
    let entry = gtk::Entry::builder()
        .text(link_name)
        .activates_default(true)
        .build();
    let label = gtk::Label::builder()
        .label(gettext("Symbolic link name:"))
        .mnemonic_widget(&entry)
        .build();
    hbox.append(&label);
    hbox.append(&entry);
    content_area.append(&hbox);

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

    ok_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        #[weak]
        entry,
        #[strong]
        directory,
        #[strong]
        file,
        move |_| {
            let link_name = entry.text();

            if link_name.is_empty() {
                show_message(dialog.upcast_ref(), &gettext("No file name given"), None);
                return;
            }

            let symlink_ile: gio::File = if link_name.starts_with('/') {
                let con = directory.connection();
                let path = con.create_path(&Path::new(&link_name));
                con.create_gfile(Some(&path.path()))
            } else {
                directory.get_child_gfile(&link_name)
            };
            let absolute_path = file.file().parse_name();

            match symlink_ile.make_symbolic_link(absolute_path, gio::Cancellable::NONE) {
                Ok(_) => {
                    if symlink_ile.parent().map_or(false, |parent| {
                        parent.equal(&directory.upcast_ref::<File>().file())
                    }) {
                        directory.file_created(&symlink_ile.uri());
                    }
                    dialog.response(gtk::ResponseType::Ok);
                }
                Err(error) => {
                    show_message(dialog.upcast_ref(), error.message(), None);
                }
            }
        }
    ));

    cancel_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| {
            dialog.response(gtk::ResponseType::Cancel);
        }
    ));

    let key_controller = gtk::EventControllerKey::new();
    key_controller.connect_key_pressed(glib::clone!(
        #[weak]
        dialog,
        #[upgrade_or]
        glib::Propagation::Proceed,
        move |_, key, _, _| {
            if key == gdk::Key::Escape {
                dialog.response(gtk::ResponseType::Cancel);
                glib::Propagation::Stop
            } else {
                glib::Propagation::Proceed
            }
        }
    ));
    dialog.add_controller(key_controller);

    dialog.present();
    dialog.run_future().await;
    dialog.close();
}
