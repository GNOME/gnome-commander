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

use super::{profile::SearchProfile, selection_profile_component::SelectionProfileComponent};
use crate::{
    data::SearchConfig,
    dialogs::profiles::{manage_profiles_dialog::manage_profiles, profiles::ProfileManager},
    dir::ffi::GnomeCmdDir,
    file::ffi::GnomeCmdFile,
};
use gettextrs::{gettext, ngettext};
use glib::translate::{from_glib_borrow, Borrowed};
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
        connection::{
            connection::{Connection, ConnectionExt},
            list::ConnectionList,
        },
        data::GeneralOptions,
        dir::Directory,
        file::File,
        file_list::list::FileList,
        select_directory_button::DirectoryButton,
        tags::tags::FileMetadataService,
        utils::{dialog_button_box, display_help, ErrorMessage},
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

        jump_button: gtk::Button,
        stop_button: gtk::Button,
        find_button: gtk::Button,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SearchDialog {
        const NAME: &'static str = "GnomeCmdSearchDialog";
        type Type = super::SearchDialog;
        type ParentType = gtk::Window;

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
            statusbar.append(&this.status_label());
            statusbar.append(&this.progress_bar());
            this.progress_bar().set_visible(false);
            grid.attach(&statusbar, 0, 3, 2, 1);

            unsafe {
                gnome_cmd_search_dialog_init(this.to_glib_none().0);
            }

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

    impl SearchDialog {
        fn selected_file(&self) -> Option<File> {
            let result_list = self.obj().result_list()?;
            let file = result_list.selected_file()?;
            Some(file)
        }

        fn jump(&self) {
            if let Some(file) = self.selected_file() {
                self.obj().close();

                unsafe {
                    gnome_cmd_search_dialog_goto(self.obj().to_glib_none().0, file.to_glib_none().0)
                };
            }
        }

        fn stop(&self) {
            self.stop_button.set_sensitive(false);
            self.obj().set_default_widget(Some(&self.find_button));
            unsafe {
                gnome_cmd_search_dialog_stop(self.obj().to_glib_none().0);
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

        fn start_directory(&self, file: &gio::File) -> Option<Directory> {
            if let Some((connection, mount)) = self.find_connection_and_mount(file) {
                let path = mount.root().relative_path(file)?;
                Some(Directory::new(&connection, connection.create_path(&path)))
            } else if file.uri_scheme().as_deref() == Some("file") {
                let connection = ConnectionList::get().home();
                Some(Directory::new(
                    &connection,
                    connection.create_path(&file.path()?),
                ))
            } else {
                None
            }
        }

        async fn find(&self) {
            let Some(file) = self.dir_browser.borrow().file() else {
                return;
            };

            let Some(start_dir) = self.start_directory(&file) else {
                ErrorMessage::brief(gettext("Failed to detect a start directory"))
                    .show(self.obj().upcast_ref())
                    .await;
                return;
            };

            let search_started = unsafe {
                gnome_cmd_search_dialog_find(
                    self.obj().to_glib_none().0,
                    start_dir.to_glib_none().0,
                ) != 0
            };

            if search_started {
                self.jump_button.set_sensitive(false);
                self.stop_button.set_sensitive(true);
                self.find_button.set_sensitive(false);
                self.obj().set_default_widget(Some(&self.stop_button));
            }
        }

        pub fn search_finished(&self, stopped: bool) {
            self.progress_bar.borrow().set_visible(false);

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

            self.status_label.borrow().set_label(&status);

            self.jump_button.set_sensitive(count > 0);
            self.stop_button.set_sensitive(false);
            self.find_button.set_sensitive(true);
            self.obj().set_default_widget(Some(&self.find_button));

            if count > 0 {
                result_list.grab_focus();
                if result_list.focused_file().is_none() {
                    result_list.select_row(None);
                }
            }
        }
    }
}

glib::wrapper! {
    pub struct SearchDialog(ObjectSubclass<imp::SearchDialog>)
        @extends gtk::Widget, gtk::Window;
}

impl SearchDialog {
    pub fn show_and_set_focus(&self) {
        unsafe { gnome_cmd_search_dialog_show_and_set_focus(self.to_glib_none().0) }
    }
}

pub type GnomeCmdSearchDialog = <SearchDialog as glib::object::ObjectType>::GlibType;

#[no_mangle]
pub extern "C" fn gnome_cmd_search_dialog_get_type() -> GType {
    SearchDialog::static_type().into_glib()
}

extern "C" {
    fn gnome_cmd_search_dialog_init(dialog: *mut GnomeCmdSearchDialog);
    fn gnome_cmd_search_dialog_dispose(dialog: *mut GnomeCmdSearchDialog);
    fn gnome_cmd_search_dialog_update_profile_menu(dialog: *mut GnomeCmdSearchDialog);
    fn gnome_cmd_search_dialog_show_and_set_focus(dialog: *mut GnomeCmdSearchDialog);

    fn gnome_cmd_search_dialog_goto(dialog: *mut GnomeCmdSearchDialog, file: *mut GnomeCmdFile);
    fn gnome_cmd_search_dialog_stop(dialog: *mut GnomeCmdSearchDialog);
    fn gnome_cmd_search_dialog_find(
        dialog: *mut GnomeCmdSearchDialog,
        start_dir: *mut GnomeCmdDir,
    ) -> gboolean;
}

#[no_mangle]
pub extern "C" fn gnome_cmd_search_dialog_search_finished(
    dialog_ptr: *mut GnomeCmdSearchDialog,
    stopped: gboolean,
) {
    let dialog: Borrowed<SearchDialog> = unsafe { from_glib_borrow(dialog_ptr) };
    dialog.imp().search_finished(stopped != 0);
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
