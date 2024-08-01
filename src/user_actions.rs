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
    dir::*, file_list::*, file_selector::*, main_win::*, types::FileSelectorID,
    utils::run_simple_dialog,
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gio::ffi::GSimpleAction,
    glib::{
        self,
        ffi::{g_list_length, GVariant},
        translate::FromGlibPtrNone,
    },
};
use std::ffi::CStr;

#[no_mangle]
pub extern "C" fn file_create_symlink(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { gtk::Window::from_glib_none(main_win_ptr as *mut gtk::ffi::GtkWindow) };

    glib::MainContext::default().spawn_local(async move {
        let active_fs = unsafe { gnome_cmd_main_win_get_fs(main_win_ptr, FileSelectorID::ACTIVE) };
        let inactive_fs =
            unsafe { gnome_cmd_main_win_get_fs(main_win_ptr, FileSelectorID::INACTIVE) };

        let active_fl = unsafe { gnome_cmd_file_selector_file_list(active_fs) };
        let f = unsafe { gnome_cmd_file_list_get_selected_files(active_fl) };
        let selected_files = unsafe { g_list_length(f) };

        if selected_files > 1 {
            let message_template = ngettext(
                "Create symbolic links of %i file in %s?",
                "Create symbolic links of %i files in %s?",
                selected_files,
            );
            let directory_cstr = unsafe {
                gnome_cmd_dir_get_display_path(gnome_cmd_file_selector_get_directory(inactive_fs))
            };
            let directory = unsafe { CStr::from_ptr(directory_cstr).to_string_lossy() };
            let msg = message_template
                .replace("%i", &selected_files.to_string())
                .replace("%s", &directory);

            let choice = run_simple_dialog(
                &main_win,
                true,
                gtk::MessageType::Question,
                &msg,
                &gettext("Create Symbolic Link"),
                Some(1),
                &[&gettext("Cancel"), &gettext("Create")],
            )
            .await;
            if choice == gtk::ResponseType::Other(1) {
                unsafe {
                    gnome_cmd_file_selector_create_symlinks(inactive_fs, f);
                }
            }
        } else {
            let focused_f = unsafe { gnome_cmd_file_list_get_focused_file(active_fl) };
            unsafe { gnome_cmd_file_selector_create_symlink(inactive_fs, focused_f) };
        }
    });
}
