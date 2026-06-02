// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    connection::{Connection, ConnectionExt, bookmark::Bookmark},
    utils::{NO_BUTTONS, SenderExt, WindowExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};

pub async fn edit_bookmark_dialog(
    parent: &gtk::Window,
    is_new: bool,
    connection: &Connection,
    bookmark: &Bookmark,
) -> Option<Bookmark> {
    let dialog = gtk::Window::builder()
        .title(if is_new {
            gettext("New Bookmark")
        } else {
            gettext("Edit Bookmark")
        })
        .transient_for(parent)
        .modal(true)
        .destroy_with_parent(true)
        .resizable(false)
        .build();
    dialog.add_css_class("dialog");

    let grid = gtk::Grid::builder().build();
    dialog.set_child(Some(&grid));

    let name_entry = gtk::Entry::builder()
        .hexpand(true)
        .vexpand(true)
        .activates_default(true)
        .build();

    let name_label = gtk::Label::builder()
        .label(gettext("Bookmark _name:"))
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
        .label(gettext("Bookmark _target:"))
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
        .label(if is_new {
            gettext("_Add Bookmark")
        } else {
            gettext("_Update Bookmark")
        })
        .use_underline(true)
        .receives_default(true)
        .sensitive(false)
        .build();
    let original_name = bookmark.name().to_string();
    ok_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        #[weak]
        dialog,
        #[weak]
        name_entry,
        #[weak]
        connection,
        move |_| {
            if name_entry.text() != original_name
                && connection
                    .bookmarks()
                    .iter()
                    .any(|b| b.name() == name_entry.text())
            {
                glib::spawn_future_local(async move {
                    gtk::AlertDialog::builder()
                        .modal(true)
                        .message(gettext("A bookmark with this name already exists. Please choose a different name."))
                        .buttons([gettext("OK")])
                        .cancel_button(0)
                        .default_button(0)
                        .build()
                        .choose_future(Some(&dialog))
                        .await
                });
            } else {
                sender.toss(true);
            }
        }
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
    dialog.set_cancel_widget(&cancel_btn);

    name_entry.set_text(bookmark.name());
    path_entry.set_text(bookmark.path());

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
