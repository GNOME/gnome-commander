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

use crate::{
    dialogs::options::common::create_category,
    options::{
        options::{ConfirmOptions, DeleteDefault},
        types::WriteResult,
    },
    types::{ConfirmOverwriteMode, DndMode},
};
use gettextrs::gettext;
use gtk::prelude::*;

pub struct CondifrmationTab {
    vbox: gtk::Box,

    confirm_delete_check: gtk::CheckButton,
    delete_default_check: gtk::CheckButton,

    copy_overwrite_query: gtk::CheckButton,
    copy_rename_all: gtk::CheckButton,
    copy_overwrite_skip_all: gtk::CheckButton,
    copy_overwrite_silently: gtk::CheckButton,

    move_overwrite_query: gtk::CheckButton,
    move_rename_all: gtk::CheckButton,
    move_overwrite_skip_all: gtk::CheckButton,
    move_overwrite_silently: gtk::CheckButton,

    dnd_query: gtk::CheckButton,
    dnd_copy: gtk::CheckButton,
    dnd_move: gtk::CheckButton,
}

impl CondifrmationTab {
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

        // Delete options
        let delete_category_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        let confirm_delete_check = gtk::CheckButton::builder()
            .label(gettext("Confirm before delete"))
            .build();
        delete_category_box.append(&confirm_delete_check);
        let delete_default_check = gtk::CheckButton::builder()
            .label(gettext("Confirm defaults to OK"))
            .build();
        delete_category_box.append(&delete_default_check);
        confirm_delete_check
            .bind_property("active", &delete_default_check, "sensitive")
            .sync_create()
            .build();
        vbox.append(&create_category(&gettext("Delete"), &delete_category_box));

        // Copy overwrite options
        let copy_overwrite_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        let copy_overwrite_query = gtk::CheckButton::builder()
            .label(gettext("Query first"))
            .build();
        copy_overwrite_box.append(&copy_overwrite_query);
        let copy_rename_all = gtk::CheckButton::builder()
            .label(gettext("Rename"))
            .group(&copy_overwrite_query)
            .build();
        copy_overwrite_box.append(&copy_rename_all);
        let copy_overwrite_skip_all = gtk::CheckButton::builder()
            .label(gettext("Skip"))
            .group(&copy_overwrite_query)
            .build();
        copy_overwrite_box.append(&copy_overwrite_skip_all);
        let copy_overwrite_silently = gtk::CheckButton::builder()
            .label(gettext("Overwrite silently"))
            .group(&copy_overwrite_query)
            .build();
        copy_overwrite_box.append(&copy_overwrite_silently);
        vbox.append(&create_category(
            &gettext("Preselected overwrite action in copy dialog"),
            &copy_overwrite_box,
        ));

        // Move overwrite options
        let move_overwrite_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        let move_overwrite_query = gtk::CheckButton::builder()
            .label(gettext("Query first"))
            .build();
        move_overwrite_box.append(&move_overwrite_query);
        let move_rename_all = gtk::CheckButton::builder()
            .label(gettext("Rename"))
            .group(&move_overwrite_query)
            .build();
        move_overwrite_box.append(&move_rename_all);
        let move_overwrite_skip_all = gtk::CheckButton::builder()
            .label(gettext("Skip"))
            .group(&move_overwrite_query)
            .build();
        move_overwrite_box.append(&move_overwrite_skip_all);
        let move_overwrite_silently = gtk::CheckButton::builder()
            .label(gettext("Overwrite silently"))
            .group(&move_overwrite_query)
            .build();
        move_overwrite_box.append(&move_overwrite_silently);
        vbox.append(&create_category(
            &gettext("Preselected overwrite action in move dialog"),
            &move_overwrite_box,
        ));

        // Drag and Drop options
        let dnd_box = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        let dnd_default = gtk::CheckButton::builder()
            .label(gettext("Confirm mouse operation"))
            .build();
        dnd_box.append(&dnd_default);
        let dnd_copy = gtk::CheckButton::builder()
            .label(gettext("Copy"))
            .group(&dnd_default)
            .build();
        dnd_box.append(&dnd_copy);
        let dnd_move = gtk::CheckButton::builder()
            .label(gettext("Move"))
            .group(&dnd_default)
            .build();
        dnd_box.append(&dnd_move);
        vbox.append(&create_category(
            &gettext("Default Drag and Drop Action"),
            &dnd_box,
        ));

        Self {
            vbox,
            confirm_delete_check,
            delete_default_check,
            copy_overwrite_query,
            copy_rename_all,
            copy_overwrite_skip_all,
            copy_overwrite_silently,
            move_overwrite_query,
            move_rename_all,
            move_overwrite_skip_all,
            move_overwrite_silently,
            dnd_query: dnd_default,
            dnd_copy,
            dnd_move,
        }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.vbox.clone().upcast()
    }

    pub fn read(&self, options: &ConfirmOptions) {
        self.confirm_delete_check
            .set_active(options.confirm_delete.get());
        self.delete_default_check
            .set_active(options.confirm_delete_default.get() != DeleteDefault::Cancel);

        match options.confirm_copy_overwrite.get() {
            ConfirmOverwriteMode::Query => &self.copy_overwrite_query,
            ConfirmOverwriteMode::RenameAll => &self.copy_rename_all,
            ConfirmOverwriteMode::SkipAll => &self.copy_overwrite_skip_all,
            ConfirmOverwriteMode::Silently => &self.copy_overwrite_silently,
        }
        .set_active(true);

        match options.confirm_move_overwrite.get() {
            ConfirmOverwriteMode::Query => &self.move_overwrite_query,
            ConfirmOverwriteMode::RenameAll => &self.move_rename_all,
            ConfirmOverwriteMode::SkipAll => &self.move_overwrite_skip_all,
            ConfirmOverwriteMode::Silently => &self.move_overwrite_silently,
        }
        .set_active(true);

        match options.dnd_mode.get() {
            DndMode::Query => &self.dnd_query,
            DndMode::Copy => &self.dnd_copy,
            DndMode::Move => &self.dnd_move,
        }
        .set_active(true);
    }

    pub fn write(&self, options: &ConfirmOptions) -> WriteResult {
        options
            .confirm_delete
            .set(self.confirm_delete_check.is_active())?;
        options
            .confirm_delete_default
            .set(if self.delete_default_check.is_active() {
                DeleteDefault::Delete
            } else {
                DeleteDefault::Cancel
            })?;

        options
            .confirm_copy_overwrite
            .set(if self.copy_overwrite_query.is_active() {
                ConfirmOverwriteMode::Query
            } else if self.copy_rename_all.is_active() {
                ConfirmOverwriteMode::RenameAll
            } else if self.copy_overwrite_skip_all.is_active() {
                ConfirmOverwriteMode::SkipAll
            } else if self.copy_overwrite_silently.is_active() {
                ConfirmOverwriteMode::Silently
            } else {
                ConfirmOverwriteMode::Query
            })?;

        options
            .confirm_move_overwrite
            .set(if self.move_overwrite_query.is_active() {
                ConfirmOverwriteMode::Query
            } else if self.move_rename_all.is_active() {
                ConfirmOverwriteMode::RenameAll
            } else if self.move_overwrite_skip_all.is_active() {
                ConfirmOverwriteMode::SkipAll
            } else if self.move_overwrite_silently.is_active() {
                ConfirmOverwriteMode::Silently
            } else {
                ConfirmOverwriteMode::Query
            })?;

        options.dnd_mode.set(if self.dnd_query.is_active() {
            DndMode::Query
        } else if self.dnd_copy.is_active() {
            DndMode::Copy
        } else if self.dnd_move.is_active() {
            DndMode::Move
        } else {
            DndMode::Query
        })?;

        Ok(())
    }
}
