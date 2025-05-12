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
use crate::{
    advanced_rename::profile::{AdvRenameProfilePtr, AdvancedRenameProfile},
    tags::tags::FileMetadataService,
};
use gettextrs::gettext;
use gtk::{
    ffi::GtkWidget,
    gio::{self, ffi::GListStore},
    glib::{
        ffi::{gboolean, GType},
        translate::{from_glib_full, from_glib_none, FromGlibPtrNone, IntoGlib, ToGlibPtr},
    },
    prelude::*,
    subclass::prelude::*,
};
use std::{ffi::c_void, rc::Rc};

struct AdvRenameConfig(*mut c_void);

impl AdvRenameConfig {
    fn default_profile(&self) -> AdvancedRenameProfile {
        unsafe { from_glib_none(gnome_cmd_advrename_config_default_profile(self.0)) }
    }

    fn profiles(&self) -> gio::ListStore {
        unsafe { gio::ListStore::from_glib_none(gnome_cmd_advrename_config_profiles(self.0)) }
    }

    pub fn set_profiles(&self, profiles: &gio::ListStore) {
        let store = self.profiles();
        store.remove_all();
        for p in profiles.iter::<AdvancedRenameProfile>().flatten() {
            store.append(&p);
        }
    }
}

extern "C" {
    fn gnome_cmd_advrename_profile_component_new(
        profile: *mut AdvRenameProfilePtr,
        fms: *mut <FileMetadataService as glib::object::ObjectType>::GlibType,
    ) -> *mut GtkWidget;
    fn gnome_cmd_advrename_profile_component_update(component: *mut GtkWidget);
    fn gnome_cmd_advrename_profile_component_copy(component: *mut GtkWidget);

    fn gnome_cmd_advrename_config_default_profile(cfg: *mut c_void) -> *mut AdvRenameProfilePtr;
    fn gnome_cmd_advrename_config_profiles(cfg: *mut c_void) -> *mut GListStore;
}

struct AdvrenameProfiles {
    profiles: gio::ListStore,
}

impl Clone for AdvrenameProfiles {
    fn clone(&self) -> Self {
        Self {
            profiles: self
                .profiles
                .iter::<AdvancedRenameProfile>()
                .flatten()
                .map(|p| p.deep_clone())
                .collect(),
        }
    }
}

impl AdvrenameProfiles {
    fn new(cfg: &AdvRenameConfig, with_default: bool) -> Self {
        let profiles: gio::ListStore = cfg
            .profiles()
            .iter::<AdvancedRenameProfile>()
            .flatten()
            .map(|p| p.deep_clone())
            .collect();

        if with_default {
            profiles.append(&cfg.default_profile().deep_clone());
        }
        Self { profiles }
    }

    fn profile(&self, profile_index: usize) -> AdvancedRenameProfile {
        self.profiles
            .item(profile_index as u32)
            .and_downcast::<AdvancedRenameProfile>()
            .unwrap()
    }

    fn len(&self) -> usize {
        self.profiles.n_items() as usize
    }

    fn profile_name(&self, profile_index: usize) -> String {
        let profile = self.profile(profile_index);
        profile.name()
    }

    fn set_profile_name(&self, profile_index: usize, name: &str) {
        let profile = self.profile(profile_index);
        profile.set_name(name);
    }

    fn profile_description(&self, profile_index: usize) -> String {
        let profile = self.profile(profile_index);
        profile.template_string()
    }

    fn reset_profile(&self, profile_index: usize) {
        let profile = self.profile(profile_index);
        profile.reset();
    }

    fn duplicate_profile(&self, profile_index: usize) -> usize {
        let profile = self.profile(profile_index).deep_clone();
        self.profiles.append(&profile);
        self.profiles.n_items() as usize - 1
    }

    fn pick(&self, indexes: &[usize]) {
        let picked: Vec<_> = indexes
            .iter()
            .filter_map(|i| self.profiles.item(*i as u32))
            .collect();
        self.profiles.remove_all();
        for p in picked {
            self.profiles.append(&p);
        }
    }
}

struct AdvRenameProfileManager {
    profiles: AdvrenameProfiles,
    file_metadata_service: FileMetadataService,
}

impl ProfileManager for AdvRenameProfileManager {
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

    fn create_component(
        &self,
        profile_index: usize,
        _labels_size_group: &gtk::SizeGroup,
    ) -> gtk::Widget {
        let widget_ptr = unsafe {
            gnome_cmd_advrename_profile_component_new(
                self.profiles.profile(profile_index).to_glib_none().0,
                self.file_metadata_service.to_glib_none().0,
            )
        };
        if widget_ptr.is_null() {
            panic!("Profile editing component is null.");
        }
        unsafe {
            gnome_cmd_advrename_profile_component_update(widget_ptr);
        }
        unsafe { from_glib_full(widget_ptr) }
    }

    fn update_component(&self, _profile_index: usize, component: &gtk::Widget) {
        unsafe { gnome_cmd_advrename_profile_component_update(component.to_glib_none().0) }
    }

    fn copy_component(&self, _profile_index: usize, component: &gtk::Widget) {
        unsafe { gnome_cmd_advrename_profile_component_copy(component.to_glib_none().0) }
    }
}

mod imp {
    use super::*;

    #[derive(Default)]
    pub struct AdvancedRenameDialog {}

    #[glib::object_subclass]
    impl ObjectSubclass for AdvancedRenameDialog {
        const NAME: &'static str = "GnomeCmdAdvancedRenameDialog";
        type Type = super::AdvancedRenameDialog;
        type ParentType = gtk::Dialog;
    }

    impl ObjectImpl for AdvancedRenameDialog {
        fn constructed(&self) {
            self.parent_constructed();

            unsafe {
                gnome_cmd_advrename_dialog_init(self.obj().to_glib_none().0);
            }
        }

        fn dispose(&self) {
            unsafe {
                gnome_cmd_advrename_dialog_dispose(self.obj().to_glib_none().0);
            }
        }
    }

    impl WidgetImpl for AdvancedRenameDialog {}
    impl WindowImpl for AdvancedRenameDialog {}
    impl DialogImpl for AdvancedRenameDialog {}
}

glib::wrapper! {
    pub struct AdvancedRenameDialog(ObjectSubclass<imp::AdvancedRenameDialog>)
        @extends gtk::Widget, gtk::Window, gtk::Dialog;
}

type GnomeCmdAdvrenameDialog = <AdvancedRenameDialog as glib::object::ObjectType>::GlibType;

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_dialog_get_type() -> GType {
    AdvancedRenameDialog::static_type().into_glib()
}

extern "C" {
    fn gnome_cmd_advrename_dialog_init(dialog: *mut GnomeCmdAdvrenameDialog);
    fn gnome_cmd_advrename_dialog_dispose(dialog: *mut GnomeCmdAdvrenameDialog);

    fn gnome_cmd_advrename_dialog_update_profile_menu(dialog: *mut GnomeCmdAdvrenameDialog);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_dialog_do_manage_profiles(
    dialog_ptr: *mut GnomeCmdAdvrenameDialog,
    cfg: *mut c_void,
    fms: *mut <FileMetadataService as glib::object::ObjectType>::GlibType,
    new_profile: gboolean,
) {
    let dialog: AdvancedRenameDialog = unsafe { from_glib_none(dialog_ptr) };
    let cfg = AdvRenameConfig(cfg);
    let file_metadata_service: FileMetadataService = unsafe { from_glib_none(fms) };

    glib::spawn_future_local(async move {
        let profiles = AdvrenameProfiles::new(&cfg, new_profile != 0);

        if new_profile != 0 {
            let last_index = profiles.len() - 1;
            profiles.set_profile_name(last_index, &gettext("New profile"));
        }

        let manager = Rc::new(AdvRenameProfileManager {
            profiles,
            file_metadata_service,
        });

        if manage_profiles(
            dialog.upcast_ref(),
            &manager,
            &gettext("Profiles"),
            Some("gnome-commander-advanced-rename"),
            new_profile != 0,
        )
        .await
        {
            let profiles = manager.profiles.clone();
            cfg.set_profiles(&profiles.profiles);
            unsafe {
                gnome_cmd_advrename_dialog_update_profile_menu(dialog_ptr);
            }
        }
    });
}
