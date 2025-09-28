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

use super::{edit_profile_dialog::edit_profile, profiles::ProfileManager};
use crate::{
    dialogs::order_utils::ordering_buttons,
    hintbox::hintbox,
    utils::{SenderExt, dialog_button_box, display_help},
};
use gettextrs::gettext;
use gtk::{gio, glib, pango, prelude::*};
use std::rc::Rc;

fn selected(view: &gtk::ColumnView) -> Option<(gio::ListStore, u32, usize)> {
    let selection = view.model().and_downcast::<gtk::SingleSelection>()?;
    let store = selection.model().and_downcast::<gio::ListStore>()?;
    let position = selection.selected();
    let profile_index = *store
        .item(position)
        .and_downcast::<glib::BoxedAnyObject>()?
        .borrow::<usize>();
    Some((store, position, profile_index))
}

fn duplicate_clicked_callback<M: ProfileManager + 'static>(
    view: &gtk::ColumnView,
    manager: &Rc<M>,
) {
    if let Some((store, _, profile_index)) = selected(view) {
        let dup_index = manager.duplicate_profile(profile_index);
        store.append(&glib::BoxedAnyObject::new(dup_index));
        let pos = store.n_items() - 1;

        view.grab_focus();
        view.scroll_to(
            pos,
            None,
            gtk::ListScrollFlags::FOCUS | gtk::ListScrollFlags::SELECT,
            None,
        );
    }
}

async fn edit_clicked_callback<M: ProfileManager + 'static>(
    dialog: &gtk::Window,
    view: &gtk::ColumnView,
    manager: &Rc<M>,
    help_id: Option<&str>,
) {
    if let Some((store, pos, profile_index)) = selected(view) {
        if edit_profile(dialog.upcast_ref(), profile_index, help_id, manager.clone()).await {
            store.items_changed(pos, 1, 1);
        }
    }
}

fn remove_clicked_callback(view: &gtk::ColumnView) {
    if let Some((store, pos, _)) = selected(view) {
        store.remove(pos);
    }
}

pub async fn manage_profiles<M: ProfileManager + 'static>(
    parent: &gtk::Window,
    manager: &Rc<M>,
    title: &str,
    help_id: Option<&str>,
    select_last: bool,
) -> bool {
    let dialog = gtk::Window::builder()
        .transient_for(parent)
        .title(title)
        .modal(true)
        .destroy_with_parent(true)
        .resizable(true)
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

    let store = gio::ListStore::new::<glib::BoxedAnyObject>();
    let selection_model = gtk::SingleSelection::new(Some(store.clone()));

    let view = gtk::ColumnView::builder()
        .model(&selection_model)
        .width_request(400)
        .height_request(200)
        .build();

    view.append_column(
        &gtk::ColumnViewColumn::builder()
            .title(gettext("Profile"))
            .factory(&profile_name_factory(manager))
            .resizable(true)
            .expand(true)
            .build(),
    );

    view.append_column(
        &gtk::ColumnViewColumn::builder()
            .title(gettext("Template"))
            .factory(&profile_description_factory(manager))
            .resizable(true)
            .expand(true)
            .build(),
    );

    let scrolled_window = gtk::ScrolledWindow::builder()
        .hscrollbar_policy(gtk::PolicyType::Never)
        .vscrollbar_policy(gtk::PolicyType::Automatic)
        .has_frame(true)
        .hexpand(true)
        .vexpand(true)
        .child(&view)
        .build();
    grid.attach(&scrolled_window, 0, 0, 1, 1);

    view.connect_activate({
        let help_id: Option<String> = help_id.map(ToOwned::to_owned);
        glib::clone!(
            #[weak]
            dialog,
            #[strong]
            manager,
            move |view, _position| {
                let view = view.clone();
                let manager = manager.clone();
                let help_id1 = help_id.clone();
                glib::spawn_future_local(async move {
                    edit_clicked_callback(&dialog, &view, &manager, help_id1.as_deref()).await;
                });
            }
        )
    });

    let vbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .spacing(12)
        .build();
    grid.attach(&vbox, 1, 0, 1, 1);

    let duplicate_button = gtk::Button::builder()
        .label(gettext("_Duplicate"))
        .use_underline(true)
        .build();
    duplicate_button.connect_clicked(glib::clone!(
        #[weak]
        view,
        #[strong]
        manager,
        move |_| duplicate_clicked_callback(&view, &manager)
    ));
    vbox.append(&duplicate_button);

    let edit_button = gtk::Button::builder()
        .label(gettext("_Edit"))
        .use_underline(true)
        .build();

    edit_button.connect_clicked({
        let help_id: Option<String> = help_id.map(ToOwned::to_owned);
        glib::clone!(
            #[weak]
            dialog,
            #[weak]
            view,
            #[strong]
            manager,
            move |_| {
                let manager = manager.clone();
                let help_id = help_id.clone();
                glib::spawn_future_local(async move {
                    edit_clicked_callback(&dialog, &view, &manager, help_id.as_deref()).await;
                });
            }
        )
    });
    vbox.append(&edit_button);

    let remove_button = gtk::Button::builder()
        .label(gettext("_Remove"))
        .use_underline(true)
        .build();
    remove_button.connect_clicked(glib::clone!(
        #[weak]
        view,
        move |_| remove_clicked_callback(&view)
    ));
    vbox.append(&remove_button);

    let (up_button, down_button) = ordering_buttons(&selection_model);
    vbox.append(&up_button);
    vbox.append(&down_button);

    selection_model.connect_selected_notify(glib::clone!(
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

    grid.attach(&hintbox(&gettext("To rename a profile, click on the corresponding row and type a new name, or press escape to cancel.")), 0, 1, 2, 1);

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
        move |_| sender.toss(true)
    ));

    grid.attach(
        &dialog_button_box(&start_buttons, &[&cancel_button, &ok_button]),
        0,
        2,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));

    for profile_index in 0..manager.len() {
        store.append(&glib::BoxedAnyObject::new(profile_index));
    }

    let select_position = if select_last {
        store.n_items().checked_sub(1)
    } else {
        (store.n_items() > 0).then_some(0)
    };
    selection_model.set_selected(select_position.unwrap_or(gtk::INVALID_LIST_POSITION));

    dialog.present();
    let result = receiver.recv().await;

    if result == Ok(true) {
        let indexes: Vec<usize> = store
            .iter::<glib::BoxedAnyObject>()
            .flatten()
            .map(|b| *b.borrow::<usize>())
            .collect();
        manager.pick(&indexes);
    }
    dialog.close();

    result == Ok(true)
}

fn profile_name_factory<M: ProfileManager + 'static>(manager: &Rc<M>) -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, obj| {
        if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
            let label = gtk::Label::builder().xalign(0.0).build();
            list_item.set_child(Some(&label));
        }
    });
    factory.connect_bind(glib::clone!(
        #[strong]
        manager,
        move |_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                if let Some(label) = list_item.child().and_downcast::<gtk::Label>() {
                    if let Some(profile_index) = list_item
                        .item()
                        .and_downcast_ref::<glib::BoxedAnyObject>()
                        .map(|b| *b.borrow::<usize>())
                    {
                        label.set_text(&manager.profile_name(profile_index));
                    } else {
                        label.set_text("");
                    }
                }
            }
        }
    ));
    factory.upcast()
}

fn profile_description_factory<M: ProfileManager + 'static>(
    manager: &Rc<M>,
) -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    let attrs = pango::AttrList::new();
    attrs.insert(pango::AttrColor::new_foreground(0x4444, 0x4444, 0x4444));
    factory.connect_setup(move |_, obj| {
        if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
            let label = gtk::Label::builder().xalign(0.0).attributes(&attrs).build();
            list_item.set_child(Some(&label));
        }
    });
    factory.connect_bind(glib::clone!(
        #[strong]
        manager,
        move |_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                if let Some(label) = list_item.child().and_downcast::<gtk::Label>() {
                    if let Some(profile_index) = list_item
                        .item()
                        .and_downcast_ref::<glib::BoxedAnyObject>()
                        .map(|b| *b.borrow::<usize>())
                    {
                        label.set_text(&manager.profile_description(profile_index));
                    } else {
                        label.set_text("");
                    }
                }
            }
        }
    ));
    factory.upcast()
}
