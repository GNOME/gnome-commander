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
    main_win::{ffi::*, MainWindow},
    types::FileSelectorID,
    utils::run_simple_dialog,
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gio::ffi::GSimpleAction,
    glib::{self, ffi::GVariant, translate::FromGlibPtrNone, Cast},
};

#[no_mangle]
pub extern "C" fn file_create_symlink(
    _action: *const GSimpleAction,
    _parameter: *const GVariant,
    main_win_ptr: *mut GnomeCmdMainWin,
) {
    let main_win = unsafe { MainWindow::from_glib_none(main_win_ptr) };

    glib::MainContext::default().spawn_local(async move {
        let active_fs = main_win.file_selector(FileSelectorID::ACTIVE);
        let inactive_fs = main_win.file_selector(FileSelectorID::INACTIVE);

        let active_fl = active_fs.file_list();
        let selected_files = active_fl.selected_files();
        let selected_files_len = selected_files.len();

        if selected_files_len > 1 {
            let directory = inactive_fs.directory().unwrap().display_path();
            let message = ngettext!(
                "Create symbolic links of {} file in {}?",
                "Create symbolic links of {} files in {}?",
                selected_files_len as u32,
                selected_files_len,
                directory
            );
            let choice = run_simple_dialog(
                main_win.upcast_ref(),
                true,
                gtk::MessageType::Question,
                &message,
                &gettext("Create Symbolic Link"),
                Some(1),
                &[&gettext("Cancel"), &gettext("Create")],
            )
            .await;
            if choice == gtk::ResponseType::Other(1) {
                inactive_fs.create_symlinks(&selected_files);
            }
        } else if let Some(focused_file) = active_fl.focused_file() {
            inactive_fs.create_symlink(&focused_file);
        }
    });
}
