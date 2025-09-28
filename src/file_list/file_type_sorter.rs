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

use crate::file::File;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::cmp;

mod imp {
    use super::*;
    use crate::{file_list::item::FileListItem, options::options::GeneralOptions};
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
    let file_info = if file.is_local() {
        Some(file.file_info())
    } else {
        match file
            .file()
            .query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)
        {
            Ok(info) => Some(info),
            Err(error) => {
                eprintln!(
                    "Could not retrieve file information for {}, error: {}",
                    file.get_name(),
                    error.message()
                );
                None
            }
        }
    };
    let file_type = file_info.map(|i| i.file_type());
    match file_type {
        Some(gio::FileType::Mountable) => 1,
        Some(gio::FileType::Shortcut) => 2,
        Some(gio::FileType::Special) => 3,
        Some(gio::FileType::SymbolicLink) if symbolic_links_as_regular_files => 6,
        Some(gio::FileType::SymbolicLink) => 4,
        Some(gio::FileType::Directory) => 5,
        Some(gio::FileType::Regular) => 6,
        _ => 8,
    }
}
