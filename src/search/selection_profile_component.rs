// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::profile::SearchProfile;
use crate::filter::PatternType;
use gettextrs::{gettext, ngettext};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{history_entry::HistoryEntry, search::profile::SearchProfile};
    use std::cell::{OnceCell, RefCell};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::SelectionProfileComponent)]
    pub struct SelectionProfileComponent {
        #[property(get, construct_only, nullable)]
        labels_size_group: OnceCell<Option<gtk::SizeGroup>>,
        #[property(get, set, nullable)]
        profile: RefCell<Option<SearchProfile>>,

        pub path_entry: HistoryEntry,
        pub path_match_case: gtk::ToggleButton,
        pub path_regex: gtk::ToggleButton,
        pub recurse_drop_down: gtk::DropDown,
        pub content_entry: HistoryEntry,
        pub content_match_case: gtk::ToggleButton,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SelectionProfileComponent {
        const NAME: &'static str = "GnomeCmdSelectionProfileComponent";
        type Type = super::SelectionProfileComponent;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            Self {
                labels_size_group: Default::default(),
                profile: Default::default(),
                path_entry: Default::default(),
                path_match_case: gtk::ToggleButton::builder()
                    .icon_name("gnome-commander-match-case")
                    .tooltip_text(gettext("Case sensitive"))
                    .build(),
                path_regex: gtk::ToggleButton::builder()
                    .icon_name("gnome-commander-regex")
                    .tooltip_text(gettext("Regex match"))
                    .build(),
                recurse_drop_down: gtk::DropDown::builder()
                    .model(&recurse_model())
                    .hexpand(true)
                    .build(),
                content_entry: Default::default(),
                content_match_case: gtk::ToggleButton::builder()
                    .icon_name("gnome-commander-match-case")
                    .tooltip_text(gettext("Case sensitive"))
                    .build(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for SelectionProfileComponent {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BinLayout::new()));

            let grid = gtk::Grid::builder()
                .css_classes(["search-profile-grid"])
                .build();
            grid.set_parent(&*self.obj());

            // search for
            let path_label = gtk::Label::builder()
                .label(gettext("File _path:"))
                .use_underline(true)
                .mnemonic_widget(&self.path_entry)
                .xalign(0.0)
                .build();
            grid.attach(&path_label, 0, 0, 1, 1);

            let path_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .build();
            self.path_entry.set_hexpand(true);
            path_box.append(&self.path_entry);
            path_box.append(&self.path_match_case);
            path_box.append(&self.path_regex);
            grid.attach(&path_box, 1, 0, 1, 1);

            // recurse check
            let recurse_label = gtk::Label::builder()
                .label(gettext("Search _recursively:"))
                .use_underline(true)
                .mnemonic_widget(&self.recurse_drop_down)
                .xalign(0.0)
                .build();
            grid.attach(&recurse_label, 0, 1, 1, 1);
            grid.attach(&self.recurse_drop_down, 1, 1, 1, 1);

            // find text
            let content_label = gtk::Label::builder()
                .label(gettext("Contains _text:"))
                .use_underline(true)
                .mnemonic_widget(&self.content_entry)
                .xalign(0.0)
                .build();
            grid.attach(&content_label, 0, 2, 1, 1);

            let content_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .build();
            self.content_entry.set_hexpand(true);
            content_box.append(&self.content_entry);
            content_box.append(&self.content_match_case);
            grid.attach(&content_box, 1, 2, 1, 1);

            if let Some(labels_size_group) = self.obj().labels_size_group() {
                labels_size_group.add_widget(&path_label);
                labels_size_group.add_widget(&recurse_label);
                labels_size_group.add_widget(&content_label);
            }

            self.path_entry.entry().set_activates_default(true);
            self.content_entry.entry().set_activates_default(true);
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for SelectionProfileComponent {
        fn grab_focus(&self) -> bool {
            self.path_entry.grab_focus()
        }
    }

    fn recurse_model() -> gio::ListModel {
        let model = gtk::StringList::new(&[
            &gettext("Unlimited depth"),
            &gettext("Current directory only"),
        ]);
        for i in 1..=40 {
            model.append(&ngettext("%i level", "%i levels", i).replace("%i", &i.to_string()));
        }
        model.upcast()
    }
}

glib::wrapper! {
    pub struct SelectionProfileComponent(ObjectSubclass<imp::SelectionProfileComponent>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl SelectionProfileComponent {
    pub fn new(labels_size_group: Option<&gtk::SizeGroup>) -> Self {
        glib::Object::builder()
            .property("labels-size-group", labels_size_group)
            .build()
    }

    pub fn with_profile(
        profile: &SearchProfile,
        labels_size_group: Option<&gtk::SizeGroup>,
    ) -> Self {
        glib::Object::builder()
            .property("profile", profile)
            .property("labels-size-group", labels_size_group)
            .build()
    }

    pub fn update(&self) {
        let Some(profile) = self.profile() else {
            return;
        };

        self.imp().path_entry.set_text(&profile.path_pattern());
        self.imp()
            .path_match_case
            .set_active(profile.path_match_case());
        self.imp()
            .path_regex
            .set_active(profile.path_syntax() == PatternType::Regex);
        self.imp()
            .recurse_drop_down
            .set_selected((profile.max_depth() + 1) as u32);
        self.imp()
            .content_entry
            .set_text(&profile.content_pattern());
        self.imp()
            .content_match_case
            .set_active(profile.content_match_case());
    }

    pub fn copy(&self) {
        let Some(profile) = self.profile() else {
            return;
        };

        let text_pattern = self.imp().content_entry.text();

        profile.set_path_pattern(self.imp().path_entry.text());
        profile.set_max_depth(self.imp().recurse_drop_down.selected() as i32 - 1);
        profile.set_path_match_case(self.imp().path_match_case.is_active());
        profile.set_path_syntax(if self.imp().path_regex.is_active() {
            PatternType::Regex
        } else {
            PatternType::FnMatch
        });
        profile.set_content_pattern(text_pattern);
        profile.set_content_match_case(self.imp().content_match_case.is_active());
    }

    pub fn set_name_patterns_history(&self, history: &[String]) {
        self.imp().path_entry.set_history(history);
    }

    pub fn set_content_patterns_history(&self, history: &[String]) {
        self.imp().content_entry.set_history(history);
    }
}
