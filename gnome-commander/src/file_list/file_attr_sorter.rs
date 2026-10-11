// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::item::FileListItem;
use crate::file::{File, FileOps};
use gtk::{glib, prelude::*, subclass::prelude::*};
use std::cmp;

mod imp {
    use super::*;
    use std::cell::OnceCell;

    type CompareFunc = Box<dyn Fn(&File, &File) -> gtk::Ordering + 'static>;

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::FileAttrSorter)]
    pub struct FileAttrSorter {
        pub compare: OnceCell<CompareFunc>,
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
                    let file1 = item1.file();
                    let file2 = item2.file();
                    let result = (self.compare.get().unwrap())(&file1, &file2);
                    if result != gtk::Ordering::Equal {
                        result
                    } else {
                        glib::FilenameCollationKey::from(&file1.name())
                            .cmp(&glib::FilenameCollationKey::from(&file2.name()))
                            .into()
                    }
                }
            }
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
        F: Fn(&File) -> K + 'static,
        K: cmp::Ord,
    {
        let this: Self = glib::Object::builder().build();
        this.imp()
            .compare
            .set(Box::new(move |file1, file2| {
                let file_key1 = (key_fn)(file1);
                let file_key2 = (key_fn)(file2);
                file_key1.cmp(&file_key2).into()
            }))
            .ok()
            .unwrap();
        this
    }

    pub fn by_name() -> Self {
        Self::by_key(|_| ())
    }

    pub fn by_ext() -> Self {
        Self::by_key(|file| {
            file.extension()
                .as_ref()
                .and_then(|e| e.to_str())
                .map(glib::FilenameCollationKey::from)
        })
    }

    pub fn by_dir() -> Self {
        Self::by_key(|file| {
            file.parent_path()
                .as_ref()
                .and_then(|d| d.to_str())
                .map(glib::FilenameCollationKey::from)
        })
    }

    pub fn by_size() -> Self {
        Self::by_key(|file| file.size())
    }

    pub fn by_perm() -> Self {
        Self::by_key(|file| file.permissions())
    }

    pub fn by_date() -> Self {
        Self::by_key(|file| file.modification_date())
    }

    pub fn by_owner() -> Self {
        Self::by_key(|file| (file.uid(), file.owner()))
    }

    pub fn by_group() -> Self {
        Self::by_key(|file| (file.gid(), file.group()))
    }
}
