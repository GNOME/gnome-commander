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

use crate::{
    connection::list::ConnectionList,
    debug::debug,
    dialogs::{
        delete_dialog::{DeleteAction, do_delete},
        transfer_progress_dialog::TransferProgressWindow,
    },
    dir::Directory,
    libgcmd::file_descriptor::FileDescriptorExt,
    options::options::GeneralOptions,
    path::GnomeCmdPath,
    types::{ConfirmOverwriteMode, GnomeCmdTransferType, SizeDisplayMode},
    utils::{ErrorMessage, nice_size, pending, time_to_string},
};
use futures::{FutureExt, StreamExt, select};
use gettextrs::{gettext, pgettext};
use gtk::{gio, glib, prelude::*};
use std::{
    cell::{Cell, RefCell},
    ops::ControlFlow,
    path::PathBuf,
};

struct XferData {
    transfer_type: GnomeCmdTransferType,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,

    src_files: Vec<gio::File>,
    dst_files: Vec<gio::File>,

    bytes_total: u64,
    files_total: u64,

    cancellable: gio::Cancellable,

    current_file_number: Cell<u64>,
    current_src_file: RefCell<Option<gio::File>>,

    file_size: Cell<u64>,
    bytes_copied_file: Cell<u64>,
    bytes_total_transferred: Cell<u64>,

    /// action to take when an error occurs
    problem_action: Cell<Option<ProblemAction>>,
}

fn create_xfer_data(
    transfer_type: GnomeCmdTransferType,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,
    src_files: Vec<gio::File>,
    dst_files: Vec<gio::File>,
    bytes_total: u64,
    files_total: u64,
    cancellable: gio::Cancellable,
) -> XferData {
    XferData {
        transfer_type,
        copy_flags,
        overwrite_mode,
        src_files,
        dst_files,
        bytes_total,
        files_total,
        cancellable,
        current_file_number: Default::default(),
        current_src_file: Default::default(),
        file_size: Default::default(),
        bytes_copied_file: Default::default(),
        bytes_total_transferred: Default::default(),
        problem_action: Default::default(),
    }
}

pub async fn copy_files(
    parent_window: gtk::Window,
    src_files: Vec<gio::File>,
    to: Directory,
    dest_fn: Option<PathBuf>,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,
) -> bool {
    if src_files.is_empty() {
        return true;
    }

    // Sanity check
    for file in &src_files {
        if file_is_parent_to_dir_or_equal(file, &to) {
            ErrorMessage::new(
                gettext("Copying a directory into itself is a bad idea."),
                Some(gettext("The whole operation was cancelled.")),
            )
            .show(&parent_window)
            .await;
            return false;
        }
        if file_is_already_in_dir(file, &to) && dest_fn.is_some() && dest_fn == file.basename() {
            debug!(
                'x',
                "Copying a file to the same directory as it's already in, is not permitted"
            );
            return false;
        }
    }

    let mut dst_files = Vec::<gio::File>::new();
    if let Some(filename) = dest_fn.as_ref().filter(|_| src_files.len() == 1) {
        dst_files.push(to.get_child_gfile(filename));
    } else {
        for file in &src_files {
            let Some(basename) = file.basename() else {
                eprintln!("Error: Cannot get basename of a file {}.", file.uri());
                return false;
            };
            dst_files.push(to.get_child_gfile(&basename));
        }
    }

    let (bytes_total, files_total) = set_files_total(&src_files).await;

    let options = GeneralOptions::new();

    let win = TransferProgressWindow::new(files_total, options.size_display_mode.get());
    win.set_transient_for(Some(&parent_window));
    win.set_title(Some(&gettext("preparing…")));

    let xfer_data = create_xfer_data(
        GnomeCmdTransferType::COPY,
        copy_flags,
        overwrite_mode,
        src_files,
        dst_files,
        bytes_total,
        files_total,
        win.cancellable().clone(),
    );

    win.present();
    let success = transfer_files(win.upcast_ref(), &xfer_data).await;
    win.close();

    success.is_continue()
}

pub async fn move_files(
    parent_window: gtk::Window,
    src_files: Vec<gio::File>,
    to: Directory,
    dest_fn: Option<PathBuf>,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,
) -> bool {
    if src_files.is_empty() {
        return true;
    }

    // Sanity check
    for file in &src_files {
        if file_is_parent_to_dir_or_equal(file, &to) {
            ErrorMessage::new(
                gettext("Moving a directory into itself is a bad idea."),
                Some(gettext("The whole operation was cancelled.")),
            )
            .show(&parent_window)
            .await;
            return false;
        }
        if file_is_already_in_dir(file, &to) && dest_fn.is_some() && dest_fn == file.basename() {
            debug!(
                'x',
                "Moving a file to the same directory as it's already in, is not permitted"
            );
            return false;
        }
    }

    let mut dst_files = Vec::<gio::File>::new();
    if let Some(filename) = dest_fn.as_ref().filter(|_| src_files.len() == 1) {
        dst_files.push(to.get_child_gfile(filename));
    } else {
        for file in &src_files {
            let Some(basename) = file.basename() else {
                eprintln!("Error: Cannot get basename of a file {}.", file.uri());
                return false;
            };
            dst_files.push(to.get_child_gfile(&basename));
        }
    }

    let (bytes_total, files_total) = set_files_total(&src_files).await;

    let options = GeneralOptions::new();

    let win = TransferProgressWindow::new(files_total, options.size_display_mode.get());
    win.set_transient_for(Some(&parent_window));
    win.set_title(Some(&gettext("preparing…")));

    let xfer_data = create_xfer_data(
        GnomeCmdTransferType::MOVE,
        copy_flags,
        overwrite_mode,
        src_files,
        dst_files,
        bytes_total,
        files_total,
        win.cancellable().clone(),
    );

    win.present();
    let success = transfer_files(win.upcast_ref(), &xfer_data).await;
    win.close();

    success.is_continue()
}

pub async fn link_files(
    parent_window: gtk::Window,
    src_files: Vec<gio::File>,
    to: Directory,
    dest_fn: Option<PathBuf>,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,
) -> bool {
    if src_files.is_empty() {
        return true;
    }

    // Sanity check
    for file in &src_files {
        if file_is_parent_to_dir_or_equal(file, &to) {
            ErrorMessage::new(
                gettext("Moving a directory into itself is a bad idea."),
                Some(gettext("The whole operation was cancelled.")),
            )
            .show(&parent_window)
            .await;
            return false;
        }
        if file_is_already_in_dir(file, &to) && dest_fn.is_some() && dest_fn == file.basename() {
            debug!(
                'x',
                "Moving a file to the same directory as it's already in, is not permitted"
            );
            return false;
        }
    }

    let mut dst_files = Vec::<gio::File>::new();
    if let Some(filename) = dest_fn.as_ref().filter(|_| src_files.len() == 1) {
        dst_files.push(to.get_child_gfile(filename));
    } else {
        for file in &src_files {
            let Some(basename) = file.basename() else {
                eprintln!("Error: Cannot get basename of a file {}.", file.uri());
                return false;
            };
            dst_files.push(to.get_child_gfile(&basename));
        }
    }

    let (bytes_total, files_total) = set_files_total(&src_files).await;

    let options = GeneralOptions::new();

    let win = TransferProgressWindow::new(files_total, options.size_display_mode.get());
    win.set_transient_for(Some(&parent_window));
    win.set_title(Some(&gettext("preparing…")));

    let xfer_data = create_xfer_data(
        GnomeCmdTransferType::LINK,
        copy_flags,
        overwrite_mode,
        src_files,
        dst_files,
        bytes_total,
        files_total,
        win.cancellable().clone(),
    );

    win.present();
    let success = transfer_files(win.upcast_ref(), &xfer_data).await;
    win.close();

    success.is_continue()
}

pub async fn download_to_temporary(
    parent_window: gtk::Window,
    src_files: Vec<gio::File>,
    dst_files: Vec<gio::File>,
    copy_flags: gio::FileCopyFlags,
) -> bool {
    let (bytes_total, files_total) = set_files_total(&src_files).await;

    let options = GeneralOptions::new();

    let win = TransferProgressWindow::new(files_total, options.size_display_mode.get());
    win.set_transient_for(Some(&parent_window));
    win.set_title(Some(&gettext("downloading to /tmp")));

    let xfer_data = create_xfer_data(
        GnomeCmdTransferType::COPY,
        copy_flags,
        ConfirmOverwriteMode::Query,
        src_files,
        dst_files,
        bytes_total,
        files_total,
        win.cancellable().clone(),
    );

    win.present();
    let success = transfer_files(win.upcast_ref(), &xfer_data).await;
    win.close();

    success.is_continue()
}

async fn set_files_total(src_list: &[gio::File]) -> (u64, u64) {
    let mut bytes_total = 0;
    let mut files_total = 0;
    for file in src_list {
        if let Ok((disk_usage, _, files)) = file
            .measure_disk_usage_future(gio::FileMeasureFlags::NONE, glib::Priority::DEFAULT)
            .0
            .await
        {
            bytes_total += disk_usage;
            files_total += files;
        }
    }
    (bytes_total, files_total)
}

async fn transfer_files(
    window: &TransferProgressWindow,
    xfer_data: &XferData,
) -> ControlFlow<BreakReason> {
    if xfer_data.src_files.len() != xfer_data.dst_files.len() {
        return ControlFlow::Break(BreakReason::Failed);
    }

    for (src, dst) in xfer_data.src_files.iter().zip(xfer_data.dst_files.iter()) {
        if xfer_data.cancellable.is_cancelled() {
            break;
        }

        match xfer_data.transfer_type {
            GnomeCmdTransferType::COPY => {
                copy_file_recursively(xfer_data, window, src, dst, xfer_data.copy_flags).await?;
            }
            GnomeCmdTransferType::MOVE => {
                move_file_recursively(xfer_data, window, src, dst, xfer_data.copy_flags).await?;
            }
            GnomeCmdTransferType::LINK => {
                let symlink_src_path = src.path().unwrap();

                match dst.make_symbolic_link(symlink_src_path, gio::Cancellable::NONE) {
                    Ok(()) => {}
                    Err(error) => {
                        report_transfer_problem(xfer_data, window, &error, src, dst).await;
                    }
                }
            }
        }
    }

    ControlFlow::Continue(())
}

fn file_is_parent_to_dir_or_equal(file: &gio::File, dir: &Directory) -> bool {
    let dir_file = dir.file();
    let is_parent = dir_file.has_parent(Some(file));
    let are_equal = dir_file.equal(file);
    is_parent || are_equal
}

fn file_is_already_in_dir(file: &gio::File, dir: &Directory) -> bool {
    file.parent()
        .map_or(false, |parent| parent.equal(&dir.file()))
}

fn file_details(
    file: &gio::File,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<String, glib::Error> {
    let attributes = format!(
        "{},{},{}",
        gio::FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
        gio::FILE_ATTRIBUTE_STANDARD_SIZE,
        gio::FILE_ATTRIBUTE_TIME_MODIFIED
    );
    let info = file.query_info(
        &attributes,
        gio::FileQueryInfoFlags::NONE,
        gio::Cancellable::NONE,
    )?;
    Ok(format!(
        "{name}\n{size}, {time}",
        name = info.display_name(),
        size = nice_size(info.size() as u64, size_display_mode),
        time = info
            .modification_date_time()
            .and_then(|dt| time_to_string(dt, date_format).ok())
            .unwrap_or_default(),
    ))
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, strum::FromRepr)]
pub enum ProblemAction {
    Abort = 0,
    Retry,
    CopyInto,
    Skip,
    Rename,
    SkipAll,
    RenameAll,
    Replace,
    ReplaceAll,
}

impl ProblemAction {
    fn reusable(self) -> bool {
        match self {
            Self::Abort => true,
            Self::Retry => false,
            Self::CopyInto => false,
            Self::Skip => false,
            Self::Rename => false,
            Self::SkipAll => true,
            Self::RenameAll => true,
            Self::Replace => false,
            Self::ReplaceAll => true,
        }
    }
}

async fn run_simple_error_dialog(
    parent_window: &gtk::Window,
    msg: &str,
    error: &glib::Error,
) -> ProblemAction {
    let answer = gtk::AlertDialog::builder()
        .modal(true)
        .message(msg)
        .detail(error.message())
        .buttons([gettext("Abort"), gettext("Retry"), gettext("Skip")])
        .cancel_button(0)
        .build()
        .choose_future(Some(parent_window))
        .await;
    match answer {
        Ok(1) => ProblemAction::Retry,
        Ok(2) => ProblemAction::Skip,
        _ => ProblemAction::Abort,
    }
}

async fn run_failure_dialog(parent_window: &gtk::Window, msg: &str, error: &glib::Error) {
    let _answer = gtk::AlertDialog::builder()
        .modal(true)
        .message(msg)
        .detail(error.message())
        .buttons([gettext("Abort")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent_window))
        .await;
}

async fn run_directory_copy_overwrite_dialog(
    parent_window: &gtk::Window,
    msg: &str,
    many: bool,
) -> ProblemAction {
    let answer = if many {
        gtk::AlertDialog::builder()
            .modal(true)
            .message(msg)
            .buttons([
                gettext("Abort"),
                gettext("Retry"),
                gettext("Copy into"),
                gettext("Rename"),
                gettext("Rename all"),
                gettext("Skip"),
                gettext("Skip all"),
            ])
            .cancel_button(0)
            .build()
            .choose_future(Some(parent_window))
            .await
    } else {
        gtk::AlertDialog::builder()
            .modal(true)
            .message(msg)
            .buttons([
                gettext("Abort"),
                gettext("Retry"),
                gettext("Copy into"),
                gettext("Rename"),
            ])
            .cancel_button(0)
            .build()
            .choose_future(Some(parent_window))
            .await
    };
    match answer {
        Ok(1) => ProblemAction::Retry,
        Ok(2) => ProblemAction::CopyInto,
        Ok(3) => ProblemAction::Rename,
        Ok(4) => ProblemAction::RenameAll,
        Ok(5) => ProblemAction::Skip,
        Ok(6) => ProblemAction::SkipAll,
        _ => ProblemAction::Abort,
    }
}

async fn run_file_copy_overwrite_dialog(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    many: bool,
) -> Result<ProblemAction, glib::Error> {
    let options = GeneralOptions::new();
    let size_display_mode = options.size_display_mode.get();
    let date_format = options.date_display_format.get();
    let msg = gettext("Overwrite file:\n\n{dst}\n\nWith:\n\n{src}")
        .replace(
            "{dst}",
            &file_details(dst, size_display_mode, &date_format)?,
        )
        .replace(
            "{src}",
            &file_details(src, size_display_mode, &date_format)?,
        );

    let answer = if many {
        gtk::AlertDialog::builder()
            .modal(true)
            .message(msg)
            .buttons([
                gettext("Abort"),
                gettext("Retry"),
                gettext("Replace"),
                gettext("Rename"),
                gettext("Skip"),
                gettext("Replace all"),
                gettext("Rename all"),
                gettext("Skip all"),
            ])
            .cancel_button(0)
            .build()
            .choose_future(Some(parent_window))
            .await
    } else {
        gtk::AlertDialog::builder()
            .modal(true)
            .message(msg)
            .buttons([
                gettext("Abort"),
                gettext("Retry"),
                gettext("Replace"),
                gettext("Rename"),
            ])
            .cancel_button(0)
            .build()
            .choose_future(Some(parent_window))
            .await
    };
    Ok(match answer {
        Ok(1) => ProblemAction::Retry,
        Ok(2) => ProblemAction::Replace,
        Ok(3) => ProblemAction::Rename,
        Ok(4) => ProblemAction::Skip,
        Ok(5) => ProblemAction::ReplaceAll,
        Ok(6) => ProblemAction::RenameAll,
        Ok(7) => ProblemAction::SkipAll,
        _ => ProblemAction::Abort,
    })
}

fn peek_path(file: &gio::File) -> Result<PathBuf, glib::Error> {
    file.peek_path().ok_or_else(|| {
        glib::Error::new(
            gio::IOErrorEnum::InvalidFilename,
            "Cannot get a path of a file.",
        )
    })
}

async fn update_transfer_gui_error_copy(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    error: &glib::Error,
    overwrite_mode: ConfirmOverwriteMode,
    many: bool,
) -> Result<ProblemAction, glib::Error> {
    let msg = gettext("Error while transferring “{file}”")
        .replace("{file}", &peek_path(src)?.display().to_string());

    if error.matches(gio::IOErrorEnum::Exists) {
        match src.query_file_type(gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE) {
            gio::FileType::Directory => {
                return match overwrite_mode {
                    ConfirmOverwriteMode::SkipAll => Ok(ProblemAction::SkipAll),
                    ConfirmOverwriteMode::RenameAll => Ok(ProblemAction::RenameAll),
                    ConfirmOverwriteMode::Silently => Ok(ProblemAction::CopyInto),
                    ConfirmOverwriteMode::Query => {
                        Ok(run_directory_copy_overwrite_dialog(parent_window, &msg, many).await)
                    }
                };
            }
            gio::FileType::Regular => {
                return match overwrite_mode {
                    ConfirmOverwriteMode::SkipAll => Ok(ProblemAction::SkipAll),
                    ConfirmOverwriteMode::RenameAll => Ok(ProblemAction::RenameAll),
                    ConfirmOverwriteMode::Silently => Ok(ProblemAction::CopyInto),
                    ConfirmOverwriteMode::Query => {
                        run_file_copy_overwrite_dialog(parent_window, src, dst, many).await
                    }
                };
            }
            _ => {}
        }
    }

    Ok(run_simple_error_dialog(parent_window, &msg, error).await)
}

async fn run_move_overwrite_dialog(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    many: bool,
) -> Result<ProblemAction, glib::Error> {
    let options = GeneralOptions::new();
    let size_display_mode = options.size_display_mode.get();
    let date_format = options.date_display_format.get();
    let msg = gettext("Overwrite file:\n\n{dst}\n\nWith:\n\n{src}")
        .replace(
            "{dst}",
            &file_details(dst, size_display_mode, &date_format)?,
        )
        .replace(
            "{src}",
            &file_details(src, size_display_mode, &date_format)?,
        );

    let answer = if many {
        gtk::AlertDialog::builder()
            .modal(true)
            .message(msg)
            .buttons([
                gettext("Abort"),
                gettext("Retry"),
                gettext("Replace"),
                gettext("Rename"),
                gettext("Skip"),
                gettext("Replace all"),
                gettext("Rename all"),
                gettext("Skip all"),
            ])
            .cancel_button(0)
            .build()
            .choose_future(Some(parent_window))
            .await
    } else {
        gtk::AlertDialog::builder()
            .modal(true)
            .message(msg)
            .buttons([
                gettext("Abort"),
                gettext("Retry"),
                gettext("Replace"),
                gettext("Rename"),
            ])
            .cancel_button(0)
            .build()
            .choose_future(Some(parent_window))
            .await
    };
    Ok(match answer {
        Ok(1) => ProblemAction::Retry,
        Ok(2) => ProblemAction::Replace,
        Ok(3) => ProblemAction::Rename,
        Ok(4) => ProblemAction::Skip,
        Ok(5) => ProblemAction::ReplaceAll,
        Ok(6) => ProblemAction::RenameAll,
        Ok(7) => ProblemAction::SkipAll,
        _ => ProblemAction::Abort,
    })
}

async fn update_transfer_gui_error_move(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    error: &glib::Error,
    overwrite_mode: ConfirmOverwriteMode,
    many: bool,
) -> Result<ProblemAction, glib::Error> {
    if !error.matches(gio::IOErrorEnum::Exists) {
        let msg = gettext("Error while transferring “{file}”")
            .replace("{file}", &peek_path(src)?.display().to_string());

        Ok(run_simple_error_dialog(parent_window, &msg, error).await)
    } else {
        match overwrite_mode {
            ConfirmOverwriteMode::SkipAll => Ok(ProblemAction::SkipAll),
            ConfirmOverwriteMode::RenameAll => Ok(ProblemAction::RenameAll),
            ConfirmOverwriteMode::Silently => {
                // Note: When doing a native move operation, there is no option to
                // move a folder into another folder like it is available for the copy
                // process. The reason is that in the former case, we don't step recursively
                // through the subdirectories. That's why we only offer a 'replace', but no
                // 'copy into' when moving folders.
                Ok(ProblemAction::ReplaceAll)
            }
            ConfirmOverwriteMode::Query => {
                run_move_overwrite_dialog(parent_window, src, dst, many).await
            }
        }
    }
}

async fn update_transfer_gui_error_link(
    parent_window: &gtk::Window,
    _src: &gio::File,
    dst: &gio::File,
    error: &glib::Error,
) -> Result<ProblemAction, glib::Error> {
    let options = GeneralOptions::new();
    let size_display_mode = options.size_display_mode.get();
    let date_format = options.date_display_format.get();
    let msg = gettext("Error while creating symlink “{file}”").replace(
        "{file}",
        &file_details(dst, size_display_mode, &date_format)?,
    );
    let _answer = gtk::AlertDialog::builder()
        .modal(true)
        .message(msg)
        .detail(error.message())
        .buttons([gettext("Ignore")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent_window))
        .await;
    Ok(ProblemAction::Skip)
}

async fn report_transfer_problem(
    xfer_data: &XferData,
    window: &TransferProgressWindow,
    error: &glib::Error,
    src: &gio::File,
    dst: &gio::File,
) -> ProblemAction {
    let action = if let Some(problem_action) = xfer_data.problem_action.get() {
        Ok(problem_action)
    } else {
        match xfer_data.transfer_type {
            GnomeCmdTransferType::COPY => {
                update_transfer_gui_error_copy(
                    window.upcast_ref(),
                    src,
                    dst,
                    error,
                    xfer_data.overwrite_mode,
                    xfer_data.files_total > 1,
                )
                .await
            }
            GnomeCmdTransferType::MOVE => {
                update_transfer_gui_error_move(
                    window.upcast_ref(),
                    src,
                    dst,
                    error,
                    xfer_data.overwrite_mode,
                    xfer_data.files_total > 1,
                )
                .await
            }
            GnomeCmdTransferType::LINK => {
                update_transfer_gui_error_link(window.upcast_ref(), src, dst, error).await
            }
        }
    };

    match action {
        Ok(action) => {
            if action.reusable() {
                xfer_data.problem_action.set(Some(action));
            } else {
                xfer_data.problem_action.set(None);
            }
            action
        }
        Err(error) => {
            eprintln!("Failure: {}", error);
            ErrorMessage::with_error(gettext("Unrecoverable error"), &error)
                .show(window.upcast_ref())
                .await;
            ProblemAction::Abort
        }
    }
}

/// This function recursively copies directories and files from `src` to `dst`.
/// As the function can call itself multiple times, some logic is build into it to handle the
/// case when the destination is already existing and the user decides up on that.
///
/// The function does return when it copied a regular file or when something went wrong
/// unexpectedly. It calls itself if a directory should be copied or if a rename should happen
/// as the original destination exists already.
#[async_recursion::async_recursion(?Send)]
async fn copy_file_recursively(
    xfer_data: &XferData,
    window: &TransferProgressWindow,
    src: &gio::File,
    dst: &gio::File,
    flags: gio::FileCopyFlags,
) -> ControlFlow<BreakReason> {
    if xfer_data.cancellable.is_cancelled() {
        return ControlFlow::Break(BreakReason::Cancelled);
    }

    match src.query_file_type(gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE) {
        gio::FileType::Directory => {
            match dst.make_directory_future(glib::Priority::DEFAULT).await {
                Ok(()) => {}
                Err(error) => {
                    match report_transfer_problem(xfer_data, window, &error, src, dst).await {
                        ProblemAction::CopyInto => {}
                        ProblemAction::Retry => {
                            return copy_file_recursively(xfer_data, window, src, dst, flags).await;
                        }
                        ProblemAction::Rename | ProblemAction::RenameAll => {
                            let dst = new_nonexisting_dest_file(dst)?;
                            return copy_file_recursively(xfer_data, window, src, &dst, flags)
                                .await;
                        }
                        ProblemAction::Skip | ProblemAction::SkipAll => {
                            return ControlFlow::Continue(());
                        }
                        ProblemAction::Abort => {
                            return ControlFlow::Break(BreakReason::Cancelled);
                        }
                        ProblemAction::Replace | ProblemAction::ReplaceAll => {
                            // This is not handled for directories when copying
                            return ControlFlow::Break(BreakReason::Failed);
                        }
                    }
                }
            };

            // After processing the user response above, we are looping through the directory now.
            let enumerator = match src.enumerate_children(
                "*",
                gio::FileQueryInfoFlags::NONE,
                gio::Cancellable::NONE,
            ) {
                Ok(enumerator) => enumerator,
                Err(error) => {
                    eprintln!("g_file_enumerate_children error: {}", error);
                    report_transfer_problem(xfer_data, window, &error, src, dst).await;
                    return ControlFlow::Break(BreakReason::Failed);
                }
            };

            loop {
                let child_file_info = match enumerator.next_file(gio::Cancellable::NONE) {
                    Ok(Some(file)) => file,
                    Ok(None) => break,
                    Err(error) => {
                        eprintln!("File enumeration error: {}", error);
                        run_failure_dialog(
                            window.upcast_ref(),
                            &gettext("File enumeration error"),
                            &error,
                        )
                        .await;
                        return ControlFlow::Break(BreakReason::Failed);
                    }
                };

                if xfer_data.cancellable.is_cancelled() {
                    let _ = enumerator.close(gio::Cancellable::NONE);
                    return ControlFlow::Break(BreakReason::Cancelled);
                }

                let child_name = child_file_info.name();
                let source_child = enumerator.container().child(&child_name);
                let target_child = dst.child(&child_name);

                copy_file_recursively(xfer_data, window, &source_child, &target_child, flags)
                    .await?;
            }
            let _ = enumerator.close(gio::Cancellable::NONE);
            ControlFlow::Continue(())
        }
        gio::FileType::Regular => {
            let action = copy_single_file(xfer_data, window, src, dst, flags).await?;
            match action {
                None | Some(ProblemAction::Skip) | Some(ProblemAction::SkipAll) => {
                    ControlFlow::Continue(())
                }
                Some(ProblemAction::Retry) => {
                    copy_file_recursively(xfer_data, window, src, dst, flags).await
                }
                Some(ProblemAction::Replace) | Some(ProblemAction::ReplaceAll) => {
                    copy_file_recursively(
                        xfer_data,
                        window,
                        src,
                        dst,
                        flags | gio::FileCopyFlags::OVERWRITE,
                    )
                    .await
                }
                Some(ProblemAction::Rename) | Some(ProblemAction::RenameAll) => {
                    let dst = new_nonexisting_dest_file(dst)?;
                    copy_file_recursively(xfer_data, window, src, &dst, flags).await
                }
                Some(ProblemAction::Abort) | Some(ProblemAction::CopyInto) => {
                    // This is not handled for files when copying
                    ControlFlow::Break(BreakReason::Failed)
                }
            }
        }
        _ => ControlFlow::Continue(()),
    }
}

#[async_recursion::async_recursion(?Send)]
async fn move_file_recursively(
    xfer_data: &XferData,
    window: &TransferProgressWindow,
    src: &gio::File,
    dst: &gio::File,
    flags: gio::FileCopyFlags,
) -> ControlFlow<BreakReason> {
    if xfer_data.cancellable.is_cancelled() {
        return ControlFlow::Break(BreakReason::Cancelled);
    }

    let src_is_directory = src
        .query_file_type(gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE)
        == gio::FileType::Directory;

    match move_file(xfer_data, window, src, dst, flags).await {
        Err(error) if error.matches(gio::IOErrorEnum::Cancelled) => {
            ControlFlow::Break(BreakReason::Cancelled)
        }
        Err(error) if error.matches(gio::IOErrorEnum::WouldRecurse) && src_is_directory => {
            if copy_file_recursively(xfer_data, window, src, dst, xfer_data.copy_flags)
                .await
                .is_continue()
            {
                // Folder was copied, now delete the source for completing the move operation
                let file_info = match src.query_info(
                    "*",
                    gio::FileQueryInfoFlags::NONE,
                    gio::Cancellable::NONE,
                ) {
                    Ok(file_info) => file_info,
                    Err(error) => {
                        run_failure_dialog(
                            window.upcast_ref(),
                            &gettext("Source “%s” could not be deleted. Aborting!")
                                .replace("%s", &src.parse_name()),
                            &error,
                        )
                        .await;
                        return ControlFlow::Break(BreakReason::Failed);
                    }
                };

                let Some(parent_file) = src.parent() else {
                    run_failure_dialog(
                        window.upcast_ref(),
                        &gettext("Source “%s” could not be deleted. Aborting!")
                            .replace("%s", &src.parse_name()),
                        &error,
                    )
                    .await;
                    return ControlFlow::Break(BreakReason::Failed);
                };

                let connection_list = ConnectionList::get();
                let parent_dir: Directory;
                let schema = parent_file.uri_scheme();

                if schema.as_deref() == Some("file") {
                    let home = connection_list.home();
                    parent_dir =
                        Directory::new(&home, GnomeCmdPath::Plain(parent_file.path().unwrap()));
                } else if schema.as_deref() == Some("smb") {
                    unimplemented!()
                } else if let Some(ref connection) =
                    connection_list.get_remote_con_for_file(&parent_file)
                {
                    let path = glib::Uri::parse(&parent_file.uri(), glib::UriFlags::NONE)
                        .unwrap()
                        .path();
                    parent_dir =
                        Directory::new(connection, GnomeCmdPath::Plain(PathBuf::from(path)));
                } else {
                    run_failure_dialog(window.upcast_ref(), &gettext("Cannot detect a connection for a source “%s”. It could not be deleted. Aborting!")
                            .replace("%s", &src.parse_name()),
                    &error).await;
                    return ControlFlow::Break(BreakReason::Failed);
                }

                let directory = Directory::new_from_file_info(&file_info, &parent_dir).unwrap();

                let mut files = glib::List::new();
                files.push_back(directory.upcast());

                do_delete(
                    window.upcast_ref(),
                    DeleteAction::DeletePermanently,
                    &files,
                    false, // do not show progress window
                )
                .await;
                ControlFlow::Continue(())
            } else {
                if !xfer_data.cancellable.is_cancelled() {
                    run_failure_dialog(
                        window.upcast_ref(),
                        &gettext("Source “%s” could not be copied. Aborting!")
                            .replace("%s", &src.parse_name()),
                        &error,
                    )
                    .await;
                }
                return ControlFlow::Break(BreakReason::Failed);
            }
        }
        Err(error) => {
            match report_transfer_problem(xfer_data, window, &error, src, dst).await {
                ProblemAction::Retry => {
                    move_file_recursively(xfer_data, window, src, dst, flags).await
                }
                ProblemAction::Replace | ProblemAction::ReplaceAll => {
                    move_file_recursively(
                        xfer_data,
                        window,
                        src,
                        dst,
                        flags | gio::FileCopyFlags::OVERWRITE,
                    )
                    .await
                }
                ProblemAction::Rename | ProblemAction::RenameAll => {
                    let dst = new_nonexisting_dest_file(dst)?;
                    move_file_recursively(xfer_data, window, src, &dst, flags).await
                }
                ProblemAction::Skip | ProblemAction::SkipAll => ControlFlow::Continue(()),
                ProblemAction::CopyInto => {
                    // Copy into can only be selected in case of copying a directory into an
                    // already existing directory, so we are ignoring this here because we deal with
                    // moving files in this if branch.
                    ControlFlow::Break(BreakReason::Failed)
                }
                ProblemAction::Abort => ControlFlow::Break(BreakReason::Cancelled),
            }
        }
        Ok(()) => {
            file_done(xfer_data);
            ControlFlow::Continue(())
        }
    }
}

async fn progress_update(window: &TransferProgressWindow, xfer_data: &XferData) {
    if xfer_data.bytes_total_transferred.get() == 0 {
        window.set_action(&gettext("copying…"));
    }

    let file_name = xfer_data
        .current_src_file
        .borrow()
        .as_ref()
        .and_then(|file| {
            file.query_info(
                gio::FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                gio::FileQueryInfoFlags::NONE,
                gio::Cancellable::NONE,
            )
            .ok()
        })
        .map(|info| info.display_name().to_string())
        .unwrap_or_default();

    window.set_msg(
        &gettext("[file {index} of {count}] “{file}”")
            .replace(
                "{index}",
                &(xfer_data.current_file_number.get() + 1).to_string(),
            )
            .replace("{count}", &xfer_data.files_total.to_string())
            .replace("{file}", &file_name),
    );
    window.set_total_progress(
        xfer_data.bytes_copied_file.get(),
        xfer_data.file_size.get(),
        xfer_data.bytes_total_transferred.get() + xfer_data.bytes_copied_file.get(),
        xfer_data.bytes_total,
    );
    pending().await;
}

async fn update_transferred_data(
    window: &TransferProgressWindow,
    xfer_data: &XferData,
    current: i64,
    total: i64,
) {
    xfer_data.bytes_copied_file.set(current as u64);
    xfer_data.file_size.set(total as u64);
    progress_update(window, xfer_data).await;
}

fn file_done(xfer_data: &XferData) {
    xfer_data.bytes_copied_file.set(0);
    xfer_data
        .current_file_number
        .set(xfer_data.current_file_number.get() + 1);
    xfer_data
        .bytes_total_transferred
        .set(xfer_data.bytes_total_transferred.get() + xfer_data.file_size.get());
}

fn new_nonexisting_dest_file(file: &gio::File) -> ControlFlow<BreakReason, gio::File> {
    match find_nonexisting_name(file) {
        Some(file) => ControlFlow::Continue(file),
        None => {
            eprintln!(
                "Cannot come up with a non-conflicting name for {}",
                file.parse_name()
            );
            ControlFlow::Break(BreakReason::Failed)
        }
    }
}

fn find_nonexisting_name(file: &gio::File) -> Option<gio::File> {
    let path = file.path()?;
    let parent_path = path.parent()?;
    let base_name = path.file_name()?.to_string_lossy();

    (1_u64..).find_map(|increment| {
        let new_name = pgettext(
            "A template for a file name. In a case of a conflict with an existing one.",
            "{name} (Copy {number})",
        )
        .replace("{name}", &base_name)
        .replace("{number}", &increment.to_string());

        let path = parent_path.join(new_name);
        if !path.exists() {
            Some(gio::File::for_path(path))
        } else {
            None
        }
    })
}

enum BreakReason {
    Cancelled,
    Failed,
}

async fn copy_file(
    xfer_data: &XferData,
    window: &TransferProgressWindow,
    src: &gio::File,
    dst: &gio::File,
    flags: gio::FileCopyFlags,
) -> Result<(), glib::Error> {
    xfer_data.current_src_file.replace(Some(src.clone()));

    let (result, mut progress) = src.copy_future(dst, flags, glib::Priority::DEFAULT);
    let mut result = result.fuse();
    loop {
        let mut next = progress.next().fuse();
        let p = select! {
            p = next => p,
            r = result => break r,
        };
        if let Some((current_num_bytes, total_num_bytes)) = p {
            update_transferred_data(window, xfer_data, current_num_bytes, total_num_bytes).await;
        }
    }
}

async fn copy_single_file(
    xfer_data: &XferData,
    window: &TransferProgressWindow,
    src: &gio::File,
    dst: &gio::File,
    flags: gio::FileCopyFlags,
) -> ControlFlow<BreakReason, Option<ProblemAction>> {
    match copy_file(xfer_data, window, src, dst, flags).await {
        Err(error) if error.matches(gio::IOErrorEnum::Cancelled) => {
            ControlFlow::Break(BreakReason::Cancelled)
        }
        Err(error) => {
            let action = report_transfer_problem(xfer_data, window, &error, src, dst).await;
            ControlFlow::Continue(Some(action))
        }
        Ok(()) => {
            file_done(xfer_data);
            if xfer_data.problem_action.get() == Some(ProblemAction::Retry) {
                xfer_data.problem_action.set(None);
            }
            ControlFlow::Continue(None)
        }
    }
}

async fn move_file(
    xfer_data: &XferData,
    window: &TransferProgressWindow,
    src: &gio::File,
    dst: &gio::File,
    flags: gio::FileCopyFlags,
) -> Result<(), glib::Error> {
    xfer_data.current_src_file.replace(Some(src.clone()));

    let (result, mut progress) = src.move_future(dst, flags, glib::Priority::DEFAULT);
    let mut result = result.fuse();
    loop {
        let mut next = progress.next().fuse();
        let p = select! {
            p = next => p,
            r = result => break r,
        };
        if let Some((current_num_bytes, total_num_bytes)) = p {
            update_transferred_data(window, xfer_data, current_num_bytes, total_num_bytes).await;
        }
    }
}
