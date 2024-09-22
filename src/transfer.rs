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
    dialogs::transfer_progress_dialog::TransferProgressWindow,
    dir::Directory,
    types::{GnomeCmdConfirmOverwriteMode, GnomeCmdTransferType},
    utils::{nice_size, pending, run_simple_dialog, show_error, time_to_string, SizeDisplayMode},
};
use async_channel::{Receiver, Sender};
use gettextrs::gettext;
use glib::{
    ffi::{gboolean, GError},
    translate::from_glib_none,
};
use gtk::{
    ffi::GtkWindow,
    gio::{
        self,
        ffi::{GCancellable, GFile},
    },
    glib::{
        self,
        translate::{IntoGlib, ToGlibPtr},
    },
    prelude::*,
};
use std::{
    ffi::{c_char, c_void},
    mem::transmute,
    path::PathBuf,
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
    Ok(gettext!(
        "<b>{}</b>\n<span color='dimgray' size='smaller'>{}, {}</span>",
        info.display_name(),
        nice_size(info.size() as u64, size_display_mode),
        time_to_string(info.modification_time(), date_format).unwrap_or_default()
    ))
}

#[repr(C)]
#[derive(Clone, Copy)]
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
    title: &str,
    msg: &str,
) -> ProblemAction {
    let answer = run_simple_dialog(
        parent_window,
        true,
        gtk::MessageType::Error,
        msg,
        title,
        None,
        &[&gettext("Abort"), &gettext("Retry"), &gettext("Skip")],
    )
    .await;
    match answer {
        gtk::ResponseType::Other(0) => ProblemAction::Abort,
        gtk::ResponseType::Other(1) => ProblemAction::Retry,
        gtk::ResponseType::Other(2) => ProblemAction::Skip,
        _ => ProblemAction::Abort,
    }
}

async fn run_failure_dialog(parent_window: &gtk::Window, msg: &str, error: &glib::Error) {
    let _answer = run_simple_dialog(
        parent_window,
        true,
        gtk::MessageType::Error,
        format!("{}\n\n{}", msg, error.message()).trim(),
        &gettext("Transfer problem"),
        None,
        &[&gettext("Abort")],
    )
    .await;
}

async fn run_directory_copy_overwrite_dialog(
    parent_window: &gtk::Window,
    msg: &str,
    many: bool,
) -> ProblemAction {
    let answer = if many {
        run_simple_dialog(
            parent_window,
            true,
            gtk::MessageType::Error,
            msg,
            &gettext("Copy problem"),
            None,
            &[
                &gettext("Abort"),
                &gettext("Retry"),
                &gettext("Copy into"),
                &gettext("Rename"),
                &gettext("Rename all"),
                &gettext("Skip"),
                &gettext("Skip all"),
            ],
        )
        .await
    } else {
        run_simple_dialog(
            parent_window,
            true,
            gtk::MessageType::Error,
            msg,
            &gettext("Copy problem"),
            None,
            &[
                &gettext("Abort"),
                &gettext("Retry"),
                &gettext("Copy into"),
                &gettext("Rename"),
            ],
        )
        .await
    };
    match answer {
        gtk::ResponseType::Other(0) => ProblemAction::Abort,
        gtk::ResponseType::Other(1) => ProblemAction::Retry,
        gtk::ResponseType::Other(2) => ProblemAction::CopyInto,
        gtk::ResponseType::Other(3) => ProblemAction::Rename,
        gtk::ResponseType::Other(4) => ProblemAction::RenameAll,
        gtk::ResponseType::Other(5) => ProblemAction::Skip,
        gtk::ResponseType::Other(6) => ProblemAction::SkipAll,
        _ => ProblemAction::Abort,
    }
}

async fn run_file_copy_overwrite_dialog(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    many: bool,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<ProblemAction, glib::Error> {
    let msg = gettext!(
        "Overwrite file:\n\n{}\n\nWith:\n\n{}",
        file_details(dst, size_display_mode, date_format)?,
        file_details(src, size_display_mode, date_format)?
    );

    let answer = if many {
        run_simple_dialog(
            parent_window,
            true,
            gtk::MessageType::Error,
            &msg,
            &gettext("Copy problem"),
            None,
            &[
                &gettext("Abort"),
                &gettext("Retry"),
                &gettext("Replace"),
                &gettext("Rename"),
                &gettext("Skip"),
                &gettext("Replace all"),
                &gettext("Rename all"),
                &gettext("Skip all"),
            ],
        )
        .await
    } else {
        run_simple_dialog(
            parent_window,
            true,
            gtk::MessageType::Error,
            &msg,
            &gettext("Copy problem"),
            None,
            &[
                &gettext("Abort"),
                &gettext("Retry"),
                &gettext("Replace"),
                &gettext("Rename"),
            ],
        )
        .await
    };
    Ok(match answer {
        gtk::ResponseType::Other(0) => ProblemAction::Abort,
        gtk::ResponseType::Other(1) => ProblemAction::Retry,
        gtk::ResponseType::Other(2) => ProblemAction::Replace,
        gtk::ResponseType::Other(3) => ProblemAction::Rename,
        gtk::ResponseType::Other(4) => ProblemAction::Skip,
        gtk::ResponseType::Other(5) => ProblemAction::ReplaceAll,
        gtk::ResponseType::Other(6) => ProblemAction::RenameAll,
        gtk::ResponseType::Other(7) => ProblemAction::SkipAll,
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
    overwrite_mode: GnomeCmdConfirmOverwriteMode,
    many: bool,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<ProblemAction, glib::Error> {
    let msg = gettext!(
        "Error while transferring “{}”\n\n{}",
        peek_path(src)?.display(),
        error.message()
    );

    if error.matches(gio::IOErrorEnum::Exists) {
        match src.query_file_type(gio::FileQueryInfoFlags::NONE, gio::Cancellable::NONE) {
            gio::FileType::Directory => {
                return match overwrite_mode {
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL => {
                        Ok(ProblemAction::SkipAll)
                    }
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL => {
                        Ok(ProblemAction::RenameAll)
                    }
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY => {
                        Ok(ProblemAction::CopyInto)
                    }
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY => {
                        Ok(run_directory_copy_overwrite_dialog(parent_window, &msg, many).await)
                    }
                };
            }
            gio::FileType::Regular => {
                return match overwrite_mode {
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL => {
                        Ok(ProblemAction::SkipAll)
                    }
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL => {
                        Ok(ProblemAction::RenameAll)
                    }
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY => {
                        Ok(ProblemAction::CopyInto)
                    }
                    GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY => {
                        run_file_copy_overwrite_dialog(
                            parent_window,
                            src,
                            dst,
                            many,
                            size_display_mode,
                            date_format,
                        )
                        .await
                    }
                };
            }
            _ => {}
        }
    }

    Ok(run_simple_error_dialog(parent_window, &gettext("Transfer problem"), &msg).await)
}

async fn run_move_overwrite_dialog(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    many: bool,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<ProblemAction, glib::Error> {
    let msg = gettext!(
        "Overwrite file:\n\n{}\n\nWith:\n\n{}",
        file_details(dst, size_display_mode, date_format)?,
        file_details(src, size_display_mode, date_format)?,
    );

    let answer = if many {
        run_simple_dialog(
            parent_window,
            true,
            gtk::MessageType::Error,
            &msg,
            &gettext("Move problem"),
            None,
            &[
                &gettext("Abort"),
                &gettext("Retry"),
                &gettext("Replace"),
                &gettext("Rename"),
                &gettext("Skip"),
                &gettext("Replace all"),
                &gettext("Rename all"),
                &gettext("Skip all"),
            ],
        )
        .await
    } else {
        run_simple_dialog(
            parent_window,
            true,
            gtk::MessageType::Error,
            &msg,
            &gettext("Move problem"),
            None,
            &[
                &gettext("Abort"),
                &gettext("Retry"),
                &gettext("Replace"),
                &gettext("Rename"),
            ],
        )
        .await
    };
    Ok(match answer {
        gtk::ResponseType::Other(0) => ProblemAction::Abort,
        gtk::ResponseType::Other(1) => ProblemAction::Retry,
        gtk::ResponseType::Other(2) => ProblemAction::Replace,
        gtk::ResponseType::Other(3) => ProblemAction::Rename,
        gtk::ResponseType::Other(4) => ProblemAction::Skip,
        gtk::ResponseType::Other(5) => ProblemAction::ReplaceAll,
        gtk::ResponseType::Other(6) => ProblemAction::RenameAll,
        gtk::ResponseType::Other(7) => ProblemAction::SkipAll,
        _ => ProblemAction::Abort,
    })
}

async fn update_transfer_gui_error_move(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    error: &glib::Error,
    overwrite_mode: GnomeCmdConfirmOverwriteMode,
    many: bool,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<ProblemAction, glib::Error> {
    if !error.matches(gio::IOErrorEnum::Exists) {
        let msg = gettext!(
            "Error while transferring “{}”\n\n{}",
            peek_path(src)?.display(),
            error.message()
        );

        Ok(run_simple_error_dialog(parent_window, &gettext("Transfer problem"), &msg).await)
    } else {
        match overwrite_mode {
            GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL => {
                Ok(ProblemAction::SkipAll)
            }
            GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL => {
                Ok(ProblemAction::RenameAll)
            }
            GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY => {
                // Note: When doing a native move operation, there is no option to
                // move a folder into another folder like it is available for the copy
                // process. The reason is that in the former case, we don't step recursively
                // through the subdirectories. That's why we only offer a 'replace', but no
                // 'copy into' when moving folders.
                Ok(ProblemAction::ReplaceAll)
            }
            GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY => {
                run_move_overwrite_dialog(
                    parent_window,
                    src,
                    dst,
                    many,
                    size_display_mode,
                    date_format,
                )
                .await
            }
        }
    }
}

async fn update_transfer_gui_error_link(
    parent_window: &gtk::Window,
    _src: &gio::File,
    dst: &gio::File,
    error: &glib::Error,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<ProblemAction, glib::Error> {
    let msg = gettext!(
        "Error while creating symlink “{}”\n\n{}",
        file_details(dst, size_display_mode, date_format)?,
        error.message()
    );
    let _answer = run_simple_dialog(
        parent_window,
        true,
        gtk::MessageType::Error,
        &gettext("Symlink creation problem"),
        &msg,
        None,
        &[&gettext("Ignore")],
    )
    .await;
    Ok(ProblemAction::Skip)
}

enum Event {
    Update {
        current_file_number: u64,
        current_src_file_name: String,
        files_total: u64,
        file_size: u64,
        bytes_copied_file: u64,
        bytes_total: u64,
        bytes_total_transferred: u64,
    },
    TransferProblem {
        error: glib::Error,
        src: gio::File,
        dst: gio::File,
    },
    Failure {
        error: glib::Error,
        message: String,
    },
    Done,
}

#[derive(Clone)]
pub struct TransferControl {
    problem_sender: Sender<Event>,
    problem_receiver: Receiver<Event>,
    action_sender: Sender<ProblemAction>,
    action_receiver: Receiver<ProblemAction>,
    cancellable: gio::Cancellable,
}

impl TransferControl {
    fn new(cancellable: &gio::Cancellable) -> Self {
        let (problem_sender, problem_receiver) = async_channel::bounded::<Event>(1);
        let (action_sender, action_receiver) = async_channel::bounded::<ProblemAction>(1);
        Self {
            problem_sender: problem_sender.clone(),
            problem_receiver: problem_receiver.clone(),
            action_sender: action_sender.clone(),
            action_receiver: action_receiver.clone(),
            cancellable: cancellable.clone(),
        }
    }
}

async fn transfer_gui_loop(
    win: &TransferProgressWindow,
    tc: &TransferControl,
    no_of_files: u64,
    transfer_type: GnomeCmdTransferType,
    overwrite_mode: GnomeCmdConfirmOverwriteMode,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> bool {
    let mut previous_problem_action: Option<ProblemAction> = None;
    let success = loop {
        match tc.problem_receiver.recv().await {
            Ok(Event::Update {
                current_file_number,
                current_src_file_name,
                files_total,
                file_size,
                bytes_copied_file,
                bytes_total,
                bytes_total_transferred,
            }) => {
                if bytes_total_transferred == 0 {
                    win.set_action(&gettext("copying…"));
                }

                win.set_msg(&gettext!(
                    "[file {} of {}] “{}”",
                    current_file_number + 1,
                    files_total,
                    current_src_file_name
                ));
                win.set_total_progress(
                    bytes_copied_file,
                    file_size,
                    bytes_total_transferred + bytes_copied_file,
                    bytes_total,
                );
                pending().await;
            }
            Ok(Event::TransferProblem { error, src, dst }) => {
                let action = if let Some(problem_action) = previous_problem_action {
                    Ok(problem_action)
                } else {
                    match transfer_type {
                        GnomeCmdTransferType::COPY => {
                            update_transfer_gui_error_copy(
                                win.upcast_ref(),
                                src.as_ref(),
                                dst.as_ref(),
                                &error,
                                overwrite_mode,
                                no_of_files > 1,
                                size_display_mode,
                                date_format,
                            )
                            .await
                        }
                        GnomeCmdTransferType::MOVE => {
                            update_transfer_gui_error_move(
                                win.upcast_ref(),
                                src.as_ref(),
                                dst.as_ref(),
                                &error,
                                overwrite_mode,
                                no_of_files > 1,
                                size_display_mode,
                                date_format,
                            )
                            .await
                        }
                        GnomeCmdTransferType::LINK => {
                            update_transfer_gui_error_link(
                                win.upcast_ref(),
                                src.as_ref(),
                                dst.as_ref(),
                                &error,
                                size_display_mode,
                                date_format,
                            )
                            .await
                        }
                    }
                };
                match action {
                    Ok(action) => {
                        if action.reusable() {
                            previous_problem_action = Some(action);
                        }
                        if let Err(error) = tc.action_sender.send_blocking(action) {
                            eprintln!("Failed to send an answer: {error}.");
                            break false;
                        }
                    }
                    Err(error) => {
                        eprintln!("Failure: {}", error);
                        show_error(win.upcast_ref(), &gettext("Unrecoverable error"), &error);
                        break false;
                    }
                }
            }
            Ok(Event::Failure { error, message }) => {
                run_failure_dialog(win.upcast_ref(), &message, &&error).await;
                break false;
            }
            Ok(Event::Done) => {
                break true;
            }
            Err(error) => {
                eprintln!("Failed to receive an event: {error}.");
                break false;
            }
        }
    };

    success
}

extern "C" {
    fn finish_xfer(xferData: *mut c_void, threaded: gboolean, success: gboolean);
}

#[no_mangle]
pub extern "C" fn start_transfer_gui(
    xfer_data: *mut c_void,
    parent_window_ptr: *mut GtkWindow,
    title: *const c_char,
    no_of_files: u64,
    transfer_type: GnomeCmdTransferType,
    overwrite_mode: GnomeCmdConfirmOverwriteMode,
    size_display_mode: SizeDisplayMode,
    date_format: *const c_char,
) -> *mut TransferControl {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };
    let title: String = unsafe { from_glib_none(title) };
    let date_format: String = unsafe { from_glib_none(date_format) };

    let win = TransferProgressWindow::new(no_of_files, size_display_mode);
    win.set_transient_for(Some(&parent_window));
    win.set_title(&title);

    let tc = Box::new(TransferControl::new(win.cancellable()));
    let tc1 = tc.clone();
    glib::MainContext::default().spawn_local(async move {
        win.present();
        let success = transfer_gui_loop(
            &win,
            &tc1,
            no_of_files,
            transfer_type,
            overwrite_mode,
            size_display_mode,
            &date_format,
        )
        .await;
        win.close();
        unsafe {
            finish_xfer(xfer_data, 1, success as gboolean);
        }
    });

    Box::into_raw(tc)
}

#[no_mangle]
pub extern "C" fn transfer_get_cancellable(tc_ptr: *mut TransferControl) -> *mut GCancellable {
    let tc = unsafe { &*tc_ptr };
    tc.cancellable.to_glib_none().0
}

#[no_mangle]
pub extern "C" fn transfer_progress(
    tc_ptr: *mut TransferControl,
    current_file_number: u64,
    current_src_file_name_ptr: *const c_char,
    files_total: u64,
    file_size: u64,
    bytes_copied_file: u64,
    bytes_total: u64,
    bytes_total_transferred: u64,
) {
    let tc = unsafe { &*tc_ptr };
    let current_src_file_name: String = unsafe { from_glib_none(current_src_file_name_ptr) };

    if let Err(error) = tc.problem_sender.send_blocking(Event::Update {
        current_file_number,
        current_src_file_name,
        files_total,
        file_size,
        bytes_copied_file,
        bytes_total,
        bytes_total_transferred,
    }) {
        eprintln!("Failed to send an event: {}", error);
    }
}

#[no_mangle]
pub extern "C" fn transfer_ask_handle_error(
    tc_ptr: *mut TransferControl,
    error_ptr: *mut GError,
    src: *mut GFile,
    dst: *mut GFile,
    message_ptr: *const c_char,
) -> ProblemAction {
    let tc = unsafe { &*tc_ptr };
    let error: glib::Error = unsafe { from_glib_none(error_ptr) };
    let src: Option<gio::File> = unsafe { from_glib_none(src) };
    let dst: Option<gio::File> = unsafe { from_glib_none(dst) };
    let message: Option<String> = unsafe { from_glib_none(message_ptr) };

    let event = match (src, dst, message) {
        (Some(src), Some(dst), _) => Event::TransferProblem { error, src, dst },
        (_, _, Some(message)) => Event::Failure { error, message },
        (_, _, _) => Event::Failure {
            error,
            message: String::new(),
        },
    };

    if let Err(error) = tc.problem_sender.send_blocking(event) {
        eprintln!("Failed to send an event: {}", error);
    }
    match tc.action_receiver.recv_blocking() {
        Ok(action) => return action,
        Err(error) => {
            eprintln!("Failed to receive a problem action: {}", error);
            return ProblemAction::Abort;
        }
    }
}

#[no_mangle]
pub extern "C" fn transfer_done(tc_ptr: *mut TransferControl) {
    let tc = unsafe { &*tc_ptr };
    if let Err(error) = tc.problem_sender.send_blocking(Event::Done) {
        eprintln!("Failed to send a DONE event: {}", error);
    }
}
