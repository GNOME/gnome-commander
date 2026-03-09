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
    file::{File, FileOps},
    main_win::{ExecutionTarget, MainWindow},
    utils::{ErrorMessage, NO_BUTTONS, SenderExt, WindowExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::ffi::OsString;

pub async fn show_open_with_other_dialog(parent_window: &MainWindow, files: &glib::List<File>) {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .title(gettext("Open with other…"))
        .resizable(false)
        .build();
    dialog.add_css_class("dialog");

    let grid = gtk::Grid::builder().build();
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
        .label(gettext("_Open"))
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

    dialog.set_default_widget(Some(&ok_button));
    dialog.set_cancel_widget(&cancel_button);

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
            if let Some(path) = file.local_path() {
                full_command.push(" ");
                full_command.push(glib::shell_quote(path));
            } else {
                eprintln!("Failed to get real path for file: {}", file.name());
            }
        }

        if parent_window
            .execute_command(
                &full_command.to_string_lossy(),
                if needs_terminal.is_active() {
                    ExecutionTarget::AnyTerminal
                } else {
                    ExecutionTarget::Background
                },
            )
            .await
        {
            break;
        }
    }
    dialog.close();
}
