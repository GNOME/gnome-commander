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

use super::prepare_transfer_dialog::PrepareTransferDialog;
use crate::{
    data::{ConfirmOptions, ConfirmOptionsRead},
    file_selector::{ffi::GnomeCmdFileSelector, FileSelector},
    libgcmd::file_base::FileBaseExt,
    main_win::{ffi::GnomeCmdMainWin, MainWindow},
    transfer::gnome_cmd_move_gfiles,
    types::GnomeCmdConfirmOverwriteMode,
    utils::bold,
};
use gettextrs::{gettext, ngettext};
use glib::translate::from_glib_none;
use gtk::{gio, glib, prelude::*};

pub async fn prepare_move_dialog_show(
    main_win: &MainWindow,
    from: &FileSelector,
    to: &FileSelector,
    options: &dyn ConfirmOptionsRead,
) {
    let src_files = from.file_list().selected_files();
    let num_files = src_files.len();
    let single_source_file = match num_files {
        0 => return,
        1 => src_files.front(),
        _ => None,
    };

    let dialog = PrepareTransferDialog::new(from, to);
    dialog.set_transient_for(Some(main_win));
    dialog.set_title(Some(&gettext("Move")));

    let label = if let Some(file) = single_source_file {
        gettext!("Move “{}” to", file.get_name())
    } else {
        ngettext!(
            "move {} file to",
            "move {} files to",
            num_files as u32,
            num_files
        )
    };
    dialog.set_dst_label(&label);

    dialog.append_to_left(
        &gtk::Label::builder()
            .label(bold(&gettext("Overwrite Files")))
            .use_markup(true)
            .halign(gtk::Align::Start)
            .build(),
    );

    let query = gtk::CheckButton::builder()
        .label(gettext("Query First"))
        .build();
    dialog.append_to_left(&query);

    let rename = gtk::CheckButton::builder()
        .label(gettext("Rename"))
        .group(&query)
        .build();
    dialog.append_to_left(&rename);

    let skip = gtk::CheckButton::builder()
        .label(gettext("Skip"))
        .group(&query)
        .build();
    dialog.append_to_left(&skip);

    let silent = gtk::CheckButton::builder()
        .label(gettext("Overwrite silently"))
        .group(&query)
        .build();
    dialog.append_to_left(&silent);

    match options.confirm_move_overwrite() {
        GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY => &silent,
        GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL => &skip,
        GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL => &rename,
        GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY => &query,
    }
    .set_active(true);

    let Some((dest_dir, dest_fn)) = dialog.run().await else {
        return;
    };

    let mut copy_flags = gio::FileCopyFlags::NONE;
    let overwrite_mode: GnomeCmdConfirmOverwriteMode;

    if silent.is_active() {
        overwrite_mode = GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY;
        copy_flags |= gio::FileCopyFlags::OVERWRITE;
    } else if query.is_active() {
        overwrite_mode = GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY;
    } else if rename.is_active() {
        overwrite_mode = GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL;
    } else {
        overwrite_mode = GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL;
    }

    let _transfer_result = gnome_cmd_move_gfiles(
        main_win.clone().upcast(),
        src_files.iter().map(|f| f.file()).collect(),
        dest_dir.clone(),
        dest_fn,
        copy_flags,
        overwrite_mode,
    )
    .await;
    dest_dir.relist_files(main_win.upcast_ref(), false).await;
    main_win.focus_file_lists();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_prepare_move_dialog_show(
    main_win_ptr: *mut GnomeCmdMainWin,
    from_ptr: *mut GnomeCmdFileSelector,
    to_ptr: *mut GnomeCmdFileSelector,
) {
    let main_win = unsafe { from_glib_none(main_win_ptr) };
    let from = unsafe { from_glib_none(from_ptr) };
    let to = unsafe { from_glib_none(to_ptr) };
    let options = ConfirmOptions::new();
    glib::spawn_future_local(async move {
        prepare_move_dialog_show(&main_win, &from, &to, &options).await;
    });
}
