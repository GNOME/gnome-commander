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

use super::device_dialog::edit_device_dialog;
use crate::{
    connection::{
        connection::{Connection, ConnectionExt},
        device::ConnectionDevice,
        list::ConnectionList,
    },
    dialogs::order_utils::ordering_buttons,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

pub fn devices_widget() -> gtk::Widget {
    let hbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(12)
        .build();

    let selection = gtk::SingleSelection::new(None::<gio::ListModel>);

    let view = gtk::ColumnView::builder()
        .model(&selection)
        .hexpand(true)
        .vexpand(true)
        .build();

    view.append_column(
        &gtk::ColumnViewColumn::builder()
            .factory(&device_icon_factory())
            .build(),
    );
    view.append_column(
        &gtk::ColumnViewColumn::builder()
            .title(gettext("Alias"))
            .expand(true)
            .factory(&device_alias_factory())
            .build(),
    );

    hbox.append(
        &gtk::ScrolledWindow::builder()
            .has_frame(true)
            .child(&view)
            .build(),
    );

    let bbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .spacing(6)
        .build();

    let add_button = gtk::Button::builder()
        .label(gettext("_Add"))
        .use_underline(true)
        .vexpand(false)
        .build();
    bbox.append(&add_button);

    let edit_button = gtk::Button::builder()
        .label(gettext("_Edit"))
        .use_underline(true)
        .vexpand(false)
        .build();
    bbox.append(&edit_button);

    let remove_button = gtk::Button::builder()
        .label(gettext("_Remove"))
        .use_underline(true)
        .vexpand(false)
        .build();
    bbox.append(&remove_button);

    let (up_button, down_button) = ordering_buttons(&selection);
    bbox.append(&up_button);
    bbox.append(&down_button);

    hbox.append(&bbox);

    selection.connect_selected_notify(glib::clone!(
        #[weak]
        edit_button,
        #[weak]
        remove_button,
        move |selection| {
            let has_selection = selection.selected() != gtk::INVALID_LIST_POSITION;
            edit_button.set_sensitive(has_selection);
            remove_button.set_sensitive(has_selection);
        }
    ));

    let list = ConnectionList::get();
    let filter = gtk::FilterListModel::new(
        Some(list.all()),
        Some(gtk::CustomFilter::new(|item| {
            item.downcast_ref::<ConnectionDevice>()
                .map_or(false, |d| !d.autovol())
        })),
    );
    selection.set_model(Some(&filter));

    add_button.connect_clicked(glib::clone!(
        #[weak]
        list,
        #[weak]
        selection,
        move |btn| {
            let Some(parent_window) = btn.root().and_downcast::<gtk::Window>() else {
                return;
            };
            glib::spawn_future_local(async move {
                if let Some(device) = edit_device_dialog(&parent_window, None).await {
                    list.add(&device);
                    selection.set_selected(selection.n_items() - 1);
                }
            });
        }
    ));
    edit_button.connect_clicked(glib::clone!(
        #[weak]
        list,
        #[weak]
        selection,
        move |btn| {
            let Some(parent_window) = btn.root().and_downcast::<gtk::Window>() else {
                return;
            };
            glib::spawn_future_local(async move {
                if let Some(device) = selection.selected_item().and_downcast::<ConnectionDevice>() {
                    if let Some(new_device) =
                        edit_device_dialog(&parent_window, Some(&device)).await
                    {
                        list.replace(&device, &new_device);
                    }
                }
            });
        }
    ));
    remove_button.connect_clicked(glib::clone!(
        #[weak]
        list,
        #[weak]
        selection,
        move |_| {
            if let Some(device) = selection.selected_item().and_downcast::<ConnectionDevice>() {
                list.remove(&device);
            }
        }
    ));

    hbox.upcast()
}

fn device_icon_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        if let Some(list_item) = item.downcast_ref::<gtk::ListItem>() {
            list_item.set_child(Some(&gtk::Image::builder().build()));
        }
    });
    factory.connect_bind(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(image) = list_item.child().and_downcast::<gtk::Image>() else {
            return;
        };
        image.set_icon_name(None);
        if let Some(icon) = list_item
            .item()
            .and_downcast::<Connection>()
            .and_then(|c| c.open_icon())
        {
            image.set_from_gicon(&icon);
        };
    });
    factory.upcast()
}

fn device_alias_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        if let Some(list_item) = item.downcast_ref::<gtk::ListItem>() {
            list_item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
        }
    });
    factory.connect_bind(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_label("");
        if let Some(device) = list_item.item().and_downcast::<Connection>() {
            label.set_label(&device.alias().unwrap_or_else(|| device.uuid()));
        };
    });
    factory.upcast()
}
