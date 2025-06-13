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
            .flat_map(|m| m.into_iter())
            .flat_map(|(tag, values)| values.into_iter().map(|v| (tag.clone(), v.clone())))
    }
}
