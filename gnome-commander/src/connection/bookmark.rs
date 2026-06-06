// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{Connection, ConnectionExt, home::ConnectionHome};
use gtk::prelude::*;

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

#[derive(Debug, glib::Variant)]
pub struct BookmarkGoToVariant {
    pub connection_alias: String,
    pub bookmark_name: String,
}

impl BookmarkGoToVariant {
    pub fn new(connection: &Connection, bookmark: &Bookmark) -> Self {
        Self {
            connection_alias: if connection.is::<ConnectionHome>() {
                String::new()
            } else {
                connection.alias().unwrap_or_default()
            },
            bookmark_name: bookmark.name().to_owned(),
        }
    }
}
