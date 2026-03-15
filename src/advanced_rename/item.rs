// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{file::File, tags::file_metadata::FileMetadata};
use gtk::glib::{prelude::*, subclass::prelude::*};
use std::rc::Rc;

mod imp {
    use super::*;
    use std::cell::{OnceCell, RefCell};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::Item)]
    pub struct Item {
        #[property(get, construct_only)]
        pub file: OnceCell<File>,

        #[property(get, set)]
        pub new_name: RefCell<String>,

        #[property(get, set, nullable)]
        pub error: RefCell<Option<String>>,

        pub metadata: RefCell<Rc<FileMetadata>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Item {
        const NAME: &'static str = "GnomeCmdAdvancedRenameItem";
        type Type = super::Item;
    }

    #[glib::derived_properties]
    impl ObjectImpl for Item {}
}

glib::wrapper! {
    pub struct Item(ObjectSubclass<imp::Item>);
}

impl Item {
    pub fn new(file: &File) -> Self {
        glib::Object::builder().property("file", file).build()
    }

    pub fn metadata(&self) -> Rc<FileMetadata> {
        self.imp().metadata.borrow().clone()
    }

    pub fn set_metadata(&self, metadata: FileMetadata) {
        self.imp().metadata.replace(Rc::new(metadata));
    }

    pub fn clear_error(&self) {
        self.set_error(None::<String>);
    }

    pub fn deep_copy(&self) -> Self {
        let new_item = Self::new(&self.file());
        new_item.set_new_name(self.new_name().to_string());
        new_item.set_error(self.error());
        new_item.imp().metadata.replace(self.metadata());
        new_item
    }
}
