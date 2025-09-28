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
    options::options::ProgramsOptions,
    spawn::run_command_indir,
    utils::{
        ErrorMessage, NO_BUTTONS, SenderExt, channel_send_action, dialog_button_box,
        handle_escape_key,
    },
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::{ffi::OsString, rc::Rc};

pub async fn show_open_with_other_dialog(
    parent_window: &gtk::Window,
    files: &glib::List<File>,
    working_directory: Option<Directory>,
    options: Rc<ProgramsOptions>,
) {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .title(gettext("Open with other…"))
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

    let command_entry = gtk::Entry::builder().activates_default(true).build();
    let label = gtk::Label::builder()
        .label(gettext("Application:"))
        .mnemonic_widget(&command_entry)
        .build();
    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&command_entry, 1, 0, 1, 1);

    let needs_terminal = gtk::CheckButton::with_label("Needs terminal");
    grid.attach(&needs_terminal, 0, 1, 2, 1);

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_button, &ok_button]),
        0,
        2,
        2,
        1,
    );

    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(false)
    ));
    ok_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));
    handle_escape_key(&dialog, &channel_send_action(&sender, false));
    dialog.set_default_widget(Some(&ok_button));

    dialog.present();
    loop {
        let response = receiver.recv().await;
        if response != Ok(true) {
            break;
        }

        let command = command_entry.text();

        if command.trim().is_empty() {
            ErrorMessage::new(gettext("Invalid command"), None::<String>)
                .show(dialog.upcast_ref())
                .await;
            continue;
        }

        let mut full_command = OsString::from(&command);
        for file in files {
            if let Some(path) = file.get_real_path() {
                full_command.push(" ");
                full_command.push(&glib::shell_quote(path));
            } else {
                eprintln!("Failed to get real path for file: {}", file.get_name());
            }
        }

        let working_directory = working_directory
            .as_ref()
            .and_then(|w| w.upcast_ref::<File>().get_real_path());
        match run_command_indir(
            working_directory.as_deref(),
            &full_command,
            needs_terminal.is_active(),
            &*options,
        ) {
            Ok(_) => break,
            Err(error) => {
                error.into_message().show(dialog.upcast_ref()).await;
            }
        }
    }
    dialog.close();
}
