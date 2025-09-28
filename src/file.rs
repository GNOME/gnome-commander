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

use crate::{
    connection::{
        connection::{Connection, ConnectionInterface},
        list::ConnectionList,
    },
    dir::Directory,
    libgcmd::file_descriptor::{FileDescriptor, FileDescriptorExt},
    options::options::{GeneralOptions, ProgramsOptions},
    path::GnomeCmdPath,
    spawn::{SpawnError, app_needs_terminal, run_command_indir},
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use libc::{gid_t, uid_t};
use std::{
    ffi::OsString,
    path::{Path, PathBuf},
    time::Instant,
};

mod imp {
    use super::*;
    use crate::libgcmd::file_descriptor::FileDescriptorImpl;
    use std::cell::{Cell, RefCell};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::File)]
    pub struct File {
        #[property(get, set)]
        pub file: RefCell<gio::File>,
        #[property(get, set=Self::set_file_info)]
        pub file_info: RefCell<gio::FileInfo>,
        pub is_dotdot: Cell<bool>,
        pub parent_dir: RefCell<Option<Directory>>,
        pub last_update: RefCell<Option<Instant>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for File {
        const NAME: &'static str = "GnomeCmdFile";
        type Type = super::File;
        type Interfaces = (FileDescriptor,);

        fn new() -> Self {
            Self {
                file: RefCell::new(gio::File::for_path("/")),
                file_info: RefCell::new(gio::FileInfo::new()),
                is_dotdot: Default::default(),
                parent_dir: Default::default(),
                last_update: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for File {}

    impl FileDescriptorImpl for File {
        fn file(&self) -> gio::File {
            self.file.borrow().clone()
        }

        fn file_info(&self) -> gio::FileInfo {
            self.file_info.borrow().clone()
        }
    }

    impl File {
        fn set_file_info(&self, file_info: gio::FileInfo) {
            self.is_dotdot.set(
                file_info.file_type() == gio::FileType::Directory
                    && file_info.display_name() == "..",
            );
            self.file_info.replace(file_info);
        }
    }
}

glib::wrapper! {
    pub struct File(ObjectSubclass<imp::File>)
        @implements FileDescriptor;
}

pub trait FileImpl: ObjectImpl {}

unsafe impl<T: FileImpl> IsSubclassable<T> for File {}

impl File {
    pub fn new(file_info: &gio::FileInfo, dir: &Directory) -> Self {
        let file = dir.file().child(file_info.name());
        Self::new_full(file_info, &file, dir)
    }

    pub fn new_full(file_info: &gio::FileInfo, file: &gio::File, dir: &Directory) -> Self {
        let this: Self = glib::Object::builder()
            .property("file", file)
            .property("file-info", file_info)
            .build();
        this.imp().parent_dir.replace(Some(dir.clone()));
        this
    }

    pub fn new_from_path(path: &Path) -> Result<Self, ErrorMessage> {
        let file = gio::File::for_path(path);

        let file_info = file
            .query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)
            .map_err(|error| {
                ErrorMessage::with_error(
                    gettext("Failed to get file info for {path}.")
                        .replace("{path}", &path.display().to_string()),
                    &error,
                )
            })?;

        let parent_path = file
            .parent()
            .and_then(|p| p.path())
            .unwrap_or_else(|| PathBuf::from("/"));

        let home = ConnectionList::get().home();
        let dir_path = home.create_path(&parent_path);
        let dir = Directory::try_new(&home, dir_path)?;

        Ok(Self::new_full(&file_info, &file, &dir))
    }

    pub fn refresh_file_info(&self) -> Result<(), glib::Error> {
        let file_info =
            self.file()
                .query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)?;
        self.set_file_info(&file_info);
        Ok(())
    }

    pub fn dotdot(dir: &Directory) -> Self {
        let info = gio::FileInfo::new();
        info.set_name("..");
        info.set_display_name("..");
        info.set_file_type(gio::FileType::Directory);
        info.set_size(0);
        Self::new(&info, dir)
    }

    pub fn get_name(&self) -> String {
        self.file_info().display_name().into()
    }

    pub fn get_real_path(&self) -> Option<PathBuf> {
        self.file().path()
    }

    pub fn get_path_through_parent(&self) -> GnomeCmdPath {
        let filename = self.file_info().name();
        if let Some(parent) = self.parent_directory() {
            parent.path().child(&filename)
        } else if let Some(directory) = self.downcast_ref::<Directory>() {
            directory.path().clone()
        } else {
            panic!("Non directory file without owning directory")
        }
    }

    pub fn get_path_string_through_parent(&self) -> PathBuf {
        self.get_path_through_parent().path()
    }

    pub fn get_dirname(&self) -> Option<PathBuf> {
        self.file().parent()?.path()
    }

    pub fn get_uri_str(&self) -> String {
        self.file().uri().into()
    }

    pub fn parent_directory(&self) -> Option<Directory> {
        self.imp().parent_dir.borrow().clone()
    }

    pub fn connection(&self) -> Connection {
        if let Some(dir) = self.downcast_ref::<Directory>() {
            dir.connection()
        } else {
            self.parent_directory()
                .expect("File should always have a parent directory")
                .connection()
        }
    }

    pub fn is_local(&self) -> bool {
        self.connection().is_local()
    }

    pub fn content_type(&self) -> Option<glib::GString> {
        if self.is_dotdot() {
            return None;
        }
        let file_info = self.file_info();
        file_info
            .attribute_string(gio::FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)
            .or_else(|| file_info.attribute_string(gio::FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE))
    }

    pub fn app_info_for_content_type(&self) -> Option<gio::AppInfo> {
        let content_type = self.content_type()?;
        let must_support_uris = !self.file().has_uri_scheme("file");
        gio::AppInfo::default_for_type(&content_type, must_support_uris)
    }

    pub fn is_executable(&self) -> bool {
        self.is_local() && {
            let file_info = self.file_info();
            file_info.file_type() == gio::FileType::Regular
                && file_info.boolean(gio::FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)
        }
    }

    pub fn execute(&self, options: &ProgramsOptions) -> Result<(), SpawnError> {
        let mut command = OsString::from("./");
        command.push(glib::shell_quote(self.file_info().display_name()));

        run_command_indir(
            self.get_dirname().as_deref(),
            &command,
            app_needs_terminal(self),
            options,
        )
    }

    pub fn rename(&self, new_name: &str) -> Result<(), glib::Error> {
        let old_uri_str = self.get_uri_str();

        let new_file = self
            .file()
            .set_display_name(new_name, gio::Cancellable::NONE)?;
        self.set_file(&new_file);
        let new_file_info =
            new_file.query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)?;
        self.set_file_info(&new_file_info);

        if let Some(parent) = self.parent_directory() {
            parent.file_renamed(self, &old_uri_str);
            if let Some(directory) = self.downcast_ref::<Directory>() {
                directory.update_path();
            }
        }
        Ok(())
    }

    pub fn chown(&self, uid: Option<uid_t>, gid: gid_t) -> Result<(), glib::Error> {
        let file_info = self.file().query_info(
            &format!(
                "{},{}",
                gio::FILE_ATTRIBUTE_UNIX_UID,
                gio::FILE_ATTRIBUTE_UNIX_GID
            ),
            gio::FileQueryInfoFlags::NONE,
            gio::Cancellable::NONE,
        )?;
        if let Some(uid) = uid {
            file_info.set_attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_UID, uid);
        }
        file_info.set_attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_GID, gid);
        self.file().set_attributes_from_info(
            &file_info,
            gio::FileQueryInfoFlags::NONE,
            gio::Cancellable::NONE,
        )?;
        if let Some(parent) = self.parent_directory() {
            parent.file_changed(&self.get_uri_str());
        }
        Ok(())
    }

    pub fn chmod(&self, permissions: u32) -> Result<(), glib::Error> {
        let file_info = self.file().query_info(
            gio::FILE_ATTRIBUTE_UNIX_MODE,
            gio::FileQueryInfoFlags::NONE,
            gio::Cancellable::NONE,
        )?;
        file_info.set_attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE, permissions);
        self.file().set_attributes_from_info(
            &file_info,
            gio::FileQueryInfoFlags::NONE,
            gio::Cancellable::NONE,
        )?;
        if let Some(parent) = self.parent_directory() {
            parent.file_changed(&self.get_uri_str());
        }
        Ok(())
    }

    pub fn is_dotdot(&self) -> bool {
        self.imp().is_dotdot.get()
    }

    pub fn set_deleted(&self) {
        if let Some(parent) = self.parent_directory() {
            parent.file_deleted(&self.get_uri_str());
        }
    }

    pub fn extension(&self) -> Option<OsString> {
        if self.file_info().file_type() == gio::FileType::Directory {
            None
        } else {
            Some(self.file_info().name().extension()?.to_owned())
        }
    }

    pub fn permissions(&self) -> u32 {
        self.file_info()
            .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE)
            & 0xFFF
    }

    pub fn uid(&self) -> u32 {
        self.file_info()
            .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_UID)
    }

    pub fn owner(&self) -> String {
        let file_info = self.file_info();
        file_info
            .attribute_string(gio::FILE_ATTRIBUTE_OWNER_USER)
            .map(|o| o.to_string())
            .unwrap_or_else(|| {
                file_info
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_UID)
                    .to_string()
            })
    }

    pub fn gid(&self) -> u32 {
        self.file_info()
            .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_GID)
    }

    pub fn group(&self) -> String {
        let file_info = self.file_info();
        file_info
            .attribute_string(gio::FILE_ATTRIBUTE_OWNER_GROUP)
            .map(|g| g.to_string())
            .unwrap_or_else(|| {
                file_info
                    .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_GID)
                    .to_string()
            })
    }

    pub fn size(&self) -> Option<u64> {
        let file_info = self.file_info();
        if file_info.file_type() == gio::FileType::Directory {
            None
        } else {
            file_info.size().try_into().ok()
        }
    }

    pub fn modification_date(&self) -> Option<glib::DateTime> {
        self.file_info().modification_date_time()
    }

    pub fn tree_size(&self) -> Option<u64> {
        match self.file_info().file_type() {
            gio::FileType::Directory => {
                match self.file().measure_disk_usage(
                    gio::FileMeasureFlags::NONE,
                    gio::Cancellable::NONE,
                    None,
                ) {
                    Ok((size, _, _)) => Some(size),
                    Err(error) => {
                        eprintln!(
                            "calc_tree_size: g_file_measure_disk_usage failed: {}",
                            error.message()
                        );
                        None
                    }
                }
            }
            gio::FileType::Regular => self.size(),
            _ => None,
        }
    }

    pub fn free_space(&self) -> Result<u64, glib::Error> {
        Ok(self
            .file()
            .query_filesystem_info(gio::FILE_ATTRIBUTE_FILESYSTEM_FREE, gio::Cancellable::NONE)?
            .attribute_uint64(gio::FILE_ATTRIBUTE_FILESYSTEM_FREE))
    }

    pub fn needs_update(&self) -> bool {
        let Some(last_update) = self.imp().last_update.borrow().clone() else {
            return true;
        };
        let now = Instant::now();
        if now.duration_since(last_update) > GeneralOptions::new().gui_update_rate.get() {
            self.imp().last_update.replace(Some(now));
            true
        } else {
            false
        }
    }
}
