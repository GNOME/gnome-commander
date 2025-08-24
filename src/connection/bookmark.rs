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

use gtk::glib::{prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use std::cell::OnceCell;

    #[derive(glib::Properties, Default)]
    #[properties(wrapper_type = super::Bookmark)]
    pub struct Bookmark {
        #[property(get, construct_only)]
        name: OnceCell<String>,
        #[property(get, construct_only)]
        path: OnceCell<String>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Bookmark {
        const NAME: &'static str = "GnomeCmdBookmark";
        type Type = super::Bookmark;
    }

    #[glib::derived_properties]
    impl ObjectImpl for Bookmark {}
}

glib::wrapper! {
    pub struct Bookmark(ObjectSubclass<imp::Bookmark>);
}

impl Bookmark {
    pub fn new(name: &str, path: &str) -> Self {
        glib::Object::builder()
            .property("name", name)
            .property("path", path)
            .build()
    }
}

#[derive(glib::Variant)]
pub struct BookmarkGoToVariant {
    pub connection_uuid: String,
    pub bookmark_name: String,
}
