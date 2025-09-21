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
    connection::{connection::ConnectionExt, device::ConnectionDevice},
    select_directory_button::DirectoryButton,
    select_icon_button::IconButton,
    utils::{
        ErrorMessage, SenderExt, channel_send_action, dialog_button_box, display_help,
        handle_escape_key,
    },
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

pub async fn edit_device_dialog(
    parent_window: &gtk::Window,
    device: Option<&ConnectionDevice>,
) -> Option<ConnectionDevice> {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .title(if device.is_none() {
            gettext("New Device")
        } else {
            gettext("Edit Device")
        })
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

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Alias:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Device/Label:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        1,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Mount point:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        2,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Icon:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        3,
        1,
        1,
    );

    let alias_entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    grid.attach(&alias_entry, 1, 0, 1, 1);

    let device_entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    grid.attach(&device_entry, 1, 1, 1, 1);

    let mount_point_entry = DirectoryButton::default();
    grid.attach(&mount_point_entry, 1, 2, 1, 1);

    let icon_entry = IconButton::default();
    grid.attach(&icon_entry, 1, 3, 1, 1);

    let help_button = gtk::Button::builder()
        .label(gettext("_Help"))
        .use_underline(true)
        .build();
    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .receives_default(true)
        .build();
    grid.attach(
        &dialog_button_box(&[&help_button], &[&cancel_button, &ok_button]),
        0,
        4,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    help_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| {
            glib::spawn_future_local(async move {
                display_help(&dialog, Some("gnome-commander-prefs-devices")).await
            });
        }
    ));
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
    dialog.connect_close_request(glib::clone!(
        #[strong]
        sender,
        move |_| {
            sender.toss(false);
            glib::Propagation::Proceed
        }
    ));
    handle_escape_key(&dialog, &channel_send_action(&sender, false));

    if let Some(device) = device {
        alias_entry.set_text(&device.alias().unwrap_or_default());
        device_entry.set_text(&device.device_fn().unwrap_or_default());
        mount_point_entry.set_file(device.mountp_string().map(|p| gio::File::for_path(p)));
        icon_entry.set_path(
            device
                .icon()
                .and_downcast::<gio::FileIcon>()
                .and_then(|i| i.file().path())
                .as_deref(),
        );
    }

    dialog.present();
    alias_entry.grab_focus();

    let result = loop {
        if receiver.recv().await != Ok(true) {
            break None;
        }

        if alias_entry.text().trim().is_empty() {
            ErrorMessage::brief(gettext("A device needs to have a non-empty alias"))
                .show(&dialog)
                .await;
            alias_entry.grab_focus();
            continue;
        }
        if device_entry.text().trim().is_empty() {
            ErrorMessage::brief(gettext("A device needs to have a non-empty path/label"))
                .show(&dialog)
                .await;
            device_entry.grab_focus();
            continue;
        }
        if mount_point_entry.file().is_none() {
            ErrorMessage::brief(gettext("A device needs to have a non-empty mount point"))
                .show(&dialog)
                .await;
            mount_point_entry.grab_focus();
            continue;
        }
        break Some(ConnectionDevice::new(
            &alias_entry.text(),
            &device_entry.text(),
            &mount_point_entry.file().unwrap().path().unwrap(),
            icon_entry
                .path()
                .map(|p| gio::FileIcon::new(&gio::File::for_path(p)).upcast())
                .as_ref(),
        ));
    };

    dialog.close();

    result
}
