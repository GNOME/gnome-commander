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

use super::profiles::ProfileManager;
use crate::utils::{dialog_button_box, display_help};
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::rc::Rc;

pub async fn edit_profile(
    parent: &gtk::Window,
    profile_index: usize,
    help_id: Option<&str>,
    manager: Rc<dyn ProfileManager>,
) -> bool {
    let dialog = gtk::Dialog::builder()
        .transient_for(parent)
        .title(gettext("Edit Profile"))
        .modal(true)
        .destroy_with_parent(true)
        .resizable(false)
        .build();

    let content_area = dialog.content_area();

    // HIG defaults
    content_area.set_margin_top(12);
    content_area.set_margin_bottom(12);
    content_area.set_margin_start(12);
    content_area.set_margin_end(12);
    content_area.set_spacing(6);

    let entry = gtk::Entry::builder()
        .text(manager.profile_name(profile_index))
        .margin_bottom(6)
        .margin_start(12)
        .build();

    let label = gtk::Label::builder()
        .label(format!("<b>{}</b>", gettext("_Name")))
        .use_underline(true)
        .use_markup(true)
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Center)
        .mnemonic_widget(&entry)
        .build();

    content_area.append(&label);
    content_area.append(&entry);

    let component = manager.create_component(profile_index);
    content_area.append(&component);

    let mut start_buttons = Vec::new();
    if let Some(help_id) = help_id {
        let help_button = gtk::Button::builder()
            .label(gettext("_Help"))
            .use_underline(true)
            .build();
        help_button.connect_clicked(glib::clone!(
            #[strong]
            parent,
            #[to_owned]
            help_id,
            move |_| display_help(&parent, Some(&help_id))
        ));
        start_buttons.push(help_button);
    }

    let reset_button = gtk::Button::builder().label(gettext("Reset")).build();
    reset_button.connect_clicked(glib::clone!(
        #[strong]
        manager,
        #[weak]
        component,
        move |_| {
            manager.reset_profile(profile_index);
            manager.update_component(profile_index, &component);
        }
    ));

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| dialog.response(gtk::ResponseType::Cancel)
    ));

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    ok_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        #[weak]
        entry,
        move |_| {
            manager.set_profile_name(profile_index, &entry.text());
            manager.copy_component(profile_index, &component);

            dialog.response(gtk::ResponseType::Ok)
        }
    ));

    content_area.append(&dialog_button_box(
        &start_buttons,
        &[&reset_button, &cancel_button, &ok_button],
    ));

    dialog.set_default_response(gtk::ResponseType::Ok);

    dialog.present();
    let result = dialog.run_future().await;
    dialog.close();

    result == gtk::ResponseType::Ok
}
