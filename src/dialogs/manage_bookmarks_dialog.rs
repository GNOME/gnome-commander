/*
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

use super::edit_bookmark_dialog::{edit_bookmark_dialog, Bookmark};
use crate::{
    connection::{connection::ConnectionExt, list::ConnectionList},
    data::{GeneralOptions, GeneralOptionsWrite},
    dir::{ffi::GnomeCmdDir, Directory},
    file::{File, GnomeCmdFileExt},
    main_win::{ffi::GnomeCmdMainWin, MainWindow},
    utils::show_message,
};
use gettextrs::gettext;
use gtk::{
    glib::{
        self,
        translate::{from_glib_none, ToGlibPtr},
    },
    prelude::*,
};

pub mod ffi {
    use super::*;
    use glib::ffi::GType;
    use gtk::ffi::{GtkTreeIter, GtkTreeView};
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdBookmarksDialog {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_bookmarks_dialog_get_type() -> GType;

        pub fn gnome_cmd_bookmarks_dialog_get_view(
            dialog: *mut GnomeCmdBookmarksDialog,
        ) -> *mut GtkTreeView;

        pub fn gnome_cmd_bookmarks_dialog_update_bookmark(
            dialog: *mut GnomeCmdBookmarksDialog,
            iter: *const GtkTreeIter,
            name: *const c_char,
            path: *const c_char,
        );
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdBookmarksDialogClass {
        pub parent_class: gtk::ffi::GtkDialog,
    }
}

glib::wrapper! {
    pub struct BookmarksDialog(Object<ffi::GnomeCmdBookmarksDialog, ffi::GnomeCmdBookmarksDialogClass>)
        @extends gtk::Dialog, gtk::Window, gtk::Widget;

    match fn {
        type_ => || ffi::gnome_cmd_bookmarks_dialog_get_type(),
    }
}

#[repr(C)]
#[allow(non_camel_case_types)]
enum Columns {
    COL_GROUP = 0,
    COL_NAME,
    COL_PATH,
    COL_SHORTCUT,
    COL_BOOKMARK,
}

impl BookmarksDialog {
    fn view(&self) -> gtk::TreeView {
        unsafe {
            from_glib_none(ffi::gnome_cmd_bookmarks_dialog_get_view(
                self.to_glib_none().0,
            ))
        }
    }

    fn update_bookmark(&self, iter: &gtk::TreeIter, bookmark: &Bookmark) {
        unsafe {
            ffi::gnome_cmd_bookmarks_dialog_update_bookmark(
                self.to_glib_none().0,
                iter.to_glib_none().0,
                bookmark.name.to_glib_none().0,
                bookmark.path.to_glib_none().0,
            )
        }
    }

    async fn edit_clicked(&self) {
        let selection = self.view().selection();

        if let Some((model, iter)) = selection.selected() {
            let bookmark = Bookmark {
                name: model
                    .value(&iter, Columns::COL_NAME as i32)
                    .get()
                    .unwrap_or_default(),
                path: model
                    .value(&iter, Columns::COL_PATH as i32)
                    .get()
                    .unwrap_or_default(),
            };

            if let Some(changed_bookmark) =
                edit_bookmark_dialog(self.upcast_ref(), &gettext("Edit Bookmark"), &bookmark).await
            {
                self.update_bookmark(&iter, &changed_bookmark);
            }
        }
    }
}

#[no_mangle]
fn gnome_cmd_bookmarks_dialog_edit_clicked_r(dialog_ptr: *mut ffi::GnomeCmdBookmarksDialog) {
    let dialog: BookmarksDialog = unsafe { from_glib_none(dialog_ptr) };

    glib::MainContext::default().spawn_local(async move {
        dialog.edit_clicked().await;
    });
}

pub async fn bookmark_directory(
    main_win: &MainWindow,
    dir: &Directory,
    options: &dyn GeneralOptionsWrite,
) {
    let file = dir.upcast_ref::<File>();
    let is_local = file.is_local();
    let path = if is_local {
        file.get_real_path()
    } else {
        file.get_path_string_through_parent()
    };

    let Some(path_str) = path.to_str() else {
        show_message(main_win.upcast_ref(), &gettext("To bookmark a directory the whole search path to the directory must be in valid UTF-8 encoding"), None);
        return;
    };

    let bookmark = Bookmark {
        name: path
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or_default()
            .to_owned(),
        path: path_str.to_owned(),
    };

    if let Some(changed_bookmark) =
        edit_bookmark_dialog(main_win.upcast_ref(), &gettext("New Bookmark"), &bookmark).await
    {
        let connection_list = ConnectionList::get();

        let con = if is_local {
            connection_list.home()
        } else {
            file.connection()
        };
        con.add_bookmark(&changed_bookmark.name, &changed_bookmark.path);

        main_win.update_bookmarks();

        if let Some(bookmarks) = connection_list.save_bookmarks() {
            options.set_bookmarks(&bookmarks);
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_bookmark_add_current(
    main_win_ptr: *mut GnomeCmdMainWin,
    dir_ptr: *mut GnomeCmdDir,
) {
    let main_win: MainWindow = unsafe { from_glib_none(main_win_ptr) };
    let dir: Directory = unsafe { from_glib_none(dir_ptr) };
    let options = GeneralOptions::new();

    glib::MainContext::default().spawn_local(async move {
        bookmark_directory(&main_win, &dir, &options).await;
    });
}
