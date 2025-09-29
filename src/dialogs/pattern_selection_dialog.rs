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

use crate::{
    file_list::list::FileList,
    filter::{Filter, PatternType},
    history_entry::HistoryEntry,
    options::options::SearchConfig,
    utils::{
        ErrorMessage, NO_BUTTONS, SenderExt, channel_send_action, dialog_button_box,
        handle_escape_key,
    },
};
use gettextrs::gettext;
use gtk::prelude::*;

pub struct SelectionPattern {
    pub pattern: String,
    pub case_sensitive: bool,
    pub pattern_type: PatternType,
}

pub async fn show_pattern_selection_dialog(
    parent_window: &gtk::Window,
    mode: bool,
    patterns: Vec<String>,
    pattern_type: PatternType,
) -> Option<SelectionPattern> {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .title(if mode {
            gettext("Select Using Pattern")
        } else {
            gettext("Unselect Using Pattern")
        })
        .build();

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
    dialog.set_child(Some(&grid));

    let pattern_entry = HistoryEntry::default();
    pattern_entry.set_hexpand(true);
    pattern_entry.set_history(&patterns);
    pattern_entry.entry().set_activates_default(true);
    pattern_entry.entry().select_region(0, -1);

    let label = gtk::Label::builder()
        .label(gettext("_Pattern:"))
        .use_underline(true)
        .mnemonic_widget(&pattern_entry)
        .build();

    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&pattern_entry, 1, 0, 1, 1);

    let case_sens = gtk::CheckButton::with_mnemonic(&gettext("Case _sensitive"));
    grid.attach(&case_sens, 0, 1, 2, 1);

    let shell_check = gtk::CheckButton::with_mnemonic(&gettext("She_ll syntax"));
    let regex_check = gtk::CheckButton::with_mnemonic(&gettext("Rege_x syntax"));
    regex_check.set_group(Some(&shell_check));
    match pattern_type {
        PatternType::FnMatch => shell_check.set_active(true),
        PatternType::Regex => regex_check.set_active(true),
    }
    grid.attach(&shell_check, 0, 2, 2, 1);
    grid.attach(&regex_check, 0, 3, 2, 1);

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    let cancel_btn = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(false)
    ));

    let ok_btn = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .receives_default(true)
        .build();
    ok_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_btn, &ok_btn]),
        0,
        4,
        2,
        1,
    );

    handle_escape_key(&dialog, &channel_send_action(&sender, false));
    dialog.connect_close_request(glib::clone!(
        #[strong]
        sender,
        move |_| {
            sender.toss(false);
            glib::Propagation::Proceed
        }
    ));

    dialog.set_default_widget(Some(&ok_btn));

    dialog.present();
    pattern_entry.grab_focus();

    let mut result = None;
    let response = receiver.recv().await;
    if response == Ok(true) {
        let pattern = pattern_entry.text();
        if !pattern.is_empty() {
            let case_sensitive = case_sens.is_active();
            let filter_type = if regex_check.is_active() {
                PatternType::Regex
            } else {
                PatternType::FnMatch
            };

            result = Some(SelectionPattern {
                pattern: pattern.into(),
                case_sensitive,
                pattern_type: filter_type,
            });
        }
    }
    dialog.close();

    result
}

pub async fn select_by_pattern(file_list: &FileList, mode: bool) {
    let search_config = SearchConfig::get();
    let Some(parent_window) = file_list.root().and_downcast::<gtk::Window>() else {
        eprintln!("No window");
        return;
    };
    if let Some(selection_pattern) = show_pattern_selection_dialog(
        &parent_window,
        mode,
        search_config.name_patterns(),
        search_config.default_profile_syntax(),
    )
    .await
    {
        let filter = Filter::new(
            &selection_pattern.pattern,
            selection_pattern.case_sensitive,
            selection_pattern.pattern_type,
        );

        match filter {
            Ok(filter) => {
                file_list.toggle_with_pattern(&filter, mode);

                search_config.add_name_pattern(&selection_pattern.pattern);
                search_config.set_default_profile_syntax(selection_pattern.pattern_type);
            }
            Err(error) => {
                ErrorMessage::with_error(gettext("Bad expression"), &*error)
                    .show(&parent_window)
                    .await;
            }
        }
    }
}
