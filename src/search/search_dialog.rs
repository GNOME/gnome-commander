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

use super::{profile::SearchProfile, selection_profile_component::SelectionProfileComponent};
use crate::{
    data::SearchConfig,
    dialogs::profiles::{manage_profiles_dialog::manage_profiles, profiles::ProfileManager},
};
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{
        ffi::{gboolean, GType},
        translate::{from_glib_none, IntoGlib, ToGlibPtr},
    },
    prelude::*,
    subclass::prelude::*,
};
use std::{ffi::c_void, rc::Rc};

struct SearchProfiles {
    profiles: gio::ListStore,
}

impl Clone for SearchProfiles {
    fn clone(&self) -> Self {
        Self {
            profiles: self
                .profiles
                .iter::<SearchProfile>()
                .flatten()
                .map(|p| p.deep_clone())
                .collect(),
        }
    }
}

impl SearchProfiles {
    fn new(cfg: &SearchConfig, with_default: bool) -> Self {
        let profiles: gio::ListStore = cfg
            .profiles()
            .iter::<SearchProfile>()
            .flatten()
            .map(|p| p.deep_clone())
            .collect();

        if with_default {
            profiles.append(&cfg.default_profile().deep_clone());
        }
        Self { profiles }
    }

    fn profile(&self, profile_index: usize) -> Option<SearchProfile> {
        self.profiles
            .item(profile_index as u32)
            .and_downcast::<SearchProfile>()
    }

    fn len(&self) -> usize {
        self.profiles.n_items() as usize
    }

    fn profile_name(&self, profile_index: usize) -> String {
        self.profile(profile_index)
            .map(|p| p.name())
            .unwrap_or_default()
    }

    fn set_profile_name(&self, profile_index: usize, name: &str) {
        if let Some(profile) = self.profile(profile_index) {
            profile.set_name(name);
        }
    }

    fn profile_description(&self, profile_index: usize) -> String {
        self.profile(profile_index)
            .map(|p| p.filename_pattern())
            .unwrap_or_default()
    }

    fn reset_profile(&self, profile_index: usize) {
        if let Some(profile) = self.profile(profile_index) {
            profile.reset();
        }
    }

    fn duplicate_profile(&self, profile_index: usize) -> usize {
        let profile = self
            .profile(profile_index)
            .map(|p| p.deep_clone())
            .unwrap_or_default();
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

struct SearchProfileManager {
    config: SearchConfig,
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

    fn create_component(
        &self,
        profile_index: usize,
        labels_size_group: &gtk::SizeGroup,
    ) -> gtk::Widget {
        let widget = SelectionProfileComponent::with_profile(
            &self.profiles.profile(profile_index).unwrap(),
            Some(labels_size_group),
        );
        widget.set_name_patterns_history(self.config.name_patterns());
        widget.set_content_patterns_history(self.config.content_patterns());
        widget.update();
        widget.grab_focus();
        widget.upcast()
    }

    fn update_component(&self, _profile_index: usize, component: &gtk::Widget) {
        component
            .downcast_ref::<SelectionProfileComponent>()
            .unwrap()
            .update();
    }

    fn copy_component(&self, _profile_index: usize, component: &gtk::Widget) {
        component
            .downcast_ref::<SelectionProfileComponent>()
            .unwrap()
            .copy();
    }
}

mod imp {
    use super::*;
    use crate::{
        data::GeneralOptions, file_list::list::FileList, select_directory_button::DirectoryButton,
        tags::tags::FileMetadataService,
    };
    use std::cell::{OnceCell, RefCell};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::SearchDialog)]
    pub struct SearchDialog {
        pub labels_size_group: gtk::SizeGroup,

        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,

        #[property(get, set)]
        pub dir_browser: RefCell<DirectoryButton>,
        #[property(get, set)]
        pub profile_component: RefCell<SelectionProfileComponent>,
        #[property(get, set)]
        pub result_list: RefCell<Option<FileList>>,
        #[property(get, set)]
        pub status_label: RefCell<gtk::Label>,
        #[property(get, set)]
        pub progress_bar: RefCell<gtk::ProgressBar>,
        #[property(get, set)]
        pub profile_menu_button: RefCell<gtk::MenuButton>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SearchDialog {
        const NAME: &'static str = "GnomeCmdSearchDialog";
        type Type = super::SearchDialog;
        type ParentType = gtk::Dialog;

        fn new() -> Self {
            let labels_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);
            Self {
                file_metadata_service: Default::default(),
                dir_browser: Default::default(),
                profile_component: RefCell::new(SelectionProfileComponent::new(Some(
                    &labels_size_group,
                ))),
                result_list: Default::default(),
                status_label: Default::default(),
                progress_bar: RefCell::new(
                    gtk::ProgressBar::builder()
                        .show_text(false)
                        .pulse_step(0.02)
                        .build(),
                ),
                profile_menu_button: RefCell::new(
                    gtk::MenuButton::builder()
                        .label(gettext("Profiles…"))
                        .build(),
                ),
                labels_size_group,
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for SearchDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();

            this.set_title(Some(&gettext("Search…")));
            this.set_resizable(true);

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .row_spacing(6)
                .column_spacing(12)
                .build();
            this.content_area().append(&grid);

            // search in
            let dir_browser_label = gtk::Label::builder()
                .label(gettext("_Look in folder:"))
                .use_underline(true)
                .mnemonic_widget(&*self.dir_browser.borrow())
                .halign(gtk::Align::Start)
                .valign(gtk::Align::Center)
                .xalign(0.0)
                .build();
            self.labels_size_group.add_widget(&dir_browser_label);
            grid.attach(&dir_browser_label, 0, 0, 1, 1);

            let dir_browser = this.dir_browser();
            dir_browser.set_hexpand(true);
            grid.attach(&dir_browser, 1, 0, 1, 1);

            // profile
            grid.attach(&this.profile_component(), 0, 1, 2, 1);

            // file list
            let result_list = FileList::new(&this.file_metadata_service());
            result_list.set_height_request(200);
            self.result_list.replace(Some(result_list.clone()));

            let sw = gtk::ScrolledWindow::builder()
                .vexpand(true)
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .child(&result_list)
                .build();
            grid.attach(&sw, 0, 2, 2, 1);

            // status & progress
            let statusbar = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .build();
            statusbar.append(&this.status_label());
            statusbar.append(&this.progress_bar());
            this.progress_bar().set_visible(false);
            grid.attach(&statusbar, 0, 3, 2, 1);

            unsafe {
                gnome_cmd_search_dialog_init(this.to_glib_none().0);
            }

            let options = GeneralOptions::new();

            remember_window_size(&*this, &options.0);
        }

        fn dispose(&self) {
            unsafe { gnome_cmd_search_dialog_dispose(self.obj().to_glib_none().0) }
        }
    }

    impl WidgetImpl for SearchDialog {}
    impl WindowImpl for SearchDialog {}
    impl DialogImpl for SearchDialog {}
}

glib::wrapper! {
    pub struct SearchDialog(ObjectSubclass<imp::SearchDialog>)
        @extends gtk::Widget, gtk::Window, gtk::Dialog;
}

type GnomeCmdSearchDialog = <SearchDialog as glib::object::ObjectType>::GlibType;

#[no_mangle]
pub extern "C" fn gnome_cmd_search_dialog_get_type() -> GType {
    SearchDialog::static_type().into_glib()
}

extern "C" {
    fn gnome_cmd_search_dialog_init(dialog: *mut GnomeCmdSearchDialog);
    fn gnome_cmd_search_dialog_dispose(dialog: *mut GnomeCmdSearchDialog);
    fn gnome_cmd_search_dialog_update_profile_menu(dialog: *mut GnomeCmdSearchDialog);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_search_dialog_do_manage_profiles(
    dialog_ptr: *mut GnomeCmdSearchDialog,
    cfg_ptr: *mut c_void,
    new_profile: gboolean,
) {
    let dialog: SearchDialog = unsafe { from_glib_none(dialog_ptr) };
    let config = unsafe { SearchConfig::from_ptr(cfg_ptr) };

    glib::spawn_future_local(async move {
        let profiles = SearchProfiles::new(&config, new_profile != 0);

        if new_profile != 0 {
            let last_index = profiles.len() - 1;
            profiles.set_profile_name(last_index, &gettext("New profile"));
        }

        let manager = Rc::new(SearchProfileManager { config, profiles });

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
            config.set_profiles(&profiles.profiles);
            unsafe {
                gnome_cmd_search_dialog_update_profile_menu(dialog_ptr);
            }
        }
    });
}

fn remember_window_size(dialog: &SearchDialog, settings: &gio::Settings) {
    settings
        .bind("search-win-width", dialog, "default-width")
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
        .bind("search-win-height", dialog, "default-height")
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
