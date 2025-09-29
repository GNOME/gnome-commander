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

use super::item::FileListItem;
use crate::file::File;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::cmp;

mod imp {
    use super::*;
    use crate::options::options::GeneralOptions;
    use std::cell::{Cell, OnceCell};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::FileAttrSorter)]
    pub struct FileAttrSorter {
        pub compare:
            OnceCell<Box<dyn Fn(&File, &File, &super::FileAttrSorter) -> gtk::Ordering + 'static>>,
        #[property(get, set = Self::set_case_sensitive)]
        case_sensitive: Cell<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileAttrSorter {
        const NAME: &'static str = "GnomeCmdFileAttrSorter";
        type Type = super::FileAttrSorter;
        type ParentType = gtk::Sorter;
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileAttrSorter {
        fn constructed(&self) {
            self.parent_constructed();

            let general_options = GeneralOptions::new();
            general_options
                .case_sensitive
                .bind(&*self.obj(), "case-sensitive")
                .build();
        }
    }

    impl SorterImpl for FileAttrSorter {
        fn compare(&self, obj1: &glib::Object, obj2: &glib::Object) -> gtk::Ordering {
            match (
                obj1.downcast_ref::<FileListItem>(),
                obj2.downcast_ref::<FileListItem>(),
            ) {
                (None, None) => gtk::Ordering::Equal,
                (None, Some(_)) => gtk::Ordering::Larger,
                (Some(_), None) => gtk::Ordering::Smaller,
                (Some(item1), Some(item2)) => {
                    (self.compare.get().unwrap())(&item1.file(), &item2.file(), &*self.obj())
                }
            }
        }
    }

    impl FileAttrSorter {
        fn set_case_sensitive(&self, value: bool) {
            self.case_sensitive.set(value);
            self.obj().changed(gtk::SorterChange::Different);
        }
    }
}

glib::wrapper! {
    pub struct FileAttrSorter(ObjectSubclass<imp::FileAttrSorter>)
        @extends gtk::Sorter;
}

impl FileAttrSorter {
    fn by_key<F, K>(key_fn: F) -> Self
    where
        F: Fn(&Self, &File) -> K + 'static,
        K: cmp::Ord,
    {
        let this: Self = glib::Object::builder().build();
        this.imp()
            .compare
            .set(Box::new(move |file1, file2, sorter| {
                let file_key1 = (key_fn)(sorter, &file1);
                let file_key2 = (key_fn)(sorter, &file2);
                file_key1.cmp(&file_key2).into()
            }))
            .ok()
            .unwrap();
        this
    }

    pub fn by_name() -> Self {
        Self::by_key(|this, file| collate_key(&file.get_name(), this.case_sensitive()))
    }

    pub fn by_ext() -> Self {
        Self::by_key(|this, file| {
            (
                file.extension()
                    .as_ref()
                    .and_then(|e| e.to_str())
                    .map(|e| collate_key(&e, this.case_sensitive())),
                collate_key(&file.get_name(), this.case_sensitive()),
            )
        })
    }

    pub fn by_dir() -> Self {
        Self::by_key(|this, file| {
            (
                file.get_dirname()
                    .as_ref()
                    .and_then(|d| d.to_str())
                    .map(|d| collate_key(&d, this.case_sensitive())),
                collate_key(&file.get_name(), this.case_sensitive()),
            )
        })
    }

    pub fn by_size() -> Self {
        Self::by_key(|this, file| {
            (
                file.file_info().size(),
                collate_key(&file.get_name(), this.case_sensitive()),
            )
        })
    }

    pub fn by_perm() -> Self {
        Self::by_key(|this, file| {
            (
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE),
                collate_key(&file.get_name(), this.case_sensitive()),
            )
        })
    }

    pub fn by_date() -> Self {
        Self::by_key(|this, file| {
            (
                file.file_info().modification_date_time(),
                collate_key(&file.get_name(), this.case_sensitive()),
            )
        })
    }

    pub fn by_owner() -> Self {
        Self::by_key(|this, file| {
            (
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_UID),
                file.file_info()
                    .attribute_string(gio::FILE_ATTRIBUTE_OWNER_USER),
                collate_key(&file.get_name(), this.case_sensitive()),
            )
        })
    }

    pub fn by_group() -> Self {
        Self::by_key(|this, file| {
            (
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_GID),
                file.file_info()
                    .attribute_string(gio::FILE_ATTRIBUTE_OWNER_GROUP),
                collate_key(&file.get_name(), this.case_sensitive()),
            )
        })
    }
}

fn collate_key(name: &str, case_sensitive: bool) -> glib::FilenameCollationKey {
    if case_sensitive {
        glib::FilenameCollationKey::from(name)
    } else {
        glib::FilenameCollationKey::from(glib::casefold(name))
    }
}
