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

use crate::utils::{dialog_button_box, ErrorMessage, NO_BUTTONS};
use gettextrs::gettext;
use gtk::{glib, prelude::*};

pub struct Bookmark {
    pub name: String,
    pub path: String,
}

pub async fn edit_bookmark_dialog(
    parent: &gtk::Window,
    title: &str,
    bookmark: &Bookmark,
) -> Option<Bookmark> {
    let dialog = gtk::Dialog::builder()
        .title(title)
        .transient_for(parent)
        .modal(true)
        .destroy_with_parent(true)
        .resizable(false)
        .build();

    let content_area = dialog.content_area();

    // HIG defaults
    content_area.set_margin_top(10);
    content_area.set_margin_bottom(10);
    content_area.set_margin_start(10);
    content_area.set_margin_end(10);
    content_area.set_spacing(6);

    let grid = gtk::Grid::builder()
        .row_spacing(6)
        .column_spacing(12)
        .build();
    content_area.append(&grid);

    let name_entry = gtk::Entry::builder()
        .hexpand(true)
        .vexpand(true)
        .activates_default(true)
        .text(&bookmark.name)
        .build();

    let name_label = gtk::Label::builder()
        .label(&gettext("Bookmark _name:"))
        .use_underline(true)
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Center)
        .hexpand(true)
        .vexpand(true)
        .mnemonic_widget(&name_entry)
        .build();

    grid.attach(&name_label, 0, 0, 1, 1);
    grid.attach(&name_entry, 1, 0, 1, 1);

    let path_entry = gtk::Entry::builder()
        .hexpand(true)
        .vexpand(true)
        .activates_default(true)
        .text(&bookmark.path)
        .build();

    let path_label = gtk::Label::builder()
        .label(&gettext("Bookmark _target:"))
        .use_underline(true)
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Center)
        .hexpand(true)
        .vexpand(true)
        .mnemonic_widget(&path_entry)
        .build();

    grid.attach(&path_label, 0, 1, 1, 1);
    grid.attach(&path_entry, 1, 1, 1, 1);

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

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[cancel_btn, ok_btn]),
        0,
        2,
        2,
        1,
    );

    dialog.set_default_response(gtk::ResponseType::Ok);

    dialog.connect_response(glib::clone!(
        #[strong]
        name_entry,
        #[strong]
        path_entry,
        move |dlg, response| {
            let validation = if response == gtk::ResponseType::Ok {
                if name_entry.text().is_empty() {
                    Err(ErrorMessage::new(
                        gettext("Bookmark name is missing."),
                        None::<String>,
                    ))
                } else if path_entry.text().is_empty() {
                    Err(ErrorMessage::new(
                        gettext("Bookmark target is missing."),
                        None::<String>,
                    ))
                } else {
                    Ok(())
                }
            } else {
                Ok(())
            };
            if let Err(error) = validation {
                let dlg = dlg.clone();
                glib::spawn_future_local(async move {
                    glib::signal::signal_stop_emission_by_name(&dlg, "response");
                    error.show(dlg.upcast_ref()).await;
                });
            }
        }
    ));

    let response = dialog.run_future().await;

    let result = if response == gtk::ResponseType::Ok {
        Some(Bookmark {
            name: name_entry.text().to_string(),
            path: path_entry.text().to_string(),
        })
    } else {
        None
    };

    dialog.close();

    result
}
