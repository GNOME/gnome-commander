// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{GnomeCmdTag, GnomeCmdTagClass};
use indexmap::IndexMap;

#[derive(Default)]
pub struct FileMetadata {
    pub tags: IndexMap<GnomeCmdTagClass, IndexMap<GnomeCmdTag, Vec<String>>>,
}

impl FileMetadata {
    pub fn add(&mut self, tag: GnomeCmdTag, value: Option<&str>) {
        let Some(value) = value.map(|v| v.trim_end()).filter(|v| !v.is_empty()) else {
            return;
        };

        if let Some(tag_class) = tag.class() {
            self.tags
                .entry(tag_class)
                .or_default()
                .entry(tag)
                .or_default()
                .push(value.to_owned());
        } else {
            eprintln!("Invalid tag \"{}\".", tag.0);
        }
    }

    pub fn has(&self, tag: &GnomeCmdTag) -> bool {
        let Some(tag_class) = tag.class() else {
            return false;
        };
        self.tags
            .get(&tag_class)
            .map(|k| k.contains_key(tag))
            .unwrap_or_default()
    }

    pub fn get(&self, tag: &GnomeCmdTag) -> Option<String> {
        let tag_class = tag.class()?;
        self.tags
            .get(&tag_class)
            .and_then(|k| k.get(tag))
            .map(|values| values.join(", "))
    }

    pub fn get_first(&self, tag: &GnomeCmdTag) -> Option<String> {
        let tag_class = tag.class()?;
        self.tags
            .get(&tag_class)
            .and_then(|k| k.get(tag))
            .and_then(|values| values.first())
            .cloned()
    }

    pub fn dump(&self) -> impl Iterator<Item = (GnomeCmdTag, String)> + '_ {
        self.tags
            .values()
            .flat_map(|m| m.iter())
            .flat_map(|(tag, values)| values.iter().map(|v| (tag.clone(), v.clone())))
    }
}
