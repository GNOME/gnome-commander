/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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
    advanced_rename::profile::{AdvRenameProfilePtr, AdvancedRenameProfile},
    dialogs::advrename_regex_dialog::{show_advrename_regex_dialog, RegexReplace},
    utils::{dialog_button_box, NO_BUTTONS},
};
use gettextrs::gettext;
use gtk::{
    ffi::{GtkTreeView, GtkWidget, GtkWindow},
    glib::{
        self,
        ffi::gpointer,
        translate::{from_glib_borrow, from_glib_none, Borrowed, ToGlibPtr},
    },
    prelude::*,
};
use std::{
    error::Error,
    ffi::{c_char, CStr},
    os::raw::c_void,
};

async fn get_selected_range(
    parent_window: &gtk::Window,
    placeholder: &str,
    filename: Option<&str>,
) -> Option<String> {
    let filename = filename.unwrap_or(concat!("Lorem ipsum dolor sit amet, consectetur adipisici elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ",
                   "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. ",
                   "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. ",
                   "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."));

    let dialog = gtk::Window::builder()
        .title(gettext("Range Selection"))
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .width_request(480)
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

    let entry = gtk::Entry::builder()
        .text(filename)
        .activates_default(true)
        .hexpand(true)
        .build();
    let label = gtk::Label::builder()
        .label(gettext("_Select range:"))
        .use_underline(true)
        .mnemonic_widget(&entry)
        .build();

    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&entry, 1, 0, 1, 1);

    let option = gtk::CheckButton::builder()
        .label(gettext("_Inverse selection"))
        .use_underline(true)
        .build();
    grid.attach(&option, 0, 1, 2, 1);

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::End)
        .build();
    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| if let Err(error) = sender.send_blocking(false) {
            eprintln!("Failed to send a 'cancel' message: {error}.")
        }
    ));

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    ok_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| if let Err(error) = sender.send_blocking(true) {
            eprintln!("Failed to send an 'ok' message: {error}.")
        }
    ));

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_button, &ok_button]),
        0,
        2,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));

    dialog.present();
    let result = receiver.recv().await.unwrap_or_default();
    dialog.close();

    let range = if result {
        let inversed = option.is_active();

        if let Some((begin, end)) = entry.selection_bounds() {
            let len = i32::from(entry.text_length());
            if !inversed {
                if end == len {
                    Some(format!("{placeholder}({begin}:)"))
                } else {
                    Some(format!("{placeholder}({begin}:{end})"))
                }
            } else {
                let b = (begin != 0).then(|| format!("{placeholder}(:{begin})"));
                let e = (end != len).then(|| format!("{placeholder}({end}:)"));
                Some(format!(
                    "{}{}",
                    b.unwrap_or_default(),
                    e.unwrap_or_default()
                ))
            }
        } else {
            if !inversed {
                None
            } else {
                Some(placeholder.to_owned())
            }
        }
    } else {
        None
    };

    range
}

extern "C" {
    fn get_selected_range_done(rtag: *const c_char, user_data: gpointer);
}

#[no_mangle]
pub extern "C" fn get_selected_range_r(
    parent_window_ptr: *const GtkWindow,
    placeholder_ptr: *const c_char,
    filename_ptr: *const c_char,
    user_data: gpointer,
) {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };
    let placeholder = unsafe { CStr::from_ptr(placeholder_ptr).to_string_lossy() };

    let filename = if filename_ptr.is_null() {
        None
    } else {
        unsafe { CStr::from_ptr(filename_ptr).to_str().ok() }
    }
    .map(|s| s.to_owned());
    unsafe {
        glib::ffi::g_free(filename_ptr as *mut c_void);
    }

    glib::spawn_future_local(async move {
        if let Some(rtag) =
            get_selected_range(&parent_window, placeholder.as_ref(), filename.as_deref()).await
        {
            unsafe {
                get_selected_range_done(rtag.to_glib_none().0, user_data);
            }
        }
    });
}

#[allow(non_camel_case_types)]
enum RegexViewColumns {
    COL_MALFORMED_REGEX = 0,
    COL_PATTERN,
    COL_REPLACE,
    COL_MATCH_CASE,
    COL_MATCH_CASE_LABEL,
}

fn get_regex_row(
    store: &gtk::ListStore,
    iter: &gtk::TreeIter,
) -> Result<RegexReplace, Box<dyn Error>> {
    use RegexViewColumns::*;
    Ok(RegexReplace {
        pattern: store.get_value(iter, COL_PATTERN as i32).get()?,
        replacement: store.get_value(iter, COL_REPLACE as i32).get()?,
        match_case: store.get_value(iter, COL_MATCH_CASE as i32).get()?,
    })
}

fn set_regex_row(store: &gtk::ListStore, iter: &gtk::TreeIter, regex: &RegexReplace) {
    use RegexViewColumns::*;
    store.set(
        iter,
        &[
            (COL_MALFORMED_REGEX as u32, &!regex.is_valid()),
            (COL_PATTERN as u32, &regex.pattern),
            (COL_REPLACE as u32, &regex.replacement),
            (COL_MATCH_CASE as u32, &regex.match_case),
            (
                COL_MATCH_CASE_LABEL as u32,
                &if regex.match_case {
                    gettext("Yes")
                } else {
                    gettext("No")
                },
            ),
        ],
    );
}

async fn regex_add(
    component: &gtk::Widget,
    tree_view: &gtk::TreeView,
) -> Result<(), Box<dyn Error>> {
    let store = tree_view
        .model()
        .and_downcast::<gtk::ListStore>()
        .ok_or_else(|| "Unexpected type of a tree model.")?;
    let window = component
        .root()
        .and_downcast::<gtk::Window>()
        .ok_or_else(|| "No parent window")?;

    if let Some(rx) = show_advrename_regex_dialog(&window, &gettext("Add Rule"), None).await {
        let iter = store.append();
        set_regex_row(&store, &iter, &rx);

        component.emit_by_name::<()>("regex-changed", &[]);
    }
    Ok(())
}

async fn regex_edit(
    component: &gtk::Widget,
    tree_view: &gtk::TreeView,
) -> Result<(), Box<dyn Error>> {
    let Some((model, iter)) = tree_view.selection().selected() else {
        return Ok(());
    };
    let store = model
        .downcast::<gtk::ListStore>()
        .map_err(|_| "Unexpected type of a tree model.")?;
    let window = component
        .root()
        .and_downcast::<gtk::Window>()
        .ok_or_else(|| "No parent window")?;

    let rx = get_regex_row(&store, &iter)?;
    if let Some(rr) = show_advrename_regex_dialog(&window, &gettext("Edit Rule"), Some(&rx)).await {
        set_regex_row(&store, &iter, &rr);
        component.emit_by_name::<()>("regex-changed", &[]);
    }
    Ok(())
}

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_profile_component_fill_regex_model(
    component_ptr: *mut GtkWidget,
    tree_view_ptr: *mut GtkTreeView,
    profile_ptr: *mut AdvRenameProfilePtr,
) {
    let component: Borrowed<gtk::Widget> = unsafe { from_glib_borrow(component_ptr) };
    let tree_view: Borrowed<gtk::TreeView> = unsafe { from_glib_borrow(tree_view_ptr) };
    let profile: Borrowed<AdvancedRenameProfile> = unsafe { from_glib_borrow(profile_ptr) };

    let Some(store) = tree_view.model().and_downcast::<gtk::ListStore>() else {
        return;
    };

    for rx in profile.patterns() {
        let iter = store.append();
        set_regex_row(&store, &iter, &rx);
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_profile_component_copy_regex_model(
    component_ptr: *mut GtkWidget,
    tree_view_ptr: *mut GtkTreeView,
    profile_ptr: *mut AdvRenameProfilePtr,
) {
    let component: Borrowed<gtk::Widget> = unsafe { from_glib_borrow(component_ptr) };
    let tree_view: Borrowed<gtk::TreeView> = unsafe { from_glib_borrow(tree_view_ptr) };
    let profile: Borrowed<AdvancedRenameProfile> = unsafe { from_glib_borrow(profile_ptr) };

    let Some(store) = tree_view.model().and_downcast::<gtk::ListStore>() else {
        return;
    };

    let mut patterns = Vec::new();
    if let Some(iter) = store.iter_first() {
        loop {
            match get_regex_row(&store, &iter) {
                Ok(rx) => patterns.push(rx),
                Err(error) => {
                    eprintln!("{error}")
                }
            }
            if !store.iter_next(&iter) {
                break;
            }
        }
    }

    profile.set_patterns(patterns);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_profile_component_regex_add(
    component_ptr: *mut GtkWidget,
    tree_view_ptr: *mut GtkTreeView,
) {
    let component: gtk::Widget = unsafe { from_glib_none(component_ptr) };
    let tree_view: gtk::TreeView = unsafe { from_glib_none(tree_view_ptr) };

    glib::spawn_future_local(async move {
        if let Err(error) = regex_add(&component, &tree_view).await {
            eprintln!("{}", error);
        }
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_profile_component_regex_edit(
    component_ptr: *mut GtkWidget,
    tree_view_ptr: *mut GtkTreeView,
) {
    let component: gtk::Widget = unsafe { from_glib_none(component_ptr) };
    let tree_view: gtk::TreeView = unsafe { from_glib_none(tree_view_ptr) };

    glib::spawn_future_local(async move {
        if let Err(error) = regex_edit(&component, &tree_view).await {
            eprintln!("{}", error);
        }
    });
}
