// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

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

            for (y, check_category) in check_categories.iter().enumerate() {
                let label = gtk::Label::builder()
                    .label(check_category)
                    .halign(gtk::Align::Start)
                    .build();
                grid_attach(&*this, &label, 0, y as i32, 1, 1);
                for (x, cb) in self.check_boxes[y].iter().enumerate() {
                    cb.update_relation(&[gtk::accessible::Relation::DescribedBy(&[
                        label.upcast_ref()
                    ])]);
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
                    .mnemonic_widget(&self.text_view)
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
                    .mnemonic_widget(&self.number_view)
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

        for (y, cb_group) in self.imp().check_boxes.iter().enumerate() {
            for (x, cb) in cb_group.iter().enumerate() {
                if cb.is_active() {
                    permissions |= PERMISSION_MASKS[y][x];
                }
            }
        }
        permissions
    }

    pub fn set_permissions(&self, permissions: u32) {
        for (y, cb_group) in self.imp().check_boxes.iter().enumerate() {
            for (x, cb) in cb_group.iter().enumerate() {
                cb.set_active((permissions & PERMISSION_MASKS[y][x]) != 0);
            }
        }
    }
}
