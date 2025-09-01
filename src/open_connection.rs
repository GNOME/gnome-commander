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
    connection::connection::{Connection, ConnectionExt},
    debug::debug,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

pub async fn open_connection(parent_window: &gtk::Window, con: &Connection) -> bool {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .build();

    let grid = gtk::Grid::builder()
        .margin_start(12)
        .margin_end(12)
        .margin_top(12)
        .margin_bottom(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
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

    let cancellable = gio::Cancellable::new();

    button.connect_clicked(glib::clone!(
        #[strong]
        cancellable,
        move |_| cancellable.cancel()
    ));
    dialog.connect_close_request(glib::clone!(
        #[strong]
        cancellable,
        move |_| {
            cancellable.cancel();
            glib::Propagation::Proceed
        }
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
