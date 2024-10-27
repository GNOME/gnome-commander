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
    hintbox::HintBox,
    utils::{dialog_button_box, display_help},
};
use gettextrs::gettext;
use gtk::{glib, pango, prelude::*};
use std::rc::Rc;

#[repr(C)]
enum Columns {
    ProfileIndex = 0,
    Name,
    Description,
}

fn create_model() -> gtk::ListStore {
    gtk::ListStore::new(&[
        u32::static_type(),
        String::static_type(),
        String::static_type(),
    ])
}

fn set_profile(
    model: &gtk::ListStore,
    iter: &gtk::TreeIter,
    manager: &dyn ProfileManager,
    profile_index: usize,
) {
    model.set(
        &iter,
        &[
            (Columns::ProfileIndex as u32, &(profile_index as u32)),
            (Columns::Name as u32, &manager.profile_name(profile_index)),
            (
                Columns::Description as u32,
                &manager.profile_description(profile_index),
            ),
        ],
    );
}

fn create_text_column(
    cell_renderer: &impl IsA<gtk::CellRenderer>,
    col_id: i32,
    title: &str,
    tooltip: &str,
) -> gtk::TreeViewColumn {
    let column = gtk::TreeViewColumn::with_attributes(title, cell_renderer, &[("text", col_id)]);
    column.set_clickable(true);
    column.set_resizable(true);
    column.button().set_tooltip_text(Some(tooltip));
    column
}

fn create_view<M: ProfileManager + 'static>(
    store: &gtk::ListStore,
    manager: &Rc<M>,
) -> gtk::TreeView {
    let view = gtk::TreeView::builder()
        .reorderable(true)
        .enable_search(true)
        .search_column(Columns::Name as i32)
        .model(store)
        .width_request(400)
        .height_request(200)
        .build();

    let name_renderer = gtk::CellRendererText::builder().editable(true).build();
    let name_column = create_text_column(
        &name_renderer,
        Columns::Name as i32,
        &gettext("Profile"),
        &gettext("Profile name"),
    );
    view.append_column(&name_column);

    name_renderer.connect_edited(glib::clone!(
        #[weak]
        store,
        #[strong]
        manager,
        move |_, path, value| {
            if let Some(iter) = store.iter(&path) {
                let profile_index: u32 = store
                    .get_value(&iter, Columns::ProfileIndex as i32)
                    .get()
                    .unwrap();
                manager.set_profile_name(profile_index as usize, value);

                store.set(&iter, &[(Columns::Name as u32, &value)]);
            }
        }
    ));

    let description_renderer = gtk::CellRendererText::builder()
        .foreground("DarkGray")
        .foreground_set(true)
        .ellipsize(pango::EllipsizeMode::End)
        .ellipsize_set(true)
        .build();
    let description_column = create_text_column(
        &description_renderer,
        Columns::Description as i32,
        &gettext("Template"),
        &gettext("Template"),
    );
    view.append_column(&description_column);

    view.selection().set_mode(gtk::SelectionMode::Browse);

    view
}

fn selected(view: &gtk::TreeView) -> Option<(gtk::ListStore, gtk::TreeIter, usize)> {
    let (model, iter) = view.selection().selected()?;
    let store = model.downcast::<gtk::ListStore>().ok()?;
    let profile_index: u32 = store
        .get_value(&iter, Columns::ProfileIndex as i32)
        .get()
        .ok()?;
    Some((store, iter, profile_index as usize))
}

fn duplicate_clicked_callback<M: ProfileManager + 'static>(view: &gtk::TreeView, manager: &Rc<M>) {
    if let Some((model, _iter, profile_index)) = selected(view) {
        let dup_index = manager.duplicate_profile(profile_index);
        let iter = model.append();
        set_profile(&model, &iter, &**manager, dup_index);

        view.grab_focus();
        let path = model.path(&iter);
        gtk::prelude::TreeViewExt::set_cursor(view, &path, view.column(0).as_ref(), true);
    }
}

async fn edit_clicked_callback<M: ProfileManager + 'static>(
    dialog: &gtk::Dialog,
    view: &gtk::TreeView,
    manager: &Rc<M>,
    help_id: Option<&str>,
) {
    if let Some((store, iter, profile_index)) = selected(view) {
        if edit_profile(dialog.upcast_ref(), profile_index, help_id, manager.clone()).await {
            set_profile(&store, &iter, &**manager, profile_index);
        }
    }
}

fn remove_clicked_callback(view: &gtk::TreeView) {
    if let Some((model, iter)) = view.selection().selected() {
        model
            .downcast_ref::<gtk::ListStore>()
            .unwrap()
            .remove(&iter);
    }
}

pub async fn manage_profiles<M: ProfileManager + 'static>(
    parent: &gtk::Window,
    manager: &Rc<M>,
    title: &str,
    help_id: Option<&str>,
    select_last: bool,
) -> bool {
    let dialog = gtk::Dialog::builder()
        .transient_for(parent)
        .title(title)
        .modal(true)
        .destroy_with_parent(true)
        .resizable(true)
        .build();

    let content_area = dialog.content_area();

    // HIG defaults
    content_area.set_margin_top(12);
    content_area.set_margin_bottom(12);
    content_area.set_margin_start(12);
    content_area.set_margin_end(12);
    content_area.set_spacing(6);

    let hbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(12)
        .hexpand(true)
        .vexpand(true)
        .build();
    content_area.append(&hbox);

    let scrolled_window = gtk::ScrolledWindow::builder()
        .hscrollbar_policy(gtk::PolicyType::Never)
        .vscrollbar_policy(gtk::PolicyType::Automatic)
        .has_frame(true)
        .hexpand(true)
        .vexpand(true)
        .build();
    hbox.append(&scrolled_window);

    let store = create_model();
    let view = create_view(&store, &*manager);
    scrolled_window.set_child(Some(&view));

    view.connect_row_activated({
        let help_id: Option<String> = help_id.map(ToOwned::to_owned);
        glib::clone!(
            #[weak]
            dialog,
            #[strong]
            manager,
            move |view, _path, _col| {
                let view = view.clone();
                let manager = manager.clone();
                let help_id1 = help_id.clone();
                glib::spawn_future_local(async move {
                    edit_clicked_callback(&dialog, &view, &manager, help_id1.as_deref()).await;
                });
            }
        )
    });

    content_area.append(&HintBox::new(&gettext("To rename a profile, click on the corresponding row and type a new name, or press escape to cancel.")));

    let vbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .spacing(12)
        .build();
    hbox.append(&vbox);

    let button = gtk::Button::builder()
        .label(gettext("_Duplicate"))
        .use_underline(true)
        .build();
    button.connect_clicked(glib::clone!(
        #[weak]
        view,
        #[strong]
        manager,
        move |_| duplicate_clicked_callback(&view, &manager)
    ));
    vbox.append(&button);

    let button = gtk::Button::builder()
        .label(gettext("_Edit"))
        .use_underline(true)
        .build();

    button.connect_clicked({
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
    vbox.append(&button);

    let button = gtk::Button::builder()
        .label(gettext("_Remove"))
        .use_underline(true)
        .build();
    button.connect_clicked(glib::clone!(
        #[weak]
        view,
        move |_| remove_clicked_callback(&view)
    ));
    vbox.append(&button);

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
        move |_| dialog.response(gtk::ResponseType::Ok)
    ));

    content_area.append(&dialog_button_box(
        &start_buttons,
        &[&cancel_button, &ok_button],
    ));

    dialog.set_default_response(gtk::ResponseType::Ok);

    for profile_index in 0..manager.len() {
        set_profile(&store, &store.append(), &**manager, profile_index);
    }

    let select_iter = if select_last {
        iter_last(&store)
    } else {
        store.iter_first()
    };
    if let Some(iter) = select_iter {
        view.selection().select_iter(&iter);
    }

    dialog.present();
    let result = dialog.run_future().await;

    if result == gtk::ResponseType::Ok {
        let mut indexes = Vec::new();
        if let Some(iter) = store.iter_first() {
            loop {
                let profile_index: u32 = store
                    .get_value(&iter, Columns::ProfileIndex as i32)
                    .get()
                    .unwrap();

                indexes.push(profile_index as usize);

                if !store.iter_next(&iter) {
                    break;
                }
            }
        }
        manager.pick(&indexes);
    }
    dialog.close();

    result == gtk::ResponseType::Ok
}

fn iter_last(store: &gtk::ListStore) -> Option<gtk::TreeIter> {
    let len = store.iter_n_children(None);
    if len > 0 {
        store.iter_nth_child(None, len - 1)
    } else {
        None
    }
}
