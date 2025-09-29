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

use super::common::create_category;
use crate::{
    options::{options::GeneralOptions, types::WriteResult},
    types::{PermissionDisplayMode, SizeDisplayMode},
};
use gettextrs::{gettext, pgettext};
use gtk::prelude::*;

pub struct FormatTab {
    vbox: gtk::Box,
    size_powered: gtk::CheckButton,
    size_locale: gtk::CheckButton,
    size_grouped: gtk::CheckButton,
    size_plain: gtk::CheckButton,
    permissions_text: gtk::CheckButton,
    permissions_number: gtk::CheckButton,
    date_format: gtk::Entry,
}

impl FormatTab {
    pub fn new() -> Self {
        let vbox = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .margin_top(6)
            .margin_bottom(6)
            .margin_start(6)
            .margin_end(6)
            .hexpand(true)
            .vexpand(true)
            .build();

        let size_display = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        let size_powered = gtk::CheckButton::builder()
            .label(pgettext("'Powered' refers to the mode of file size display (here - display using units of data: kB, MB, GB, ...)", "Powered"))
            .build();
        size_display.append(&size_powered);

        let size_locale = gtk::CheckButton::builder()
            .label(pgettext("'<locale>' refers to the mode of file size display (here - use current locale settings)", "<locale>"))
            .group(&size_powered)
            .build();
        size_display.append(&size_locale);

        let size_grouped = gtk::CheckButton::builder()
            .label(gettext("Grouped"))
            .group(&size_powered)
            .build();
        size_display.append(&size_grouped);

        let size_plain = gtk::CheckButton::builder()
            .label(gettext("Plain"))
            .group(&size_powered)
            .build();
        size_display.append(&size_plain);

        vbox.append(&create_category(
            &gettext("Size display mode"),
            &size_display,
        ));

        let permission_display = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        let permissions_text = gtk::CheckButton::builder()
            .label(gettext("Text (rw-r--r--)"))
            .build();
        permission_display.append(&permissions_text);

        let permissions_number = gtk::CheckButton::builder()
            .label(gettext("Number (644)"))
            .group(&permissions_text)
            .build();
        permission_display.append(&permissions_number);

        vbox.append(&create_category(
            &gettext("Permission display mode"),
            &permission_display,
        ));

        let grid = gtk::Grid::builder()
            .column_spacing(12)
            .row_spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Date format"), &grid));

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Format:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            0,
            1,
            1,
        );
        let date_format = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&date_format, 1, 0, 1, 1);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Test result:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            1,
            1,
            1,
        );
        let date_format_test = gtk::Label::builder()
            .halign(gtk::Align::Start)
            .hexpand(true)
            .build();
        grid.attach(&date_format_test, 1, 1, 1, 1);
        grid.attach(
            &gtk::Label::builder()
                .label(gettext(
                    "See the help page in the documentation on how to set the format string.",
                ))
                .halign(gtk::Align::Start)
                .build(),
            1,
            2,
            1,
            1,
        );

        date_format
            .bind_property("text", &date_format_test, "label")
            .transform_to(|_, format: &str| {
                glib::DateTime::now_local()
                    .ok()
                    .and_then(|dt| dt.format(format).ok())
            })
            .sync_create()
            .build();

        Self {
            vbox,
            size_powered,
            size_locale,
            size_grouped,
            size_plain,
            permissions_text,
            permissions_number,
            date_format,
        }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.vbox.clone().upcast()
    }

    pub fn read(&self, options: &GeneralOptions) {
        match options.size_display_mode.get() {
            SizeDisplayMode::Powered => &self.size_powered,
            SizeDisplayMode::Locale => &self.size_locale,
            SizeDisplayMode::Grouped => &self.size_grouped,
            SizeDisplayMode::Plain => &self.size_plain,
        }
        .set_active(true);

        match options.permissions_display_mode.get() {
            PermissionDisplayMode::Text => &self.permissions_text,
            PermissionDisplayMode::Number => &self.permissions_number,
        }
        .set_active(true);

        self.date_format
            .set_text(&options.date_display_format.get());
    }

    pub fn write(&self, options: &GeneralOptions) -> WriteResult {
        options
            .size_display_mode
            .set(if self.size_powered.is_active() {
                SizeDisplayMode::Powered
            } else if self.size_locale.is_active() {
                SizeDisplayMode::Locale
            } else if self.size_grouped.is_active() {
                SizeDisplayMode::Grouped
            } else if self.size_plain.is_active() {
                SizeDisplayMode::Plain
            } else {
                SizeDisplayMode::default()
            })?;

        options
            .permissions_display_mode
            .set(if self.permissions_text.is_active() {
                PermissionDisplayMode::Text
            } else if self.permissions_number.is_active() {
                PermissionDisplayMode::Number
            } else {
                PermissionDisplayMode::default()
            })?;

        options.date_display_format.set(self.date_format.text())?;

        Ok(())
    }
}
