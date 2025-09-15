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

use super::state::State;
use gtk::{
    gio,
    glib::{self, prelude::*, translate::*},
};

pub mod ffi {
    use crate::libgcmd::state::ffi::GnomeCmdState;
    use gtk::{
        ffi::GtkWindow,
        gio::ffi::GMenuModel,
        glib::ffi::{GType, GVariant},
    };
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdFileActions {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    unsafe extern "C" {
        pub fn gnome_cmd_file_actions_get_type() -> GType;

        pub fn gnome_cmd_file_actions_create_main_menu(
            fa: *mut GnomeCmdFileActions,
        ) -> *mut GMenuModel;
        pub fn gnome_cmd_file_actions_create_popup_menu_items(
            fa: *mut GnomeCmdFileActions,
            state: *mut GnomeCmdState,
        ) -> *mut GMenuModel;
        pub fn gnome_cmd_file_actions_execute(
            fa: *mut GnomeCmdFileActions,
            action: *const c_char,
            parameter: *mut GVariant,
            parent_window: *const GtkWindow,
            state: *mut GnomeCmdState,
        );
    }
}

glib::wrapper! {
    pub struct FileActions(Interface<ffi::GnomeCmdFileActions, ffi::GnomeCmdFileActions>);

    match fn {
        type_ => || ffi::gnome_cmd_file_actions_get_type(),
    }
}

pub trait FileActionsExt: IsA<FileActions> + 'static {
    fn create_main_menu(&self) -> Option<gio::MenuModel> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_actions_create_main_menu(
                self.as_ref().to_glib_none().0,
            ))
        }
    }

    fn create_popup_menu_items(&self, state: &State) -> Option<gio::MenuModel> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_actions_create_popup_menu_items(
                self.as_ref().to_glib_none().0,
                state.to_glib_none().0,
            ))
        }
    }

    fn execute(
        &self,
        action: &str,
        parameter: Option<&glib::Variant>,
        parent_window: &gtk::Window,
        state: &State,
    ) {
        unsafe {
            ffi::gnome_cmd_file_actions_execute(
                self.as_ref().to_glib_none().0,
                action.to_glib_none().0,
                parameter.to_glib_none().0,
                parent_window.to_glib_none().0,
                state.to_glib_none().0,
            )
        }
    }
}

impl<O: IsA<FileActions>> FileActionsExt for O {}
