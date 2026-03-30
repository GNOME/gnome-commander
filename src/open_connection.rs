// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    connection::{Connection, ConnectionExt},
    debug::debug,
    utils::WindowExt,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

pub async fn open_connection(parent_window: &gtk::Window, con: &Connection) -> bool {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .build();
    dialog.add_css_class("dialog");

    let grid = gtk::Grid::builder().build();
    dialog.set_child(Some(&grid));

    let label = gtk::Label::new(con.open_message().as_deref());
    grid.attach(&label, 0, 0, 1, 1);

    let progress_bar = gtk::ProgressBar::builder()
        .show_text(false)
        .pulse_step(0.02)
        .build();
    grid.attach(&progress_bar, 0, 1, 1, 1);

    let button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::Center)
        .build();
    grid.attach(&button, 0, 2, 1, 1);
    dialog.set_cancel_widget(&button);

    let cancellable = gio::Cancellable::new();

    button.connect_clicked(glib::clone!(
        #[strong]
        cancellable,
        move |_| cancellable.cancel()
    ));

    dialog.present();

    let result = con.open(parent_window, Some(&cancellable)).await;

    dialog.close();

    debug!('m', "connecion open result {:?}", result);
    match result {
        Ok(()) => true,
        Err(error) => {
            error.show(parent_window).await;
            false
        }
    }
}
