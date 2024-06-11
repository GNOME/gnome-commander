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

use crate::utils::show_message;
use gettextrs::gettext;
use glib::translate::{from_glib_full, FromGlib, IntoGlib};
use gtk::{
    ffi::{GtkCellRendererAccel, GtkTreeView},
    gdk,
    gdk::ffi::GdkModifierType,
    glib::{self, translate::from_glib_none},
    prelude::*,
};
use std::ffi::c_char;

#[repr(C)]
#[allow(non_camel_case_types)]
enum Columns {
    COL_ACCEL_KEY = 0,
    COL_ACCEL_MASK,
    COL_ACTION,
    COL_NAME,
    COL_OPTION,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Accel {
    pub key: u32,
    pub mask: gdk::ModifierType,
}

impl Accel {
    pub fn label(self) -> String {
        extern "C" {
            fn egg_accelerator_get_label(
                accel_key: u32,
                accel_mods: GdkModifierType,
            ) -> *mut c_char;
        }
        unsafe { from_glib_full(egg_accelerator_get_label(self.key, self.mask.into_glib())) }
    }
}

fn get_accel(model: &gtk::ListStore, iter: &gtk::TreeIter) -> Accel {
    let key: u32 = model
        .get_value(iter, Columns::COL_ACCEL_KEY as i32)
        .get()
        .unwrap();
    let mask: gdk::ModifierType = model
        .get_value(iter, Columns::COL_ACCEL_MASK as i32)
        .get()
        .unwrap();
    Accel { key, mask }
}

fn set_accel(model: &gtk::ListStore, path: &gtk::TreePath, accel: Accel) {
    if let Some(iter) = model.iter(&path) {
        model.set(
            &iter,
            &[
                (Columns::COL_ACCEL_KEY as u32, &accel.key),
                (Columns::COL_ACCEL_MASK as u32, &accel.mask),
            ],
        );
    }
}

fn find_accel(model: &gtk::ListStore, accel: Accel) -> Option<gtk::TreeIter> {
    let iter = model.iter_first()?;
    loop {
        if get_accel(model, &iter) == accel {
            return Some(iter);
        }

        if !model.iter_next(&iter) {
            break;
        }
    }
    None
}

async fn conflict_confirm(parent: &gtk::Window, action: &str, accel: Accel) -> bool {
    let dlg = gtk::MessageDialog::builder()
        .transient_for(parent)
        .modal(true)
        .destroy_with_parent(true)
        .title(gettext("Conflicting Shortcuts"))
        .message_type(gtk::MessageType::Warning)
        .text(gettext!(
            "Shortcut “{}” is already taken by “{}”.",
            accel.label(),
            action
        ))
        .secondary_text(gettext!(
            "Reassigning the shortcut will cause it to be removed from “{}”.",
            action
        ))
        .build();
    dlg.add_button(&gettext("_Cancel"), gtk::ResponseType::Cancel);
    dlg.add_button(&gettext("_Reassign shortcut"), gtk::ResponseType::Ok);
    dlg.set_default_response(gtk::ResponseType::Cancel);

    dlg.present();
    let response = dlg.run_future().await;
    dlg.close();

    response == gtk::ResponseType::Ok
}

async fn accel_edited_callback(path_string: &str, accel: Accel, view: &gtk::TreeView) {
    let Some(window) = view.root().and_downcast::<gtk::Window>() else {
        eprintln!("No window.");
        return;
    };

    if accel.key == 0 {
        show_message(&window, &gettext("Invalid shortcut."), None);
        return;
    }

    let Some(model) = view.model().and_downcast::<gtk::ListStore>() else {
        eprintln!("Bad model. GtkListStore is expected.");
        return;
    };
    let Some(path) = gtk::TreePath::from_string(&path_string) else {
        show_message(&window, &gettext("Invalid path."), None);
        return;
    };

    let iter = model.iter(&path);

    // do nothing if accelerators haven't changed...
    if iter
        .as_ref()
        .map_or(false, |i| get_accel(&model, &i) == accel)
    {
        return;
    }

    if let Some(duplicate_iter) = find_accel(&model, accel) {
        let action: String = model
            .get_value(&duplicate_iter, Columns::COL_ACTION as i32)
            .get()
            .unwrap();

        if conflict_confirm(&window, &action, accel).await {
            set_accel(&model, &path, accel);
            model.remove(&duplicate_iter);
        }
    } else {
        set_accel(&model, &path, accel);
    }
}

#[no_mangle]
pub extern "C" fn accel_edited_callback_r(
    _accel: *mut GtkCellRendererAccel,
    path_string_ptr: *const c_char,
    accel_key: u32,
    accel_mask: GdkModifierType,
    _hardware_keycode: u32,
    view_ptr: *mut GtkTreeView,
) {
    let view: gtk::TreeView = unsafe { from_glib_none(view_ptr) };
    let path_string: String = unsafe { from_glib_none(path_string_ptr) };
    let accel = Accel {
        key: accel_key,
        mask: unsafe { gdk::ModifierType::from_glib(accel_mask) },
    };

    glib::MainContext::default().spawn_local(async move {
        accel_edited_callback(&path_string, accel, &view).await;
    });
}
