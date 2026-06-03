// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Bookmark {
    name: String,
    path: String,
}

impl Bookmark {
    pub fn new(name: &str, path: &str) -> Self {
        Self {
            name: name.to_string(),
            path: path.to_string(),
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn path(&self) -> &str {
        &self.path
    }
}

#[derive(glib::Variant)]
pub struct BookmarkGoToVariant {
    pub connection_uuid: String,
    pub bookmark_name: String,
}
