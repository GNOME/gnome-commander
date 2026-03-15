// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::utils::{SenderExt, toggle_file_name_selection};
use gtk::{gdk, glib, prelude::*};

pub async fn show_rename_popover(
    name: &str,
    parent: &impl IsA<gtk::Widget>,
    rect: &gdk::Rectangle,
) -> Option<String> {
    let entry = gtk::Entry::builder()
        .width_request(rect.width())
        .height_request(rect.height())
        .text(name)
        .build();

    let popover = gtk::Popover::builder()
        .child(&entry)
        .pointing_to(rect)
        .build();

    let (sender, receiver) = async_channel::bounded::<Option<String>>(1);

    popover.connect_closed(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(None)
    ));

    let key_controller = gtk::EventControllerKey::builder()
        .propagation_phase(gtk::PropagationPhase::Capture)
        .build();
    key_controller.connect_key_pressed(glib::clone!(
        #[weak]
        entry,
        #[strong]
        sender,
        #[upgrade_or]
        glib::Propagation::Proceed,
        move |_, key, _, _| {
            match key {
                gdk::Key::Escape => {
                    sender.toss(None);
                    glib::Propagation::Stop
                }
                gdk::Key::Return | gdk::Key::KP_Enter => {
                    sender.toss(Some(entry.text().to_string()));
                    glib::Propagation::Stop
                }
                gdk::Key::F2 | gdk::Key::F5 | gdk::Key::F6 => {
                    toggle_file_name_selection(&entry);
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }
    ));
    entry.add_controller(key_controller);

    popover.set_parent(parent);
    popover.present();
    popover.popup();

    entry.grab_focus();
    entry.select_region(0, -1);

    let result = receiver
        .recv()
        .await
        .ok()
        .flatten()
        .filter(|n| !n.is_empty());
    popover.popdown();

    result
}
