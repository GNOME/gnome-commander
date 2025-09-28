/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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
    backend::{SearchBackend, SearchMessage},
    profile::SearchProfile,
    selection_profile_component::SelectionProfileComponent,
};
use crate::{
    dialogs::profiles::{manage_profiles_dialog::manage_profiles, profiles::ProfileManager},
    dir::Directory,
    libgcmd::file_descriptor::FileDescriptorExt,
    main_win::MainWindow,
    options::options::SearchConfig,
    tags::tags::FileMetadataService,
};
use gettextrs::{gettext, ngettext};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::rc::Rc;

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
    config: Rc<SearchConfig>,
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
        widget.set_name_patterns_history(&self.config.name_patterns());
        widget.set_content_patterns_history(&self.config.content_patterns());
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
        connection::{
            connection::{Connection, ConnectionInterface},
            list::ConnectionList,
        },
        dir::Directory,
        file::File,
        file_list::list::FileList,
        intviewer::search_dialog::gnome_cmd_viewer_search_text_add_to_history,
        main_win::MainWindow,
        options::{options::GeneralOptions, utils::remember_window_size},
        select_directory_button::DirectoryButton,
        types::FileSelectorID,
        utils::{ErrorMessage, dialog_button_box, display_help},
    };
    use std::cell::{OnceCell, RefCell};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::SearchDialog)]
    pub struct SearchDialog {
        pub labels_size_group: gtk::SizeGroup,

        pub config: OnceCell<Rc<SearchConfig>>,

        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,
        #[property(get, construct_only)]
        pub main_window: OnceCell<MainWindow>,

        #[property(get, set)]
        pub dir_browser: RefCell<DirectoryButton>,
        #[property(get, set)]
        pub profile_component: RefCell<SelectionProfileComponent>,
        #[property(get, set)]
        pub result_list: RefCell<Option<FileList>>,
        pub status_label: gtk::Label,
        pub progress_bar: gtk::ProgressBar,
        #[property(get, set)]
        pub profile_menu_button: RefCell<gtk::MenuButton>,

        jump_button: gtk::Button,
        stop_button: gtk::Button,
        find_button: gtk::Button,

        #[property(get, set, nullable)]
        start_dir: RefCell<Option<Directory>>,

        #[property(get, set, nullable)]
        cancellable: RefCell<Option<gio::Cancellable>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SearchDialog {
        const NAME: &'static str = "GnomeCmdSearchDialog";
        type Type = super::SearchDialog;
        type ParentType = gtk::Window;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action_async("search.save-profile", None, |obj, _, _| async move {
                obj.imp().save_profile().await
            });
            klass.install_action_async("search.manage-profiles", None, |obj, _, _| async move {
                obj.imp().manage_profiles().await
            });
            klass.install_action(
                "search.load-profile",
                Some(&u32::static_variant_type()),
                |obj, _, param| {
                    if let Some(profile_idx) = param.and_then(u32::from_variant) {
                        obj.imp().load_profile(profile_idx);
                    }
                },
            );
        }

        fn new() -> Self {
            let labels_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);
            Self {
                config: Default::default(),
                file_metadata_service: Default::default(),
                main_window: Default::default(),
                dir_browser: Default::default(),
                profile_component: RefCell::new(SelectionProfileComponent::new(Some(
                    &labels_size_group,
                ))),
                result_list: Default::default(),
                status_label: Default::default(),
                progress_bar: gtk::ProgressBar::builder()
                    .show_text(false)
                    .pulse_step(0.02)
                    .build(),
                profile_menu_button: RefCell::new(
                    gtk::MenuButton::builder()
                        .label(gettext("Profiles…"))
                        .build(),
                ),

                jump_button: gtk::Button::builder()
                    .label(gettext("_Jump to"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                stop_button: gtk::Button::builder()
                    .label(gettext("_Stop"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                find_button: gtk::Button::builder()
                    .label(gettext("_Find"))
                    .use_underline(true)
                    .build(),

                labels_size_group,

                start_dir: Default::default(),

                cancellable: Default::default(),
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
            this.set_child(Some(&grid));

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
            statusbar.append(&self.status_label);
            statusbar.append(&self.progress_bar);
            self.progress_bar.set_visible(false);
            grid.attach(&statusbar, 0, 3, 2, 1);

            let help_button = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .build();
            help_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| {
                    glib::spawn_future_local(async move {
                        display_help(this.upcast_ref(), Some("gnome-commander-search")).await;
                    });
                }
            ));

            let close_button = gtk::Button::builder()
                .label(gettext("_Close"))
                .use_underline(true)
                .build();
            close_button.connect_clicked(glib::clone!(
                #[weak]
                this,
                move |_| this.close()
            ));

            self.jump_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.jump()
            ));
            self.stop_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.stop()
            ));
            self.find_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move {
                        imp.find().await;
                    });
                }
            ));

            this.set_default_widget(Some(&self.find_button));

            grid.attach(
                &dialog_button_box(
                    &[&help_button],
                    &[
                        self.profile_menu_button
                            .borrow()
                            .upcast_ref::<gtk::Widget>(),
                        close_button.upcast_ref(),
                        self.jump_button.upcast_ref(),
                        self.stop_button.upcast_ref(),
                        self.find_button.upcast_ref(),
                    ],
                ),
                0,
                4,
                2,
                1,
            );

            this.set_hide_on_close(true);
            this.connect_hide(glib::clone!(
                #[weak]
                this,
                move |_| {
                    this.profile_component().copy();
                    if let Some(file_list) = this.result_list() {
                        file_list.clear();
                    }
                }
            ));

            let options = GeneralOptions::new();
            remember_window_size(
                &*this,
                &options.search_window_width,
                &options.search_window_height,
            );
        }

        fn dispose(&self) {
            if let Some(cancellable) = self.cancellable.replace(None) {
                cancellable.cancel();
            }
        }
    }

    impl WidgetImpl for SearchDialog {}
    impl WindowImpl for SearchDialog {}

    impl SearchDialog {
        pub fn config(&self) -> Rc<SearchConfig> {
            self.config.get().unwrap().clone()
        }

        pub fn update_profile_menu(&self) {
            let menu = gio::Menu::new();
            menu.append(
                Some(&gettext("_Save Profile As…")),
                Some("search.save-profile"),
            );

            let search_config = self.config();
            let profiles = search_config.profiles();
            let count = profiles.n_items();
            if count > 0 {
                menu.append(
                    Some(&gettext("_Manage Profiles…")),
                    Some("search.manage-profiles"),
                );

                let profiles_section = gio::Menu::new();
                for (index, profile) in profiles.iter::<SearchProfile>().flatten().enumerate() {
                    let item = gio::MenuItem::new(Some(&profile.name()), None);
                    item.set_action_and_target_value(
                        Some("search.load-profile"),
                        Some(&(index as u32).to_variant()),
                    );
                    profiles_section.append_item(&item);
                }
                menu.append_section(None, &profiles_section);
            }

            self.obj().profile_menu_button().set_menu_model(Some(&menu));
        }

        async fn save_profile(&self) {
            self.obj().profile_component().copy();
            self.do_manage_profiles(true).await;
        }

        async fn manage_profiles(&self) {
            self.do_manage_profiles(false).await;
        }

        async fn do_manage_profiles(&self, new_profile: bool) {
            let config = self.config();

            let profiles = SearchProfiles::new(&config, new_profile);

            if new_profile {
                let last_index = profiles.len() - 1;
                profiles.set_profile_name(last_index, &gettext("New profile"));
            }

            let manager = Rc::new(SearchProfileManager { config, profiles });

            if manage_profiles(
                self.obj().upcast_ref(),
                &manager,
                &gettext("Profiles"),
                Some("gnome-commander-search"),
                new_profile,
            )
            .await
            {
                let profiles = manager.profiles.clone();
                manager.config.set_profiles(&profiles.profiles);
                self.update_profile_menu();
            }
        }

        fn load_profile(&self, profile_idx: u32) {
            let config = self.config();
            let Some(profile) = config
                .profiles()
                .item(profile_idx)
                .and_downcast::<SearchProfile>()
            else {
                return;
            };
            config.default_profile().copy_from(&profile);
            self.obj().profile_component().update();
        }

        fn selected_file(&self) -> Option<File> {
            let result_list = self.obj().result_list()?;
            let file = result_list.selected_file()?;
            Some(file)
        }

        fn jump(&self) {
            let Some(file) = self.selected_file() else {
                return;
            };
            self.obj().close();

            let file_selector = self
                .obj()
                .main_window()
                .file_selector(FileSelectorID::ACTIVE);
            file_selector.go_to_file(&file);
            file_selector.grab_focus();
        }

        fn stop(&self) {
            self.stop_button.set_sensitive(false);
            self.obj().set_default_widget(Some(&self.find_button));
            if let Some(cancellable) = self.cancellable.replace(None) {
                cancellable.cancel();
            }
        }

        fn find_connection_and_mount(&self, file: &gio::File) -> Option<(Connection, gio::Mount)> {
            let mount = file.find_enclosing_mount(gio::Cancellable::NONE).ok()?;
            ConnectionList::get()
                .all()
                .iter::<Connection>()
                .flatten()
                .find(|con| con.find_mount().as_ref() == Some(&mount))
                .map(move |con| (con, mount))
        }

        fn start_directory(&self, file: &gio::File) -> Result<Directory, ErrorMessage> {
            if let Some((connection, mount)) = self.find_connection_and_mount(file) {
                let path = mount.root().relative_path(file).ok_or_else(|| {
                    ErrorMessage::brief(gettext("Cannot extract a relative file path"))
                })?;
                Directory::try_new(&connection, connection.create_path(&path))
            } else if file.uri_scheme().as_deref() == Some("file") {
                let connection = ConnectionList::get().home();
                let path = file
                    .path()
                    .ok_or_else(|| ErrorMessage::brief(gettext("Cannot extract a file path")))?;
                Directory::try_new(&connection, connection.create_path(&path))
            } else {
                Err(ErrorMessage::brief(gettext(
                    "Failed to detect a start directory",
                )))
            }
        }

        async fn find(&self) {
            let Some(file) = self.dir_browser.borrow().file() else {
                return;
            };

            let start_dir = match self.start_directory(&file) {
                Ok(dir) => dir,
                Err(error) => {
                    error.show(self.obj().upcast_ref()).await;
                    return;
                }
            };

            let Some(result_list) = self.result_list.borrow().clone() else {
                return;
            };

            if let Some(cancellable) = self.cancellable.replace(None) {
                cancellable.cancel();
            }
            let cancellable = gio::Cancellable::new();
            self.cancellable.replace(Some(cancellable.clone()));

            self.save_default_settings();
            let profile = self.config().default_profile().clone();

            let progress_bar = &self.progress_bar;
            let update_gui_timeout_id = glib::timeout_add_local(
                GeneralOptions::new().gui_update_rate.get(),
                glib::clone!(
                    #[weak]
                    progress_bar,
                    #[upgrade_or]
                    glib::ControlFlow::Break,
                    move || {
                        progress_bar.pulse();
                        glib::ControlFlow::Continue
                    }
                ),
            );

            self.status_label.set_text("");
            self.progress_bar.set_visible(true);
            self.jump_button.set_sensitive(false);
            self.stop_button.set_sensitive(true);
            self.find_button.set_sensitive(false);
            self.obj().set_default_widget(Some(&self.stop_button));

            result_list.clear();
            result_list.set_base_dir(start_dir.upcast_ref::<File>().get_real_path());

            let backend = if start_dir.connection().is_local() {
                SearchBackend::Local
            } else {
                SearchBackend::Generic
            };
            let search_result = backend
                .search(
                    &profile,
                    &start_dir,
                    &|message| match message {
                        SearchMessage::File(file) => result_list.append_file(&file),
                        SearchMessage::Status(status) => self.status_label.set_text(&status),
                    },
                    &cancellable,
                )
                .await;

            update_gui_timeout_id.remove();

            let canceelled = self
                .cancellable
                .borrow()
                .as_ref()
                .map_or(false, |c| c.is_cancelled());
            self.search_finished(canceelled);

            if let Err(error) = search_result {
                error.show(self.obj().upcast_ref()).await;
            }
        }

        fn search_finished(&self, stopped: bool) {
            self.progress_bar.set_visible(false);

            let Some(result_list) = self.obj().result_list() else {
                return;
            };

            let count = result_list.size();

            let status = if stopped {
                ngettext(
                    "Found {} match — search aborted",
                    "Found {} matches — search aborted",
                    count as u32,
                )
            } else {
                ngettext("Found {} match", "Found {} matches", count as u32)
            }
            .replace("{}", count.to_string().as_str());

            self.status_label.set_label(&status);

            self.jump_button.set_sensitive(count > 0);
            self.stop_button.set_sensitive(false);
            self.find_button.set_sensitive(true);
            self.obj().set_default_widget(Some(&self.find_button));

            if count > 0 {
                result_list.grab_focus();
                if result_list.focused_file().is_none() {
                    result_list.select_row(0);
                }
            }
        }

        pub fn save_default_settings(&self) {
            let profile_component = self.obj().profile_component();
            let config = self.config();
            let default_profile = config.default_profile();

            profile_component.copy();

            let filename_pattern = default_profile.filename_pattern();
            if !filename_pattern.is_empty() {
                config.add_name_pattern(&filename_pattern);
                profile_component.set_name_patterns_history(&config.name_patterns());
            }

            if default_profile.content_search() {
                let text_pattern = default_profile.text_pattern();
                if !text_pattern.is_empty() {
                    config.add_content_pattern(&text_pattern);
                    gnome_cmd_viewer_search_text_add_to_history(&text_pattern);
                    profile_component.set_content_patterns_history(&config.content_patterns());
                }
            }
        }
    }
}

glib::wrapper! {
    pub struct SearchDialog(ObjectSubclass<imp::SearchDialog>)
        @extends gtk::Widget, gtk::Window,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl SearchDialog {
    pub fn new(
        config: Rc<SearchConfig>,
        file_metadata_service: &FileMetadataService,
        main_window: &MainWindow,
        transient: bool,
    ) -> Self {
        let this: Self = glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .property("main-window", main_window)
            .build();
        this.set_transient_for(if transient { Some(main_window) } else { None });
        this.imp().config.set(config.clone()).ok().unwrap();
        this.imp().update_profile_menu();

        let profile_component = this.profile_component();
        profile_component.set_profile(Some(config.default_profile()));
        profile_component.set_name_patterns_history(&config.name_patterns());
        profile_component.set_content_patterns_history(&config.content_patterns());
        profile_component.grab_focus();

        this
    }

    pub fn show_and_set_focus(&self, start_dir: Option<&Directory>) {
        self.present();
        self.profile_component().grab_focus();

        self.profile_component().update();
        self.set_start_dir(start_dir);

        self.dir_browser().set_file(start_dir.map(|d| d.file()));
    }

    pub fn update_style(&self) {
        if let Some(fl) = self.result_list() {
            fl.update_style();
        }
    }
}
