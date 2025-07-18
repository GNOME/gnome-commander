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
    connection::connection::{ffi::GnomeCmdCon, Connection, ConnectionExt},
    data::{GeneralOptions, GeneralOptionsRead},
    debug::debug,
    file_list::list::{ffi::GnomeCmdFileList, FileList},
    utils::{ErrorMessage, SenderExt},
};
use async_channel::TryRecvError;
use gettextrs::gettext;
use gtk::{
    ffi::GtkWindow,
    glib::{self, translate::from_glib_none},
    prelude::*,
};

pub async fn open_connection(file_list: &FileList, parent_window: &gtk::Window, con: &Connection) {
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

    let label = gtk::Label::builder().label(con.open_message()).build();
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

    #[derive(Debug)]
    enum ConnectEvent {
        Done,
        Failure(Option<String>),
        Cancel,
    }
    let (sender, receiver) = async_channel::unbounded::<ConnectEvent>();

    button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(ConnectEvent::Cancel)
    ));
    dialog.connect_close_request(glib::clone!(
        #[strong]
        sender,
        move |_| {
            sender.toss(ConnectEvent::Cancel);
            glib::Propagation::Proceed
        }
    ));

    dialog.present();

    let done = con.connect(
        "open-done",
        false,
        glib::clone!(
            #[strong]
            sender,
            move |_| {
                sender.toss(ConnectEvent::Done);
                None
            }
        ),
    );
    let failed = con.connect(
        "open-failed",
        false,
        glib::clone!(
            #[strong]
            sender,
            move |values| {
                let message: Option<String> =
                    values.get(0).and_then(|v| v.get_owned::<String>().ok());
                sender.toss(ConnectEvent::Failure(message));
                None
            }
        ),
    );
    con.open(parent_window);

    let gui_update_rate = GeneralOptions::new().gui_update_rate();
    let result = loop {
        progress_bar.pulse();
        match receiver.try_recv() {
            Ok(result) => break result,
            Err(TryRecvError::Empty) => {
                glib::timeout_future(gui_update_rate).await;
            }
            Err(TryRecvError::Closed) => {
                break ConnectEvent::Cancel;
            }
        }
    };

    con.disconnect(done);
    con.disconnect(failed);
    dialog.close();

    debug!('m', "connecion open result {:?}", result);
    match result {
        ConnectEvent::Done => file_list.set_connection(con, None),
        ConnectEvent::Failure(message) => {
            ErrorMessage::new(gettext("Failed to open connection."), message)
                .show(parent_window)
                .await;
        }
        ConnectEvent::Cancel => con.cancel_open(),
    }
}

#[no_mangle]
pub extern "C" fn open_connection_r(
    fl: *mut GnomeCmdFileList,
    parent_window: *mut GtkWindow,
    con: *mut GnomeCmdCon,
) {
    let file_list: FileList = unsafe { from_glib_none(fl) };
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window) };
    let con: Connection = unsafe { from_glib_none(con) };
    glib::spawn_future_local(async move {
        open_connection(&file_list, &parent_window, &con).await;
    });
}
