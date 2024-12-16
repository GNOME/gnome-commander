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

use super::profiles::{manage_profiles_dialog::manage_profiles, profiles::ProfileManager};
use crate::data::SearchConfig;
use gettextrs::gettext;
use glib::ffi::gboolean;
use gtk::{
    ffi::{GtkDialog, GtkWidget},
    glib::translate::{from_glib_full, from_glib_none, ToGlibPtr},
    prelude::*,
};
use std::{
    ffi::{c_char, c_void},
    marker::PhantomData,
    rc::Rc,
};

type SearchProfilePtr = c_void;
type SearchProfilesPtr = c_void;

extern "C" {
    fn gnome_cmd_search_profile_component_new(profile: *mut SearchProfilePtr) -> *mut GtkWidget;
    fn gnome_cmd_search_profile_component_update(component: *mut GtkWidget);
    fn gnome_cmd_search_profile_component_copy(component: *mut GtkWidget);

    fn gnome_cmd_search_config_new_profiles(
        cfg: *mut c_void,
        with_default: gboolean,
    ) -> *mut SearchProfilesPtr;
    fn gnome_cmd_search_config_take_profiles(cfg: *mut c_void, profiles: *mut SearchProfilesPtr);

    fn gnome_cmd_search_profiles_dup(profiles: *mut SearchProfilesPtr) -> *mut SearchProfilesPtr;
    fn gnome_cmd_search_profiles_len(profiles: *mut SearchProfilesPtr) -> u32;
    fn gnome_cmd_search_profiles_get(
        profiles: *mut SearchProfilesPtr,
        profile_index: u32,
    ) -> *mut SearchProfilePtr;
    fn gnome_cmd_search_profiles_get_name(
        profiles: *mut SearchProfilesPtr,
        profile_index: u32,
    ) -> *const c_char;
    fn gnome_cmd_search_profiles_set_name(
        profiles: *mut SearchProfilesPtr,
        profile_index: u32,
        name: *const c_char,
    );
    fn gnome_cmd_search_profiles_get_description(
        profiles: *mut SearchProfilesPtr,
        profile_index: u32,
    ) -> *mut c_char;
    fn gnome_cmd_search_profiles_reset(profiles: *mut SearchProfilesPtr, profile_index: u32);
    fn gnome_cmd_search_profiles_duplicate(
        profiles: *mut SearchProfilesPtr,
        profile_index: u32,
    ) -> u32;
    fn gnome_cmd_search_profiles_pick(
        profiles: *mut SearchProfilesPtr,
        indexes: *const u32,
        size: u32,
    );
}

struct SearchProfiles(*mut SearchProfilesPtr);
struct SearchProfile<'p>(*mut SearchProfilePtr, PhantomData<&'p mut c_void>);

impl Clone for SearchProfiles {
    fn clone(&self) -> Self {
        Self(unsafe { gnome_cmd_search_profiles_dup(self.0) })
    }
}

impl SearchProfiles {
    fn profile(&self, profile_index: usize) -> SearchProfile<'_> {
        SearchProfile(
            unsafe { gnome_cmd_search_profiles_get(self.0, profile_index as u32) },
            PhantomData,
        )
    }

    fn len(&self) -> usize {
        unsafe { gnome_cmd_search_profiles_len(self.0) as usize }
    }

    fn profile_name(&self, profile_index: usize) -> String {
        unsafe {
            from_glib_none(gnome_cmd_search_profiles_get_name(
                self.0,
                profile_index as u32,
            ))
        }
    }

    fn set_profile_name(&self, profile_index: usize, name: &str) {
        unsafe {
            gnome_cmd_search_profiles_set_name(self.0, profile_index as u32, name.to_glib_none().0)
        }
    }

    fn profile_description(&self, profile_index: usize) -> String {
        unsafe {
            from_glib_full(gnome_cmd_search_profiles_get_description(
                self.0,
                profile_index as u32,
            ))
        }
    }

    fn reset_profile(&self, profile_index: usize) {
        unsafe { gnome_cmd_search_profiles_reset(self.0, profile_index as u32) }
    }

    fn duplicate_profile(&self, profile_index: usize) -> usize {
        unsafe { gnome_cmd_search_profiles_duplicate(self.0, profile_index as u32) as usize }
    }

    fn pick(&self, indexes: &[usize]) {
        let indexes_u32: Vec<u32> = indexes.iter().map(|i| *i as u32).collect();
        unsafe {
            gnome_cmd_search_profiles_pick(self.0, indexes_u32.as_ptr(), indexes.len() as u32)
        }
    }
}

struct SearchProfileManager {
    profiles: SearchProfiles,
}

impl ProfileManager for SearchProfileManager {
    fn len(&self) -> usize {
        self.profiles.len()
    }

    fn profile_name(&self, profile_index: usize) -> String {
        self.profiles.profile_name(profile_index)
    }

    fn set_profile_name(&self, profile_index: usize, name: &str) {
        self.profiles.set_profile_name(profile_index, name);
    }

    fn profile_description(&self, profile_index: usize) -> String {
        self.profiles.profile_description(profile_index)
    }

    fn reset_profile(&self, profile_index: usize) {
        self.profiles.reset_profile(profile_index);
    }

    fn duplicate_profile(&self, profile_index: usize) -> usize {
        self.profiles.duplicate_profile(profile_index)
    }

    fn pick(&self, profile_indexes: &[usize]) {
        self.profiles.pick(profile_indexes);
    }

    fn create_component(&self, profile_index: usize) -> gtk::Widget {
        let widget_ptr = unsafe {
            gnome_cmd_search_profile_component_new(self.profiles.profile(profile_index).0)
        };
        if widget_ptr.is_null() {
            panic!("Profile editing component is null.");
        }
        unsafe { from_glib_full(widget_ptr) }
    }

    fn update_component(&self, _profile_index: usize, component: &gtk::Widget) {
        unsafe { gnome_cmd_search_profile_component_update(component.to_glib_none().0) }
    }

    fn copy_component(&self, _profile_index: usize, component: &gtk::Widget) {
        unsafe { gnome_cmd_search_profile_component_copy(component.to_glib_none().0) }
    }
}

type GnomeCmdSearchDialog = GtkDialog;

extern "C" {
    fn gnome_cmd_search_dialog_update_profile_menu(dialog: *mut GnomeCmdSearchDialog);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_search_dialog_do_manage_profiles(
    dialog_ptr: *mut GnomeCmdSearchDialog,
    cfg_ptr: *mut c_void,
    new_profile: gboolean,
) {
    let dialog: gtk::Dialog = unsafe { from_glib_none(dialog_ptr) };
    let cfg = unsafe { SearchConfig::from_ptr(cfg_ptr) };

    glib::spawn_future_local(async move {
        let profiles = SearchProfiles(unsafe {
            gnome_cmd_search_config_new_profiles(cfg.as_ptr(), new_profile)
        });

        if new_profile != 0 {
            let last_index = profiles.len() - 1;
            profiles.set_profile_name(last_index, &gettext("New profile"));
        }

        let manager = Rc::new(SearchProfileManager { profiles });

        if manage_profiles(
            dialog.upcast_ref(),
            &manager,
            &gettext("Profiles"),
            Some("gnome-commander-search"),
            new_profile != 0,
        )
        .await
        {
            let profiles = manager.profiles.clone();
            unsafe {
                gnome_cmd_search_config_take_profiles(cfg.as_ptr(), profiles.0);
            }
            unsafe {
                gnome_cmd_search_dialog_update_profile_menu(dialog_ptr);
            }
        }
    });
}
