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

use crate::{
    app::{App, AppExt, AppTarget, UserDefinedApp},
    select_icon_button::IconButton,
    utils::{
        attributes_bold, channel_send_action, dialog_button_box, handle_escape_key, ErrorMessage,
        SenderExt, NO_BUTTONS,
    },
};
use gettextrs::gettext;
use gtk::{
    ffi::{GtkTreeView, GtkWindow},
    glib::{self, ffi::gpointer, translate::from_glib_none},
    prelude::*,
};

async fn edit_app_dialog(
    parent_window: &gtk::Window,
    app_iter: Option<&gtk::TreeIter>,
    app: &mut App,
    is_new: bool,
    store: &gtk::ListStore,
) -> bool {
    let user_app = match app {
        App::Regular(_) => return false,
        App::UserDefined(a) => a,
    };

    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .title(if is_new {
            gettext("New Application")
        } else {
            gettext("Edit Application")
        })
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

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Label:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Command:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        1,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Icon:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        2,
        1,
        1,
    );

    let name_entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    grid.attach(&name_entry, 1, 0, 1, 1);

    let cmd_entry = gtk::Entry::builder()
        .hexpand(true)
        .activates_default(true)
        .build();
    grid.attach(&cmd_entry, 1, 1, 1, 1);

    let icon_entry = IconButton::default();
    grid.attach(&icon_entry, 1, 2, 1, 1);

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Options"))
            .attributes(&attributes_bold())
            .halign(gtk::Align::Start)
            .build(),
        0,
        3,
        2,
        1,
    );

    let handles_multiple = gtk::CheckButton::builder()
        .label(gettext("Can handle multiple files"))
        .build();
    grid.attach(&handles_multiple, 0, 4, 2, 1);

    let handles_uris = gtk::CheckButton::builder()
        .label(gettext("Can handle URIs"))
        .build();
    grid.attach(&handles_uris, 0, 5, 2, 1);

    let requires_terminal = gtk::CheckButton::builder()
        .label(gettext("Requires terminal"))
        .build();
    grid.attach(&requires_terminal, 0, 6, 2, 1);

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Show for"))
            .attributes(&attributes_bold())
            .halign(gtk::Align::Start)
            .build(),
        0,
        7,
        2,
        1,
    );

    let show_for_all_files = gtk::CheckButton::builder()
        .label(gettext("All files"))
        .build();
    grid.attach(&show_for_all_files, 0, 8, 2, 1);

    let show_for_all_dirs = gtk::CheckButton::builder()
        .label(gettext("All directories"))
        .group(&show_for_all_files)
        .build();
    grid.attach(&show_for_all_dirs, 0, 9, 2, 1);

    let show_for_all_dirs_and_files = gtk::CheckButton::builder()
        .label(gettext("All directories and files"))
        .group(&show_for_all_files)
        .build();
    grid.attach(&show_for_all_dirs_and_files, 0, 10, 2, 1);

    let show_for_some_files = gtk::CheckButton::builder()
        .label(gettext("Some files"))
        .group(&show_for_all_files)
        .build();
    grid.attach(&show_for_some_files, 0, 11, 2, 1);

    let pattern_label = gtk::Label::builder()
        .label(gettext("File patterns"))
        .margin_start(24)
        .build();
    grid.attach(&pattern_label, 0, 12, 1, 1);

    let pattern_entry = gtk::Entry::builder().activates_default(true).build();
    grid.attach(&pattern_entry, 1, 12, 1, 1);

    show_for_some_files.connect_toggled(glib::clone!(
        #[strong]
        pattern_entry,
        move |toggle| {
            if toggle.is_active() {
                pattern_label.set_sensitive(true);
                pattern_entry.set_sensitive(true);
                pattern_entry.grab_focus();
            } else {
                pattern_label.set_sensitive(false);
                pattern_entry.set_sensitive(false);
            }
        }
    ));

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .receives_default(true)
        .build();
    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_button, &ok_button]),
        0,
        13,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(false)
    ));
    ok_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));
    dialog.connect_close_request(glib::clone!(
        #[strong]
        sender,
        move |_| {
            sender.toss(false);
            glib::Propagation::Proceed
        }
    ));
    handle_escape_key(&dialog, &channel_send_action(&sender, false));

    {
        name_entry.set_text(&user_app.name());
        cmd_entry.set_text(&user_app.command_template);
        icon_entry.set_path(user_app.icon_path.as_deref());
        handles_multiple.set_active(user_app.handles_multiple);
        handles_uris.set_active(user_app.handles_uris);
        requires_terminal.set_active(user_app.requires_terminal);

        match user_app.target {
            AppTarget::AllFiles => &show_for_all_files,
            AppTarget::AllDirs => &show_for_all_dirs,
            AppTarget::AllDirsAndFiles => &show_for_all_dirs_and_files,
            AppTarget::SomeFiles => &show_for_some_files,
        }
        .set_active(true);

        pattern_entry.set_text(&user_app.pattern_string);
    }

    dialog.present();
    name_entry.grab_focus();

    let result = loop {
        if receiver.recv().await != Ok(true) {
            break false;
        }

        let name = name_entry.text();
        if name.trim().is_empty() {
            ErrorMessage::brief(gettext("An app needs to have a non-empty label"))
                .show(&dialog)
                .await;
            continue;
        }
        if fav_app_is_name_double(store, app_iter, &name) {
            ErrorMessage::brief(gettext(
                "An app with this label exists already.\nPlease choose another label.",
            ))
            .show(&dialog)
            .await;
            continue;
        }
        break true;
    };

    if result {
        user_app.name = name_entry.text().to_string();
        user_app.command_template = cmd_entry.text().to_string();
        user_app.icon_path = icon_entry.path();
        if show_for_all_files.is_active() {
            user_app.target = AppTarget::AllFiles;
            user_app.pattern_string.clear();
        } else if show_for_all_dirs.is_active() {
            user_app.target = AppTarget::AllDirs;
            user_app.pattern_string.clear();
        } else if show_for_all_dirs_and_files.is_active() {
            user_app.target = AppTarget::AllDirsAndFiles;
            user_app.pattern_string.clear();
        } else {
            user_app.target = AppTarget::SomeFiles;
            user_app.pattern_string = pattern_entry.text().to_string();
        }
        user_app.handles_uris = handles_uris.is_active();
        user_app.handles_multiple = handles_multiple.is_active();
        user_app.requires_terminal = requires_terminal.is_active();
    }

    dialog.close();

    result
}

#[no_mangle]
pub extern "C" fn gnome_cmd_fav_app_new(
    parent_window_ptr: *mut GtkWindow,
    tree_view_ptr: *mut GtkTreeView,
) {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };
    let tree_view: gtk::TreeView = unsafe { from_glib_none(tree_view_ptr) };
    let Some(store) = tree_view.model().and_downcast::<gtk::ListStore>() else {
        return;
    };

    glib::spawn_future_local(async move {
        let mut app = App::UserDefined(UserDefinedApp {
            name: String::new(),
            command_template: String::new(),
            icon_path: None,
            target: AppTarget::SomeFiles,
            pattern_string: String::from("*.ext1;*.ext2"),
            handles_uris: false,
            handles_multiple: false,
            requires_terminal: false,
        });
        if edit_app_dialog(&parent_window, None, &mut app, true, &store).await {
            let icon = app.icon();
            let name = app.name();
            let command = app.command();
            let app_ptr = unsafe { app.into_raw() };

            unsafe {
                gnome_cmd_add_fav_app(app_ptr);
            }

            let iter = store.append();
            store.set(
                &iter,
                &[
                    (0, &icon),
                    (1, &name),
                    (2, &command),
                    (3, &(app_ptr as glib::ffi::gpointer)),
                ],
            );
        }
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_fav_app_edit(
    parent_window_ptr: *mut GtkWindow,
    tree_view_ptr: *mut GtkTreeView,
) {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };
    let tree_view: gtk::TreeView = unsafe { from_glib_none(tree_view_ptr) };

    let Some((model, tree_iter)) = tree_view.selection().selected() else {
        return;
    };
    let Ok(store) = model.downcast::<gtk::ListStore>() else {
        return;
    };

    glib::spawn_future_local(async move {
        if let Some(app) = get_app_from_store(&store, &tree_iter) {
            if edit_app_dialog(&parent_window, Some(&tree_iter), app, false, &store).await {
                store.set(&tree_iter, &[(0, &app.icon()), (1, &app.name())]);
            }
        }
    });
}

fn get_app_from_store<'g>(store: &'g gtk::ListStore, iter: &gtk::TreeIter) -> Option<&'g mut App> {
    let ptr = store.get_value(iter, 3).get::<gpointer>().ok()?;
    let app: &mut App = unsafe { &mut *(ptr as *mut App) };
    Some(app)
}

fn fav_app_is_name_double(
    store: &gtk::ListStore,
    app_iter: Option<&gtk::TreeIter>,
    name: &str,
) -> bool {
    let app_path = app_iter.map(|i| store.path(i));
    if let Some(iter) = store.iter_first() {
        loop {
            if app_path != Some(store.path(&iter)) {
                if let Some(a) = get_app_from_store(&store, &iter) {
                    if a.name() == name {
                        return true;
                    }
                }
            }
            if !store.iter_next(&iter) {
                break;
            }
        }
    }
    false
}

extern "C" {
    fn gnome_cmd_add_fav_app(app: *const App);
}
