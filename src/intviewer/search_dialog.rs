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

use gettextrs::gettext;
use glib::translate::from_glib_none;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{ffi::c_char, fmt};

const GCMD_INTERNAL_VIEWER: &str = "org.gnome.gnome-commander.preferences.internal-viewer";
const GCMD_SETTINGS_IV_CASE_SENSITIVE: &str = "case-sensitive-search";
const GCMD_SETTINGS_IV_SEARCH_MODE: &str = "search-mode";
const GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT: &str = "search-pattern-text";
const GCMD_SETTINGS_IV_SEARCH_PATTERN_HEX: &str = "search-pattern-hex";

const INTVIEWER_HISTORY_SIZE: usize = 16;

mod imp {
    use super::*;
    use crate::utils::{
        channel_send_action, dialog_button_box, handle_escape_key, SenderExt, NO_BUTTONS,
    };
    use std::cell::OnceCell;

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::SearchDialog)]
    pub struct SearchDialog {
        pub search_entry: gtk::ComboBoxText,
        pub text_mode: gtk::CheckButton,
        pub hex_mode: gtk::CheckButton,
        pub case_sensitive: gtk::CheckButton,
        pub cancel_button: gtk::Button,
        pub ok_button: gtk::Button,
        #[property(get, construct_only)]
        pub settings: OnceCell<gio::Settings>,
        pub sender: async_channel::Sender<bool>,
        pub receiver: async_channel::Receiver<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SearchDialog {
        const NAME: &'static str = "GnomeCmdViewerSearchDialog";
        type Type = super::SearchDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            let (sender, receiver) = async_channel::bounded(1);
            Self {
                search_entry: gtk::ComboBoxText::with_entry(),
                text_mode: gtk::CheckButton::with_mnemonic(&gettext("_Text")),
                hex_mode: gtk::CheckButton::with_mnemonic(&gettext("_Hexadecimal")),
                case_sensitive: gtk::CheckButton::with_mnemonic(&gettext("_Match case")),
                cancel_button: gtk::Button::with_mnemonic(&gettext("_Cancel")),
                ok_button: gtk::Button::with_mnemonic(&gettext("_OK")),
                settings: Default::default(),
                sender,
                receiver,
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for SearchDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dialog = self.obj();

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .column_spacing(12)
                .row_spacing(6)
                .build();
            dialog.set_child(Some(&grid));

            if let Some(entry) = self.search_entry.child().and_downcast::<gtk::Entry>() {
                entry.set_activates_default(true);
            }
            let label = gtk::Label::builder()
                .label(gettext("_Search for:"))
                .use_underline(true)
                .mnemonic_widget(&self.search_entry)
                .build();
            grid.attach(&label, 0, 0, 1, 1);
            grid.attach(&self.search_entry, 1, 0, 2, 1);

            self.search_entry.connect_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.entry_changed()
            ));

            // Search mode radio buttons
            self.hex_mode.set_group(Some(&self.text_mode));
            self.text_mode.connect_toggled(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.change_search_mode()
            ));
            self.hex_mode.connect_toggled(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.change_search_mode()
            ));
            grid.attach(&self.text_mode, 1, 1, 1, 1);
            grid.attach(&self.hex_mode, 1, 2, 1, 1);

            grid.attach(&self.case_sensitive, 2, 1, 1, 1);

            grid.attach(
                &dialog_button_box(NO_BUTTONS, &[&self.cancel_button, &self.ok_button]),
                0,
                3,
                3,
                1,
            );

            self.cancel_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(false)
            ));
            self.ok_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(true)
            ));

            dialog.set_default_widget(Some(&self.ok_button));
            handle_escape_key(
                dialog.upcast_ref(),
                &channel_send_action(&self.sender, false),
            );
        }
    }

    impl WidgetImpl for SearchDialog {}
    impl WindowImpl for SearchDialog {}

    impl SearchDialog {
        pub(super) fn search_mode(&self) -> Mode {
            if self.hex_mode.is_active() {
                Mode::Binary
            } else {
                Mode::Text
            }
        }

        fn search_text(&self) -> Option<glib::GString> {
            self.search_entry
                .active_text()
                .filter(|text| !text.is_empty())
        }

        fn search_hex(&self) -> Option<Vec<u8>> {
            self.search_text().and_then(|text| hex_to_bytes(&text))
        }

        fn case_sensitive(&self) -> bool {
            self.case_sensitive.is_active()
        }

        pub(super) fn search_request(&self) -> Option<SearchRequest> {
            match self.search_mode() {
                Mode::Text => Some(SearchRequest::Text {
                    pattern: self.search_text()?,
                    case_sensitive: self.case_sensitive(),
                }),
                Mode::Binary => Some(SearchRequest::Binary {
                    query: self.search_text()?,
                    pattern: self.search_hex()?,
                }),
            }
        }

        fn entry_changed(&self) {
            self.ok_button
                .set_sensitive(self.search_request().is_some());
        }

        fn change_search_mode(&self) {
            match self.search_mode() {
                Mode::Text => {
                    self.case_sensitive.set_sensitive(true);
                    self.set_history(GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT);
                }
                Mode::Binary => {
                    self.case_sensitive.set_sensitive(false);
                    self.set_history(GCMD_SETTINGS_IV_SEARCH_PATTERN_HEX);
                }
            }
            self.entry_changed();
            self.search_entry.grab_focus();
        }

        fn set_history(&self, key: &str) {
            self.search_entry.remove_all();
            for item in self
                .obj()
                .settings()
                .strv(key)
                .into_iter()
                .take(INTVIEWER_HISTORY_SIZE)
            {
                self.search_entry.append_text(&item);
            }
        }

        pub(super) fn init_from_settings(&self) {
            let settings = self.obj().settings();
            match Mode::from_repr(settings.enum_(GCMD_SETTINGS_IV_SEARCH_MODE)).unwrap_or_default()
            {
                Mode::Text => {
                    self.text_mode.set_active(true);
                    self.case_sensitive
                        .set_active(settings.boolean(GCMD_SETTINGS_IV_CASE_SENSITIVE));
                    self.search_entry.set_active(Some(0));
                }
                Mode::Binary => {
                    self.hex_mode.set_active(true);
                    self.search_entry.set_active(Some(0));
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
    pub fn new(parent: &gtk::Window, settings: &gio::Settings) -> Self {
        glib::Object::builder()
            .property("title", gettext("Find"))
            .property("modal", true)
            .property("transient-for", parent)
            .property("resizable", false)
            .property("settings", settings)
            .build()
    }

    pub async fn show(parent: &gtk::Window) -> Option<SearchRequest> {
        let settings = gio::Settings::new(GCMD_INTERNAL_VIEWER);
        let dialog = Self::new(parent, &settings);

        dialog.imp().init_from_settings();

        dialog.present();
        dialog.imp().search_entry.grab_focus();

        let response = dialog.imp().receiver.recv().await;
        dialog.close();

        if response != Ok(true) {
            return None;
        }

        let result = dialog.imp().search_request()?;

        if let Err(error) = result.save(&settings) {
            eprintln!("Failed to save search parameters: {error}")
        }

        Some(result)
    }
}

#[derive(Clone, Copy, strum::FromRepr, Default)]
#[repr(i32)]
enum Mode {
    #[default]
    Text = 0,
    Binary,
}

#[derive(Clone)]
pub enum SearchRequest {
    Text {
        pattern: glib::GString,
        case_sensitive: bool,
    },
    Binary {
        query: glib::GString,
        pattern: Vec<u8>,
    },
}

impl SearchRequest {
    pub fn len(&self) -> u64 {
        match self {
            Self::Text { pattern, .. } => pattern.len() as u64,
            Self::Binary { pattern, .. } => pattern.len() as u64,
        }
    }

    fn save(&self, settings: &gio::Settings) -> Result<(), glib::BoolError> {
        match self {
            SearchRequest::Binary { query, .. } => {
                settings.set_enum(GCMD_SETTINGS_IV_SEARCH_MODE, Mode::Binary as i32)?;
                add_to_history(&settings, GCMD_SETTINGS_IV_SEARCH_PATTERN_HEX, query)?;
            }
            SearchRequest::Text {
                pattern,
                case_sensitive,
            } => {
                settings.set_enum(GCMD_SETTINGS_IV_SEARCH_MODE, Mode::Text as i32)?;
                settings.set_boolean(GCMD_SETTINGS_IV_CASE_SENSITIVE, *case_sensitive)?;
                add_to_history(&settings, GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT, &pattern)?;
            }
        }
        Ok(())
    }
}

impl fmt::Display for SearchRequest {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Text { pattern, .. } => {
                write!(f, "{pattern}")?;
            }
            Self::Binary { pattern, .. } => {
                for byte in pattern {
                    write!(f, "{:X}", byte)?;
                }
            }
        }
        Ok(())
    }
}

fn hex_to_bytes(text: &str) -> Option<Vec<u8>> {
    let nibs: Vec<u8> = text
        .chars()
        .filter(|c| !c.is_ascii_whitespace())
        .map(|ch| ch.to_digit(16).map(|d| d as u8))
        .collect::<Option<Vec<u8>>>()?;

    if nibs.is_empty() || nibs.len() % 2 != 0 {
        return None;
    }

    Some(
        nibs.chunks(2)
            .map(|chunk| chunk[0] * 16 + chunk[1])
            .collect(),
    )
}

fn add_to_history(settings: &gio::Settings, key: &str, value: &str) -> Result<(), glib::BoolError> {
    let history = std::iter::once(glib::GString::from(value))
        .chain(settings.strv(key).into_iter().filter(|p| *p != value))
        .take(INTVIEWER_HISTORY_SIZE)
        .collect::<glib::StrV>();
    settings.set_strv(key, history)
}

#[no_mangle]
pub extern "C" fn gnome_cmd_viewer_search_text_add_to_history(value: *const c_char) {
    let value: String = unsafe { from_glib_none(value) };
    let settings = gio::Settings::new(GCMD_INTERNAL_VIEWER);
    if let Err(error) = add_to_history(&settings, GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT, &value) {
        eprintln!("Failed to save search history: {error}");
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_hex_to_bytes() {
        assert_eq!(hex_to_bytes(""), None);
        assert_eq!(hex_to_bytes(" "), None);
        assert_eq!(hex_to_bytes(" 1"), None);
        assert_eq!(hex_to_bytes(" 12"), Some(vec![18]));
        assert_eq!(hex_to_bytes("20 20 3a"), Some(vec![32, 32, 58]));
        assert_eq!(
            hex_to_bytes("00 0102 03 04 05 06 AA BB CC FE"),
            Some(vec![0, 1, 2, 3, 4, 5, 6, 0xAA, 0xBB, 0xCC, 0xFE])
        );
    }
}
