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

use crate::utils::{NO_BUTTONS, SenderExt, dialog_button_box};
use gettextrs::gettext;
use gtk::{glib, prelude::*};

#[derive(Clone)]
pub struct RegexReplace {
    pub pattern: String,
    pub replacement: String,
    pub match_case: bool,
}

impl RegexReplace {
    pub fn is_valid(&self) -> bool {
        !self.pattern.is_empty() && self.compile_pattern().is_ok()
    }

    pub fn compile_pattern(&self) -> Result<glib::Regex, String> {
        let mut compile_options = glib::RegexCompileFlags::OPTIMIZE;
        if !self.match_case {
            compile_options |= glib::RegexCompileFlags::CASELESS
        }
        let regex = glib::Regex::new(
            &self.pattern,
            compile_options,
            glib::RegexMatchFlags::NOTEMPTY,
        )
        .map_err(|err| err.message().to_owned())?
        .ok_or_else(|| "Bad pattern".to_owned())?;
        Ok(regex)
    }
}

pub async fn show_advrename_regex_dialog(
    parent_window: &gtk::Window,
    title: &str,
    rx: Option<&RegexReplace>,
) -> Option<RegexReplace> {
    let dialog = gtk::Window::builder()
        .title(title)
        .transient_for(parent_window)
        .modal(true)
        .destroy_with_parent(true)
        .resizable(false)
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

    let pattern = gtk::Entry::builder().activates_default(true).build();
    let label = gtk::Label::builder()
        .label(gettext("_Search for:"))
        .use_underline(true)
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Center)
        .mnemonic_widget(&pattern)
        .build();
    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&pattern, 1, 0, 1, 1);

    let replacement = gtk::Entry::builder().activates_default(true).build();
    let label = gtk::Label::builder()
        .label(gettext("_Replace for:"))
        .use_underline(true)
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Center)
        .mnemonic_widget(&replacement)
        .build();
    grid.attach(&label, 0, 1, 1, 1);
    grid.attach(&replacement, 1, 1, 1, 1);

    let match_case = gtk::CheckButton::builder()
        .label(gettext("_Match case"))
        .use_underline(true)
        .margin_top(6)
        .margin_start(12)
        .build();
    grid.attach(&match_case, 0, 2, 2, 1);

    if let Some(r) = rx {
        pattern.set_text(&r.pattern);
        replacement.set_text(&r.replacement);
        match_case.set_active(r.match_case);
    }

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
        3,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_btn));

    dialog.present();
    pattern.grab_focus();

    let response = receiver.recv().await;
    let result = if response == Ok(true) {
        Some(RegexReplace {
            pattern: pattern.text().to_string(),
            replacement: replacement.text().to_string(),
            match_case: match_case.is_active(),
        })
    } else {
        None
    };
    dialog.close();

    result
}
