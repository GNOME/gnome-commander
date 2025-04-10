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
    app::{load_favorite_apps, save_favorite_apps, AppExt, AppTarget, UserDefinedApp},
    utils::swap_list_store_items,
};
use gettextrs::gettext;
use gtk::{
    ffi::GtkWidget,
    gio,
    glib::translate::{from_glib_none, ToGlibPtr},
    prelude::*,
};
use std::collections::BTreeSet;

fn favorite_apps_widget() -> (gtk::Widget, gtk::SingleSelection) {
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

    let up_button = gtk::Button::builder()
        .label(gettext("_Up"))
        .use_underline(true)
        .build();
    bbox.append(&up_button);

    let down_button = gtk::Button::builder()
        .label(gettext("_Down"))
        .use_underline(true)
        .build();
    bbox.append(&down_button);

    selection.connect_selected_notify(glib::clone!(
        #[weak]
        edit_button,
        #[weak]
        remove_button,
        #[weak]
        up_button,
        #[weak]
        down_button,
        move |selection| {
            let position = selection.selected();
            if position != gtk::INVALID_LIST_POSITION {
                let len = selection.n_items();
                edit_button.set_sensitive(true);
                remove_button.set_sensitive(true);
                up_button.set_sensitive(position > 0);
                down_button.set_sensitive(position + 1 < len);
            } else {
                edit_button.set_sensitive(false);
                remove_button.set_sensitive(false);
                up_button.set_sensitive(false);
                down_button.set_sensitive(false);
            }
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
            if let Some(store) = selection.model().and_downcast::<gio::ListStore>() {
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
            if let Some(store) = selection.model().and_downcast::<gio::ListStore>() {
                swap_list_store_items(&store, &item, &next_item);
                selection.set_selected(next_position);
            }
        }
    ));

    (hbox.upcast(), selection)
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

#[no_mangle]
pub extern "C" fn gnome_cmd_favorite_apps_widget() -> *mut GtkWidget {
    let (widget, selection) = favorite_apps_widget();
    unsafe {
        widget.set_data("selection", selection);
    }
    widget.to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_favorite_apps_widget_load_apps(widget_ptr: *mut GtkWidget) {
    let widget: gtk::Widget = unsafe { from_glib_none(widget_ptr) };
    let selection: &gtk::SingleSelection = unsafe { widget.data("selection").unwrap().as_ref() };

    let model: gio::ListStore = load_favorite_apps()
        .into_iter()
        .map(|app| glib::BoxedAnyObject::new(app))
        .collect();
    selection.set_model(Some(&model));
}

#[no_mangle]
pub extern "C" fn gnome_cmd_favorite_apps_widget_save_apps(widget_ptr: *mut GtkWidget) {
    let widget: gtk::Widget = unsafe { from_glib_none(widget_ptr) };
    let selection: &gtk::SingleSelection = unsafe { widget.data("selection").unwrap().as_ref() };

    let Some(model) = selection.model() else {
        return;
    };

    let apps: Vec<UserDefinedApp> = model
        .iter::<glib::BoxedAnyObject>()
        .flatten()
        .map(|b| b.borrow::<UserDefinedApp>().clone())
        .collect();

    save_favorite_apps(&apps);
}
