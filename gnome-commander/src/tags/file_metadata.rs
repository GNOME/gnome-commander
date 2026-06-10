// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::Tag;

struct GroupsIterator<'a> {
    tags: &'a [(Box<dyn Tag>, String)],
    index: usize,
}

impl<'a> Iterator for GroupsIterator<'a> {
    type Item = Vec<(&'a Box<dyn Tag>, &'a str)>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index >= self.tags.len() {
            return None;
        }

        let class = self.tags[self.index].0.class();
        let mut tags = Vec::new();
        while self.index < self.tags.len() && self.tags[self.index].0.class() == class {
            tags.push((&self.tags[self.index].0, self.tags[self.index].1.as_str()));
            self.index += 1;
        }
        Some(tags)
    }
}

#[derive(Debug, Default)]
pub struct FileMetadata {
    tags: Vec<(Box<dyn Tag>, String)>,
    sorted: bool,
}

impl FileMetadata {
    pub fn add(&mut self, tag: Box<dyn Tag>, mut value: String) {
        value.truncate(value.trim_end().len());
        if !value.is_empty() {
            self.tags.push((tag, value));
            self.sorted = false;
        }
    }

    pub fn get(&self, tag: &str) -> Option<String> {
        let values = self
            .tags
            .iter()
            .filter(|(t, _)| t.id() == tag)
            .map(|(_, v)| v.as_str())
            .collect::<Vec<_>>();
        if values.is_empty() {
            None
        } else {
            Some(values.join(", "))
        }
    }

    pub fn get_first(&self, tag: &str) -> Option<String> {
        self.tags
            .iter()
            .find(|(t, _)| t.id() == tag)
            .map(|(_, v)| v.clone())
    }

    pub fn groups(&mut self) -> impl Iterator<Item = Vec<(&Box<dyn Tag>, &str)>> {
        if !self.sorted {
            self.tags
                .sort_by_cached_key(|(tag, _)| tag.class().to_string());
            self.sorted = true;
        }
        GroupsIterator {
            tags: &self.tags,
            index: 0,
        }
    }

    pub fn to_tsv(&self) -> String {
        self.tags.iter().fold(String::new(), |tsv, (tag, value)| {
            tsv + tag.id() + "\t" + &tag.name() + "\t" + value + "\n"
        })
    }
}
