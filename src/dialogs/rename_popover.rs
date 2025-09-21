/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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
