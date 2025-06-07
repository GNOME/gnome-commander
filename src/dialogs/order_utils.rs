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

use crate::utils::swap_list_store_items;
use gettextrs::gettext;
use gtk::{gio, prelude::*};

pub fn ordering_buttons(selection: &gtk::SingleSelection) -> (gtk::Button, gtk::Button) {
    let up_button = gtk::Button::builder()
        .label(gettext("_Up"))
        .use_underline(true)
        .build();

    let down_button = gtk::Button::builder()
        .label(gettext("_Down"))
        .use_underline(true)
        .build();

    fn update_buttons(
        selection: &gtk::SingleSelection,
        up_button: &gtk::Button,
        down_button: &gtk::Button,
    ) {
        let position = selection.selected();
        if position != gtk::INVALID_LIST_POSITION {
            let len = selection.n_items();
            up_button.set_sensitive(position > 0);
            down_button.set_sensitive(position + 1 < len);
        } else {
            up_button.set_sensitive(false);
            down_button.set_sensitive(false);
        }
    }

    selection.connect_items_changed(glib::clone!(
        #[weak]
        up_button,
        #[weak]
        down_button,
        move |selection, _, _, _| update_buttons(selection, &up_button, &down_button)
    ));

    selection.connect_selected_notify(glib::clone!(
        #[weak]
        up_button,
        #[weak]
        down_button,
        move |selection| update_buttons(selection, &up_button, &down_button)
    ));

    up_button.connect_clicked(glib::clone!(
        #[weak]
        selection,
        move |_| {
            let position = selection.selected();
            if position == gtk::INVALID_LIST_POSITION {
                return;
            };
            let Some(item) = selection.item(position) else {
                return;
            };
            let Some(prev_position) = position.checked_sub(1) else {
                return;
            };
            let Some(prev_item) = selection.item(prev_position) else {
                return;
            };
            if let Some(store) = selection.model().as_ref().and_then(underlying_store) {
                swap_list_store_items(&store, &item, &prev_item);
                selection.set_selected(prev_position);
            }
        }
    ));
    down_button.connect_clicked(glib::clone!(
        #[weak]
        selection,
        move |_| {
            let position = selection.selected();
            if position == gtk::INVALID_LIST_POSITION {
                return;
            };
            let Some(item) = selection.item(position) else {
                return;
            };
            let Some(next_position) = position.checked_add(1).filter(|p| *p < selection.n_items())
            else {
                return;
            };
            let Some(next_item) = selection.item(next_position) else {
                return;
            };
            if let Some(store) = selection.model().as_ref().and_then(underlying_store) {
                swap_list_store_items(&store, &item, &next_item);
                selection.set_selected(next_position);
            }
        }
    ));

    update_buttons(selection, &up_button, &down_button);

    (up_button, down_button)
}

fn underlying_store(model: &gio::ListModel) -> Option<gio::ListStore> {
    if let Some(store) = model.downcast_ref::<gio::ListStore>() {
        Some(store.clone())
    } else if let Some(map) = model.downcast_ref::<gtk::MapListModel>() {
        underlying_store(&map.model()?)
    } else if let Some(filter) = model.downcast_ref::<gtk::FilterListModel>() {
        underlying_store(&filter.model()?)
    } else {
        None
    }
}
