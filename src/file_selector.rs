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

use crate::{dir::Directory, file::File, file_list::FileList};
use gtk::glib::{
    self,
    translate::{from_glib_none, ToGlibPtr},
};

pub mod ffi {
    use crate::{dir::ffi::GnomeCmdDir, file::ffi::GnomeCmdFile, file_list::ffi::GnomeCmdFileList};
    use gtk::glib::ffi::{GList, GType};

    #[repr(C)]
    pub struct GnomeCmdFileSelector {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_file_selector_get_type() -> GType;

        pub fn gnome_cmd_file_selector_file_list(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GnomeCmdFileList;

        pub fn gnome_cmd_file_selector_get_directory(
            fs: *mut GnomeCmdFileSelector,
        ) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_file_selector_create_symlink(
            fs: *mut GnomeCmdFileSelector,
            f: *mut GnomeCmdFile,
        );

        pub fn gnome_cmd_file_selector_create_symlinks(
            fs: *mut GnomeCmdFileSelector,
            files: *mut GList,
        );
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileSelectorClass {
        pub parent_class: gtk::ffi::GtkBoxClass,
    }
}

glib::wrapper! {
    pub struct FileSelector(Object<ffi::GnomeCmdFileSelector, ffi::GnomeCmdFileSelectorClass>)
        @extends gtk::Box, gtk::Container, gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_file_selector_get_type(),
    }
}

impl FileSelector {
    pub fn file_list(&self) -> FileList {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_file_list(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn directory(&self) -> Option<Directory> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_selector_get_directory(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn create_symlink(&self, file: &File) {
        unsafe {
            ffi::gnome_cmd_file_selector_create_symlink(
                self.to_glib_none().0,
                file.to_glib_none().0,
            )
        }
    }

    pub fn create_symlinks(&self, files: &glib::List<File>) {
        unsafe {
            ffi::gnome_cmd_file_selector_create_symlinks(
                self.to_glib_none().0,
                files.to_glib_none().0,
            )
        }
    }
}
