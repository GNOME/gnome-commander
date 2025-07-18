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
use crate::data::{FiltersOptionsRead, FiltersOptionsWrite, WriteResult};
use gettextrs::gettext;
use gtk::prelude::*;

pub struct FiltersTab {
    vbox: gtk::Box,
    hide_unknown: gtk::CheckButton,
    hide_regular: gtk::CheckButton,
    hide_directory: gtk::CheckButton,
    hide_special: gtk::CheckButton,
    hide_shortcut: gtk::CheckButton,
    hide_mountable: gtk::CheckButton,
    hide_virtual: gtk::CheckButton,
    hide_volatile: gtk::CheckButton,
    hide_hidden: gtk::CheckButton,
    hide_backup: gtk::CheckButton,
    hide_symlink: gtk::CheckButton,
    backup_pattern: gtk::Entry,
}

impl FiltersTab {
    pub fn new() -> FiltersTab {
        let filters = FiltersTab {
            vbox: gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(6)
                .margin_top(6)
                .margin_bottom(6)
                .margin_start(6)
                .margin_end(6)
                .build(),
            hide_unknown: gtk::CheckButton::builder()
                .label(gettext("Unknown"))
                .build(),
            hide_regular: gtk::CheckButton::builder()
                .label(gettext("Regular files"))
                .build(),
            hide_directory: gtk::CheckButton::builder()
                .label(gettext("Directories"))
                .build(),
            hide_special: gtk::CheckButton::builder()
                .label(gettext("Socket, fifo, block, or character devices"))
                .build(),
            hide_shortcut: gtk::CheckButton::builder()
                .label(gettext("Shortcuts (Windows systems)"))
                .build(),
            hide_mountable: gtk::CheckButton::builder()
                .label(gettext("Mountable locations"))
                .build(),
            hide_virtual: gtk::CheckButton::builder()
                .label(gettext("Virtual files"))
                .build(),
            hide_volatile: gtk::CheckButton::builder()
                .label(gettext("Volatile files"))
                .build(),
            hide_hidden: gtk::CheckButton::builder()
                .label(gettext("Hidden files"))
                .build(),
            hide_backup: gtk::CheckButton::builder()
                .label(gettext("Backup files"))
                .build(),
            hide_symlink: gtk::CheckButton::builder()
                .label(gettext("Symlinks"))
                .build(),
            backup_pattern: gtk::Entry::builder().sensitive(false).build(),
        };

        let c = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        filters
            .vbox
            .append(&create_category(&gettext("Filetypes to hide"), &c));
        c.append(&filters.hide_unknown);
        c.append(&filters.hide_regular);
        c.append(&filters.hide_directory);
        c.append(&filters.hide_special);
        c.append(&filters.hide_shortcut);
        c.append(&filters.hide_mountable);
        c.append(&filters.hide_virtual);
        c.append(&filters.hide_volatile);

        let c = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        filters
            .vbox
            .append(&create_category(&gettext("Also hide"), &c));
        c.append(&filters.hide_hidden);
        c.append(&filters.hide_backup);
        c.append(&filters.hide_symlink);

        let c = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        filters
            .vbox
            .append(&create_category(&gettext("Backup files"), &c));
        c.append(&filters.backup_pattern);

        filters
            .hide_backup
            .bind_property("active", &filters.backup_pattern, "sensitive")
            .sync_create()
            .build();

        filters
    }

    pub fn widget(&self) -> gtk::Widget {
        self.vbox.clone().upcast()
    }

    pub fn read(&self, options: &dyn FiltersOptionsRead) {
        self.hide_unknown.set_active(options.hide_unknown());
        self.hide_regular.set_active(options.hide_regular());
        self.hide_directory.set_active(options.hide_directory());
        self.hide_special.set_active(options.hide_special());
        self.hide_shortcut.set_active(options.hide_shortcut());
        self.hide_mountable.set_active(options.hide_mountable());
        self.hide_virtual.set_active(options.hide_virtual());
        self.hide_volatile.set_active(options.hide_volatile());
        self.hide_hidden.set_active(options.hide_hidden());
        self.hide_backup.set_active(options.hide_backup());
        self.hide_symlink.set_active(options.hide_symlink());
        self.backup_pattern.set_text(&options.backup_pattern());
    }

    pub fn write(&self, options: &dyn FiltersOptionsWrite) -> WriteResult {
        options.set_hide_unknown(self.hide_unknown.is_active())?;
        options.set_hide_regular(self.hide_regular.is_active())?;
        options.set_hide_directory(self.hide_directory.is_active())?;
        options.set_hide_special(self.hide_special.is_active())?;
        options.set_hide_shortcut(self.hide_shortcut.is_active())?;
        options.set_hide_mountable(self.hide_mountable.is_active())?;
        options.set_hide_virtual(self.hide_virtual.is_active())?;
        options.set_hide_volatile(self.hide_volatile.is_active())?;
        options.set_hide_hidden(self.hide_hidden.is_active())?;
        options.set_hide_backup(self.hide_backup.is_active())?;
        options.set_hide_symlink(self.hide_symlink.is_active())?;
        options.set_backup_pattern(&self.backup_pattern.text())?;
        Ok(())
    }
}
