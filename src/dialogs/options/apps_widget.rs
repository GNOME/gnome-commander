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

use super::app_dialog::edit_app_dialog;
use crate::{
    app::{AppExt, AppTarget, UserDefinedApp, load_favorite_apps, save_favorite_apps},
    dialogs::order_utils::ordering_buttons,
    options::{options::GeneralOptions, types::WriteResult},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};
use std::collections::BTreeSet;

pub struct FavoriteApps {
    hbox: gtk::Box,
    selection: gtk::SingleSelection,
}

impl FavoriteApps {
    pub fn new() -> Self {
        let hbox = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(12)
            .build();

        let selection = gtk::SingleSelection::new(None::<gio::ListModel>);

        let view = gtk::ColumnView::builder().model(&selection).build();

        view.append_column(
            &gtk::ColumnViewColumn::builder()
                .factory(&app_icon_factory())
                .build(),
        );
        view.append_column(
            &gtk::ColumnViewColumn::builder()
                .title(gettext("Label"))
                .factory(&app_label_factory())
                .resizable(true)
                .build(),
        );
        view.append_column(
            &gtk::ColumnViewColumn::builder()
                .title(gettext("Command"))
                .factory(&app_command_factory())
                .expand(true)
                .resizable(true)
                .build(),
        );

        hbox.append(
            &gtk::ScrolledWindow::builder()
                .has_frame(true)
                .child(&view)
                .hexpand(true)
                .build(),
        );

        let bbox = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        hbox.append(&bbox);

        let add_button = gtk::Button::builder()
            .label(gettext("_Add"))
            .use_underline(true)
            .build();
        bbox.append(&add_button);

        let edit_button = gtk::Button::builder()
            .label(gettext("_Edit"))
            .use_underline(true)
            .build();
        bbox.append(&edit_button);

        let remove_button = gtk::Button::builder()
            .label(gettext("_Remove"))
            .use_underline(true)
            .build();
        bbox.append(&remove_button);

        let (up_button, down_button) = ordering_buttons(&selection);
        bbox.append(&up_button);
        bbox.append(&down_button);

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

        add_button.connect_clicked(glib::clone!(
            #[weak]
            selection,
            move |btn| {
                let Some(parent_window) = btn.root().and_downcast::<gtk::Window>() else {
                    return;
                };
                let Some(store) = selection.model().and_downcast::<gio::ListStore>() else {
                    return;
                };
                glib::spawn_future_local(async move {
                    let mut app = UserDefinedApp {
                        name: String::new(),
                        command_template: String::new(),
                        icon_path: None,
                        target: AppTarget::SomeFiles,
                        pattern_string: String::from("*.ext1;*.ext2"),
                        handles_uris: false,
                        handles_multiple: false,
                        requires_terminal: false,
                    };
                    let taken_names = app_names(store.upcast_ref(), gtk::INVALID_LIST_POSITION);
                    if edit_app_dialog(&parent_window, &mut app, true, taken_names).await {
                        store.append(&glib::BoxedAnyObject::new(app));
                        selection.set_selected(selection.n_items() - 1);
                    }
                });
            }
        ));
        edit_button.connect_clicked(glib::clone!(
            #[weak]
            selection,
            move |btn| {
                let Some(parent_window) = btn.root().and_downcast::<gtk::Window>() else {
                    return;
                };
                let Some(store) = selection.model().and_downcast::<gio::ListStore>() else {
                    return;
                };
                glib::spawn_future_local(async move {
                    let position = selection.selected();
                    let Some(mut app) = store
                        .item(position)
                        .and_downcast::<glib::BoxedAnyObject>()
                        .map(|b| b.borrow::<UserDefinedApp>().clone())
                    else {
                        return;
                    };
                    let taken_names = app_names(store.upcast_ref(), position);
                    if edit_app_dialog(&parent_window, &mut app, false, taken_names).await {
                        store.splice(position, 1, &[glib::BoxedAnyObject::new(app)]);
                    }
                });
            }
        ));
        remove_button.connect_clicked(glib::clone!(
            #[weak]
            selection,
            move |_| {
                if let Some(store) = selection.model().and_downcast::<gio::ListStore>() {
                    store.remove(selection.selected());
                }
            }
        ));

        Self { hbox, selection }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.hbox.clone().upcast()
    }

    pub fn read(&self, options: &GeneralOptions) {
        let model: gio::ListStore = load_favorite_apps(options)
            .into_iter()
            .map(|app| glib::BoxedAnyObject::new(app))
            .collect();
        self.selection.set_model(Some(&model));
    }

    pub fn write(&self, options: &GeneralOptions) -> WriteResult {
        if let Some(model) = self.selection.model() {
            let apps: Vec<UserDefinedApp> = model
                .iter::<glib::BoxedAnyObject>()
                .flatten()
                .map(|b| b.borrow::<UserDefinedApp>().clone())
                .collect();
            save_favorite_apps(&apps, options)?;
        }
        Ok(())
    }
}

fn app_icon_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        list_item.set_child(Some(&gtk::Image::new()));
    });
    factory.connect_bind(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(image) = list_item.child().and_downcast::<gtk::Image>() else {
            return;
        };
        let icon = list_item
            .item()
            .and_downcast::<glib::BoxedAnyObject>()
            .as_ref()
            .and_then(|b| b.borrow::<UserDefinedApp>().icon());
        match icon {
            Some(icon) => image.set_from_gicon(&icon),
            None => image.clear(),
        }
    });
    factory.upcast()
}

fn app_label_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        list_item.set_child(Some(
            &gtk::Label::builder().xalign(0.0).width_request(100).build(),
        ));
    });
    factory.connect_bind(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        let name = list_item
            .item()
            .and_downcast::<glib::BoxedAnyObject>()
            .as_ref()
            .map(|b| b.borrow::<UserDefinedApp>().name())
            .unwrap_or_default();
        label.set_text(&name);
    });
    factory.upcast()
}

fn app_command_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        list_item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
    });
    factory.connect_bind(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        let command = list_item
            .item()
            .and_downcast::<glib::BoxedAnyObject>()
            .as_ref()
            .map(|b| b.borrow::<UserDefinedApp>().command_template.clone())
            .unwrap_or_default();
        label.set_text(&command);
    });
    factory.upcast()
}

fn app_names(model: &gio::ListModel, exclude_position: u32) -> BTreeSet<String> {
    (0..model.n_items())
        .filter(|i| *i != exclude_position)
        .filter_map(|i| model.item(i).and_downcast::<glib::BoxedAnyObject>())
        .map(|b| b.borrow::<UserDefinedApp>().name())
        .collect()
}
