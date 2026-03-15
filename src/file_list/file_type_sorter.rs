// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::file::File;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::cmp;

mod imp {
    use super::*;
    use crate::{file_list::item::FileListItem, options::GeneralOptions};
    use std::cell::Cell;

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::FileTypeSorter)]
    pub struct FileTypeSorter {
        #[property(get, set = Self::set_symbolic_links_as_regular_files)]
        symbolic_links_as_regular_files: Cell<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileTypeSorter {
        const NAME: &'static str = "GnomeCmdFileTypeSorter";
        type Type = super::FileTypeSorter;
        type ParentType = gtk::Sorter;
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileTypeSorter {
        fn constructed(&self) {
            self.parent_constructed();

            let general_options = GeneralOptions::new();
            general_options
                .symbolic_links_as_regular_files
                .bind(&*self.obj(), "symbolic-links-as-regular-files")
                .build();
        }
    }

    impl SorterImpl for FileTypeSorter {
        fn compare(&self, obj1: &glib::Object, obj2: &glib::Object) -> gtk::Ordering {
            let (file1, file2) = match (
                obj1.downcast_ref::<FileListItem>(),
                obj2.downcast_ref::<FileListItem>(),
            ) {
                (None, None) => return gtk::Ordering::Equal,
                (None, Some(_)) => return gtk::Ordering::Larger,
                (Some(_), None) => return gtk::Ordering::Smaller,
                (Some(item1), Some(item2)) => (item1.file(), item2.file()),
            };

            let symbolic_links_as_regular_files = self.symbolic_links_as_regular_files.get();
            let file_type_key1 = file_type_key(&file1, symbolic_links_as_regular_files);
            let file_type_key2 = file_type_key(&file2, symbolic_links_as_regular_files);

            file_type_key1.cmp(&file_type_key2).into()
        }
    }

    impl FileTypeSorter {
        fn set_symbolic_links_as_regular_files(&self, value: bool) {
            self.symbolic_links_as_regular_files.set(value);
            self.obj().changed(gtk::SorterChange::Different);
        }
    }
}

glib::wrapper! {
    pub struct FileTypeSorter(ObjectSubclass<imp::FileTypeSorter>)
        @extends gtk::Sorter;
}

impl Default for FileTypeSorter {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

fn file_type_key(file: &File, symbolic_links_as_regular_files: bool) -> impl cmp::Ord {
    if file.is_dotdot() {
        return 0;
    }
    match file.file_type() {
        gio::FileType::Mountable => 1,
        gio::FileType::Shortcut => 2,
        gio::FileType::Special => 3,
        gio::FileType::SymbolicLink if symbolic_links_as_regular_files => 6,
        gio::FileType::SymbolicLink => 4,
        gio::FileType::Directory => 5,
        gio::FileType::Regular => 6,
        _ => 8,
    }
}
