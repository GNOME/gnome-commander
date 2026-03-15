// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{generic::generic_search, local::local_search, profile::SearchProfile};
use crate::{dir::Directory, file::File, utils::ErrorMessage};
use gtk::gio;

pub enum SearchMessage {
    Status(String),
    File(File),
}

pub enum SearchBackend {
    Local,
    Generic,
}

impl SearchBackend {
    pub async fn search(
        &self,
        profile: &SearchProfile,
        start_dir: &Directory,
        on_message: &dyn Fn(SearchMessage),
        cancellable: &gio::Cancellable,
    ) -> Result<(), ErrorMessage> {
        match self {
            SearchBackend::Local => local_search(profile, start_dir, on_message, cancellable).await,
            SearchBackend::Generic => {
                generic_search(profile, start_dir, on_message, cancellable).await
            }
        }
    }
}
