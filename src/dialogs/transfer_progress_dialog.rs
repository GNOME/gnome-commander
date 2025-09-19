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

use crate::{types::SizeDisplayMode, utils::size_to_string};
use gettextrs::{gettext, pgettext};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use std::cell::Cell;

    pub struct TransferProgressWindow {
        pub message_label: gtk::Label,
        pub file_progress_label: gtk::Label,
        pub total_progress: gtk::ProgressBar,
        pub file_progress: gtk::ProgressBar,
        pub cancellable: gio::Cancellable,
        pub size_display_mode: Cell<SizeDisplayMode>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for TransferProgressWindow {
        const NAME: &'static str = "GnomeCmdTransferProgressWindow";
        type Type = super::TransferProgressWindow;
        type ParentType = gtk::Window;

        fn new() -> Self {
            Self {
                message_label: gtk::Label::builder().build(),
                file_progress_label: gtk::Label::builder().build(),
                total_progress: gtk::ProgressBar::builder().show_text(true).build(),
                file_progress: gtk::ProgressBar::builder().show_text(true).build(),
                cancellable: gio::Cancellable::new(),
                size_display_mode: Cell::new(SizeDisplayMode::Plain),
            }
        }
    }

    impl ObjectImpl for TransferProgressWindow {
        fn constructed(&self) {
            self.parent_constructed();

            let win = self.obj();

            win.set_title(Some(&gettext("Progress")));
            win.set_resizable(false);
            win.set_size_request(300, -1);

            let vbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(6)
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .build();
            win.set_child(Some(&vbox));

            vbox.append(&self.message_label);
            vbox.append(&self.file_progress_label);
            vbox.append(&self.total_progress);
            vbox.append(&self.file_progress);

            let bbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .build();
            vbox.append(&bbox);

            let button = gtk::Button::builder()
                .label(gettext("_Cancel"))
                .use_underline(true)
                .hexpand(true)
                .halign(gtk::Align::Center)
                .build();
            bbox.append(&button);

            win.set_default_widget(Some(&button));

            button.connect_clicked(glib::clone!(
                #[weak]
                win,
                move |_| {
                    win.set_action(&gettext("stopping…"));
                    win.set_sensitive(false);
                    win.imp().cancellable.cancel();
                }
            ));
        }
    }

    impl WidgetImpl for TransferProgressWindow {}
    impl WindowImpl for TransferProgressWindow {}
}

glib::wrapper! {
    pub struct TransferProgressWindow(ObjectSubclass<imp::TransferProgressWindow>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl TransferProgressWindow {
    pub fn new(no_of_files: u64, size_display_mode: SizeDisplayMode) -> Self {
        let win: Self = glib::Object::builder().build();
        win.imp().size_display_mode.set(size_display_mode);
        if no_of_files <= 1 {
            win.imp().file_progress.set_visible(false);
        }
        win
    }

    pub fn set_total_progress(
        &self,
        file_bytes_copied: u64,
        file_size: u64,
        bytes_copied: u64,
        bytes_total: u64,
    ) {
        let total_prog = if bytes_total > 0 {
            (bytes_copied as f64) / (bytes_total as f64)
        } else {
            -1.0_f64
        };
        self.imp().total_progress.set_fraction(total_prog);

        if self.imp().file_progress.is_visible() && file_size > 0 {
            let file_prog = (file_bytes_copied as f64) / (file_size as f64);
            self.imp().file_progress.set_fraction(file_prog);
        }

        let size_display_mode = self.imp().size_display_mode.get();
        let bytes_total_str = size_to_string(bytes_total, size_display_mode);
        let bytes_copied_str = size_to_string(bytes_copied, size_display_mode);
        self.imp().file_progress_label.set_text(
            &pgettext("size copied", "{copied} of {total} copied")
                .replace("{copied}", &bytes_copied_str)
                .replace("{total}", &bytes_total_str),
        );
        self.set_title(Some(
            // xgettext:no-c-format
            &pgettext("percentage copied", "{}% copied")
                .replace("{}", &format!("{:.0}", total_prog * 100.0)),
        ));
    }

    pub fn set_msg(&self, msg: &str) {
        self.imp().message_label.set_text(msg);
    }

    pub fn set_action(&self, title: &str) {
        self.set_title(Some(title));
    }

    pub fn cancellable(&self) -> &gio::Cancellable {
        &self.imp().cancellable
    }
}
