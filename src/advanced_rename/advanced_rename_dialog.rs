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

use super::{
    profile_component::{ffi::GnomeCmdAdvrenameProfileComponent, AdvancedRenameProfileComponent},
    template::{generate_file_name, CounterOptions, Template},
};
use crate::{
    advanced_rename::profile::{AdvRenameProfilePtr, AdvancedRenameProfile},
    dialogs::profiles::{manage_profiles_dialog::manage_profiles, profiles::ProfileManager},
    tags::tags::FileMetadataService,
};
use gettextrs::gettext;
use gtk::{
    ffi::GtkListStore,
    gio::{self, ffi::GListStore},
    glib::{
        ffi::{gboolean, GList, GType},
        translate::{
            from_glib_borrow, from_glib_none, Borrowed, FromGlibPtrNone, IntoGlib, ToGlibPtr,
        },
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

    pub fn template_history(&self) -> glib::List<glib::GStringPtr> {
        unsafe { glib::List::from_glib_none(gnome_cmd_advrename_config_template_history(self.0)) }
    }
}

extern "C" {
    fn gnome_cmd_advrename_config_default_profile(cfg: *mut c_void) -> *mut AdvRenameProfilePtr;
    fn gnome_cmd_advrename_config_profiles(cfg: *mut c_void) -> *mut GListStore;
    fn gnome_cmd_advrename_config_template_history(cfg: *mut c_void) -> *const GList;
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
    config: AdvRenameConfig,
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
        let component = AdvancedRenameProfileComponent::new(&self.file_metadata_service);
        component.set_profile(Some(self.profiles.profile(profile_index)));
        component.set_template_history(self.config.template_history().iter().map(|s| s.as_str()));
        component.update();
        component.grab_focus();
        component.upcast()
    }

    fn update_component(&self, _profile_index: usize, component: &gtk::Widget) {
        let component = component
            .downcast_ref::<AdvancedRenameProfileComponent>()
            .unwrap();
        component.set_template_history(self.config.template_history().iter().map(|s| s.as_str()));
        component.update();
    }

    fn copy_component(&self, _profile_index: usize, component: &gtk::Widget) {
        let component = component
            .downcast_ref::<AdvancedRenameProfileComponent>()
            .unwrap();
        component.copy();
    }
}

mod imp {
    use super::*;
    use crate::{
        data::GeneralOptions,
        file::{ffi::GnomeCmdFile, File},
        tags::file_metadata::FileMetadata,
    };
    use std::cell::RefCell;

    #[derive(Default)]
    pub struct AdvancedRenameDialog {
        pub rename_template: RefCell<Option<Rc<Template>>>,
    }

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

            let options = GeneralOptions::new();

            remember_window_size(&*self.obj(), &options.0);
        }
    }

    impl WidgetImpl for AdvancedRenameDialog {}
    impl WindowImpl for AdvancedRenameDialog {}
    impl DialogImpl for AdvancedRenameDialog {}

    impl AdvancedRenameDialog {
        fn files(&self) -> gtk::ListStore {
            unsafe {
                from_glib_none(gnome_cmd_advrename_dialog_get_files(
                    self.obj().to_glib_none().0,
                ))
            }
        }

        fn profile_component(&self) -> AdvancedRenameProfileComponent {
            unsafe {
                from_glib_none(gnome_cmd_advrename_dialog_get_profile_component(
                    self.obj().to_glib_none().0,
                ))
            }
        }

        fn config(&self) -> AdvRenameConfig {
            unsafe {
                AdvRenameConfig(gnome_cmd_advrename_dialog_get_config(
                    self.obj().to_glib_none().0,
                ))
            }
        }

        pub fn update_template(&self) {
            let entry = self.profile_component().template_entry();
            match Template::new(&entry) {
                Ok(template) => {
                    self.rename_template.replace(Some(template));
                }
                Err(error) => {
                    eprintln!("Parse error: {error}");
                    self.rename_template.replace(None);
                }
            }
        }

        pub fn update_new_filenames(&self) {
            let Some(rename_template) = self.rename_template.borrow().clone() else {
                return;
            };
            let files = self.files();
            let profile_component = self.profile_component();
            let trim_blanks = profile_component.profile().map(|p| p.trim_blanks());
            let case_convesion = profile_component.profile().map(|p| p.case_conversion());

            let count = files.iter_n_children(None) as u64;

            let rx: Vec<_> = profile_component
                .valid_regexes()
                .into_iter()
                .filter_map(|r| match r.compile_pattern() {
                    Ok(regex) => Some((r, regex)),
                    Err(error) => {
                        eprintln!("{error}");
                        None
                    }
                })
                .collect();

            let default_profile = self.config().default_profile();
            let options = CounterOptions {
                start: default_profile.counter_start() as i64,
                step: default_profile.counter_step() as i64,
                precision: default_profile.counter_width() as usize,
            };

            let mut index = 0;
            if let Some(iter) = files.iter_first() {
                loop {
                    let file_ptr = files.get::<glib::Pointer>(&iter, FileColumns::COL_FILE as i32)
                        as *mut GnomeCmdFile;
                    let file: Borrowed<File> = unsafe { from_glib_borrow(file_ptr) };

                    let metadata_ptr =
                        files.get::<glib::Pointer>(&iter, FileColumns::COL_METADATA as i32);
                    let metadata: &FileMetadata = unsafe { &*(metadata_ptr as *mut FileMetadata) };

                    let mut file_name = generate_file_name(
                        &rename_template,
                        options,
                        index,
                        count,
                        &file,
                        metadata,
                    );

                    for (r, regex) in &rx {
                        match regex.replace(
                            &file_name,
                            0,
                            &r.replacement,
                            glib::RegexMatchFlags::NOTEMPTY,
                        ) {
                            Ok(new) => {
                                file_name = new.to_string();
                            }
                            Err(error) => {
                                eprintln!("{error}")
                            }
                        }
                    }
                    if let Some(case_convesion) = case_convesion {
                        file_name = case_convesion.apply(&file_name);
                    }
                    if let Some(trim_blanks) = trim_blanks {
                        file_name = trim_blanks.apply(&file_name).to_owned();
                    }

                    files.set(&iter, &[(FileColumns::COL_NEW_NAME as u32, &file_name)]);

                    if !files.iter_next(&iter) {
                        break;
                    }

                    index += 1;
                }
            }
        }
    }

    #[repr(i32)]
    pub enum FileColumns {
        COL_FILE = 0,
        COL_NAME,
        COL_NEW_NAME,
        COL_SIZE,
        COL_DATE,
        COL_RENAME_FAILED,
        COL_METADATA,
    }
}

glib::wrapper! {
    pub struct AdvancedRenameDialog(ObjectSubclass<imp::AdvancedRenameDialog>)
        @extends gtk::Widget, gtk::Window, gtk::Dialog;
}

fn remember_window_size(dialog: &AdvancedRenameDialog, settings: &gio::Settings) {
    settings
        .bind("advrename-win-width", &*dialog, "default-width")
        .mapping(|v, _| {
            let width: i32 = v.get::<u32>()?.try_into().ok()?;
            Some(width.to_value())
        })
        .set_mapping(|v, _| {
            let width: u32 = v.get::<i32>().ok()?.try_into().ok()?;
            Some(width.to_variant())
        })
        .build();

    settings
        .bind("advrename-win-height", &*dialog, "default-height")
        .mapping(|v, _| {
            let height: i32 = v.get::<u32>()?.try_into().ok()?;
            Some(height.to_value())
        })
        .set_mapping(|v, _| {
            let height: u32 = v.get::<i32>().ok()?.try_into().ok()?;
            Some(height.to_variant())
        })
        .build();
}

type GnomeCmdAdvrenameDialog = <AdvancedRenameDialog as glib::object::ObjectType>::GlibType;

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_dialog_get_type() -> GType {
    AdvancedRenameDialog::static_type().into_glib()
}

extern "C" {
    fn gnome_cmd_advrename_dialog_init(dialog: *mut GnomeCmdAdvrenameDialog);

    fn gnome_cmd_advrename_dialog_update_profile_menu(dialog: *mut GnomeCmdAdvrenameDialog);

    fn gnome_cmd_advrename_dialog_get_files(
        dialog: *mut GnomeCmdAdvrenameDialog,
    ) -> *mut GtkListStore;
    fn gnome_cmd_advrename_dialog_get_profile_component(
        dialog: *mut GnomeCmdAdvrenameDialog,
    ) -> *mut GnomeCmdAdvrenameProfileComponent;
    fn gnome_cmd_advrename_dialog_get_config(dialog: *mut GnomeCmdAdvrenameDialog) -> *mut c_void;
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
            config: cfg,
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
            manager.config.set_profiles(&profiles.profiles);
            unsafe {
                gnome_cmd_advrename_dialog_update_profile_menu(dialog_ptr);
            }
        }
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_dialog_update_template(dialog: *mut GnomeCmdAdvrenameDialog) {
    let dialog: Borrowed<AdvancedRenameDialog> = unsafe { from_glib_borrow(dialog) };
    dialog.imp().update_template();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_advrename_dialog_update_new_filenames(
    dialog: *mut GnomeCmdAdvrenameDialog,
) {
    let dialog: Borrowed<AdvancedRenameDialog> = unsafe { from_glib_borrow(dialog) };
    dialog.imp().update_new_filenames();
}
