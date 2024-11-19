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

use super::tags::{GnomeCmdTag, GnomeCmdTagClass};
use glib::{
    ffi::gboolean,
    translate::{from_glib_none, ToGlibPtr},
};
use std::{
    collections::BTreeMap,
    ffi::{c_char, c_int},
};

#[derive(Default)]
pub struct FileMetadata {
    pub tags: BTreeMap<GnomeCmdTagClass, BTreeMap<GnomeCmdTag, Vec<String>>>,
}

impl FileMetadata {
    pub fn mark_as_accessed(&self, _tag_class: GnomeCmdTagClass) {
        //
    }

    pub fn is_accessed(&self, tag_class: GnomeCmdTagClass) -> bool {
        self.tags.contains_key(&tag_class)
    }

    pub fn add(&mut self, tag: GnomeCmdTag, value: Option<&str>) {
        let Some(value) = value.map(|v| v.trim_end()).filter(|v| !v.is_empty()) else {
            return;
        };

        let tag_class = tag.class();
        self.tags
            .entry(tag_class)
            .or_default()
            .entry(tag)
            .or_default()
            .push(value.to_owned());
    }

    pub fn has(&self, tag: GnomeCmdTag) -> bool {
        let tag_class = tag.class();
        self.tags
            .get(&tag_class)
            .map(|k| k.contains_key(&tag))
            .unwrap_or_default()
    }

    pub fn get(&self, tag: GnomeCmdTag) -> Option<String> {
        let tag_class = tag.class();
        self.tags
            .get(&tag_class)
            .and_then(|k| k.get(&tag))
            .map(|values| values.join(", "))
    }

    pub fn to_tsv(&self) -> String {
        let mut tsv = String::new();
        for (_, tags) in &self.tags {
            for (tag, values) in tags {
                for value in values {
                    tsv.push_str(&tag.name());
                    tsv.push('\t');
                    tsv.push_str(&tag.title());
                    tsv.push('\t');
                    tsv.push_str(value);
                    tsv.push('\n');
                }
            }
        }
        tsv
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_new() -> *mut FileMetadata {
    let metadata = Box::new(FileMetadata::default());
    Box::leak(metadata)
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_free(fm: *mut FileMetadata) {
    let metadata = unsafe { Box::from_raw(fm) };
    drop(metadata);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_is_accessed(
    fm: *mut FileMetadata,
    tag_class: c_int,
) -> gboolean {
    let fm: &mut FileMetadata = unsafe { &mut *fm };
    let tag_class = GnomeCmdTagClass(tag_class);
    fm.is_accessed(tag_class) as gboolean
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_mark_as_accessed(
    fm: *mut FileMetadata,
    tag_class: c_int,
) {
    let fm: &mut FileMetadata = unsafe { &mut *fm };
    let tag_class = GnomeCmdTagClass(tag_class);
    fm.mark_as_accessed(tag_class);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_add(
    fm: *mut FileMetadata,
    tag: c_int,
    value: *const c_char,
) {
    let fm: &mut FileMetadata = unsafe { &mut *fm };
    let tag = GnomeCmdTag(tag);
    let value: Option<String> = unsafe { from_glib_none(value) };
    fm.add(tag, value.as_deref());
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_has_tag(fm: *mut FileMetadata, tag: c_int) -> gboolean {
    let fm: &mut FileMetadata = unsafe { &mut *fm };
    let tag = GnomeCmdTag(tag);
    fm.has(tag) as gboolean
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_get(fm: *mut FileMetadata, tag: c_int) -> *mut c_char {
    let fm: &mut FileMetadata = unsafe { &mut *fm };
    let tag = GnomeCmdTag(tag);
    let value = fm.get(tag);
    value.to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metadata_to_tsv(fm: *mut FileMetadata) -> *mut c_char {
    let fm: &mut FileMetadata = unsafe { &mut *fm };
    let value = fm.to_tsv();
    value.to_glib_full()
}
