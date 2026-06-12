// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{history_entry::HistoryEntry, options::ViewerOptions, utils::u32_enum};
use component_framework::{
    action_list,
    helpers::{ActionGroup, ActionListOutput},
    prelude::*,
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};

u32_enum! {
    enum State {
        #[default]
        Editing,
        Searching,
    }
}

impl From<State> for String {
    fn from(value: State) -> Self {
        u32::from(value).to_string()
    }
}

u32_enum! {
    pub enum SearchBarError {
        #[default]
        None,
        NotHex,
        NotFound,
        EncodingError,
    }
}

impl From<SearchBarError> for String {
    fn from(value: SearchBarError) -> Self {
        u32::from(value).to_string()
    }
}

u32_enum! {
    pub enum Mode {
        #[default]
        Text,
        Binary,
    }
}

#[derive(Debug)]
pub enum SearchSettings {
    Text { pattern: String, match_case: bool },
    Binary { pattern: Vec<u8>, match_case: bool },
}

action_list! {
    enum SearchBarActions {
        "searchbar.close" as Close,
    }
}

#[derive(Debug, Default)]
pub struct SearchBarView {
    searchbar: gtk::SearchBar,
    entry: HistoryEntry,
    entry_buttons: gtk::Stack,
    stop_button: gtk::Button,
    match_case: gtk::ToggleButton,
    hex_mode: gtk::ToggleButton,
    message: gtk::Stack,
}

impl SearchBarView {
    pub fn settings(&self) -> Result<Option<SearchSettings>, SearchBarError> {
        let text = self.entry.text();
        if self.hex_mode.is_active() {
            // Whitespace is ignored in hex patterns
            if text.trim().is_empty() {
                Ok(None)
            } else if let Some(pattern) = hex_to_bytes(&text) {
                Ok(Some(SearchSettings::Binary {
                    pattern,
                    match_case: self.match_case.is_active(),
                }))
            } else {
                Err(SearchBarError::NotHex)
            }
        } else {
            // Whitespace matters for text patterns
            if text.is_empty() {
                Ok(None)
            } else {
                Ok(Some(SearchSettings::Text {
                    pattern: text.to_string(),
                    match_case: self.match_case.is_active(),
                }))
            }
        }
    }

    pub fn add_to_history(&self) {
        let mut text = self.entry.text().to_string();
        let mode = if self.hex_mode.is_active() {
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
        if let Err(error) = ViewerOptions::instance().add_to_history(&text, mode) {
            eprintln!("{error}");
        }
        self.update_search_mode(mode);
    }

    pub fn show_progress(&self, progress: u32) {
        self.set_state(State::Searching);

        if self
            .entry
            .root()
            .and_then(|root| root.focus())
            .is_some_and(|focus| focus.is_ancestor(&self.entry))
        {
            self.stop_button.grab_focus();
        }

        self.entry
            .set_progress_fraction(f64::from(progress.clamp(0, 1000)) / 1000.0);
        self.entry.set_sensitive(false);
        self.match_case.set_sensitive(false);
        self.hex_mode.set_sensitive(false);
    }

    pub fn hide_progress(&self) {
        self.entry.set_progress_fraction(0.0);
        self.entry.set_sensitive(true);
        self.match_case.set_sensitive(true);
        self.hex_mode.set_sensitive(true);

        if self.stop_button.has_focus() {
            self.entry.grab_focus();
        }

        self.set_state(State::Editing);
    }

    pub fn update_search_mode(&self, mode: Mode) {
        let option = match mode {
            Mode::Text => &ViewerOptions::instance().search_pattern_text,
            Mode::Binary => &ViewerOptions::instance().search_pattern_hex,
        };
        self.entry.set_history(&option.get());
    }

    fn is_state(&self, state: State) -> bool {
        self.entry_buttons.visible_child_name().as_deref() == Some(&String::from(state))
    }

    fn set_state(&self, state: State) {
        self.entry_buttons
            .set_visible_child_name(&String::from(state));
    }
}

#[derive(Debug)]
pub enum SearchBarInput {
    Show,
    Close,
    Closed,
    StartSearch(bool),
    Progress(u32),
    HideProgress,
    ShowError(SearchBarError),
    ToggleMatchCase,
    ToggleHexMode,
}

#[derive(Debug)]
pub enum SearchBarOutput {
    StartSearch(bool, SearchSettings),
    StopSearch,
    Closed,
}

#[derive(Debug, Default)]
pub struct SearchBar {
    action_group: ComponentController<ActionGroup<SearchBarActions::List>>,
    error: SearchBarError,
}

impl Component for SearchBar {
    type Input = SearchBarInput;
    type Output = SearchBarOutput;
    type View = SearchBarView;
    type Root = gtk::SearchBar;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = Self::View::default();
        let options = ViewerOptions::instance();

        with!(&view.searchbar {
            .set_show_close_button(true);
            .connect_search_mode_enabled_notify({
                let sender = sender.clone();
                move |inner| {
                    if !inner.is_search_mode() {
                        sender.input(Self::Input::Closed);
                    }
                }
            });

            .insert_action_group(SearchBarActions::prefix(), Some(self.action_group.attach(
                sender, |message| {
                    match message {
                        SearchBarActions::Output::Close => Self::Input::Close,
                    }
                }
            )));

            .add_controller(with!(gtk::ShortcutController {
                SearchBarActions::Output::Close.shortcut("Escape");
            }));

            gtk::Box {
                .set_orientation(gtk::Orientation::Horizontal);
                .add_css_class("spacing");

                gtk::Box {
                    .set_orientation(gtk::Orientation::Horizontal);

                    &view.entry {
                        .set_hexpand(true);
                        .set_placeholder_text(Some(&gettext("Search…")));
                        .set_primary_icon_name(Some("system-search"));
                        .set_primary_icon_activatable(true);
                        .set_popover_position(gtk::PositionType::Top);
                        .connect_activate(forward!(sender.input(Self::Input::StartSearch(true))));
                        .connect_icon_press({
                            let sender = sender.clone();
                            move |_, icon_pos| {
                                if icon_pos == gtk::EntryIconPosition::Primary {
                                    sender.input(Self::Input::StartSearch(true));
                                }
                            }
                        });
                        .connect_changed(forward!(sender.input(
                            Self::Input::ShowError(SearchBarError::None)
                        )));
                    }

                    &view.entry_buttons {
                        .add_named(&with!(gtk::Box {
                            .set_orientation(gtk::Orientation::Horizontal);

                            gtk::Button {
                                .set_icon_name("go-up");
                                .set_tooltip_text(Some(&gettext("Previous Match")));
                                .connect_clicked(forward!(sender.input(
                                    Self::Input::StartSearch(false)
                                )));
                            }

                            gtk::Button {
                                .set_icon_name("go-down");
                                .set_tooltip_text(Some(&gettext("Next Match")));
                                .connect_clicked(forward!(sender.input(
                                    Self::Input::StartSearch(true)
                                )));
                            }
                        }), Some(&String::from(State::Editing)));

                        .add_named(with!(&view.stop_button {
                            .set_icon_name("edit-delete");
                            .set_tooltip_text(Some(&gettext("Stop")));
                            .connect_clicked(forward!(sender.output(Self::Output::StopSearch)));
                        }), Some(&String::from(State::Searching)));
                    }
                }

                &view.match_case {
                    .set_icon_name("gnome-commander-match-case");
                    .set_tooltip_text(Some(&gettext("Case sensitive")));
                    .set_active(options.case_sensitive_search.get());
                    .connect_toggled(forward!(sender.input(Self::Input::ToggleMatchCase)));
                }

                &view.hex_mode {
                    .set_icon_name("gnome-commander-hex");
                    .set_tooltip_text(Some(&gettext("Hexadecimal")));
                    .set_active(options.search_mode.get() == Mode::Binary);
                    .connect_toggled(forward!(sender.input(Self::Input::ToggleHexMode)));
                }

                &view.message {
                    .add_named(
                        &gtk::Label::new(None), Some(&String::from(SearchBarError::None))
                    );
                    .add_named(
                        &gtk::Label::new(Some(&gettext("Invalid hexadecimal pattern"))),
                        Some(&String::from(SearchBarError::NotHex)),
                    );
                    .add_named(
                        &gtk::Label::new(Some(&gettext("Not found"))),
                        Some(&String::from(SearchBarError::NotFound)),
                    );
                    .add_named(
                        &gtk::Label::new(Some(&gettext("Cannot translate into text encoding"))),
                        Some(&String::from(SearchBarError::EncodingError)),
                    );
                }
            }
        });

        view.update_search_mode(options.search_mode.get());
        self.show_error(SearchBarError::None, &view);

        view
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        &view.searchbar
    }

    async fn update(
        &mut self,
        msg: Self::Input,
        sender: &ComponentSender<Self>,
        view: &mut Self::View,
    ) {
        match msg {
            Self::Input::Show => {
                view.searchbar.set_search_mode(true);
                view.entry.grab_focus();
            }
            Self::Input::Close => view.searchbar.set_search_mode(false),
            Self::Input::StartSearch(forward) => {
                view.searchbar.set_search_mode(true);
                match view.settings() {
                    Ok(Some(settings)) => {
                        view.add_to_history();
                        sender.output(Self::Output::StartSearch(forward, settings));
                    }
                    Ok(None) => {
                        view.entry.grab_focus();
                    }
                    Err(error) => self.show_error(error, view),
                }
            }
            Self::Input::Progress(progress) => {
                view.show_progress(progress);
                self.show_error(SearchBarError::None, view);
            }
            Self::Input::HideProgress => view.hide_progress(),
            Self::Input::Closed => {
                if view.is_state(State::Searching) {
                    sender.output(Self::Output::StopSearch);
                }
                sender.output(Self::Output::Closed);
            }
            Self::Input::ShowError(error) => self.show_error(error, view),
            Self::Input::ToggleMatchCase => {
                let _ = ViewerOptions::instance()
                    .case_sensitive_search
                    .set(view.match_case.is_active());
                self.show_error(SearchBarError::None, view);
            }
            Self::Input::ToggleHexMode => {
                let mode = if view.hex_mode.is_active() {
                    Mode::Binary
                } else {
                    Mode::Text
                };
                let _ = ViewerOptions::instance().search_mode.set(mode);
                view.update_search_mode(mode);
                self.show_error(SearchBarError::None, view);
            }
        }
    }

    async fn handle_subcomponents(&mut self) {
        self.action_group.handle_incoming().await
    }
}

impl SearchBar {
    fn show_error(&mut self, error: SearchBarError, view: &SearchBarView) {
        self.error = error;

        view.message.set_visible_child_name(&String::from(error));
        if error == SearchBarError::None {
            view.entry.remove_css_class("error");
        } else {
            view.entry.add_css_class("error");
        }
    }

    pub fn displayed_error(&self) -> SearchBarError {
        self.error
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
