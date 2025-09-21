/*
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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
    app::{AppExt, AppTarget, UserDefinedApp},
    select_icon_button::IconButton,
    utils::{
        ErrorMessage, NO_BUTTONS, SenderExt, attributes_bold, channel_send_action,
        dialog_button_box, handle_escape_key,
    },
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::collections::BTreeSet;

pub async fn edit_app_dialog(
    parent_window: &gtk::Window,
    user_app: &mut UserDefinedApp,
    is_new: bool,
    taken_names: BTreeSet<String>,
) -> bool {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .title(if is_new {
            gettext("New Application")
        } else {
            gettext("Edit Application")
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
            .label(gettext("Label:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Command:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        1,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Icon:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        2,
        1,
        1,
    );

    let name_entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    grid.attach(&name_entry, 1, 0, 1, 1);

    let cmd_entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    grid.attach(&cmd_entry, 1, 1, 1, 1);

    let icon_entry = IconButton::default();
    grid.attach(&icon_entry, 1, 2, 1, 1);

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Options"))
            .attributes(&attributes_bold())
            .halign(gtk::Align::Start)
            .build(),
        0,
        3,
        2,
        1,
    );

    let handles_multiple = gtk::CheckButton::builder()
        .label(gettext("Can handle multiple files"))
        .build();
    grid.attach(&handles_multiple, 0, 4, 2, 1);

    let handles_uris = gtk::CheckButton::builder()
        .label(gettext("Can handle URIs"))
        .build();
    grid.attach(&handles_uris, 0, 5, 2, 1);

    let requires_terminal = gtk::CheckButton::builder()
        .label(gettext("Requires terminal"))
        .build();
    grid.attach(&requires_terminal, 0, 6, 2, 1);

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Show for"))
            .attributes(&attributes_bold())
            .halign(gtk::Align::Start)
            .build(),
        0,
        7,
        2,
        1,
    );

    let show_for_all_files = gtk::CheckButton::builder()
        .label(gettext("All files"))
        .build();
    grid.attach(&show_for_all_files, 0, 8, 2, 1);

    let show_for_all_dirs = gtk::CheckButton::builder()
        .label(gettext("All directories"))
        .group(&show_for_all_files)
        .build();
    grid.attach(&show_for_all_dirs, 0, 9, 2, 1);

    let show_for_all_dirs_and_files = gtk::CheckButton::builder()
        .label(gettext("All directories and files"))
        .group(&show_for_all_files)
        .build();
    grid.attach(&show_for_all_dirs_and_files, 0, 10, 2, 1);

    let show_for_some_files = gtk::CheckButton::builder()
        .label(gettext("Some files"))
        .group(&show_for_all_files)
        .build();
    grid.attach(&show_for_some_files, 0, 11, 2, 1);

    let pattern_label = gtk::Label::builder()
        .label(gettext("File patterns"))
        .margin_start(24)
        .build();
    grid.attach(&pattern_label, 0, 12, 1, 1);

    let pattern_entry = gtk::Entry::builder().activates_default(true).build();
    grid.attach(&pattern_entry, 1, 12, 1, 1);

    show_for_some_files.connect_toggled(glib::clone!(
        #[strong]
        pattern_entry,
        move |toggle| {
            if toggle.is_active() {
                pattern_label.set_sensitive(true);
                pattern_entry.set_sensitive(true);
                pattern_entry.grab_focus();
            } else {
                pattern_label.set_sensitive(false);
                pattern_entry.set_sensitive(false);
            }
        }
    ));

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
        &dialog_button_box(NO_BUTTONS, &[&cancel_button, &ok_button]),
        0,
        13,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));

    let (sender, receiver) = async_channel::bounded::<bool>(1);

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

    {
        name_entry.set_text(&user_app.name());
        cmd_entry.set_text(&user_app.command_template);
        icon_entry.set_path(user_app.icon_path.as_deref());
        handles_multiple.set_active(user_app.handles_multiple);
        handles_uris.set_active(user_app.handles_uris);
        requires_terminal.set_active(user_app.requires_terminal);

        match user_app.target {
            AppTarget::AllFiles => &show_for_all_files,
            AppTarget::AllDirs => &show_for_all_dirs,
            AppTarget::AllDirsAndFiles => &show_for_all_dirs_and_files,
            AppTarget::SomeFiles => &show_for_some_files,
        }
        .set_active(true);

        pattern_entry.set_text(&user_app.pattern_string);
    }

    dialog.present();
    name_entry.grab_focus();

    let result = loop {
        if receiver.recv().await != Ok(true) {
            break false;
        }

        let name = name_entry.text();
        if name.trim().is_empty() {
            ErrorMessage::brief(gettext("An app needs to have a non-empty label"))
                .show(&dialog)
                .await;
            continue;
        }
        if taken_names.contains(name.as_str()) {
            ErrorMessage::brief(gettext(
                "An app with this label exists already.\nPlease choose another label.",
            ))
            .show(&dialog)
            .await;
            continue;
        }
        break true;
    };

    if result {
        user_app.name = name_entry.text().to_string();
        user_app.command_template = cmd_entry.text().to_string();
        user_app.icon_path = icon_entry.path();
        if show_for_all_files.is_active() {
            user_app.target = AppTarget::AllFiles;
            user_app.pattern_string.clear();
        } else if show_for_all_dirs.is_active() {
            user_app.target = AppTarget::AllDirs;
            user_app.pattern_string.clear();
        } else if show_for_all_dirs_and_files.is_active() {
            user_app.target = AppTarget::AllDirsAndFiles;
            user_app.pattern_string.clear();
        } else {
            user_app.target = AppTarget::SomeFiles;
            user_app.pattern_string = pattern_entry.text().to_string();
        }
        user_app.handles_uris = handles_uris.is_active();
        user_app.handles_multiple = handles_multiple.is_active();
        user_app.requires_terminal = requires_terminal.is_active();
    }

    dialog.close();

    result
}
