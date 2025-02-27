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

use crate::{file::File, libgcmd::file_descriptor::FileDescriptorExt};
use gtk::{
    ffi::{GtkSortType, GtkSorter},
    gio,
    glib::{
        ffi::gboolean,
        translate::{FromGlib, ToGlibPtr},
    },
    prelude::*,
};
use std::cmp;

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

fn sorter<K, O>(
    symbolic_links_as_regular_files: bool,
    key: K,
    sort_type: gtk::SortType,
) -> gtk::Sorter
where
    K: Fn(&File) -> O + 'static,
    O: cmp::Ord + 'static,
{
    gtk::CustomSorter::new(move |obj1: &glib::Object, obj2: &glib::Object| {
        let (file1, file2) = match (obj1.downcast_ref::<File>(), obj2.downcast_ref::<File>()) {
            (None, None) => return gtk::Ordering::Equal,
            (None, Some(_)) => return gtk::Ordering::Larger,
            (Some(_), None) => return gtk::Ordering::Smaller,
            (Some(file1), Some(file2)) => (file1, file2),
        };

        let file_type_key1 = file_type_key(file1, symbolic_links_as_regular_files);
        let file_key1 = (key)(file1);

        let file_type_key2 = file_type_key(file2, symbolic_links_as_regular_files);
        let file_key2 = (key)(file2);

        match sort_type {
            gtk::SortType::Ascending => {
                let key1 = (file_type_key1, file_key1);
                let key2 = (file_type_key2, file_key2);
                key1.cmp(&key2).into()
            }
            gtk::SortType::Descending => {
                let key1 = (file_type_key1, cmp::Reverse(file_key1));
                let key2 = (file_type_key2, cmp::Reverse(file_key2));
                key1.cmp(&key2).into()
            }
            _ => gtk::Ordering::Equal,
        }
    })
    .upcast()
}

fn collate_key_for_filename(file: &File, case_sensitive: bool) -> glib::FilenameCollationKey {
    let name = file.get_name();
    if case_sensitive {
        glib::FilenameCollationKey::from(name)
    } else {
        glib::FilenameCollationKey::from(glib::casefold(&name))
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_name(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| collate_key_for_filename(file, case_sensitive != 0),
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_ext(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| {
            (
                file.extension(),
                collate_key_for_filename(file, case_sensitive != 0),
            )
        },
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_dir(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| {
            (
                file.get_dirname(),
                collate_key_for_filename(file, case_sensitive != 0),
            )
        },
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_size(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| {
            (
                file.file_info().size(),
                collate_key_for_filename(file, case_sensitive != 0),
            )
        },
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_perm(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| {
            (
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE),
                collate_key_for_filename(file, case_sensitive != 0),
            )
        },
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_date(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| {
            (
                file.file_info().modification_date_time(),
                collate_key_for_filename(file, case_sensitive != 0),
            )
        },
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_owner(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| {
            (
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_UID),
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_OWNER_USER),
                collate_key_for_filename(file, case_sensitive != 0),
            )
        },
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_sort_by_group(
    case_sensitive: gboolean,
    symbolic_links_as_regular_files: gboolean,
    sort_type: GtkSortType,
) -> *mut GtkSorter {
    sorter(
        symbolic_links_as_regular_files != 0,
        move |file| {
            (
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_GID),
                file.file_info()
                    .attribute_uint32(gio::FILE_ATTRIBUTE_OWNER_GROUP),
                collate_key_for_filename(file, case_sensitive != 0),
            )
        },
        unsafe { gtk::SortType::from_glib(sort_type) },
    )
    .to_glib_full()
}
