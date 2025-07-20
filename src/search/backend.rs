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
            SearchBackend::Local => {
                local_search(&profile, start_dir, on_message, cancellable).await
            }
            SearchBackend::Generic => {
                generic_search(&profile, start_dir, on_message, cancellable).await
            }
        }
    }
}
