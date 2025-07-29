/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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

use crate::{
    connection::{
        connection::{ffi::GnomeCmdCon, Connection, ConnectionExt},
        list::ConnectionList,
    },
    data::{GeneralOptions, GeneralOptionsRead, ProgramsOptionsRead},
    dir::{ffi::GnomeCmdDir, Directory},
    libgcmd::file_descriptor::{ffi::GnomeCmdFileDescriptor, FileDescriptor, FileDescriptorExt},
    path::GnomeCmdPath,
    spawn::{app_needs_terminal, run_command_indir, SpawnError},
};
use glib::ffi::{gboolean, GError};
use gtk::{
    gio::{
        self,
        ffi::{GFile, GFileInfo},
        prelude::*,
    },
    glib::{self, translate::*},
};
use libc::{gid_t, uid_t};
use std::{
    cell::RefCell,
    ffi::OsString,
    path::{Path, PathBuf},
    sync::LazyLock,
    time::Instant,
};

pub mod ffi {
    use super::*;
    use crate::connection::connection::ffi::GnomeCmdCon;
    use gtk::glib::ffi::GType;
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdFile {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_file_get_type() -> GType;

        pub fn gnome_cmd_file_get_real_path(f: *const GnomeCmdFile) -> *mut c_char;

        pub fn gnome_cmd_file_get_connection(f: *mut GnomeCmdFile) -> *mut GnomeCmdCon;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct File(Object<ffi::GnomeCmdFile, ffi::GnomeCmdFileClass>)
        @implements FileDescriptor;

    match fn {
        type_ => || ffi::gnome_cmd_file_get_type(),
    }
}

#[derive(Default)]
struct FilePrivate {
    file: RefCell<Option<gio::File>>,
    file_info: RefCell<Option<gio::FileInfo>>,
    is_dotdot: bool,
    parent_dir: RefCell<Option<Directory>>,
    last_update: Option<Instant>,
}

impl File {
    fn private(&self) -> &mut FilePrivate {
        static QUARK: LazyLock<glib::Quark> =
            LazyLock::new(|| glib::Quark::from_str("file-private"));

        unsafe {
            if let Some(mut private) = self.qdata::<FilePrivate>(*QUARK) {
                private.as_mut()
            } else {
                self.set_qdata(*QUARK, FilePrivate::default());
                self.qdata::<FilePrivate>(*QUARK).unwrap().as_mut()
            }
        }
    }

    pub fn new(file_info: &gio::FileInfo, dir: &Directory) -> Self {
        let file = dir.file().child(file_info.name());
        Self::new_full(file_info, &file, dir)
    }

    pub fn new_full(file_info: &gio::FileInfo, file: &gio::File, dir: &Directory) -> Self {
        let this: Self = glib::Object::builder().build();
        this.set_file_info(file_info);
        this.set_file(file);
        this.private().parent_dir.replace(Some(dir.clone()));
        this.private().is_dotdot =
            file_info.file_type() == gio::FileType::Directory && file_info.display_name() == "..";
        this
    }

    pub fn new_from_path(path: &Path) -> Result<Self, glib::Error> {
        let file = gio::File::for_path(path);

        let file_info =
            file.query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)?;

        let parent_path = file
            .parent()
            .and_then(|p| p.path())
            .unwrap_or_else(|| PathBuf::from("/"));

        let home = ConnectionList::get().home();
        let dir_path = home.create_path(&parent_path);
        let dir = Directory::new(&home, dir_path);

        Ok(Self::new_full(&file_info, &file, &dir))
    }

    fn setup(&self, file: &gio::File) -> Result<(), glib::Error> {
        let file_info =
            file.query_info("*", gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)?;
        self.set_file_info(&file_info);
        self.set_file(&file);
        self.private().is_dotdot =
            file_info.file_type() == gio::FileType::Directory && file_info.display_name() == "..";
        Ok(())
    }

    fn set_file(&self, file: &gio::File) {
        self.private().file.replace(Some(file.clone()));
    }

    fn set_file_info(&self, file_info: &gio::FileInfo) {
        self.private().file_info.replace(Some(file_info.clone()));
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

    pub fn get_real_path(&self) -> PathBuf {
        unsafe { from_glib_full(ffi::gnome_cmd_file_get_real_path(self.to_glib_none().0)) }
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

    pub fn is_local(&self) -> bool {
        self.connection().is_local()
    }

    pub fn parent_directory(&self) -> Option<Directory> {
        self.private().parent_dir.borrow().clone()
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

    pub fn execute(&self, options: &dyn ProgramsOptionsRead) -> Result<(), SpawnError> {
        let path = self.get_real_path();
        let working_directory = path.parent();

        let mut command = OsString::from("./");
        command.push(glib::shell_quote(self.file_info().display_name()));

        run_command_indir(
            working_directory,
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
        self.private().is_dotdot
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
        let Some(last_update) = self.private().last_update else {
            return true;
        };
        let now = Instant::now();
        if now.duration_since(last_update) > GeneralOptions::new().gui_update_rate() {
            self.private().last_update = Some(now);
            true
        } else {
            false
        }
    }
}

pub trait GnomeCmdFileExt {
    fn connection(&self) -> Connection;
}

impl GnomeCmdFileExt for File {
    fn connection(&self) -> Connection {
        unsafe { from_glib_none(ffi::gnome_cmd_file_get_connection(self.to_glib_none().0)) }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_new_full(
    file_info: *mut GFileInfo,
    file: *mut GFile,
    dir: *mut GnomeCmdDir,
) -> *mut ffi::GnomeCmdFile {
    let file_info: gio::FileInfo = unsafe { from_glib_full(file_info) };
    let file: gio::File = unsafe { from_glib_full(file) };
    let dir: Borrowed<Directory> = unsafe { from_glib_borrow(dir) };
    File::new_full(&file_info, &file, &*dir).to_glib_full()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_setup(
    dir: *mut ffi::GnomeCmdFile,
    file: *mut GFile,
    error: *mut *mut GError,
) -> gboolean {
    let object: Borrowed<File> = unsafe { from_glib_borrow(dir) };
    let file: gio::File = unsafe { from_glib_full(file) };
    match object.setup(&file) {
        Ok(()) => 1,
        Err(e) => unsafe {
            *error = e.to_glib_full();
            0
        },
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_get_file(fd: *mut GnomeCmdFileDescriptor) -> *mut GFile {
    let fd: Borrowed<FileDescriptor> = unsafe { from_glib_borrow(fd) };
    let file = fd.downcast_ref::<File>().unwrap().private().file.borrow();
    file.to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_get_file_info(fd: *mut GnomeCmdFileDescriptor) -> *mut GFileInfo {
    let fd: Borrowed<FileDescriptor> = unsafe { from_glib_borrow(fd) };
    let info = fd
        .downcast_ref::<File>()
        .unwrap()
        .private()
        .file_info
        .borrow();
    info.to_glib_none().0
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_update_file_info(
    f: *mut ffi::GnomeCmdFile,
    file_info: *mut GFileInfo,
) {
    let f: Borrowed<File> = unsafe { from_glib_borrow(f) };
    let file_info: Borrowed<gio::FileInfo> = unsafe { from_glib_borrow(file_info) };
    f.set_file_info(&file_info);
}

#[no_mangle]
pub extern "C" fn file_get_connection(f: *mut ffi::GnomeCmdFile) -> *mut GnomeCmdCon {
    let f: Borrowed<File> = unsafe { from_glib_borrow(f) };
    f.parent_directory()
        .map(|d| d.connection())
        .to_glib_none()
        .0
}
