// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::common::create_category;
use crate::options::{FiltersOptions, types::WriteResult};
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

    pub fn read(&self, options: &FiltersOptions) {
        self.hide_unknown.set_active(options.hide_unknown.get());
        self.hide_regular.set_active(options.hide_regular.get());
        self.hide_directory.set_active(options.hide_directory.get());
        self.hide_special.set_active(options.hide_special.get());
        self.hide_shortcut.set_active(options.hide_shortcut.get());
        self.hide_mountable.set_active(options.hide_mountable.get());
        self.hide_virtual.set_active(options.hide_virtual.get());
        self.hide_volatile.set_active(options.hide_volatile.get());
        self.hide_hidden.set_active(options.hide_hidden.get());
        self.hide_backup.set_active(options.hide_backup.get());
        self.hide_symlink.set_active(options.hide_symlink.get());
        self.backup_pattern.set_text(&options.backup_pattern.get());
    }

    pub fn write(&self, options: &FiltersOptions) -> WriteResult {
        options.hide_unknown.set(self.hide_unknown.is_active())?;
        options.hide_regular.set(self.hide_regular.is_active())?;
        options
            .hide_directory
            .set(self.hide_directory.is_active())?;
        options.hide_special.set(self.hide_special.is_active())?;
        options.hide_shortcut.set(self.hide_shortcut.is_active())?;
        options
            .hide_mountable
            .set(self.hide_mountable.is_active())?;
        options.hide_virtual.set(self.hide_virtual.is_active())?;
        options.hide_volatile.set(self.hide_volatile.is_active())?;
        options.hide_hidden.set(self.hide_hidden.is_active())?;
        options.hide_backup.set(self.hide_backup.is_active())?;
        options.hide_symlink.set(self.hide_symlink.is_active())?;
        options.backup_pattern.set(self.backup_pattern.text())?;
        Ok(())
    }
}
