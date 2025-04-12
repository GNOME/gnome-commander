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
    connection::connection::Connection,
    dirlist::list_directory,
    file::{File, GnomeCmdFileExt},
    libgcmd::file_descriptor::FileDescriptor,
    path::GnomeCmdPath,
};
use gtk::{
    gio,
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_full, from_glib_none, ToGlibPtr},
    },
    prelude::*,
};
use std::ffi::CStr;

pub mod ffi {
    use crate::{
        connection::connection::ffi::GnomeCmdCon, file::ffi::GnomeCmdFile, path::GnomeCmdPath,
    };
    use gtk::{
        gio::ffi::{GFile, GFileInfo},
        glib::ffi::{gboolean, GList, GType},
    };
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdDir {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_dir_get_type() -> GType;

        pub fn gnome_cmd_dir_new_from_gfileinfo(
            file_info: *mut GFileInfo,
            parent: *mut GnomeCmdDir,
        ) -> *mut GnomeCmdDir;
        pub fn gnome_cmd_dir_new(
            dir: *mut GnomeCmdCon,
            path: *const GnomeCmdPath,
            is_startup: gboolean,
        ) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_dir_get_parent(dir: *mut GnomeCmdDir) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_dir_get_display_path(dir: *mut GnomeCmdDir) -> *const c_char;

        pub fn gnome_cmd_dir_get_files(dir: *mut GnomeCmdDir) -> *const GList;

        pub fn gnome_cmd_dir_set_state(dir: *mut GnomeCmdDir, state: i32);
        pub fn gnome_cmd_dir_set_files(dir: *mut GnomeCmdDir, files: *mut GList);

        pub fn gnome_cmd_dir_get_child_gfile(
            dir: *mut GnomeCmdDir,
            filename: *const c_char,
        ) -> *mut GFile;

        pub fn gnome_cmd_dir_get_path(dir: *mut GnomeCmdDir) -> *mut GnomeCmdPath;

        pub fn gnome_cmd_dir_file_created(dir: *mut GnomeCmdDir, uri_str: *const c_char);
        pub fn gnome_cmd_dir_file_deleted(dir: *mut GnomeCmdDir, uri_str: *const c_char);
        pub fn gnome_cmd_dir_file_changed(dir: *mut GnomeCmdDir, uri_str: *const c_char);
        pub fn gnome_cmd_dir_file_renamed(
            dir: *mut GnomeCmdDir,
            f: *mut GnomeCmdFile,
            old_uri_str: *const c_char,
        );
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdDirClass {
        pub parent_class: crate::file::ffi::GnomeCmdFileClass,
    }
}

glib::wrapper! {
    pub struct Directory(Object<ffi::GnomeCmdDir, ffi::GnomeCmdDirClass>)
        @extends File,
        @implements FileDescriptor;

    match fn {
        type_ => || ffi::gnome_cmd_dir_get_type(),
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
enum DirectoryState {
    Empty = 0,
    Listed,
    Listing,
    Canceling,
}

impl Directory {
    pub fn new_from_file_info(file_info: &gio::FileInfo, parent: &Directory) -> Option<Self> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_new_from_gfileinfo(
                file_info.to_glib_full(),
                parent.to_glib_none().0,
            ))
        }
    }

    pub fn new(connection: &impl IsA<Connection>, path: GnomeCmdPath) -> Self {
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_new(
                connection.as_ref().to_glib_none().0,
                path.into_raw(),
                false as gboolean,
            ))
        }
    }

    pub fn new_startup(connection: &Connection, path: GnomeCmdPath) -> Option<Self> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_new(
                connection.to_glib_none().0,
                path.into_raw(),
                true as gboolean,
            ))
        }
    }

    pub fn parent(&self) -> Option<Directory> {
        unsafe { from_glib_full(ffi::gnome_cmd_dir_get_parent(self.to_glib_none().0)) }
    }

    pub fn display_path(&self) -> String {
        let ptr = unsafe { ffi::gnome_cmd_dir_get_display_path(self.to_glib_none().0) };
        let str = unsafe { CStr::from_ptr(ptr).to_string_lossy() };
        str.to_string()
    }

    pub fn files(&self) -> glib::List<File> {
        unsafe { glib::List::from_glib_none(ffi::gnome_cmd_dir_get_files(self.to_glib_none().0)) }
    }

    fn set_state(&self, state: DirectoryState) {
        unsafe { ffi::gnome_cmd_dir_set_state(self.to_glib_none().0, state as i32) }
    }

    fn set_files(&self, files: glib::List<File>) {
        unsafe { ffi::gnome_cmd_dir_set_files(self.to_glib_none().0, files.into_raw()) }
    }

    pub async fn relist_files(&self, parent_window: &gtk::Window, visual: bool) {
        let Some(lock) = DirectoryLock::try_acquire(self) else {
            return;
        };

        let window = if visual { Some(parent_window) } else { None };
        match list_directory(self, window).await {
            Ok(file_infos) => {
                self.set_state(DirectoryState::Listed);
                self.set_files(
                    file_infos
                        .into_iter()
                        .filter_map(|file_info| create_file_from_file_info(&file_info, self))
                        .collect(),
                );
                self.emit_by_name::<()>("list-ok", &[]);
            }
            Err(error) => {
                self.set_state(DirectoryState::Empty);
                self.emit_by_name::<()>("list-failed", &[&error]);
            }
        }

        lock.release();
    }

    pub async fn list_files(&self, parent_window: &gtk::Window, visual: bool) {
        let files = self.files();
        if files.is_empty() || self.upcast_ref::<File>().is_local() {
            self.relist_files(parent_window, visual).await;
        } else {
            self.emit_by_name::<()>("list-ok", &[]);
        }
    }

    pub fn get_child_gfile(&self, filename: &str) -> gio::File {
        unsafe {
            from_glib_full(ffi::gnome_cmd_dir_get_child_gfile(
                self.to_glib_none().0,
                filename.to_glib_none().0,
            ))
        }
    }

    pub fn path(&self) -> &GnomeCmdPath {
        unsafe { &*ffi::gnome_cmd_dir_get_path(self.to_glib_none().0) }
    }

    pub fn file_created(&self, uri_str: &str) {
        unsafe { ffi::gnome_cmd_dir_file_created(self.to_glib_none().0, uri_str.to_glib_none().0) }
    }

    pub fn file_deleted(&self, uri_str: &str) {
        unsafe { ffi::gnome_cmd_dir_file_deleted(self.to_glib_none().0, uri_str.to_glib_none().0) }
    }

    pub fn file_changed(&self, uri_str: &str) {
        unsafe { ffi::gnome_cmd_dir_file_changed(self.to_glib_none().0, uri_str.to_glib_none().0) }
    }

    pub fn file_renamed(&self, f: &File, old_uri_str: &str) {
        unsafe {
            ffi::gnome_cmd_dir_file_renamed(
                self.to_glib_none().0,
                f.to_glib_none().0,
                old_uri_str.to_glib_none().0,
            )
        }
    }
}

struct DirectoryLock<'d>(&'d Directory);

impl<'d> DirectoryLock<'d> {
    fn try_acquire(dir: &'d Directory) -> Option<Self> {
        if unsafe { dir.data::<bool>("lock") }.is_some() {
            None
        } else {
            unsafe { dir.set_data::<bool>("lock", true) }
            Some(Self(dir))
        }
    }

    fn release(self) {
        drop(self)
    }
}

impl<'d> Drop for DirectoryLock<'d> {
    fn drop(&mut self) {
        unsafe {
            self.0.steal_data::<bool>("lock");
        }
    }
}

fn create_file_from_file_info(file_info: &gio::FileInfo, parent: &Directory) -> Option<File> {
    let name = file_info.display_name();
    if name == "." || name == ".." {
        return None;
    }

    if file_info.file_type() == gio::FileType::Directory {
        Directory::new_from_file_info(&file_info, parent).and_upcast::<File>()
    } else {
        File::new(&file_info, parent)
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_dir_relist_files(
    parent_window_ptr: *const gtk::ffi::GtkWindow,
    dir_ptr: *const ffi::GnomeCmdDir,
    visual: gboolean,
) {
    let dir: Directory = unsafe { from_glib_none(dir_ptr) };
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };

    glib::spawn_future_local(async move {
        dir.relist_files(&parent_window, visual != 0).await;
    });
}

#[no_mangle]
pub extern "C" fn gnome_cmd_dir_list_files(
    parent_window_ptr: *const gtk::ffi::GtkWindow,
    dir_ptr: *const ffi::GnomeCmdDir,
    visual: gboolean,
) {
    let dir: Directory = unsafe { from_glib_none(dir_ptr) };
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };

    glib::spawn_future_local(async move {
        dir.list_files(&parent_window, visual != 0).await;
    });
}

impl GnomeCmdFileExt for Directory {
    fn connection(&self) -> Connection {
        self.upcast_ref::<File>().connection()
    }
}
