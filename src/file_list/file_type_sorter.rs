// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::item::FileListItem;
use crate::file::File;
use gtk::prelude::*;
use std::cmp;

pub struct FileTypeSorter;

impl FileTypeSorter {
    pub fn get() -> impl IsA<gtk::Sorter> {
        gtk::CustomSorter::new(|obj1, obj2| {
            let (file1, file2) = match (
                obj1.downcast_ref::<FileListItem>(),
                obj2.downcast_ref::<FileListItem>(),
            ) {
                (None, None) => return gtk::Ordering::Equal,
                (None, Some(_)) => return gtk::Ordering::Larger,
                (Some(_), None) => return gtk::Ordering::Smaller,
                (Some(item1), Some(item2)) => (item1.file(), item2.file()),
            };

            let key1 = file_type_key(&file1);
            let key2 = file_type_key(&file2);

            key1.cmp(&key2).into()
        })
    }
}

fn file_type_key(file: &File) -> impl cmp::Ord {
    if file.is_dotdot() {
        0
    } else if file.is_directory() {
        1
    } else {
        2
    }
}
