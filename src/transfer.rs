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

use std::mem::transmute;

use crate::{dir::Directory, types::GnomeCmdConfirmOverwriteMode};
use async_channel::Sender;
use gtk::{
    gio,
    glib::{
        self,
        translate::{IntoGlib, ToGlibPtr},
    },
};

mod ffi {
    use crate::{dir::ffi::GnomeCmdDir, types::GnomeCmdConfirmOverwriteMode};
    use gtk::{gio, glib};
    use std::{ffi::c_char, os::raw::c_void};

    extern "C" {
        pub fn gnome_cmd_copy_gfiles_start(
            parent_window: *const gtk::ffi::GtkWindow,
            src_uri_list: *const glib::ffi::GList,
            to: *const GnomeCmdDir,
            dest_fn: *const c_char,
            copy_flags: gio::ffi::GFileCopyFlags,
            overwrite_mode: GnomeCmdConfirmOverwriteMode,
            on_completed_func: *const c_void,
            on_completed_data: *mut c_void,
        );

        pub fn gnome_cmd_move_gfiles_start(
            parent_window: *const gtk::ffi::GtkWindow,
            src_uri_list: *const glib::ffi::GList,
            to: *const GnomeCmdDir,
            dest_fn: *const c_char,
            copy_flags: gio::ffi::GFileCopyFlags,
            overwrite_mode: GnomeCmdConfirmOverwriteMode,
            on_completed_func: *const c_void,
            on_completed_data: *mut c_void,
        );

        pub fn gnome_cmd_link_gfiles_start(
            parent_window: *const gtk::ffi::GtkWindow,
            src_uri_list: *const glib::ffi::GList,
            to: *const GnomeCmdDir,
            dest_fn: *const c_char,
            copy_flags: gio::ffi::GFileCopyFlags,
            overwrite_mode: GnomeCmdConfirmOverwriteMode,
            on_completed_func: *const c_void,
            on_completed_data: *mut c_void,
        );

        pub fn gnome_cmd_tmp_download(
            parent_window: *const gtk::ffi::GtkWindow,
            src_gfile_list: *const glib::ffi::GList,
            dest_gfile_list: *const glib::ffi::GList,
            copy_flags: gio::ffi::GFileCopyFlags,
            on_completed_func: *const c_void,
            on_completed_data: *mut c_void,
        );
    }
}

pub async fn gnome_cmd_copy_gfiles(
    parent_window: gtk::Window,
    src_uri_list: glib::List<gio::File>,
    to: Directory,
    dest_fn: String,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: GnomeCmdConfirmOverwriteMode,
) -> bool {
    let (sender, receiver) = async_channel::bounded::<bool>(1);
    unsafe extern "C" fn send(succeess: glib::ffi::gboolean, user_data: glib::ffi::gpointer) {
        let sender = Box::<Sender<bool>>::from_raw(user_data as *mut Sender<bool>);
        let _ = sender.send_blocking(succeess != 0);
    }
    unsafe {
        ffi::gnome_cmd_copy_gfiles_start(
            parent_window.to_glib_none().0,
            src_uri_list.into_raw(),
            to.to_glib_none().0,
            dest_fn.to_glib_none().0,
            copy_flags.into_glib(),
            overwrite_mode,
            transmute(send as *const ()),
            Box::into_raw(Box::new(sender)) as *mut _,
        )
    }
    let result = receiver.recv().await.unwrap_or_default();
    result
}

pub fn gnome_cmd_move_gfiles_start<F: Fn(bool) + 'static>(
    parent_window: &gtk::Window,
    src_uri_list: &glib::List<gio::File>,
    to: &Directory,
    dest_fn: &str,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: GnomeCmdConfirmOverwriteMode,
    on_completed_func: F,
) {
    unsafe extern "C" fn trampoline<F: Fn(bool) + 'static>(
        succeess: glib::ffi::gboolean,
        user_data: glib::ffi::gpointer,
    ) {
        let f = Box::<F>::from_raw(user_data as *mut F);
        f(succeess != 0)
    }
    unsafe {
        let f: Box<F> = Box::new(on_completed_func);
        ffi::gnome_cmd_move_gfiles_start(
            parent_window.to_glib_none().0,
            src_uri_list.to_glib_none().0,
            to.to_glib_none().0,
            dest_fn.to_glib_none().0,
            copy_flags.into_glib(),
            overwrite_mode,
            transmute(trampoline::<F> as *const ()),
            Box::into_raw(f) as *mut _,
        )
    }
}

pub fn gnome_cmd_link_gfiles_start<F: Fn(bool) + 'static>(
    parent_window: &gtk::Window,
    src_uri_list: &glib::List<gio::File>,
    to: &Directory,
    dest_fn: &str,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: GnomeCmdConfirmOverwriteMode,
    on_completed_func: F,
) {
    unsafe extern "C" fn trampoline<F: Fn(bool) + 'static>(
        succeess: glib::ffi::gboolean,
        user_data: glib::ffi::gpointer,
    ) {
        let f = Box::<F>::from_raw(user_data as *mut F);
        f(succeess != 0)
    }
    unsafe {
        let f: Box<F> = Box::new(on_completed_func);
        ffi::gnome_cmd_link_gfiles_start(
            parent_window.to_glib_none().0,
            src_uri_list.to_glib_none().0,
            to.to_glib_none().0,
            dest_fn.to_glib_none().0,
            copy_flags.into_glib(),
            overwrite_mode,
            transmute(trampoline::<F> as *const ()),
            Box::into_raw(f) as *mut _,
        )
    }
}

pub async fn gnome_cmd_tmp_download(
    parent_window: gtk::Window,
    src_gfile_list: glib::List<gio::File>,
    dest_gfile_list: glib::List<gio::File>,
    copy_flags: gio::FileCopyFlags,
) -> bool {
    let (sender, receiver) = async_channel::bounded::<bool>(1);
    unsafe extern "C" fn send(succeess: glib::ffi::gboolean, user_data: glib::ffi::gpointer) {
        let sender = Box::<Sender<bool>>::from_raw(user_data as *mut Sender<bool>);
        let _ = sender.send_blocking(succeess != 0);
    }
    unsafe {
        ffi::gnome_cmd_tmp_download(
            parent_window.to_glib_none().0,
            src_gfile_list.into_raw(),
            dest_gfile_list.into_raw(),
            copy_flags.into_glib(),
            transmute(send as *const ()),
            Box::into_raw(Box::new(sender)) as *mut _,
        )
    }
    let result = receiver.recv().await.unwrap_or_default();
    result
}
