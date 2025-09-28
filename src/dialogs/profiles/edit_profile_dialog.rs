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
use crate::utils::{SenderExt, dialog_button_box, display_help};
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::rc::Rc;

pub async fn edit_profile(
    parent: &gtk::Window,
    profile_index: usize,
    help_id: Option<&str>,
    manager: Rc<dyn ProfileManager>,
) -> bool {
    let dialog = gtk::Window::builder()
        .transient_for(parent)
        .title(gettext("Edit Profile"))
        .modal(true)
        .destroy_with_parent(true)
        .resizable(false)
        .build();

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .column_spacing(12)
        .row_spacing(6)
        .build();
    dialog.set_child(Some(&grid));

    let labels_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);

    let entry = gtk::Entry::builder()
        .text(manager.profile_name(profile_index))
        .hexpand(true)
        .build();

    let label = gtk::Label::builder()
        .label(format!("<b>{}</b>", gettext("_Name")))
        .use_underline(true)
        .use_markup(true)
        .xalign(0.0)
        .mnemonic_widget(&entry)
        .build();
    labels_size_group.add_widget(&label);

    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&entry, 1, 0, 1, 1);

    let component = manager.create_component(profile_index, &labels_size_group);
    grid.attach(&component, 0, 1, 2, 1);

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
            move |_| {
                let parent = parent.clone();
                let help_id = help_id.clone();
                glib::spawn_future_local(
                    async move { display_help(&parent, Some(&help_id)).await },
                );
            }
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

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(false)
    ));

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    ok_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        #[weak]
        entry,
        #[weak]
        component,
        move |_| {
            manager.set_profile_name(profile_index, &entry.text());
            manager.copy_component(profile_index, &component);

            sender.toss(true)
        }
    ));

    grid.attach(
        &dialog_button_box(&start_buttons, &[&reset_button, &cancel_button, &ok_button]),
        0,
        2,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));

    dialog.present();
    let result = receiver.recv().await;
    dialog.close();

    result == Ok(true)
}
