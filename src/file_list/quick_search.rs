// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::list::FileList;
use crate::{
    file::{File, FileOps},
    filter::{Filter, PatternType},
    options::GeneralOptions,
    utils::{EnumAll, u32_enum},
};
use gettextrs::gettext;
use gtk::{gdk, glib, prelude::*, subclass::prelude::*};

u32_enum! {
    #[derive(strum::FromRepr)]
    pub enum QuickSearchMode {
        #[default]
        Search,
        Filter,
    }
}

impl QuickSearchMode {
    pub fn label(&self) -> String {
        match self {
            Self::Search => gettext("Search"),
            Self::Filter => gettext("Filter"),
        }
    }
}

mod imp {
    use super::*;
    use crate::utils::{NO_MOD, SHIFT};
    use std::{
        cell::{OnceCell, RefCell},
        time::Duration,
    };

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::QuickSearch)]
    pub struct QuickSearch {
        #[property(get, construct_only)]
        file_list: OnceCell<FileList>,
        pub(super) mode: gtk::DropDown,
        pub(super) entry: gtk::Entry,
        case_sensitive: gtk::ToggleButton,
        filter: RefCell<Filter>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for QuickSearch {
        const NAME: &'static str = "GnomeCmdQuickSearch";
        type Type = super::QuickSearch;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for QuickSearch {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BoxLayout::new(gtk::Orientation::Horizontal)));
            obj.add_css_class("quick-search");

            self.mode.set_model(Some(&gtk::StringList::from_iter(
                QuickSearchMode::all().map(|mode| mode.label()),
            )));
            self.mode.set_parent(&*obj);

            self.entry.set_hexpand(true);
            self.entry
                .update_relation(&[gtk::accessible::Relation::LabelledBy(&[self
                    .mode
                    .upcast_ref()])]);
            self.entry.set_parent(&*obj);

            self.case_sensitive
                .set_icon_name("gnome-commander-match-case");
            self.case_sensitive
                .set_tooltip_text(Some(&gettext("Case sensitive")));
            self.case_sensitive.set_parent(&*obj);

            let options = GeneralOptions::new();
            options
                .quick_search_case_sensitive
                .bind(&self.case_sensitive, "active")
                .build();

            let close_button = gtk::Button::builder()
                .css_classes(["flat"])
                .icon_name("window-close")
                .tooltip_text(gettext("Close"))
                .build();
            close_button.connect_clicked(glib::clone!(
                #[weak]
                obj,
                move |_| obj.remove()
            ));
            close_button.set_parent(&*obj);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.key_pressed(key, state)
            ));
            obj.add_controller(key_controller);

            self.mode.connect_selected_item_notify(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    if imp.current_mode() != QuickSearchMode::Filter {
                        imp.obj().file_list().set_filter(None);
                    }
                    imp.entry_changed();
                }
            ));
            self.entry.connect_closure(
                "changed",
                true,
                glib::closure_local!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_: &gtk::Entry| imp.entry_changed()
                ),
            );
            self.case_sensitive.connect_active_notify(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.entry_changed()
            ));
            // gtk::Entry translates Return key into activate signal, translate it back
            self.entry.connect_closure(
                "activate",
                true,
                glib::closure_local!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_: &gtk::Entry| {
                        imp.key_pressed(gdk::Key::Return, NO_MOD);
                    }
                ),
            );
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for QuickSearch {}

    impl QuickSearch {
        /// Moves focus to the previously focused widget, making sure that entry field state isn't
        /// modified in the process (it would normally select all text when focused).
        pub(super) fn restore_focus(&self, widget: &gtk::Widget) {
            let position = self.entry.position();
            let selection_bounds = self.entry.selection_bounds();

            widget.grab_focus();

            self.entry.set_position(position);
            if let Some((start_pos, end_pos)) = selection_bounds {
                self.entry.select_region(start_pos, end_pos);
            }
        }

        fn focus_file(&self, file: &File) {
            // ColumnView won't focus an entry unless it is focused itself, so we focus the list
            // temporarily.
            let previous_focus = self.obj().root().and_then(|root| root.focus());
            let file_list = self.obj().file_list();
            file_list.grab_focus();
            file_list.focus_file(&file.path_name(), true);
            if let Some(widget) = previous_focus {
                self.restore_focus(&widget);
            }
        }

        fn current_mode(&self) -> QuickSearchMode {
            QuickSearchMode::from(self.mode.selected())
        }

        fn entry_changed(&self) {
            self.set_filter(&self.entry.text());
            match self.current_mode() {
                QuickSearchMode::Search => self.find_next(false),
                QuickSearchMode::Filter => self
                    .obj()
                    .file_list()
                    .set_filter(Some(self.filter.borrow().clone())),
            };
        }

        fn trigger_shortcuts(
            key: gdk::Key,
            state: gdk::ModifierType,
            widget: &gtk::Widget,
            phase: gtk::PropagationPhase,
        ) -> bool {
            for controller in widget
                .observe_controllers()
                .iter::<glib::Object>()
                .filter_map(|controller| controller.ok().and_downcast::<gtk::ShortcutController>())
                .filter(|controller| controller.propagation_phase() == phase)
            {
                for shortcut in controller
                    .iter::<glib::Object>()
                    .filter_map(|shortcut| shortcut.ok().and_downcast::<gtk::Shortcut>())
                {
                    if shortcut
                        .trigger()
                        .and_downcast::<gtk::KeyvalTrigger>()
                        .is_some_and(|trigger| {
                            trigger.keyval() == key && trigger.modifiers() == state
                        })
                        && let Some(action) = shortcut.action()
                    {
                        action.activate(
                            gtk::ShortcutActionFlags::EXCLUSIVE,
                            widget,
                            shortcut.arguments().as_ref(),
                        );
                        return true;
                    }
                }
            }

            // This is the exact opposite of how event propagation normally works: capturing should
            // go down, bubbling up. But this hack is good enough for our use case.
            if phase == gtk::PropagationPhase::Capture {
                match widget.parent() {
                    Some(widget) => Self::trigger_shortcuts(key, state, &widget, phase),
                    None => false,
                }
            } else {
                let mut child = widget.first_child();
                while let Some(widget) = child {
                    if Self::trigger_shortcuts(key, state, &widget, phase) {
                        return true;
                    }
                    child = widget.next_sibling();
                }
                false
            }
        }

        fn key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match (state, key) {
                (NO_MOD, gdk::Key::Escape) => {
                    self.obj().remove();
                    glib::Propagation::Stop
                }
                (NO_MOD, gdk::Key::Up | gdk::Key::KP_Up | gdk::Key::KP_8) => {
                    self.find_previous(true);
                    glib::Propagation::Stop
                }
                (NO_MOD, gdk::Key::Down | gdk::Key::KP_Down | gdk::Key::KP_2) => {
                    self.find_next(true);
                    glib::Propagation::Stop
                }
                (NO_MOD | SHIFT, gdk::Key::Tab | gdk::Key::ISO_Left_Tab) => {
                    glib::Propagation::Proceed
                }
                (
                    NO_MOD | SHIFT,
                    gdk::Key::Page_Down
                    | gdk::Key::KP_Page_Down
                    | gdk::Key::Page_Up
                    | gdk::Key::KP_Page_Up,
                ) => {
                    if state == SHIFT {
                        self.obj().file_list().start_range_selection(false);
                    }
                    let previous_focus = self.obj().root().and_then(|root| root.focus());
                    if Self::trigger_shortcuts(
                        key,
                        state,
                        self.obj().file_list().view().upcast_ref::<gtk::Widget>(),
                        gtk::PropagationPhase::Bubble,
                    ) {
                        if let Some(widget) = previous_focus {
                            self.restore_focus(&widget);
                        }
                        glib::Propagation::Stop
                    } else {
                        glib::Propagation::Proceed
                    }
                }
                (state, key) => {
                    // Forward all unhandled keyboard input to the file list's shortcut handlers.
                    if Self::trigger_shortcuts(
                        key,
                        state,
                        self.obj().file_list().upcast_ref::<gtk::Widget>(),
                        gtk::PropagationPhase::Capture,
                    ) {
                        glib::Propagation::Stop
                    } else {
                        glib::Propagation::Proceed
                    }
                }
            }
        }

        fn set_filter(&self, text: &str) {
            let pattern = if text.contains('*') || text.contains('?') {
                text.to_owned()
            } else {
                format!("*{text}*")
            };

            let case_sensitive = self.case_sensitive.is_active();
            self.filter.replace(
                Filter::new(&pattern, case_sensitive, PatternType::FnMatch).unwrap_or_default(),
            );
        }

        fn find<F, I>(&self, files: F, skip_current: bool) -> Option<File>
        where
            I: Iterator<Item = File>,
            F: Fn() -> I,
        {
            let filter = self.filter.borrow();
            let current = self.obj().file_list().focused_file();

            let mut seen_current = false;
            for file in files() {
                let is_current = Some(&file) == current.as_ref();
                if !skip_current && is_current {
                    seen_current = true;
                }
                if seen_current && filter.matches(&file.name()) {
                    return Some(file);
                }
                if skip_current && is_current {
                    seen_current = true;
                }
            }

            for file in files() {
                if Some(&file) == current.as_ref() {
                    break;
                }
                if filter.matches(&file.name()) {
                    return Some(file);
                }
            }

            None
        }

        fn show_match(&self, file: Option<File>) {
            if let Some(file) = file {
                self.focus_file(&file);
            } else {
                self.obj().add_css_class("bell");
                glib::timeout_add_local_once(
                    Duration::from_millis(500),
                    glib::clone!(
                        #[weak(rename_to = obj)]
                        self.obj(),
                        move || obj.remove_css_class("bell"),
                    ),
                );
            }
        }

        fn find_next(&self, skip_current: bool) {
            let fl = self.obj().file_list();
            self.show_match(self.find(|| fl.files(), skip_current));
        }

        fn find_previous(&self, skip_current: bool) {
            let fl = self.obj().file_list();
            self.show_match(self.find(|| fl.files().rev(), skip_current));
        }
    }
}

glib::wrapper! {
    pub struct QuickSearch(ObjectSubclass<imp::QuickSearch>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl QuickSearch {
    pub fn new(file_list: &FileList) -> Self {
        glib::Object::builder()
            .property("file-list", file_list)
            .build()
    }

    pub fn set_mode(&self, mode: QuickSearchMode) {
        self.imp().mode.set_selected(mode.into());
    }

    pub fn remove(&self) {
        self.unparent();
        self.file_list().set_filter(None);
        self.file_list().grab_focus();
    }

    pub fn add_text(&self, text: &str) {
        let entry = &self.imp().entry;
        let mut position = entry.position();
        entry.insert_text(text, &mut position);
        entry.set_position(position);
    }

    pub fn grab_focus_without_selecting(&self) {
        self.imp()
            .restore_focus(self.imp().entry.upcast_ref::<gtk::Widget>());
    }
}
