// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    main_win::MainWindow,
    plugins::{IncomingPluginMessage, OutgoingPluginMessage, PluginChannel, PluginData},
    utils::{NO_BUTTONS, SenderExt, WindowExt, dialog_button_box},
};
use futures::FutureExt;
use gettextrs::gettext;
use gtk::prelude::*;
use std::{
    borrow::Cow,
    collections::{BTreeMap, BTreeSet},
};

pub async fn show_plugin_manager(channel: &PluginChannel, parent: &MainWindow) {
    if let Some(dialog) = parent.get_dialog::<gtk::Window>("plugins") {
        dialog.present();
        return;
    }

    let dialog = gtk::Window::builder()
        .transient_for(parent)
        .title(gettext("Available plugins"))
        .width_request(500)
        .height_request(300)
        .resizable(true)
        .build();
    dialog.add_css_class("dialog");
    let dialog = parent.set_dialog("plugins", dialog);

    let vbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .build();
    vbox.add_css_class("spacing");
    dialog.set_child(Some(&vbox));

    let list = gtk::ListBox::builder()
        .selection_mode(gtk::SelectionMode::None)
        .build();
    let mut items: BTreeMap<String, gtk::ListBoxRow> = BTreeMap::new();

    let scrolled_window = gtk::ScrolledWindow::builder()
        .child(&list)
        .hexpand(true)
        .vexpand(true)
        .has_frame(true)
        .build();
    vbox.append(&scrolled_window);

    let (sender, receiver) = async_channel::bounded::<()>(1);

    let close_button = gtk::Button::builder()
        .label(gettext("_Close"))
        .use_underline(true)
        .build();
    {
        let sender = sender.clone();
        close_button.connect_clicked(move |_| sender.toss(()));
    }
    dialog.set_cancel_widget(&close_button);

    vbox.append(&dialog_button_box(NO_BUTTONS, &[close_button]));

    dialog.present();
    list.grab_focus();

    channel.send(IncomingPluginMessage::GetPlugins);

    loop {
        futures::select!(
            msg = channel.receive().fuse() => {
                match msg {
                    OutgoingPluginMessage::Plugins(plugins) => {
                        let mut removed: BTreeSet<_> =
                            items.keys().map(|name| name.to_owned()).collect();
                        for (name, data) in plugins.into_iter() {
                            let row = if let Some(row) = items.get(&name) {
                                Cow::Borrowed(row)
                            } else {
                                let row = gtk::ListBoxRow::builder()
                                    .activatable(true)
                                    .css_classes(["plugin-row"])
                                    .build();

                                // Find insertion point for alphabetic sorting
                                let mut next_name = None::<String>;
                                for key in items.keys() {
                                    if key > &name
                                        && next_name
                                            .as_ref()
                                            .is_none_or(|next_name| next_name > key)
                                    {
                                        next_name = Some(key.to_owned());
                                    }
                                }

                                if let Some(next_row) =
                                    next_name.and_then(|name| items.get(&name))
                                {
                                    list.insert(&row, next_row.index());
                                } else {
                                    list.append(&row);
                                }

                                items.insert(name.clone(), row.clone());
                                Cow::Owned(row)
                            };

                            setup_row(&name, &row, data, channel);
                            removed.remove(&name);
                        }

                        for name in removed {
                            if let Some(row) = items.get(&name) {
                                row.unparent();
                            }
                            items.remove(&name);
                        }
                    }

                    OutgoingPluginMessage::PluginUpdated(name, data) => {
                        if let Some(row) = items.get(&name) {
                            setup_row(&name, row, data, channel);
                        }
                    }
                }
            }
            _ = receiver.recv().fuse() => break,
        );
    }
    dialog.close();
}

fn setup_row(name: &str, row: &gtk::ListBoxRow, data: PluginData, channel: &PluginChannel) {
    let hbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .build();

    let description = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .hexpand(true)
        .valign(gtk::Align::Center)
        .build();
    if let Some(name) = data.metadata.name() {
        description.append(
            &gtk::Label::builder()
                .label(name)
                .halign(gtk::Align::Start)
                .css_classes(["title"])
                .build(),
        );
    }
    if let Some(version) = data.metadata.version() {
        description.append(
            &gtk::Label::builder()
                .label(gettext("Version: {}").replace("{}", &version))
                .halign(gtk::Align::Start)
                .build(),
        );
    }
    description.append(
        &gtk::Label::builder()
            .label(gettext("File: {}").replace("{}", name))
            .halign(gtk::Align::Start)
            .build(),
    );
    hbox.append(&description);

    if data.initializing {
        hbox.append(&gtk::Label::builder().label(gettext("Starting…")).build());
    } else {
        if !data.errors.is_empty() {
            let button = gtk::Button::builder()
                .icon_name("dialog-warning")
                .tooltip_text(gettext("Plugin has errors:") + "\n\n" + &data.errors.join("\n\n"))
                .valign(gtk::Align::Center)
                .build();
            button.add_css_class("flat");
            hbox.append(&button);
        }

        if data.metadata.copyright().is_some()
            || data.metadata.comments().is_some()
            || !data.metadata.authors().is_empty()
            || !data.metadata.documenters().is_empty()
            || !data.metadata.translators().is_empty()
            || data.metadata.webpage().is_some()
        {
            let button = gtk::Button::builder()
                .icon_name("help-about")
                .tooltip_text(gettext("About"))
                .valign(gtk::Align::Center)
                .build();
            button.add_css_class("flat");
            hbox.append(&button);
        }

        if data.metadata.enabled() {
            let button = gtk::Button::builder()
                .icon_name("media-playback-stop")
                .tooltip_text(gettext("Stop Plugin"))
                .valign(gtk::Align::Center)
                .build();
            button.add_css_class("flat");
            let name = name.to_owned();
            let channel = channel.clone();
            button.connect_clicked(move |_| {
                channel.send(IncomingPluginMessage::StopPlugin(name.clone()));
            });
            hbox.append(&button);
        } else {
            let button = gtk::Button::builder()
                .icon_name("media-playback-start")
                .tooltip_text(gettext("Start Plugin"))
                .valign(gtk::Align::Center)
                .build();
            button.add_css_class("flat");
            let name = name.to_owned();
            let channel = channel.clone();
            button.connect_clicked(move |_| {
                channel.send(IncomingPluginMessage::StartPlugin(name.clone()));
            });
            hbox.append(&button);
        }
    }

    row.set_child(Some(&hbox));
}
