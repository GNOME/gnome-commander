/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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

use crate::utils::PERMISSION_MASKS;
use gtk::{glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::utils::{grid_attach, permissions_to_numbers, permissions_to_text};
    use gettextrs::gettext;

    pub struct ChmodComponent {
        pub check_boxes: [[gtk::CheckButton; 3]; 3],
        pub text_view: gtk::Label,
        pub number_view: gtk::Label,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ChmodComponent {
        const NAME: &'static str = "GnomeCmdChmodComponent";
        type Type = super::ChmodComponent;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            fn row() -> [gtk::CheckButton; 3] {
                [
                    gtk::CheckButton::builder()
                        .label(gettext("Read"))
                        .hexpand(true)
                        .halign(gtk::Align::Start)
                        .build(),
                    gtk::CheckButton::builder()
                        .label(gettext("Write"))
                        .hexpand(true)
                        .halign(gtk::Align::Start)
                        .build(),
                    gtk::CheckButton::builder()
                        .label(gettext("Execute"))
                        .hexpand(true)
                        .halign(gtk::Align::Start)
                        .build(),
                ]
            }
            Self {
                check_boxes: [row(), row(), row()],
                text_view: gtk::Label::builder().halign(gtk::Align::Start).build(),
                number_view: gtk::Label::builder().halign(gtk::Align::Start).build(),
            }
        }
    }

    impl ObjectImpl for ChmodComponent {
        fn constructed(&self) {
            self.parent_constructed();
            let this = self.obj();

            let layout = gtk::GridLayout::builder()
                .column_spacing(12)
                .row_spacing(6)
                .build();
            this.set_layout_manager(Some(layout.clone()));

            let check_categories = [gettext("Owner:"), gettext("Group:"), gettext("Others:")];

            for y in 0..3 {
                grid_attach(
                    &*this,
                    &gtk::Label::builder()
                        .label(&check_categories[y])
                        .halign(gtk::Align::Start)
                        .build(),
                    0,
                    y as i32,
                    1,
                    1,
                );
                for x in 0..3 {
                    let cb = &self.check_boxes[y][x];
                    cb.connect_toggled(glib::clone!(
                        #[weak(rename_to = imp)]
                        self,
                        move |_| imp.on_check_toggled()
                    ));
                    grid_attach(&*this, cb, x as i32 + 1, y as i32, 1, 1);
                }
            }

            grid_attach(
                &*this,
                &gtk::Separator::new(gtk::Orientation::Horizontal),
                0,
                3,
                4,
                1,
            );

            grid_attach(
                &*this,
                &gtk::Label::builder()
                    .label(gettext("Text view:"))
                    .halign(gtk::Align::Start)
                    .build(),
                0,
                4,
                1,
                1,
            );
            grid_attach(&*this, &self.text_view, 1, 4, 3, 1);

            grid_attach(
                &*this,
                &gtk::Label::builder()
                    .label(gettext("Number view:"))
                    .halign(gtk::Align::Start)
                    .build(),
                0,
                5,
                1,
                1,
            );
            grid_attach(&*this, &self.number_view, 1, 5, 3, 1);
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for ChmodComponent {}

    impl ChmodComponent {
        fn on_check_toggled(&self) {
            let permissions = self.obj().permissions();
            self.text_view.set_label(&permissions_to_text(permissions));
            self.number_view
                .set_label(&permissions_to_numbers(permissions));
        }
    }
}

glib::wrapper! {
    pub struct ChmodComponent(ObjectSubclass<imp::ChmodComponent>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for ChmodComponent {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ChmodComponent {
    pub fn new(permissions: u32) -> Self {
        let this = Self::default();
        this.set_permissions(permissions);
        this
    }

    pub fn permissions(&self) -> u32 {
        let mut permissions = 0;

        for y in 0..3 {
            for x in 0..3 {
                if self.imp().check_boxes[y][x].is_active() {
                    permissions |= PERMISSION_MASKS[y][x];
                }
            }
        }
        permissions
    }

    pub fn set_permissions(&self, permissions: u32) {
        for y in 0..3 {
            for x in 0..3 {
                self.imp().check_boxes[y][x]
                    .set_active((permissions & PERMISSION_MASKS[y][x]) != 0);
            }
        }
    }
}
