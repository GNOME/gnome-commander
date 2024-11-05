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

use crate::{connection::connection::Connection, dir::Directory, file::File};
use gtk::{
    glib::{
        self,
        translate::{from_glib_none, ToGlibPtr},
    },
    prelude::*,
};

pub mod ffi {
    use super::*;
    use crate::{connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir};
    use glib::ffi::{gboolean, GList, GType};

    #[repr(C)]
    pub struct GnomeCmdFileList {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_file_list_get_type() -> GType;

        pub fn gnome_cmd_file_list_get_selected_files(fl: *mut GnomeCmdFileList) -> *mut GList;

        pub fn gnome_cmd_file_list_get_cwd(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_file_list_get_connection(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_file_list_get_directory(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdDir;

        pub fn gnome_cmd_file_list_is_locked(fl: *mut GnomeCmdFileList) -> gboolean;

        pub fn gnome_cmd_file_list_reload(fl: *mut GnomeCmdFileList);

        pub fn gnome_cmd_file_list_set_connection(
            fl: *mut GnomeCmdFileList,
            con: *mut GnomeCmdCon,
            start_dir: *mut GnomeCmdDir,
        );

    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileListClass {
        pub parent_class: gtk::ffi::GtkTreeViewClass,
    }
}

glib::wrapper! {
    pub struct FileList(Object<ffi::GnomeCmdFileList, ffi::GnomeCmdFileListClass>)
        @extends gtk::TreeView, gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_file_list_get_type(),
    }
}

#[allow(non_camel_case_types)]
enum ColumnID {
    COLUMN_ICON = 0,
    COLUMN_NAME,
    COLUMN_EXT,
    COLUMN_DIR,
    COLUMN_SIZE,
    COLUMN_DATE,
    COLUMN_PERM,
    COLUMN_OWNER,
    COLUMN_GROUP,
}

#[allow(non_camel_case_types)]
enum DataColumns {
    DATA_COLUMN_FILE = 9,
    DATA_COLUMN_ICON_NAME,
    DATA_COLUMN_SELECTED,
}

impl FileList {
    pub fn selected_files(&self) -> glib::List<File> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_file_list_get_selected_files(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn cwd(&self) -> Option<Directory> {
        unsafe { from_glib_none(ffi::gnome_cmd_file_list_get_cwd(self.to_glib_none().0)) }
    }

    pub fn is_locked(&self) -> bool {
        unsafe { ffi::gnome_cmd_file_list_is_locked(self.to_glib_none().0) != 0 }
    }

    pub fn file_at_row(&self, iter: &gtk::TreeIter) -> Option<File> {
        self.model()?
            .get_value(iter, DataColumns::DATA_COLUMN_FILE as i32)
            .get()
            .ok()
    }

    pub fn focused_file_iter(&self) -> Option<gtk::TreeIter> {
        let model = self.model()?;
        let path = TreeViewExt::cursor(self).0?;
        let iter = model.iter(&path)?;
        Some(iter)
    }

    pub fn focused_file(&self) -> Option<File> {
        let iter = self.focused_file_iter()?;
        let file = self.file_at_row(&iter)?;
        Some(file)
    }

    pub fn selected_file(&self) -> Option<File> {
        self.focused_file().filter(|f| !f.is_dotdot())
    }

    pub fn connection(&self) -> Option<Connection> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_connection(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn directory(&self) -> Option<Directory> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_directory(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn reload(&self) {
        unsafe { ffi::gnome_cmd_file_list_reload(self.to_glib_none().0) }
    }

    pub fn set_connection(&self, connection: &Connection, start_dir: Option<&Directory>) {
        unsafe {
            ffi::gnome_cmd_file_list_set_connection(
                self.to_glib_none().0,
                connection.to_glib_none().0,
                start_dir.to_glib_none().0,
            )
        }
    }
}
