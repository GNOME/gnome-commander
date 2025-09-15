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

use gettextrs::gettext;
use gtk::{glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;

    pub struct SearchProgressDialog {
        pub label: gtk::Label,
        pub progress_bar: gtk::ProgressBar,
        pub stop_button: gtk::Button,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SearchProgressDialog {
        const NAME: &'static str = "GnomeCmdViewerSearchProgressDialog";
        type Type = super::SearchProgressDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            Self {
                label: gtk::Label::builder().build(),
                progress_bar: gtk::ProgressBar::builder()
                    .text("0.0")
                    .fraction(0.0)
                    .build(),
                stop_button: gtk::Button::builder()
                    .label(gettext("_Stop"))
                    .use_underline(true)
                    .hexpand(true)
                    .halign(gtk::Align::Center)
                    .build(),
            }
        }
    }

    impl ObjectImpl for SearchProgressDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dlg = self.obj();
            dlg.set_title(Some(&gettext("Searching…")));
            dlg.set_modal(true);
            dlg.set_resizable(false);

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .row_spacing(6)
                .column_spacing(12)
                .build();
            dlg.set_child(Some(&grid));

            grid.attach(&self.label, 0, 0, 1, 1);
            grid.attach(&self.progress_bar, 0, 1, 1, 1);

            grid.attach(&self.stop_button, 0, 2, 1, 1);
            self.stop_button.connect_clicked(glib::clone!(
                #[weak]
                dlg,
                move |_| dlg.close()
            ));
        }
    }

    impl WidgetImpl for SearchProgressDialog {}
    impl WindowImpl for SearchProgressDialog {}
}

glib::wrapper! {
    pub struct SearchProgressDialog(ObjectSubclass<imp::SearchProgressDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Native, gtk::Root;
}

impl SearchProgressDialog {
    pub fn new(parent: &gtk::Window, searching_text: &str) -> Self {
        let this: Self = glib::Object::builder().build();
        this.set_transient_for(Some(parent));
        this.imp()
            .label
            .set_label(&gettext("Searching for “{}”").replace("{}", searching_text));
        this
    }

    pub fn connect_stop<F: Fn() + 'static>(&self, f: F) -> glib::SignalHandlerId {
        self.imp().stop_button.connect_clicked(move |_| f())
    }

    pub fn set_progress(&self, progress: u32) {
        let value: f64 = progress.clamp(0, 1000).into();
        let progress_bar = &self.imp().progress_bar;
        progress_bar.set_text(Some(&format!("{:.1}%", value / 10.0)));
        progress_bar.set_fraction(value / 1000.0);
    }
}
