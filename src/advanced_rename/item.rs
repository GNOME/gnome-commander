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
