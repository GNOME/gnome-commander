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

use crate::utils::Gtk3to4BoxCompat;
use gettextrs::gettext;
use glib::{
    ffi::{GRegex, GRegexCompileFlags, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY, G_REGEX_OPTIMIZE},
    translate::ToGlibPtr,
};
use gtk::{glib, prelude::*};

pub struct RegexReplace {
    pub pattern: String,
    pub replacement: String,
    pub match_case: bool,
}

impl RegexReplace {
    pub fn is_valid(&self) -> bool {
        !self.pattern.is_empty() && self.compile_pattern().is_ok()
    }

    pub fn compile_pattern(&self) -> Result<Regex, glib::Error> {
        let compile_flags: GRegexCompileFlags = if self.match_case {
            G_REGEX_OPTIMIZE
        } else {
            G_REGEX_OPTIMIZE | G_REGEX_CASELESS
        };
        let regex = unsafe {
            glib::ffi::g_regex_new(
                self.pattern.to_glib_none().0,
                compile_flags,
                G_REGEX_MATCH_NOTEMPTY,
                std::ptr::null_mut(),
            )
        };
        Ok(Regex(regex))
    }
}

pub struct Regex(*mut GRegex);

impl Drop for Regex {
    fn drop(&mut self) {
        unsafe { glib::ffi::g_regex_unref(self.0) }
    }
}

pub async fn show_advrename_regex_dialog(
    parent_window: &gtk::Window,
    title: &str,
    rx: Option<&RegexReplace>,
) -> Option<RegexReplace> {
    let dialog = gtk::Dialog::builder()
        .title(title)
        .transient_for(parent_window)
        .modal(true)
        .destroy_with_parent(true)
        .resizable(false)
        .build();

    let grid = gtk::Grid::builder()
        .hexpand(true)
        .vexpand(true)
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
    dialog.content_area().append(&grid);

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

    let buttonbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .hexpand(false)
        .halign(gtk::Align::End)
        .build();
    let buttonbox_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);
    grid.attach(&buttonbox, 0, 3, 2, 1);

    let cancel_btn = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::End)
        .build();
    cancel_btn.connect_clicked(
        glib::clone!(@weak dialog => move |_| dialog.response(gtk::ResponseType::Cancel)),
    );
    buttonbox.append(&cancel_btn);
    buttonbox_size_group.add_widget(&cancel_btn);

    let ok_btn = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .can_default(true)
        .build();
    ok_btn.connect_clicked(
        glib::clone!(@weak dialog => move |_| dialog.response(gtk::ResponseType::Ok)),
    );
    buttonbox.append(&ok_btn);
    buttonbox_size_group.add_widget(&ok_btn);

    pattern.grab_focus();
    dialog.show_all();
    ok_btn.set_has_default(true);
    dialog.set_default_response(gtk::ResponseType::Ok);

    let response = dialog.run_future().await;
    let result = if response == gtk::ResponseType::Ok {
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
