// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::utils::u32_enum;
use gtk::{gdk, glib, prelude::*, subclass::prelude::*};

const STATE_EDITING: &str = "editing";
const STATE_SEARCHING: &str = "searching";

const MESSAGE_NONE: &str = "";
const MESSAGE_NOT_HEX: &str = "not_hex";
const MESSAGE_NOT_FOUND: &str = "not_found";

mod imp {
    use super::*;
    use crate::{
        history_entry::HistoryEntry, options::ViewerOptions, utils::NO_MOD, utils::hbox_builder,
    };
    use gettextrs::gettext;
    use std::sync::OnceLock;

    pub struct SearchBar {
        pub(super) inner: gtk::SearchBar,
        pub(super) entry: HistoryEntry,
        pub(super) entry_buttons: gtk::Stack,
        pub(super) stop_button: gtk::Button,
        pub(super) match_case: gtk::ToggleButton,
        pub(super) hex_mode: gtk::ToggleButton,
        pub(super) message: gtk::Stack,
        pub(super) options: ViewerOptions,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SearchBar {
        const NAME: &'static str = "GnomeCmdViewerSearchBar";
        type Type = super::SearchBar;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            let options = ViewerOptions::new();

            Self {
                inner: gtk::SearchBar::builder().show_close_button(true).build(),
                entry: Default::default(),
                entry_buttons: gtk::Stack::new(),
                stop_button: gtk::Button::builder()
                    .icon_name("edit-delete")
                    .tooltip_text(gettext("Stop"))
                    .build(),
                match_case: gtk::ToggleButton::builder()
                    .icon_name("gnome-commander-match-case")
                    .tooltip_text(gettext("Case sensitive"))
                    .active(options.case_sensitive_search.get())
                    .build(),
                hex_mode: gtk::ToggleButton::builder()
                    .icon_name("gnome-commander-hex")
                    .tooltip_text(gettext("Hexadecimal"))
                    .active(options.search_mode.get() == Mode::Binary)
                    .build(),
                message: gtk::Stack::new(),
                options,
            }
        }
    }

    impl ObjectImpl for SearchBar {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BoxLayout::new(gtk::Orientation::Vertical)));

            self.inner.set_child(Some(
                &hbox_builder()
                    .add_css_class("spacing")
                    .append(
                        &hbox_builder()
                            .append({
                                self.entry.set_hexpand(true);
                                self.entry.set_placeholder_text(Some(&gettext("Search…")));
                                self.entry.set_primary_icon_name(Some("system-search"));
                                self.entry.set_primary_icon_activatable(true);
                                self.entry.set_popover_position(gtk::PositionType::Top);
                                self.entry.connect_activate(glib::clone!(
                                    #[weak(rename_to = imp)]
                                    self,
                                    move |_| {
                                        imp.start_search(true);
                                    }
                                ));
                                self.entry.connect_icon_press(move |entry, icon_pos| {
                                    if icon_pos == gtk::EntryIconPosition::Primary {
                                        entry.emit_activate();
                                    }
                                });
                                self.entry.connect_changed(glib::clone!(
                                    #[weak(rename_to = obj)]
                                    self.obj(),
                                    move |_| {
                                        obj.set_message(MESSAGE_NONE);
                                    }
                                ));
                                &self.entry
                            })
                            .append({
                                self.entry_buttons.add_named(
                                    &hbox_builder()
                                        .append(&{
                                            let button = gtk::Button::builder()
                                                .icon_name("go-up")
                                                .tooltip_text(gettext("Previous Match"))
                                                .build();
                                            button.connect_clicked(glib::clone!(
                                                #[weak(rename_to = imp)]
                                                self,
                                                move |_| {
                                                    imp.start_search(false);
                                                }
                                            ));
                                            button
                                        })
                                        .append(&{
                                            let button = gtk::Button::builder()
                                                .icon_name("go-down")
                                                .tooltip_text(gettext("Next Match"))
                                                .build();
                                            button.connect_clicked(glib::clone!(
                                                #[weak(rename_to = imp)]
                                                self,
                                                move |_| {
                                                    imp.start_search(true);
                                                }
                                            ));
                                            button
                                        })
                                        .build(),
                                    Some(STATE_EDITING),
                                );
                                self.entry_buttons.add_named(
                                    {
                                        self.stop_button.connect_clicked(glib::clone!(
                                            #[weak(rename_to = imp)]
                                            self,
                                            move |_| {
                                                imp.stop_search();
                                            }
                                        ));
                                        &self.stop_button
                                    },
                                    Some(STATE_SEARCHING),
                                );
                                &self.entry_buttons
                            })
                            .build(),
                    )
                    .append({
                        self.match_case.connect_toggled(glib::clone!(
                            #[weak(rename_to = imp)]
                            self,
                            move |button| {
                                let _ = imp.options.case_sensitive_search.set(button.is_active());
                                imp.obj().set_message(MESSAGE_NONE);
                            }
                        ));
                        &self.match_case
                    })
                    .append({
                        self.hex_mode.connect_toggled(glib::clone!(
                            #[weak(rename_to = imp)]
                            self,
                            move |button| {
                                let mode = if button.is_active() {
                                    Mode::Binary
                                } else {
                                    Mode::Text
                                };
                                let _ = imp.options.search_mode.set(mode);
                                imp.update_search_mode(mode);
                                imp.obj().set_message(MESSAGE_NONE);
                            }
                        ));
                        self.update_search_mode(self.options.search_mode.get());
                        &self.hex_mode
                    })
                    .append({
                        self.message
                            .add_named(&gtk::Label::new(None), Some(MESSAGE_NONE));
                        self.message.add_named(
                            &gtk::Label::new(Some(&gettext("Invalid hexadecimal pattern"))),
                            Some(MESSAGE_NOT_HEX),
                        );
                        self.message.add_named(
                            &gtk::Label::new(Some(&gettext("Not found"))),
                            Some(MESSAGE_NOT_FOUND),
                        );
                        &self.message
                    })
                    .build(),
            ));

            let controller = gtk::ShortcutController::new();
            controller.add_shortcut(
                gtk::Shortcut::builder()
                    .trigger(&gtk::KeyvalTrigger::new(gdk::Key::Escape, NO_MOD))
                    .action(&gtk::CallbackAction::new(glib::clone!(
                        #[weak(rename_to = obj)]
                        self.obj(),
                        #[upgrade_or]
                        glib::Propagation::Proceed,
                        move |_, _| {
                            obj.hide();
                            glib::Propagation::Stop
                        }
                    )))
                    .build(),
            );
            self.inner.add_controller(controller);

            self.inner.set_parent(&*obj);
        }

        fn dispose(&self) {
            self.inner.unparent();
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("search")
                        .param_types([bool::static_type()])
                        .build(),
                    glib::subclass::Signal::builder("abort").build(),
                ]
            })
        }
    }

    impl WidgetImpl for SearchBar {}

    impl SearchBar {
        fn start_search(&self, forward: bool) {
            self.obj().emit_by_name::<()>("search", &[&forward]);
        }

        fn stop_search(&self) {
            self.obj().emit_by_name::<()>("abort", &[]);
        }

        pub(super) fn update_search_mode(&self, mode: Mode) {
            self.match_case.set_sensitive(mode == Mode::Text);

            let option = match mode {
                Mode::Text => &self.options.search_pattern_text,
                Mode::Binary => &self.options.search_pattern_hex,
            };
            self.entry.set_history(&option.get());
        }
    }
}

glib::wrapper! {
    pub struct SearchBar(ObjectSubclass<imp::SearchBar>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl SearchBar {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn show(&self, focus: bool) {
        self.imp().inner.set_search_mode(true);
        if focus {
            self.imp().entry.grab_focus();
        }
    }

    pub fn hide(&self) {
        self.imp().inner.set_search_mode(false);
    }

    fn set_state(&self, state: &'static str) {
        self.imp().entry_buttons.set_visible_child_name(state);
    }

    fn set_message(&self, message: &'static str) {
        self.imp().message.set_visible_child_name(message);
        if message == MESSAGE_NONE {
            self.imp().entry.remove_css_class("error");
        } else {
            self.imp().entry.add_css_class("error");
        }
    }

    pub fn settings(&self) -> Option<SearchSettings> {
        let text = self.imp().entry.text();
        if self.imp().hex_mode.is_active() {
            // Whitespace is ignored in hex patterns
            if text.trim().is_empty() {
                None
            } else if let Some(pattern) = hex_to_bytes(&text) {
                Some(SearchSettings::Binary { pattern })
            } else {
                self.set_message(MESSAGE_NOT_HEX);
                None
            }
        } else {
            // Whitespace matters for text patterns
            if text.is_empty() {
                None
            } else {
                Some(SearchSettings::Text {
                    pattern: text.to_string(),
                    match_case: self.imp().match_case.is_active(),
                })
            }
        }
    }

    pub fn add_to_history(&self) {
        let mut text = self.imp().entry.text().to_string();
        let mode = if self.imp().hex_mode.is_active() {
            Mode::Binary
        } else {
            Mode::Text
        };
        if mode == Mode::Binary {
            text = text.trim().to_owned();
        }
        if text.is_empty() {
            return;
        }
        if let Err(error) = self.imp().options.add_to_history(&text, mode) {
            eprintln!("{error}");
        }
        self.imp().update_search_mode(mode);
    }

    pub fn show_progress(&self, progress: u32) {
        self.set_message(MESSAGE_NONE);
        self.set_state(STATE_SEARCHING);

        let entry = &self.imp().entry;
        if entry
            .root()
            .and_then(|root| root.focus())
            .is_some_and(|focus| focus.is_ancestor(entry))
        {
            self.imp().stop_button.grab_focus();
        }

        entry.set_progress_fraction(f64::from(progress.clamp(0, 1000)) / 1000.0);
        entry.set_sensitive(false);
        self.imp().match_case.set_sensitive(false);
        self.imp().hex_mode.set_sensitive(false);
    }

    pub fn hide_progress(&self) {
        let entry = &self.imp().entry;
        entry.set_progress_fraction(0.0);
        entry.set_sensitive(true);
        self.imp()
            .match_case
            .set_sensitive(!self.imp().hex_mode.is_active());
        self.imp().hex_mode.set_sensitive(true);

        if self.imp().stop_button.has_focus() {
            entry.grab_focus();
        }

        self.set_state(STATE_EDITING);
    }

    pub fn show_not_found(&self) {
        self.set_message(MESSAGE_NOT_FOUND);
    }

    pub fn showing_not_found(&self) -> bool {
        self.imp().message.visible_child_name().as_deref() == Some(MESSAGE_NOT_FOUND)
    }

    pub fn connect_search<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, bool) + 'static,
    {
        self.connect_closure(
            "search",
            false,
            glib::closure_local!(move |this: &Self, forward: bool| { (callback)(this, forward) }),
        )
    }

    pub fn connect_abort<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self) + 'static,
    {
        self.connect_closure(
            "abort",
            false,
            glib::closure_local!(move |this: &Self| { (callback)(this) }),
        )
    }
}

fn hex_to_bytes(text: &str) -> Option<Vec<u8>> {
    let nibs = text
        .chars()
        .filter(|c| !c.is_ascii_whitespace())
        .map(|ch| ch.to_digit(16).and_then(|d| d.try_into().ok()))
        .collect::<Option<Vec<u8>>>()?;

    if let (chunks, []) = nibs.as_chunks::<2>() {
        Some(
            chunks
                .iter()
                .map(|[upper, lower]| upper * 16 + lower)
                .collect(),
        )
    } else {
        // There is a remainder, not an even length
        None
    }
}

u32_enum! {
    #[derive(strum::FromRepr)]
    pub enum Mode {
        #[default]
        Text,
        Binary,
    }
}

#[derive(Debug)]
pub enum SearchSettings {
    Text { pattern: String, match_case: bool },
    Binary { pattern: Vec<u8> },
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_hex_to_bytes() {
        assert_eq!(hex_to_bytes("1axy"), None);
        assert_eq!(hex_to_bytes(""), Some(vec![]));
        assert_eq!(hex_to_bytes(" "), Some(vec![]));
        assert_eq!(hex_to_bytes(" 1"), None);
        assert_eq!(hex_to_bytes(" 12"), Some(vec![18]));
        assert_eq!(hex_to_bytes("20 20 3a"), Some(vec![32, 32, 58]));
        assert_eq!(
            hex_to_bytes("00 0102 03 04 05 06 AA BB CC FE"),
            Some(vec![0, 1, 2, 3, 4, 5, 6, 0xAA, 0xBB, 0xCC, 0xFE])
        );
    }
}
