// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::item::Item;
use gtk::{glib, prelude::*, subclass::prelude::*};
use std::cell::Ref;

mod imp {
    use super::*;
    use std::cell::RefCell;

    #[derive(Default)]
    pub struct ListCell {
        pub item: RefCell<Option<Item>>,
        pub label: gtk::Label,
        pub bindings: RefCell<Vec<glib::Binding>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ListCell {
        const NAME: &'static str = "GnomeCmdAdvRenameListCell";
        type Type = super::ListCell;
        type ParentType = gtk::Widget;
    }

    impl ObjectImpl for ListCell {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_layout_manager(Some(gtk::BinLayout::new()));
            this.add_css_class("cell");
            self.label.set_parent(&*this);
        }

        fn dispose(&self) {
            self.label.unparent();
        }
    }

    impl WidgetImpl for ListCell {}
}

glib::wrapper! {
    pub struct ListCell(ObjectSubclass<imp::ListCell>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for ListCell {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ListCell {
    pub fn set_item(&self, item: Option<Item>) {
        for binding in self.imp().bindings.borrow_mut().drain(..) {
            binding.unbind();
        }
        self.imp().item.replace(item);
    }

    pub fn add_binding(&self, binding: glib::Binding) {
        self.imp().bindings.borrow_mut().push(binding);
    }

    pub fn item(&self) -> Ref<'_, Option<Item>> {
        self.imp().item.borrow()
    }

    pub fn label(&self) -> &gtk::Label {
        &self.imp().label
    }
}
