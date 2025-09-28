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

use crate::{
    connection::bookmark::Bookmark,
    utils::{NO_BUTTONS, SenderExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};

pub async fn edit_bookmark_dialog(
    parent: &gtk::Window,
    title: &str,
    bookmark: &Bookmark,
) -> Option<Bookmark> {
    let dialog = gtk::Window::builder()
        .title(title)
        .transient_for(parent)
        .modal(true)
        .destroy_with_parent(true)
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

    let name_entry = gtk::Entry::builder()
        .hexpand(true)
        .vexpand(true)
        .activates_default(true)
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
        .sensitive(false)
        .build();
    ok_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));

    name_entry.connect_changed(glib::clone!(
        #[weak]
        ok_btn,
        #[weak]
        path_entry,
        move |name_entry| ok_btn
            .set_sensitive(!name_entry.text().is_empty() && !path_entry.text().is_empty())
    ));
    path_entry.connect_changed(glib::clone!(
        #[weak]
        ok_btn,
        #[weak]
        name_entry,
        move |path_entry| ok_btn
            .set_sensitive(!name_entry.text().is_empty() && !path_entry.text().is_empty())
    ));

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_btn, &ok_btn]),
        0,
        2,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_btn));

    name_entry.set_text(&bookmark.name());
    path_entry.set_text(&bookmark.path());

    dialog.present();

    let response = receiver.recv().await;
    let result = if response == Ok(true) {
        Some(Bookmark::new(&name_entry.text(), &path_entry.text()))
    } else {
        None
    };

    dialog.close();

    result
}
