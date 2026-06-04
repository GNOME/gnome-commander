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
    use crate::options::GeneralOptions;
    use std::cell::{Cell, OnceCell};

    type CompareFunc = Box<dyn Fn(&File, &File, bool) -> gtk::Ordering + 'static>;

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::FileAttrSorter)]
    pub struct FileAttrSorter {
        pub compare: OnceCell<CompareFunc>,
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

            GeneralOptions::instance()
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
                    let case_sensitive = self.case_sensitive.get();
                    let file1 = item1.file();
                    let file2 = item2.file();
                    let result = (self.compare.get().unwrap())(&file1, &file2, case_sensitive);
                    if result != gtk::Ordering::Equal {
                        result
                    } else {
                        collate_key(&file1.name(), case_sensitive)
                            .cmp(&collate_key(&file2.name(), case_sensitive))
                            .into()
                    }
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
        F: Fn(&File, bool) -> K + 'static,
        K: cmp::Ord,
    {
        let this: Self = glib::Object::builder().build();
        this.imp()
            .compare
            .set(Box::new(move |file1, file2, case_sensitive| {
                let file_key1 = (key_fn)(file1, case_sensitive);
                let file_key2 = (key_fn)(file2, case_sensitive);
                file_key1.cmp(&file_key2).into()
            }))
            .ok()
            .unwrap();
        this
    }

    pub fn by_name() -> Self {
        Self::by_key(|_, _| ())
    }

    pub fn by_ext() -> Self {
        Self::by_key(|file, case_sensitive| {
            file.extension()
                .as_ref()
                .and_then(|e| e.to_str())
                .map(|e| collate_key(e, case_sensitive))
        })
    }

    pub fn by_dir() -> Self {
        Self::by_key(|file, case_sensitive| {
            file.parent_path()
                .as_ref()
                .and_then(|d| d.to_str())
                .map(|d| collate_key(d, case_sensitive))
        })
    }

    pub fn by_size() -> Self {
        Self::by_key(|file, _| file.size())
    }

    pub fn by_perm() -> Self {
        Self::by_key(|file, _| file.permissions())
    }

    pub fn by_date() -> Self {
        Self::by_key(|file, _| file.modification_date())
    }

    pub fn by_owner() -> Self {
        Self::by_key(|file, _| (file.uid(), file.owner()))
    }

    pub fn by_group() -> Self {
        Self::by_key(|file, _| (file.gid(), file.group()))
    }
}

fn collate_key(name: &str, case_sensitive: bool) -> glib::FilenameCollationKey {
    if case_sensitive {
        glib::FilenameCollationKey::from(name)
    } else {
        glib::FilenameCollationKey::from(glib::casefold(name))
    }
}
