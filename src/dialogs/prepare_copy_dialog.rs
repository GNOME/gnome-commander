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
    file_selector::FileSelector, main_win::MainWindow, options::options::ConfirmOptions,
    transfer::copy_files, types::ConfirmOverwriteMode, utils::bold,
};
use gettextrs::{gettext, ngettext};
use gtk::{gio, prelude::*};
use std::path::PathBuf;

pub async fn prepare_copy_dialog_show(
    main_win: &MainWindow,
    from: &FileSelector,
    to: &FileSelector,
    options: &ConfirmOptions,
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
    dialog.set_title(Some(&gettext("Copy")));

    let label = if let Some(file) = single_source_file {
        gettext("Copy “{}” to").replace("{}", &file.get_name())
    } else {
        ngettext("copy {} file to", "copy {} files to", num_files as u32)
            .replace("{}", &num_files.to_string())
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

    match options.confirm_copy_overwrite.get() {
        ConfirmOverwriteMode::Silently => &silent,
        ConfirmOverwriteMode::SkipAll => &skip,
        ConfirmOverwriteMode::RenameAll => &rename,
        ConfirmOverwriteMode::Query => &query,
    }
    .set_active(true);

    dialog.append_to_right(
        &gtk::Label::builder()
            .label(bold(&gettext("Options")))
            .halign(gtk::Align::Start)
            .use_markup(true)
            .build(),
    );

    let follow_links = gtk::CheckButton::builder()
        .label(gettext("Follow Links"))
        .build();
    dialog.append_to_right(&follow_links);

    let Some((dest_dir, dest_fn)) = dialog.run().await else {
        return;
    };

    let mut copy_flags = gio::FileCopyFlags::NONE;
    let overwrite_mode: ConfirmOverwriteMode;

    if silent.is_active() {
        overwrite_mode = ConfirmOverwriteMode::Silently;
        copy_flags |= gio::FileCopyFlags::OVERWRITE;
    } else if query.is_active() {
        overwrite_mode = ConfirmOverwriteMode::Query;
    } else if rename.is_active() {
        overwrite_mode = ConfirmOverwriteMode::RenameAll;
    } else {
        overwrite_mode = ConfirmOverwriteMode::SkipAll;
    }

    if !follow_links.is_active() {
        copy_flags |= gio::FileCopyFlags::NOFOLLOW_SYMLINKS;
    }

    let _transfer_result = copy_files(
        main_win.clone().upcast(),
        src_files.iter().map(|f| f.file()).collect(),
        dest_dir.clone(),
        dest_fn.map(PathBuf::from),
        copy_flags,
        overwrite_mode,
    )
    .await;

    if let Err(error) = dest_dir.relist_files(main_win.upcast_ref(), false).await {
        error.show(main_win.upcast_ref()).await;
    }

    main_win.focus_file_lists();
}
