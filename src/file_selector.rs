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

use crate::{dir::GnomeCmdDir, file::GnomeCmdFile, file_list::GnomeCmdFileList};
use gtk::glib::ffi::GList;

#[repr(C)]
pub struct GnomeCmdFileSelector(
    [u8; 0],
    std::marker::PhantomData<std::marker::PhantomPinned>,
);

extern "C" {
    pub fn gnome_cmd_file_selector_file_list(
        fs: *mut GnomeCmdFileSelector,
    ) -> *mut GnomeCmdFileList;

    pub fn gnome_cmd_file_selector_get_directory(fs: *mut GnomeCmdFileSelector)
        -> *mut GnomeCmdDir;

    pub fn gnome_cmd_file_selector_create_symlink(
        fs: *mut GnomeCmdFileSelector,
        f: *mut GnomeCmdFile,
    );

    pub fn gnome_cmd_file_selector_create_symlinks(
        fs: *mut GnomeCmdFileSelector,
        files: *mut GList,
    );
}
