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

use super::devices_widget::devices_widget;
use crate::options::{options::GeneralOptions, types::WriteResult};
use gettextrs::gettext;
use gtk::prelude::*;

pub struct DevicesTab {
    vbox: gtk::Box,
    samba_workgroups_button: gtk::CheckButton,
    device_only_icon: gtk::CheckButton,
}

impl DevicesTab {
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

        vbox.append(&devices_widget());

        let samba_workgroups_button = gtk::CheckButton::builder()
            .label(gettext(
                "Show Samba workgroups button\n(Needs program restart if altered)",
            ))
            .build();
        vbox.append(&samba_workgroups_button);

        let device_only_icon = gtk::CheckButton::builder()
            .label(gettext("Show only the icons"))
            .build();
        vbox.append(&device_only_icon);

        Self {
            vbox,
            samba_workgroups_button,
            device_only_icon,
        }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.vbox.clone().upcast()
    }

    pub fn read(&self, options: &GeneralOptions) {
        self.samba_workgroups_button
            .set_active(options.show_samba_workgroups_button.get());
        self.device_only_icon
            .set_active(options.device_only_icon.get());
    }

    pub fn write(&self, options: &GeneralOptions) -> WriteResult {
        options
            .show_samba_workgroups_button
            .set(self.samba_workgroups_button.is_active())?;
        options
            .device_only_icon
            .set(self.device_only_icon.is_active())?;
        Ok(())
    }
}
