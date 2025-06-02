/*
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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
    profile::{load_advrename_profiles, save_advrename_profiles},
    profile_component::AdvancedRenameProfileComponent,
    template::{generate_file_name, CounterOptions, Template},
};
use crate::{
    advanced_rename::profile::AdvancedRenameProfile,
    connection::history::History,
    data::{GeneralOptions, GeneralOptionsRead, ProgramsOptions},
    dialogs::profiles::{manage_profiles_dialog::manage_profiles, profiles::ProfileManager},
    file::File,
    file_list::list::FileList,
    tags::tags::FileMetadataService,
    utils::{size_to_string, time_to_string},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, pango, prelude::*, subclass::prelude::*};
use std::rc::Rc;

const ADVRENAME_HISTORY_SIZE: usize = 10;

struct AdvRenameConfigInner {
    default_profile: AdvancedRenameProfile,
    profiles: gio::ListStore,
    templates: History<String>,
}

#[derive(Clone)]
struct AdvRenameConfig(Rc<AdvRenameConfigInner>);

impl AdvRenameConfig {
    fn new() -> Self {
        Self(Rc::new(AdvRenameConfigInner {
            default_profile: AdvancedRenameProfile::default(),
            profiles: gio::ListStore::new::<AdvancedRenameProfile>(),
            templates: History::new(ADVRENAME_HISTORY_SIZE),
        }))
    }

    fn load(&self) {
        let options = GeneralOptions::new();

        let variant = options.0.value("advrename-profiles");
        load_advrename_profiles(&variant, &self.0.default_profile, &self.0.profiles);

        let entries: Vec<_> = options
            .0
            .strv("advrename-template-history")
            .iter()
            .map(|entry| entry.to_string())
            .collect();
        for entry in entries.into_iter().rev() {
            self.0.templates.add(entry);
        }
    }

    fn save(&self) -> Result<(), glib::BoolError> {
        let options = GeneralOptions::new();

        let variant =
            save_advrename_profiles(&self.0.default_profile, self.0.profiles.upcast_ref());
        options.0.set_value("advrename-profiles", &variant)?;

        options
            .0
            .set_strv("advrename-template-history", self.0.templates.export())?;
        Ok(())
    }

    fn default_profile(&self) -> AdvancedRenameProfile {
        self.0.default_profile.clone()
    }

    fn profiles(&self) -> gio::ListStore {
        self.0.profiles.clone()
    }

    pub fn set_profiles(&self, profiles: &gio::ListStore) {
        let store = self.profiles();
        store.remove_all();
        for p in profiles.iter::<AdvancedRenameProfile>().flatten() {
            store.append(&p);
        }
    }

    pub fn template_history(&self) -> Vec<String> {
        self.0.templates.export()
    }

    pub fn template_history_add(&self, entry: &str) {
        self.0.templates.add(entry.to_owned());
    }
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
        dialogs::file_properties_dialog::FilePropertiesDialog,
        file::File,
        file_list::list::FileList,
        file_view::file_view,
        tags::file_metadata::FileMetadata,
        utils::{
            attributes_bold, dialog_button_box, display_help, handle_escape_key, MenuBuilderExt,
        },
    };
    use std::{
        cell::{OnceCell, RefCell},
        path::Path,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::AdvancedRenameDialog)]
    pub struct AdvancedRenameDialog {
        #[property(get, construct_only)]
        file_metadata_service: OnceCell<FileMetadataService>,

        pub(super) config: OnceCell<AdvRenameConfig>,

        #[property(get, set)]
        profile_component: OnceCell<AdvancedRenameProfileComponent>,

        pub(super) files: gtk::ListStore,
        pub(super) files_view: gtk::TreeView,
        profile_menu_button: gtk::MenuButton,

        pub rename_template: RefCell<Option<Rc<Template>>>,

        #[property(get, set, nullable)]
        file_list: RefCell<Option<FileList>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for AdvancedRenameDialog {
        const NAME: &'static str = "GnomeCmdAdvancedRenameDialog";
        type Type = super::AdvancedRenameDialog;
        type ParentType = gtk::Window;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action("advrename.file-list-remove", None, |obj, _, _| {
                obj.imp().file_list_remove();
            });
            klass.install_action_async("advrename.file-list-view", None, |obj, _, _| async move {
                obj.imp().file_list_view().await
            });
            klass.install_action_async(
                "advrename.file-list-properties",
                None,
                |obj, _, _| async move { obj.imp().file_list_properties().await },
            );
            klass.install_action("advrename.update-file-list", None, |obj, _, _| {
                obj.imp().file_list_update_files();
            });
            klass.install_action_async("advrename.save-profile", None, |obj, _, _| async move {
                obj.imp().save_profile().await
            });
            klass.install_action_async("advrename.manage-profiles", None, |obj, _, _| async move {
                obj.imp().manage_profiles().await
            });
            klass.install_action(
                "advrename.load-profile",
                Some(&i32::static_variant_type()),
                |obj, _, param| {
                    if let Some(index) = param.and_then(|p| p.get::<i32>()) {
                        obj.imp().load_profile(index);
                    }
                },
            );
        }

        fn new() -> Self {
            Self {
                file_metadata_service: Default::default(),
                config: Default::default(),
                profile_component: Default::default(),

                files: gtk::ListStore::new(&[
                    File::static_type(),
                    String::static_type(),
                    String::static_type(),
                    String::static_type(),
                    String::static_type(),
                    bool::static_type(),
                    glib::Pointer::static_type(),
                ]),
                files_view: gtk::TreeView::builder()
                    .reorderable(true)
                    .enable_search(true)
                    .search_column(FileColumns::COL_NAME as i32)
                    .build(),
                profile_menu_button: gtk::MenuButton::builder()
                    .label(gettext("Profiles…"))
                    .build(),

                rename_template: Default::default(),

                file_list: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for AdvancedRenameDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();

            this.set_title(Some(&gettext("Advanced Rename Tool")));
            this.set_resizable(true);

            let vbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .spacing(6)
                .build();
            this.set_child(Some(&vbox));

            let profile_component =
                AdvancedRenameProfileComponent::new(&this.file_metadata_service());
            profile_component.set_vexpand(true);
            vbox.append(&profile_component);
            self.profile_component
                .set(profile_component.clone())
                .unwrap();

            vbox.append(
                &gtk::Label::builder()
                    .label(gettext("Results"))
                    .attributes(&attributes_bold())
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .build(),
            );

            self.files_view
                .selection()
                .set_mode(gtk::SelectionMode::Browse);
            self.files_view.append_column(&column(
                FileColumns::COL_NAME,
                &gettext("Old name"),
                &gettext("Current file name"),
                0.0,
            ));
            self.files_view.append_column(&column(
                FileColumns::COL_NEW_NAME,
                &gettext("New name"),
                &gettext("New file name"),
                0.0,
            ));
            self.files_view.append_column(&column(
                FileColumns::COL_SIZE,
                &gettext("Size"),
                &gettext("File size"),
                1.0,
            ));
            self.files_view.append_column(&column(
                FileColumns::COL_DATE,
                &gettext("Date"),
                &gettext("File modification date"),
                0.0,
            ));

            vbox.append(
                &gtk::ScrolledWindow::builder()
                    .hscrollbar_policy(gtk::PolicyType::Never)
                    .vscrollbar_policy(gtk::PolicyType::Automatic)
                    .has_frame(true)
                    .vexpand(true)
                    .child(&self.files_view)
                    .build(),
            );

            profile_component.connect_local(
                "template-changed",
                true,
                glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    #[upgrade_or]
                    None,
                    move |_| {
                        imp.update_template();
                        imp.update_new_filenames();
                        None
                    }
                ),
            );
            profile_component.connect_local(
                "counter-changed",
                true,
                glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    #[upgrade_or]
                    None,
                    move |_| {
                        imp.update_new_filenames();
                        None
                    }
                ),
            );
            profile_component.connect_local(
                "regex-changed",
                true,
                glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    #[upgrade_or]
                    None,
                    move |_| {
                        imp.update_new_filenames();
                        None
                    }
                ),
            );

            self.files.connect_row_deleted(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _| imp.update_new_filenames()
            ));
            self.files_view.connect_row_activated(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _, _| {
                    glib::spawn_future_local(async move { imp.file_list_properties().await });
                }
            ));
            self.files_view.connect_cursor_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.file_list_cursor_changed()
            ));
            let button_gesture = gtk::GestureClick::builder().button(3).build();
            button_gesture.connect_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, n_press, x, y| imp.file_list_button_pressed(n_press, x, y)
            ));
            self.files_view.add_controller(button_gesture);

            let help_button = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .build();
            help_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| {
                    glib::spawn_future_local(async move {
                        display_help(this.upcast_ref(), Some("gnome-commander-advanced-rename"))
                            .await
                    });
                }
            ));
            let reset_button = gtk::Button::builder()
                .label(gettext("Reset"))
                .use_underline(true)
                .build();
            reset_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| {
                    let config = this.imp().config();
                    config.default_profile().reset();
                    this.profile_component()
                        .set_template_history(config.template_history().iter().map(|s| s.as_str()));
                    this.profile_component().update();
                }
            ));
            let close_button = gtk::Button::builder()
                .label(gettext("_Close"))
                .use_underline(true)
                .build();
            close_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| {
                    this.profile_component().copy();
                    this.close();
                    this.imp().unset();
                }
            ));
            let apply_button = gtk::Button::builder()
                .label(gettext("_Apply"))
                .use_underline(true)
                .build();
            apply_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.apply()
            ));

            this.set_default_widget(Some(&apply_button));

            vbox.append(&dialog_button_box(
                &[&help_button],
                &[
                    self.profile_menu_button.upcast_ref::<gtk::Widget>(),
                    reset_button.upcast_ref(),
                    close_button.upcast_ref(),
                    apply_button.upcast_ref(),
                ],
            ));

            handle_escape_key(
                this.upcast_ref(),
                &gtk::CallbackAction::new(glib::clone!(
                    #[weak]
                    this,
                    #[upgrade_or]
                    glib::Propagation::Proceed,
                    move |_, _| {
                        this.profile_component().copy();
                        this.close();
                        this.imp().unset();
                        glib::Propagation::Proceed
                    }
                )),
            );

            let options = GeneralOptions::new();

            remember_window_size(&*self.obj(), &options.0);
        }
    }

    impl WidgetImpl for AdvancedRenameDialog {}
    impl WindowImpl for AdvancedRenameDialog {}

    impl AdvancedRenameDialog {
        fn config(&self) -> AdvRenameConfig {
            self.config.get().unwrap().clone()
        }

        pub fn update_template(&self) {
            let entry = self.obj().profile_component().template_entry();
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
            let profile_component = self.obj().profile_component();
            let trim_blanks = profile_component.profile().map(|p| p.trim_blanks());
            let case_convesion = profile_component.profile().map(|p| p.case_conversion());

            let count = self.files.iter_n_children(None) as u64;

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
            if let Some(iter) = self.files.iter_first() {
                loop {
                    let file = self.files.get::<File>(&iter, FileColumns::COL_FILE as i32);

                    let metadata_ptr = self
                        .files
                        .get::<glib::Pointer>(&iter, FileColumns::COL_METADATA as i32);
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

                    self.files
                        .set(&iter, &[(FileColumns::COL_NEW_NAME as u32, &file_name)]);

                    if !self.files.iter_next(&iter) {
                        break;
                    }

                    index += 1;
                }
            }
        }

        fn file_list_remove(&self) {
            if let Some((_, iter)) = self.files_view.selection().selected() {
                self.files.remove(&iter);
            }
        }

        async fn file_list_view(&self) {
            if let Some((model, iter)) = self.files_view.selection().selected() {
                let file = model.get::<File>(&iter, FileColumns::COL_FILE as i32);

                let options = ProgramsOptions::new();

                if let Err(error) = file_view(
                    self.obj().upcast_ref(),
                    &file,
                    None,
                    &options,
                    &self.obj().file_metadata_service(),
                )
                .await
                {
                    error.show(self.obj().upcast_ref()).await;
                }
            }
        }

        async fn file_list_properties(&self) {
            if let Some((model, iter)) = self.files_view.selection().selected() {
                let file = model.get::<File>(&iter, FileColumns::COL_FILE as i32);

                let file_changed = FilePropertiesDialog::show(
                    self.obj().upcast_ref(),
                    &self.obj().file_metadata_service(),
                    &file,
                )
                .await;
                if file_changed {
                    self.file_list_update_files();
                }
            }
        }

        fn file_list_cursor_changed(&self) {
            if let Some((model, iter)) = self.files_view.selection().selected() {
                let file = model.get::<File>(&iter, FileColumns::COL_FILE as i32);

                self.obj()
                    .profile_component()
                    .set_sample_file_name(Some(file.get_name()));
            }
        }

        fn file_list_button_pressed(&self, n_press: i32, x: f64, y: f64) {
            if n_press != 1 {
                return;
            }

            let menu = gio::Menu::new()
                .section(
                    gio::Menu::new()
                        .item(
                            gettext("Remove from file list"),
                            "advrename.file-list-remove",
                        )
                        .item(gettext("View file"), "advrename.file-list-view")
                        .item(gettext("File properties"), "advrename.file-list-properties"),
                )
                .item(gettext("Update file list"), "advrename.update-file-list");

            let popover = gtk::PopoverMenu::builder()
                .menu_model(&menu)
                .pointing_to(&gdk::Rectangle::new(x as i32, y as i32, 0, 0))
                .build();
            popover.set_parent(&self.files_view);
            popover.present();
            popover.popup();
        }

        fn file_list_update_files(&self) {
            let file_metadata_service = self.obj().file_metadata_service();
            let options = GeneralOptions::new();

            let size_mode = options.size_display_mode();
            let date_format = options.date_display_format();

            if let Some(iter) = self.files.iter_first() {
                loop {
                    let file = self.files.get::<File>(&iter, FileColumns::COL_FILE as i32);

                    let metadata = Box::new(file_metadata_service.extract_metadata(&file));

                    self.files.set(
                        &iter,
                        &[
                            (FileColumns::COL_NAME as u32, &file.get_name()),
                            (
                                FileColumns::COL_SIZE as u32,
                                &file.size().map(|s| size_to_string(s, size_mode)),
                            ),
                            (
                                FileColumns::COL_DATE as u32,
                                &file
                                    .modification_date()
                                    .and_then(|dt| time_to_string(dt, &date_format).ok()),
                            ),
                            (FileColumns::COL_RENAME_FAILED as u32, &false),
                            (
                                FileColumns::COL_METADATA as u32,
                                &(Box::leak(metadata) as *mut _ as glib::Pointer),
                            ),
                        ],
                    );

                    if !self.files.iter_next(&iter) {
                        break;
                    }
                }
            }

            self.update_template();
            self.update_new_filenames();
        }

        pub fn update_profile_menu(&self) {
            if let Some(config) = self.config.get() {
                self.profile_menu_button
                    .set_menu_model(Some(&create_profiles_menu(&config)));
            }
        }

        async fn save_profile(&self) {
            self.obj().profile_component().copy();
            let cfg = self.config();

            let profiles = AdvrenameProfiles::new(&cfg, true);
            let last_index = profiles.len() - 1;
            profiles.set_profile_name(last_index, &gettext("New profile"));

            let manager = Rc::new(AdvRenameProfileManager {
                config: cfg,
                profiles,
                file_metadata_service: self.obj().file_metadata_service(),
            });

            if manage_profiles(
                self.obj().upcast_ref(),
                &manager,
                &gettext("Profiles"),
                Some("gnome-commander-advanced-rename"),
                true,
            )
            .await
            {
                let profiles = manager.profiles.clone();
                manager.config.set_profiles(&profiles.profiles);
                self.update_profile_menu();
            }
        }

        async fn manage_profiles(&self) {
            let cfg = self.config();

            let profiles = AdvrenameProfiles::new(&cfg, false);

            let manager = Rc::new(AdvRenameProfileManager {
                config: cfg,
                profiles,
                file_metadata_service: self.obj().file_metadata_service(),
            });

            if manage_profiles(
                self.obj().upcast_ref(),
                &manager,
                &gettext("Profiles"),
                Some("gnome-commander-advanced-rename"),
                false,
            )
            .await
            {
                let profiles = manager.profiles.clone();
                manager.config.set_profiles(&profiles.profiles);
                self.update_profile_menu();
            }
        }

        fn load_profile(&self, index: i32) {
            let cfg = self.config();
            let Some(profile) = index
                .try_into()
                .ok()
                .and_then(|i| cfg.profiles().item(i))
                .and_downcast::<AdvancedRenameProfile>()
            else {
                return;
            };

            cfg.default_profile().copy_from(&profile);

            self.obj()
                .profile_component()
                .set_template_history(cfg.template_history().iter().map(|i| i.as_str()));
            self.obj().profile_component().update();

            self.update_new_filenames();
        }

        fn apply(&self) {
            let old_focused_file_name = self
                .obj()
                .file_list()
                .and_then(|fl| fl.focused_file())
                .map(|f| f.get_name());
            let mut new_focused_file_name = None;

            if let Some(iter) = self.files.iter_first() {
                loop {
                    let file = self.files.get::<File>(&iter, FileColumns::COL_FILE as i32);
                    let new_name = self
                        .files
                        .get::<Option<String>>(&iter, FileColumns::COL_NEW_NAME as i32)
                        .filter(|n| !n.is_empty());

                    if let Some(new_name) = new_name {
                        let old_name = file.get_name();
                        if old_name != new_name {
                            let result = file.rename(&new_name);

                            self.files.set(
                                &iter,
                                &[
                                    (FileColumns::COL_NAME as u32, &file.get_name()),
                                    (FileColumns::COL_RENAME_FAILED as u32, &result.is_err()),
                                ],
                            );

                            if new_focused_file_name.is_none()
                                && result.is_ok()
                                && old_focused_file_name.as_ref() == Some(&old_name)
                            {
                                new_focused_file_name = Some(new_name);
                            }
                        }
                    }

                    if !self.files.iter_next(&iter) {
                        break;
                    }
                }
            }

            if let Some(new_focused_file_name) = new_focused_file_name {
                if let Some(fl) = self.obj().file_list() {
                    fl.focus_file(&Path::new(&new_focused_file_name), true);
                }
            }

            self.update_new_filenames();
            let template_entry = self.obj().profile_component().template_entry();
            self.config().template_history_add(&template_entry);
            self.obj()
                .profile_component()
                .set_template_history(self.config().template_history().iter().map(|i| i.as_str()));
        }

        fn unset(&self) {
            self.files_view.set_model(gtk::TreeModel::NONE);
            self.files.clear();
            self.config().save();
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

    fn column(column: FileColumns, title: &str, tooltip: &str, xalign: f32) -> gtk::TreeViewColumn {
        let renderer = gtk::CellRendererText::new();
        renderer.set_foreground(Some("red"));
        renderer.set_style(pango::Style::Italic);
        renderer.set_xalign(xalign);

        let col = gtk::TreeViewColumn::with_attributes(
            title,
            &renderer,
            &[
                ("text", column as i32),
                ("foreground-set", FileColumns::COL_RENAME_FAILED as i32),
                ("style-set", FileColumns::COL_RENAME_FAILED as i32),
            ],
        );
        col.set_clickable(true);
        col.set_resizable(true);
        col.button().set_tooltip_text(Some(tooltip));
        col
    }

    fn create_profiles_menu(cfg: &AdvRenameConfig) -> gio::Menu {
        let menu = gio::Menu::new();

        menu.append(
            Some(&gettext("_Save Profile As…")),
            Some("advrename.save-profile"),
        );

        let profiles = cfg.profiles();
        if profiles.n_items() > 0 {
            menu.append(
                Some(&gettext("_Manage Profiles…")),
                Some(&"advrename.manage-profiles"),
            );

            let profiles_menu = gio::Menu::new();
            for (i, profile) in profiles
                .iter::<AdvancedRenameProfile>()
                .flatten()
                .enumerate()
            {
                let item = gio::MenuItem::new(Some(&profile.name()), None);
                item.set_action_and_target_value(
                    Some("advrename.load-profile"),
                    Some(&(i as i32).to_variant()),
                );
                profiles_menu.append_item(&item);
            }
            menu.append_section(None, &profiles_menu);
        }

        menu
    }
}

glib::wrapper! {
    pub struct AdvancedRenameDialog(ObjectSubclass<imp::AdvancedRenameDialog>)
        @extends gtk::Widget, gtk::Window;
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

pub fn advanced_rename_dialog_show(
    parent_window: &gtk::Window,
    file_list: &FileList,
    file_metadata_service: &FileMetadataService,
) {
    let files = file_list.selected_files();
    if files.is_empty() {
        return;
    }

    let cfg = AdvRenameConfig::new();
    cfg.load();

    let dialog: AdvancedRenameDialog = glib::Object::builder()
        .property("file-metadata-service", file_metadata_service)
        .property("transient-for", parent_window)
        .property("file-list", file_list)
        .build();

    dialog.imp().config.set(cfg.clone()).ok().unwrap();

    dialog.imp().update_profile_menu();

    dialog
        .profile_component()
        .set_profile(Some(cfg.default_profile()));
    dialog.imp().update_template();
    dialog
        .profile_component()
        .set_template_history(cfg.template_history().iter().map(|s| s.as_str()));
    dialog.profile_component().update();

    dialog.profile_component().grab_focus();

    gnome_cmd_advrename_dialog_set(&dialog, &files);

    dialog.present();
}

fn gnome_cmd_advrename_dialog_set(dialog: &AdvancedRenameDialog, file_list: &glib::List<File>) {
    dialog
        .profile_component()
        .set_sample_file_name(file_list.front().map(|f| f.get_name()));

    let options = GeneralOptions::new();

    let size_mode = options.size_display_mode();
    let date_format = options.date_display_format();

    let file_metadata_service = dialog.file_metadata_service();
    let files = &dialog.imp().files;
    for file in file_list {
        let metadata = Box::new(file_metadata_service.extract_metadata(&file));

        let iter = files.append();
        files.set(
            &iter,
            &[
                (imp::FileColumns::COL_FILE as u32, &file),
                (imp::FileColumns::COL_NAME as u32, &file.get_name()),
                (
                    imp::FileColumns::COL_SIZE as u32,
                    &file.size().map(|s| size_to_string(s, size_mode)),
                ),
                (
                    imp::FileColumns::COL_DATE as u32,
                    &file
                        .modification_date()
                        .and_then(|dt| time_to_string(dt, &date_format).ok()),
                ),
                (imp::FileColumns::COL_RENAME_FAILED as u32, &false),
                (
                    imp::FileColumns::COL_METADATA as u32,
                    &(Box::leak(metadata) as *mut _ as glib::Pointer),
                ),
            ],
        );
    }
    dialog.imp().files_view.set_model(Some(files));
    dialog.imp().update_new_filenames();
}
