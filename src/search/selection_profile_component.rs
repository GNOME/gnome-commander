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

use super::profile::SearchProfile;
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

        pub filter_type_drop_down: gtk::DropDown,
        pub pattern_entry: HistoryEntry,
        pub recurse_label: gtk::Label,
        pub recurse_drop_down: gtk::DropDown,
        pub find_text_entry: HistoryEntry,
        pub find_text_check: gtk::CheckButton,
        pub case_check: gtk::CheckButton,
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
                filter_type_drop_down: gtk::DropDown::builder()
                    .model(&filter_type_model())
                    .factory(&same_size_labels_factory())
                    .build(),
                pattern_entry: Default::default(),
                recurse_label: gtk::Label::builder()
                    .label(gettext("Search _recursively:"))
                    .use_underline(true)
                    .xalign(0.0)
                    .build(),
                recurse_drop_down: gtk::DropDown::builder()
                    .model(&recurse_model())
                    .hexpand(true)
                    .build(),
                find_text_entry: Default::default(),
                find_text_check: gtk::CheckButton::builder()
                    .label(gettext("Contains _text:"))
                    .use_underline(true)
                    .build(),
                case_check: gtk::CheckButton::builder()
                    .label(gettext("Case sensiti_ve"))
                    .use_underline(true)
                    .sensitive(false)
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
                .row_spacing(6)
                .column_spacing(12)
                .build();
            grid.set_parent(&*self.obj());

            // search for
            grid.attach(&self.filter_type_drop_down, 0, 0, 1, 1);
            self.pattern_entry.set_hexpand(true);
            grid.attach(&self.pattern_entry, 1, 0, 1, 1);

            // recurse check
            grid.attach(&self.recurse_label, 0, 1, 1, 1);
            grid.attach(&self.recurse_drop_down, 1, 1, 1, 1);

            // find text
            grid.attach(&self.find_text_check, 0, 2, 1, 1);
            self.find_text_entry.set_hexpand(true);
            self.find_text_entry.set_sensitive(false);
            grid.attach(&self.find_text_entry, 1, 2, 1, 1);

            // case check
            grid.attach(&self.case_check, 1, 3, 1, 1);

            if let Some(labels_size_group) = self.obj().labels_size_group() {
                labels_size_group.add_widget(&self.filter_type_drop_down);
                labels_size_group.add_widget(&self.recurse_label);
                labels_size_group.add_widget(&self.find_text_check);
            }

            self.filter_type_drop_down
                .connect_selected_notify(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| {
                        imp.pattern_entry.grab_focus();
                    }
                ));
            self.find_text_check.connect_toggled(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |cb| {
                    if cb.is_active() {
                        imp.find_text_entry.set_sensitive(true);
                        imp.case_check.set_sensitive(true);
                        imp.find_text_entry.grab_focus();
                    } else {
                        imp.find_text_entry.set_sensitive(false);
                        imp.case_check.set_sensitive(false);
                    }
                }
            ));

            self.pattern_entry.entry().set_activates_default(true);
            self.find_text_entry.entry().set_activates_default(true);
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for SelectionProfileComponent {
        fn grab_focus(&self) -> bool {
            self.pattern_entry.grab_focus()
        }
    }

    fn filter_type_model() -> gio::ListModel {
        gtk::StringList::new(&[&gettext("Path matches regex:"), &gettext("Name contains:")]).into()
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

    fn same_size_labels_factory() -> gtk::ListItemFactory {
        let size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(glib::clone!(
            #[strong]
            size_group,
            move |_, item| {
                if let Some(item) = item.downcast_ref::<gtk::ListItem>() {
                    let label = gtk::Label::builder().xalign(0.0).build();
                    item.set_child(Some(&label));
                    size_group.add_widget(&label);
                }
            }
        ));
        factory.connect_bind(move |_, item| {
            if let Some(item) = item.downcast_ref::<gtk::ListItem>() {
                if let Some(label) = item.child().and_downcast::<gtk::Label>() {
                    if let Some(text) = item.item().and_downcast::<gtk::StringObject>() {
                        label.set_label(&text.string());
                    } else {
                        label.set_label("");
                    }
                }
            }
        });
        factory.upcast()
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

        self.imp()
            .pattern_entry
            .set_text(&profile.filename_pattern());
        self.imp()
            .filter_type_drop_down
            .set_selected(profile.syntax() as u32);
        self.imp()
            .recurse_drop_down
            .set_selected((profile.max_depth() + 1) as u32);
        self.imp().find_text_entry.set_text(&profile.text_pattern());
        self.imp()
            .find_text_check
            .set_active(profile.content_search());
        self.imp().case_check.set_active(profile.match_case());
    }

    pub fn copy(&self) {
        let Some(profile) = self.profile() else {
            return;
        };

        let text_pattern = self.imp().find_text_entry.text();
        let content_search = !text_pattern.is_empty() && self.imp().find_text_check.is_active();

        profile.set_filename_pattern(self.imp().pattern_entry.text());
        profile.set_max_depth(self.imp().recurse_drop_down.selected() as i32 - 1);
        profile.set_syntax(self.imp().filter_type_drop_down.selected() as i32);
        profile.set_text_pattern(text_pattern);
        profile.set_content_search(&content_search);
        profile.set_match_case(content_search && self.imp().case_check.is_active());
    }

    pub fn set_name_patterns_history(&self, history: &[String]) {
        self.imp().pattern_entry.set_history(history);
    }

    pub fn set_content_patterns_history(&self, history: &[String]) {
        self.imp().find_text_entry.set_history(history);
    }
}
