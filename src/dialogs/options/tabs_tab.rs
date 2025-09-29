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
    tab_label::TabLockIndicator,
};
use gettextrs::gettext;
use gtk::prelude::*;

pub struct TabsTab {
    vbox: gtk::Box,
    always_show_tabs: gtk::CheckButton,
    icon: gtk::CheckButton,
    asterisk: gtk::CheckButton,
    styled_text: gtk::CheckButton,
}

impl TabsTab {
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

        let always_show_tabs = gtk::CheckButton::builder()
            .label(gettext("Always show the tab bar"))
            .build();

        vbox.append(&create_category(&gettext("Tab bar"), &always_show_tabs));

        let tab_lock = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        let icon = gtk::CheckButton::builder()
            .label(gettext("Lock icon"))
            .build();
        tab_lock.append(&icon);

        let asterisk = gtk::CheckButton::builder()
            .label(gettext("* (asterisk)"))
            .group(&icon)
            .build();
        tab_lock.append(&asterisk);

        let styled_text = gtk::CheckButton::builder()
            .label(gettext("Styled text"))
            .group(&icon)
            .build();
        tab_lock.append(&styled_text);

        vbox.append(&create_category(&gettext("Tab lock indicator"), &tab_lock));

        Self {
            vbox,
            always_show_tabs,
            icon,
            asterisk,
            styled_text,
        }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.vbox.clone().upcast()
    }

    pub fn read(&self, options: &GeneralOptions) {
        self.always_show_tabs
            .set_active(options.always_show_tabs.get());
        match options.tab_lock_indicator.get() {
            TabLockIndicator::Icon => &self.icon,
            TabLockIndicator::Asterisk => &self.asterisk,
            TabLockIndicator::StyledText => &self.styled_text,
        }
        .set_active(true);
    }

    pub fn write(&self, options: &GeneralOptions) -> WriteResult {
        options
            .always_show_tabs
            .set(self.always_show_tabs.is_active())?;

        let tab_lock_indicator = if self.icon.is_active() {
            TabLockIndicator::Icon
        } else if self.asterisk.is_active() {
            TabLockIndicator::Asterisk
        } else if self.styled_text.is_active() {
            TabLockIndicator::StyledText
        } else {
            TabLockIndicator::Icon
        };
        options.tab_lock_indicator.set(tab_lock_indicator)?;
        Ok(())
    }
}
