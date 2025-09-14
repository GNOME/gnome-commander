/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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

use super::connection::{Connection, ConnectionExt, ConnectionInterface};
use crate::{
    debug::debug,
    path::GnomeCmdPath,
    utils::{ErrorMessage, GnomeCmdFileExt},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::{
    cell::RefCell,
    fmt,
    future::Future,
    path::{Path, PathBuf},
    pin::Pin,
};

mod imp {
    use super::*;
    use crate::connection::connection::ConnectionImpl;

    #[derive(Default)]
    pub struct ConnectionSmb {
        pub entities: SmbEntities,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectionSmb {
        const NAME: &'static str = "GnomeCmdConSmb";
        type Type = super::ConnectionSmb;
        type ParentType = Connection;
    }

    impl ObjectImpl for ConnectionSmb {
        fn constructed(&self) {
            self.parent_constructed();
            self.obj().set_alias(Some(&gettext("SMB")));
        }
    }
    impl ConnectionImpl for ConnectionSmb {}
}

glib::wrapper! {
    /// @brief Class for connecting to samba and show available workgroups
    ///
    /// This class is _not_ meant to be used when connecting to a single samba remote, e.g. to smb://server/share.
    /// Instead, it is used to search workgroups, therefore it will list available workgroubs through the connection
    /// to smb:///.
    pub struct ConnectionSmb(ObjectSubclass<imp::ConnectionSmb>)
        @extends Connection;
}

impl Default for ConnectionSmb {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ConnectionSmb {
    pub fn smb_discovery(&self) -> &SmbEntities {
        &self.imp().entities
    }
}

impl ConnectionInterface for ConnectionSmb {
    fn open_impl(
        &self,
        _window: gtk::Window,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async move {
            if self.base_path().is_none() {
                self.set_base_path(Some(GnomeCmdPath::Smb(SmbResource::Root)));
            }

            let path = self.base_path().unwrap();
            let file = self.create_gfile(&path);

            let uri_string = file.uri();
            debug!('s', "Connecting to {}", uri_string);
            match file
                .query_info_future("*", gio::FileQueryInfoFlags::NONE, glib::Priority::DEFAULT)
                .await
            {
                Ok(file_info) => {
                    self.set_base_file_info(Some(&file_info));
                    Ok(())
                }
                Err(error) => {
                    self.set_base_file_info(None);
                    Err(ErrorMessage::with_error(
                        gettext("Failed to browse the network. Is Samba supported on the system?"),
                        &error,
                    ))
                }
            }
        })
    }

    fn close_impl(
        &self,
        _window: Option<gtk::Window>,
    ) -> Pin<Box<dyn Future<Output = Result<(), ErrorMessage>> + '_>> {
        Box::pin(async move {
            self.set_default_dir(None);
            self.set_base_path(None);
            Ok(())
        })
    }

    fn create_gfile(&self, path: &GnomeCmdPath) -> gio::File {
        let root = gio::File::for_uri("smb:");
        root.resolve_relative_path(path.path())
    }

    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        if path.components().count() == 0 {
            return GnomeCmdPath::Smb(SmbResource::Root);
        }
        if let Some(smb_resource) = path
            .to_str()
            .and_then(|p| SmbResource::from_str(p, self.smb_discovery()))
        {
            GnomeCmdPath::Smb(smb_resource)
        } else {
            eprintln!("Can't find a host or workgroup for path {}", path.display());
            GnomeCmdPath::Smb(SmbResource::Root)
        }
    }

    fn is_local(&self) -> bool {
        false
    }

    fn open_is_needed(&self) -> bool {
        true
    }

    fn is_closeable(&self) -> bool {
        false
    }

    fn needs_open_visprog(&self) -> bool {
        true
    }

    fn needs_list_visprog(&self) -> bool {
        true
    }

    fn open_message(&self) -> Option<String> {
        Some(gettext("Searching for workgroups and hosts"))
    }

    fn go_text(&self) -> Option<String> {
        Some(gettext("Go to: Samba Network"))
    }

    fn open_icon(&self) -> Option<gio::Icon> {
        Some(gio::ThemedIcon::new("folder-remote").upcast())
    }
}

fn name_eq_ignore_ascii_case(file_info: &gio::FileInfo, name: &str) -> bool {
    file_info
        .name()
        .to_str()
        .map_or(false, |n| n.eq_ignore_ascii_case(name))
}

#[derive(Clone, PartialEq, Debug)]
pub enum SmbResource {
    Root,
    Workgroup(PathBuf),
    Resource(PathBuf, PathBuf),
    Path(PathBuf, PathBuf, PathBuf),
}

impl SmbResource {
    pub fn path(&self) -> PathBuf {
        match self {
            Self::Root => PathBuf::from("/"),
            Self::Workgroup(workgroup) => PathBuf::from(workgroup),
            Self::Resource(workgroup, resource) => PathBuf::from(workgroup).join(resource),
            Self::Path(workgroup, resource, resource_path) => {
                PathBuf::from(workgroup).join(resource).join(resource_path)
            }
        }
    }

    pub fn parent(&self) -> Option<Self> {
        match self {
            Self::Root => None,
            Self::Workgroup(..) => Some(Self::Root),
            Self::Resource(workgroup, ..) => Some(Self::Workgroup(workgroup.clone())),
            Self::Path(workgroup, resource, resource_path) => match resource_path.parent() {
                Some(parent) => Some(Self::Path(
                    workgroup.to_owned(),
                    resource.to_owned(),
                    parent.to_owned(),
                )),
                None => Some(Self::Resource(workgroup.clone(), resource.clone())),
            },
        }
    }

    pub fn child(&self, child: &Path) -> Self {
        match self {
            Self::Root => Self::Workgroup(PathBuf::from(child)),
            Self::Workgroup(workgroup) => {
                Self::Resource(workgroup.to_owned(), PathBuf::from(child))
            }
            Self::Resource(workgroup, resource) => Self::Path(
                workgroup.to_owned(),
                resource.to_owned(),
                PathBuf::from(child),
            ),
            Self::Path(workgroup, resource, resource_path) => Self::Path(
                workgroup.clone(),
                resource.clone(),
                resource_path.join(child),
            ),
        }
    }

    pub fn from_str(s: &str, discovery: &SmbEntities) -> Option<Self> {
        let mut iter = s.split('\\').filter(|p| !p.is_empty()).fuse();
        let Some(first_item) = iter.next() else {
            return Some(Self::Root);
        };
        match discovery.get(first_item) {
            Some(SmbEntity::Workgroup(wg, hosts)) => {
                if let Some(hostname) = iter.next() {
                    let host = hosts
                        .iter()
                        .find(|h| name_eq_ignore_ascii_case(h, hostname))?;
                    if let Some(path_item) = iter.next() {
                        let mut path = PathBuf::from(path_item);
                        path.extend(iter);
                        Some(Self::Path(wg.name(), host.name(), path))
                    } else {
                        Some(Self::Resource(wg.name(), host.name()))
                    }
                } else {
                    Some(Self::Workgroup(wg.name()))
                }
            }
            Some(SmbEntity::Host(wg, host)) => {
                if let Some(path_item) = iter.next() {
                    let mut path = PathBuf::from(path_item);
                    path.extend(iter);
                    Some(Self::Path(wg.name(), host.name(), path))
                } else {
                    Some(Self::Resource(wg.name(), host.name()))
                }
            }
            None => None,
        }
    }
}

impl fmt::Display for SmbResource {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Root => write!(f, "\\\\"),
            Self::Workgroup(workgroup) => write!(f, "\\\\{}", workgroup.display()),
            Self::Resource(workgroup, resource) => {
                write!(f, "\\\\{}\\{}", workgroup.display(), resource.display())
            }
            Self::Path(workgroup, resource, resource_path) => {
                write!(f, "\\\\{}\\{}", workgroup.display(), resource.display())?;
                for part in resource_path {
                    write!(f, "\\{}", part.to_string_lossy())?;
                }
                Ok(())
            }
        }
    }
}

#[derive(Default)]
pub struct SmbEntities {
    pub entities: RefCell<Vec<SmbEntity>>,
}

impl SmbEntities {
    fn find<'a>(&'a self, name: &str) -> Option<SmbEntity> {
        for entity in self.entities.borrow().iter() {
            let file_info = match entity {
                SmbEntity::Workgroup(file_info, _) => file_info,
                SmbEntity::Host(_, file_info) => file_info,
            };
            if name_eq_ignore_ascii_case(file_info, name) {
                return Some(entity.clone());
            }
        }
        None
    }

    pub fn get(&self, name: &str) -> Option<SmbEntity> {
        if let Some(entity) = self.find(name) {
            return Some(entity);
        } else {
            // Entity not found, rebuilding the database
            match discover_smb_entities() {
                Ok(entities) => {
                    self.entities.replace(entities);
                }
                Err(error) => {
                    eprintln!("Unable to discover smb resources: {}", error.message());
                    self.entities.borrow_mut().clear();
                }
            }
            self.find(name)
        }
    }
}

#[derive(Clone)]
pub enum SmbEntity {
    Workgroup(gio::FileInfo, Vec<gio::FileInfo>),
    Host(gio::FileInfo, gio::FileInfo),
}

fn discover_smb_entities() -> Result<Vec<SmbEntity>, glib::Error> {
    let mut result = Vec::new();
    for wg in get_workgroups()? {
        let hosts = get_hosts(&wg.display_name())?;
        result.push(SmbEntity::Workgroup(wg.clone(), hosts.clone()));
        for host in hosts {
            result.push(SmbEntity::Host(wg.clone(), host));
        }
    }
    Ok(result)
}

fn get_workgroups() -> Result<Vec<gio::FileInfo>, glib::Error> {
    gio::File::for_uri("smb:").all_children(
        "*",
        gio::FileQueryInfoFlags::NONE,
        gio::Cancellable::NONE,
    )
}

fn get_hosts(wg: &str) -> Result<Vec<gio::FileInfo>, glib::Error> {
    gio::File::for_uri(&format!("smb://{}", wg)).all_children(
        "*",
        gio::FileQueryInfoFlags::NONE,
        gio::Cancellable::NONE,
    )
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_from_str() {
        let discovery = SmbEntities::default();
        let smb_resource = SmbResource::from_str("\\\\", &discovery);
        assert_eq!(smb_resource, Some(SmbResource::Root));
    }
}
