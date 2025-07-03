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

use super::{
    bookmark::Bookmark, device::ConnectionDevice, history::History, remote::ConnectionRemote,
};
use crate::{dir::Directory, path::GnomeCmdPath};
use gtk::{
    gio::{
        self,
        ffi::{GFileType, G_FILE_TYPE_UNKNOWN},
    },
    glib::{self, ffi::GStrv, translate::*},
    prelude::*,
};
use std::{ffi::c_char, path::Path, ptr, sync::LazyLock};

pub mod ffi {
    use super::*;
    use crate::dir::ffi::GnomeCmdDir;
    use gtk::{
        ffi::GtkWindow,
        gio::ffi::{GFile, GFileType, GIcon, GListModel},
        glib::ffi::{gboolean, GError, GType, GUri},
    };
    use std::ffi::c_char;

    #[repr(C)]
    pub struct GnomeCmdCon {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gnome_cmd_con_get_type() -> GType;
        pub fn gnome_cmd_con_get_uuid(con: *const GnomeCmdCon) -> *const c_char;

        pub fn gnome_cmd_con_get_alias(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_set_alias(con: *const GnomeCmdCon, alias: *const c_char);

        pub fn gnome_cmd_con_get_uri(con: *const GnomeCmdCon) -> *const GUri;
        pub fn gnome_cmd_con_set_uri(con: *const GnomeCmdCon, uri: *const GUri);

        pub fn gnome_cmd_con_get_uri_string(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_set_uri_string(con: *const GnomeCmdCon, uri: *const c_char);

        pub fn gnome_cmd_con_create_path(
            con: *const GnomeCmdCon,
            path_str: *const c_char,
        ) -> *mut GnomeCmdPath;

        pub fn gnome_cmd_con_create_gfile(
            con: *const GnomeCmdCon,
            uri: *const c_char,
        ) -> *mut GFile;

        pub fn gnome_cmd_con_get_default_dir(con: *const GnomeCmdCon) -> *const GnomeCmdDir;
        pub fn gnome_cmd_con_set_default_dir(con: *const GnomeCmdCon, dir: *mut GnomeCmdDir);

        pub fn gnome_cmd_con_can_show_free_space(con: *const GnomeCmdCon) -> gboolean;

        pub fn gnome_cmd_con_set_base_path(con: *const GnomeCmdCon, path: *mut GnomeCmdPath);

        pub fn gnome_cmd_con_is_local(con: *const GnomeCmdCon) -> gboolean;

        pub fn gnome_cmd_con_is_open(con: *const GnomeCmdCon) -> gboolean;

        pub fn gnome_cmd_con_add_bookmark(
            con: *const GnomeCmdCon,
            bookmark: *const <Bookmark as glib::object::ObjectType>::GlibType,
        );

        pub fn gnome_cmd_con_erase_bookmarks(con: *mut GnomeCmdCon);

        pub fn gnome_cmd_con_get_bookmarks(con: *mut GnomeCmdCon) -> *const GListModel;

        pub fn gnome_cmd_con_get_path_target_type(
            con: *const GnomeCmdCon,
            path: *const c_char,
            ftype: *mut GFileType,
        ) -> gboolean;

        pub fn gnome_cmd_con_mkdir(
            con: *const GnomeCmdCon,
            path_str: *const c_char,
            error: *mut *mut GError,
        ) -> gboolean;

        pub fn gnome_cmd_con_get_open_msg(con: *mut GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_set_open_msg(con: *mut GnomeCmdCon, message: *const c_char);
        pub fn gnome_cmd_con_open(con: *mut GnomeCmdCon, parent_window: *mut GtkWindow);
        pub fn gnome_cmd_con_cancel_open(con: *mut GnomeCmdCon);

        pub fn gnome_cmd_con_is_closeable(con: *mut GnomeCmdCon) -> gboolean;
        pub fn gnome_cmd_con_close(con: *mut GnomeCmdCon, parent_window: *mut GtkWindow);

        pub fn gnome_cmd_con_get_go_text(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_get_open_text(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_get_close_text(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_get_go_tooltip(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_get_open_tooltip(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_get_close_tooltip(con: *const GnomeCmdCon) -> *const c_char;
        pub fn gnome_cmd_con_get_go_icon(con: *const GnomeCmdCon) -> *mut GIcon;
        pub fn gnome_cmd_con_get_open_icon(con: *const GnomeCmdCon) -> *mut GIcon;
        pub fn gnome_cmd_con_get_close_icon(con: *const GnomeCmdCon) -> *mut GIcon;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdConClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct Connection(Object<ffi::GnomeCmdCon, ffi::GnomeCmdConClass>);

    match fn {
        type_ => || ffi::gnome_cmd_con_get_type(),
    }
}

struct ConnectionPrivate {
    history: History<String>, // TODO: consider Rc<GnomeCmdPath>
}

impl Default for ConnectionPrivate {
    fn default() -> Self {
        Self {
            history: History::new(20),
        }
    }
}

impl Connection {
    fn private(&self) -> &mut ConnectionPrivate {
        static QUARK: LazyLock<glib::Quark> =
            LazyLock::new(|| glib::Quark::from_str("connection-private"));

        unsafe {
            if let Some(mut private) = self.qdata::<ConnectionPrivate>(*QUARK) {
                private.as_mut()
            } else {
                self.set_qdata(*QUARK, ConnectionPrivate::default());
                self.qdata::<ConnectionPrivate>(*QUARK).unwrap().as_mut()
            }
        }
    }

    pub fn dir_history(&self) -> &History<String> {
        &self.private().history
    }

    pub fn find_mount(&self) -> Option<gio::Mount> {
        if let Some(device) = self.downcast_ref::<ConnectionDevice>() {
            if let Some(mount_path) = device.mountp_string() {
                let file = gio::File::for_path(mount_path);
                file.find_enclosing_mount(gio::Cancellable::NONE).ok()
            } else {
                device.mount()
            }
        } else if let Some(remote) = self.downcast_ref::<ConnectionRemote>() {
            let file = gio::File::for_uri(&remote.uri_string()?);
            file.find_enclosing_mount(gio::Cancellable::NONE).ok()
        } else {
            None
        }
    }
}

pub trait ConnectionExt: IsA<Connection> + 'static {
    fn uuid(&self) -> String {
        unsafe { from_glib_none(ffi::gnome_cmd_con_get_uuid(self.as_ref().to_glib_none().0)) }
    }

    fn alias(&self) -> Option<String> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_get_alias(self.as_ref().to_glib_none().0)) }
    }

    fn set_alias(&self, alias: Option<&str>) {
        unsafe {
            ffi::gnome_cmd_con_set_alias(self.as_ref().to_glib_none().0, alias.to_glib_none().0)
        }
    }

    fn uri(&self) -> Option<glib::Uri> {
        unsafe { from_glib_none(ffi::gnome_cmd_con_get_uri(self.as_ref().to_glib_none().0)) }
    }

    fn set_uri(&self, uri: Option<&glib::Uri>) {
        unsafe { ffi::gnome_cmd_con_set_uri(self.as_ref().to_glib_none().0, uri.to_glib_none().0) }
    }

    fn uri_string(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_uri_string(
                self.as_ref().to_glib_none().0,
            ))
        }
    }

    fn set_uri_string(&self, uri: Option<&str>) {
        unsafe {
            ffi::gnome_cmd_con_set_uri_string(self.as_ref().to_glib_none().0, uri.to_glib_none().0)
        }
    }

    fn create_path(&self, path: &Path) -> GnomeCmdPath {
        unsafe {
            GnomeCmdPath::from_raw(ffi::gnome_cmd_con_create_path(
                self.as_ref().to_glib_none().0,
                path.to_glib_none().0,
            ))
        }
    }

    fn create_gfile(&self, path: Option<&Path>) -> gio::File {
        unsafe {
            from_glib_full(ffi::gnome_cmd_con_create_gfile(
                self.as_ref().to_glib_none().0,
                path.to_glib_none().0,
            ))
        }
    }

    fn default_dir(&self) -> Option<Directory> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_default_dir(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn set_default_dir(&self, dir: Option<&Directory>) {
        unsafe {
            ffi::gnome_cmd_con_set_default_dir(self.as_ref().to_glib_none().0, dir.to_glib_none().0)
        }
    }

    fn set_base_path(&self, path: GnomeCmdPath) {
        unsafe { ffi::gnome_cmd_con_set_base_path(self.as_ref().to_glib_none().0, path.into_raw()) }
    }

    fn is_local(&self) -> bool {
        unsafe { ffi::gnome_cmd_con_is_local(self.as_ref().to_glib_none().0) != 0 }
    }

    fn is_open(&self) -> bool {
        unsafe { ffi::gnome_cmd_con_is_open(self.as_ref().to_glib_none().0) != 0 }
    }

    fn is_closeable(&self) -> bool {
        unsafe { ffi::gnome_cmd_con_is_closeable(self.as_ref().to_glib_none().0) != 0 }
    }

    fn close(&self, parent_window: Option<&gtk::Window>) {
        unsafe {
            ffi::gnome_cmd_con_close(
                self.as_ref().to_glib_none().0,
                parent_window.to_glib_none().0,
            )
        }
    }

    fn can_show_free_space(&self) -> bool {
        unsafe { ffi::gnome_cmd_con_can_show_free_space(self.as_ref().to_glib_none().0) != 0 }
    }

    fn add_bookmark(&self, bookmark: &Bookmark) {
        unsafe {
            ffi::gnome_cmd_con_add_bookmark(self.as_ref().to_glib_none().0, bookmark.as_ptr())
        }
    }

    fn erase_bookmarks(&self) {
        unsafe { ffi::gnome_cmd_con_erase_bookmarks(self.as_ref().to_glib_none().0) }
    }

    fn bookmarks(&self) -> gio::ListModel {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_bookmarks(
                self.as_ref().to_glib_none().0,
            ))
        }
    }

    fn replace_bookmark(&self, old_bookmark: &Bookmark, new_bookmark: Bookmark) {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(old_bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return;
        };
        bookmarks.splice(position, 1, &[new_bookmark]);
    }

    fn move_bookmark_up(&self, bookmark: &Bookmark) -> Option<u32> {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return None;
        };
        if position > 0 {
            bookmarks.remove(position);
            bookmarks.insert(position - 1, bookmark);
            Some(position - 1)
        } else {
            None
        }
    }

    fn move_bookmark_down(&self, bookmark: &Bookmark) -> Option<u32> {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return None;
        };
        if position + 1 < bookmarks.n_items() {
            bookmarks.remove(position);
            bookmarks.insert(position + 1, bookmark);
            Some(position + 1)
        } else {
            None
        }
    }

    fn remove_bookmark(&self, bookmark: &Bookmark) {
        let bookmarks = self.bookmarks().downcast::<gio::ListStore>().unwrap();
        let Some(position): Option<u32> = bookmarks
            .iter()
            .position(|b| b.as_ref() == Ok(bookmark))
            .and_then(|p| p.try_into().ok())
        else {
            return;
        };
        bookmarks.remove(position);
    }

    fn path_target_type(&self, path: &Path) -> Option<gio::FileType> {
        let mut file_type: GFileType = G_FILE_TYPE_UNKNOWN;
        let result = unsafe {
            ffi::gnome_cmd_con_get_path_target_type(
                self.as_ref().to_glib_none().0,
                path.to_glib_none().0,
                &mut file_type as *mut _,
            )
        };
        if result != 0 {
            Some(unsafe { gio::FileType::from_glib(file_type) })
        } else {
            None
        }
    }

    fn mkdir(&self, path: &Path) -> Result<(), glib::Error> {
        unsafe {
            let mut error = ptr::null_mut();
            let _is_ok = ffi::gnome_cmd_con_mkdir(
                self.as_ref().to_glib_none().0,
                path.to_glib_none().0,
                &mut error,
            );
            if error.is_null() {
                Ok(())
            } else {
                Err(from_glib_full(error))
            }
        }
    }

    fn go_text(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_go_text(
                self.as_ref().to_glib_none().0,
            ))
        }
    }

    fn open_text(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_open_text(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn close_text(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_close_text(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn go_tooltip(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_go_tooltip(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn open_tooltip(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_open_tooltip(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn close_tooltip(&self) -> Option<String> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_close_tooltip(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn go_icon(&self) -> Option<gio::Icon> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_go_icon(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn open_icon(&self) -> Option<gio::Icon> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_open_icon(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn close_icon(&self) -> Option<gio::Icon> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_close_icon(
                self.as_ref().to_glib_none().0,
            ))
        }
    }

    fn open_message(&self) -> String {
        unsafe {
            from_glib_none(ffi::gnome_cmd_con_get_open_msg(
                self.as_ref().to_glib_none().0,
            ))
        }
    }

    fn set_open_message(&self, message: &str) {
        unsafe {
            ffi::gnome_cmd_con_set_open_msg(
                self.as_ref().to_glib_none().0,
                message.to_glib_none().0,
            )
        }
    }

    fn connect_updated<F: Fn() + 'static>(&self, f: F) -> glib::SignalHandlerId {
        self.connect_local("updated", false, move |_| {
            (f)();
            None
        })
    }

    fn open(&self, parent_window: &gtk::Window) {
        unsafe {
            ffi::gnome_cmd_con_open(
                self.as_ref().to_glib_none().0,
                parent_window.to_glib_none().0,
            )
        };
    }

    fn cancel_open(&self) {
        unsafe { ffi::gnome_cmd_con_cancel_open(self.as_ref().to_glib_none().0) };
    }
}

impl<O: IsA<Connection>> ConnectionExt for O {}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_dir_history_add(con: *mut ffi::GnomeCmdCon, entry: *const c_char) {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let entry: Option<String> = unsafe { from_glib_none(entry) };
    if let Some(entry) = entry.filter(|s| !s.is_empty()) {
        con.dir_history().add(entry);
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_con_export_dir_history(con: *mut ffi::GnomeCmdCon) -> GStrv {
    let con: Borrowed<Connection> = unsafe { from_glib_borrow(con) };
    let dir_history: glib::StrV = con
        .dir_history()
        .export()
        .into_iter()
        .map(|s| s.into())
        .collect();
    dir_history.to_glib_full()
}
