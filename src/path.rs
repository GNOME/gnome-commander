/*
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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

use crate::connection::smb::SmbResource;
use gtk::{gio, prelude::*};
use std::{
    fmt,
    path::{Path, PathBuf},
};

#[derive(Clone)]
pub enum GnomeCmdPath {
    Plain(PathBuf),
    Smb(SmbResource),
}

impl GnomeCmdPath {
    pub fn path(&self) -> PathBuf {
        match self {
            Self::Plain(path) => path.clone(),
            Self::Smb(resource) => resource.path(),
        }
    }

    pub fn parent(&self) -> Option<Self> {
        match self {
            Self::Plain(path) => {
                let absolute_path = if path.is_absolute() {
                    path.clone()
                } else {
                    PathBuf::from("/").join(path)
                };
                let parent_path = gio::File::for_path(&absolute_path).parent()?.path()?;
                Some(Self::Plain(parent_path))
            }
            Self::Smb(resource) => Some(Self::Smb(resource.parent()?)),
        }
    }

    pub fn child(&self, child: &Path) -> Self {
        match self {
            Self::Plain(path) => Self::Plain(path.join(child)),
            Self::Smb(resource) => Self::Smb(resource.child(child)),
        }
    }
}

impl fmt::Display for GnomeCmdPath {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Plain(path) => write!(f, "{}", path.to_string_lossy()),
            Self::Smb(resource) => write!(f, "{resource}"),
        }
    }
}
