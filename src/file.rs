// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    dir::Directory, libgcmd::file_descriptor::FileDescriptor, options::GeneralOptions,
    utils::ErrorMessage,
};
use futures::{
    future::{Either, select},
    stream::StreamExt,
};
use gettextrs::gettext;
use gtk::{gio, glib, glib::object::WeakRef, prelude::*, subclass::prelude::*};
use libc::{gid_t, uid_t};
use std::{
    cell::Ref,
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
        pub file: RefCell<gio::File>,
        #[property(get, set=Self::set_file_info)]
        pub file_info: RefCell<gio::FileInfo>,
        pub is_dotdot: Cell<bool>,
        pub parent_dir: WeakRef<Directory>,
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

impl File {
    pub const DEFAULT_ATTRIBUTES: &str =
        "standard::*,access::*,time::*,owner::*,unix::uid,unix::gid,unix::mode";

    pub fn new(uri: &str, file_info: impl AsRef<gio::FileInfo>) -> Self {
        Self::new_from_file(gio::File::for_uri(uri), file_info)
    }

    pub fn new_from_file(
        file: impl AsRef<gio::File>,
        file_info: impl AsRef<gio::FileInfo>,
    ) -> Self {
        let this: Self = glib::Object::builder()
            .property("file-info", file_info.as_ref())
            .build();
        this.set_file(file.as_ref().clone());
        this
    }

    pub fn new_from_path(path: &Path) -> Result<Self, ErrorMessage> {
        let file = gio::File::for_path(path);

        let file_info = file
            .query_info(
                Self::DEFAULT_ATTRIBUTES,
                gio::FileQueryInfoFlags::NONE,
                gio::Cancellable::NONE,
            )
            .map_err(|error| {
                ErrorMessage::with_error(
                    gettext("Failed to get file info for {path}.")
                        .replace("{path}", &path.display().to_string()),
                    &error,
                )
            })?;
        Ok(Self::new_from_file(file, &file_info))
    }

    pub fn refresh_file_info(&self) -> Result<(), glib::Error> {
        let file_info = self.file().query_info(
            Self::DEFAULT_ATTRIBUTES,
            gio::FileQueryInfoFlags::NONE,
            gio::Cancellable::NONE,
        )?;
        self.set_file_info(&file_info);
        Ok(())
    }

    pub fn dotdot(dir: &Directory) -> Self {
        let info = gio::FileInfo::new();
        info.set_name("..");
        info.set_display_name("..");
        info.set_file_type(gio::FileType::Directory);
        info.set_is_symlink(false);
        info.set_size(0);
        info.set_attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE, 0xFFF);
        Self::new_from_file(
            dir.file().parent().unwrap_or_else(|| dir.file().clone()),
            &info,
        )
    }

    /// Returns a file name which can be used in a text field when the file is being renamed.
    pub fn edit_name(&self) -> String {
        self.file_info().edit_name().into()
    }

    pub fn parent_directory(&self) -> Option<Directory> {
        self.imp().parent_dir.upgrade()
    }

    pub fn set_parent_directory(&self, dir: Option<&Directory>) {
        self.imp().parent_dir.set(dir);
    }

    pub fn content_type(&self) -> Option<glib::GString> {
        if self.is_dotdot() {
            return Some("inode/directory".into());
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
        self.is_local()
            && self.is_regular()
            && self
                .file_info()
                .boolean(gio::FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)
    }

    pub fn is_dotdot(&self) -> bool {
        self.imp().is_dotdot.get()
    }

    pub fn file_type(&self) -> gio::FileType {
        self.file_info().file_type()
    }

    pub fn is_directory(&self) -> bool {
        self.file_type() == gio::FileType::Directory
    }

    pub fn is_regular(&self) -> bool {
        self.file_type() == gio::FileType::Regular
    }

    pub fn is_special(&self) -> bool {
        self.file_type() == gio::FileType::Special
    }

    pub fn is_symlink(&self) -> bool {
        self.file_info().is_symlink()
    }

    pub fn symlink_target(&self) -> Option<PathBuf> {
        if self.is_symlink() {
            self.file_info().symlink_target()
        } else {
            None
        }
    }

    pub fn extension(&self) -> Option<OsString> {
        if self.is_directory() {
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
        if self.is_directory() {
            None
        } else {
            self.file_info().size().try_into().ok()
        }
    }

    pub fn modification_date(&self) -> Option<glib::DateTime> {
        self.file_info().modification_date_time()
    }

    pub fn access_date(&self) -> Option<glib::DateTime> {
        self.file_info().access_date_time()
    }

    pub async fn tree_size(
        &self,
        progress_callback: impl Fn(u64) + 'static,
        cancellable: gio::Cancellable,
    ) -> Option<u64> {
        match self.file_type() {
            gio::FileType::Directory => {
                let (result, stream) = self.file().measure_disk_usage_future(
                    gio::FileMeasureFlags::NONE,
                    glib::Priority::DEFAULT,
                );

                let callback = &progress_callback;
                let mut stream = stream.take_until(select(cancellable.into_future(), result));
                stream
                    .by_ref()
                    .for_each(|(_, size, _, _)| async move { callback(size) })
                    .await;

                match stream.take_result() {
                    // Stream done but the terminating future isn’t, should not happen
                    None => None,
                    // Cancelled
                    Some(Either::Left((_, _))) => None,
                    // Done
                    Some(Either::Right((result, _))) => match result {
                        Ok((size, _, _)) => Some(size),
                        Err(error) => {
                            eprintln!(
                                "calc_tree_size: g_file_measure_disk_usage_async failed: {error}",
                            );
                            None
                        }
                    },
                }
            }
            gio::FileType::Regular => self.size(),
            _ => None,
        }
    }

    pub fn needs_update(&self) -> bool {
        let Some(last_update) = *self.imp().last_update.borrow() else {
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

/// Trait implemented by File and Directory types, providing various operations on the underlying
/// gio::File instance.
pub trait FileOps {
    fn file(&self) -> Ref<'_, gio::File>;
    fn set_file(&self, file: gio::File);

    fn on_deleted(&self) {}
    fn on_renamed(&self, _old_uri_str: &str) {}
    fn on_changed(&self) {}

    fn uri(&self) -> String {
        self.file().uri().into()
    }

    /// Checks whether a file is local. This will include files on locally mounted remote
    /// filesystems but exclude files available via GVFS.
    fn is_local(&self) -> bool {
        self.file().is_native()
    }

    /// Returns the local filesystem path of the file if any. A path isn't only being returned for
    /// local files but also for files available locally via GVFS.
    fn local_path(&self) -> Option<PathBuf> {
        self.file().path()
    }

    /// Local filesystem path to the parent directory if any.
    fn parent_path(&self) -> Option<PathBuf> {
        Some(self.local_path()?.parent()?.to_path_buf())
    }

    /// Returns the absolute file path. For remote files that path will be relative to server root.
    fn path_from_root(&self) -> PathBuf {
        self.is_local()
            .then(|| self.local_path())
            .flatten()
            .or_else(|| {
                // No local path, fall back to URI path
                glib::Uri::parse(&self.file().uri(), glib::UriFlags::NONE)
                    .ok()
                    .map(|uri| uri.path().into())
            })
            .unwrap_or_default()
    }

    /// Returns the visible name of the file. This is only meant to be displayed in the user
    /// interface and in messages to the user, not in any file operations.
    fn name(&self) -> String {
        self.path_name().to_string_lossy().to_string()
    }

    /// Returns the name of the file as a path, suitable for file operations.
    fn path_name(&self) -> PathBuf {
        self.local_path()
            .and_then(|path| path.file_name().map(PathBuf::from))
            .unwrap_or_else(|| self.file().basename().unwrap_or_default())
    }

    fn rename(&self, new_name: &str) -> Result<(), glib::Error> {
        let old_uri_str = self.uri();

        let new_file = self
            .file()
            .set_display_name(new_name, gio::Cancellable::NONE)?;
        self.set_file(new_file);

        self.on_renamed(&old_uri_str);
        Ok(())
    }

    fn chown(&self, uid: Option<uid_t>, gid: gid_t) -> Result<(), glib::Error> {
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
        self.on_changed();
        Ok(())
    }

    fn chmod(&self, permissions: u32) -> Result<(), glib::Error> {
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
        self.on_changed();
        Ok(())
    }

    fn free_space(&self) -> Result<u64, glib::Error> {
        Ok(self
            .file()
            .query_filesystem_info(gio::FILE_ATTRIBUTE_FILESYSTEM_FREE, gio::Cancellable::NONE)?
            .attribute_uint64(gio::FILE_ATTRIBUTE_FILESYSTEM_FREE))
    }
}

impl FileOps for File {
    fn file(&self) -> Ref<'_, gio::File> {
        self.imp().file.borrow()
    }

    fn name(&self) -> String {
        self.file_info().display_name().into()
    }

    fn path_name(&self) -> PathBuf {
        self.file_info().name()
    }

    fn set_file(&self, file: gio::File) {
        self.imp().file.replace(file);
    }

    fn on_deleted(&self) {
        if let Some(parent) = self.parent_directory() {
            parent.file_deleted(&self.uri());
        }
    }

    fn on_renamed(&self, old_uri_str: &str) {
        let _ = self.refresh_file_info();
        if let Some(parent) = self.parent_directory() {
            parent.file_renamed(self, old_uri_str);
        }
    }

    fn on_changed(&self) {
        if let Some(parent) = self.parent_directory() {
            parent.file_changed(&self.uri());
        }
    }
}
