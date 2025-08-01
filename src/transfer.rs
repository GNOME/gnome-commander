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
    data::{GeneralOptions, GeneralOptionsRead},
    dialogs::transfer_progress_dialog::TransferProgressWindow,
    dir::Directory,
    types::{ConfirmOverwriteMode, GnomeCmdTransferType, SizeDisplayMode},
    utils::{nice_size, pending, time_to_string, ErrorMessage},
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
    use crate::{dir::ffi::GnomeCmdDir, types::ConfirmOverwriteMode};
    use gtk::{gio, glib};
    use std::{ffi::c_char, os::raw::c_void};

    extern "C" {
        pub fn gnome_cmd_copy_gfiles_start(
            parent_window: *const gtk::ffi::GtkWindow,
            src_uri_list: *const glib::ffi::GList,
            to: *const GnomeCmdDir,
            dest_fn: *const c_char,
            copy_flags: gio::ffi::GFileCopyFlags,
            overwrite_mode: ConfirmOverwriteMode,
            on_completed_func: *const c_void,
            on_completed_data: *mut c_void,
        );

        pub fn gnome_cmd_move_gfiles_start(
            parent_window: *const gtk::ffi::GtkWindow,
            src_uri_list: *const glib::ffi::GList,
            to: *const GnomeCmdDir,
            dest_fn: *const c_char,
            copy_flags: gio::ffi::GFileCopyFlags,
            overwrite_mode: ConfirmOverwriteMode,
            on_completed_func: *const c_void,
            on_completed_data: *mut c_void,
        );

        pub fn gnome_cmd_link_gfiles_start(
            parent_window: *const gtk::ffi::GtkWindow,
            src_uri_list: *const glib::ffi::GList,
            to: *const GnomeCmdDir,
            dest_fn: *const c_char,
            copy_flags: gio::ffi::GFileCopyFlags,
            overwrite_mode: ConfirmOverwriteMode,
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
    dest_fn: Option<String>,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,
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
            to.to_glib_full(),
            dest_fn.to_glib_full(),
            copy_flags.into_glib(),
            overwrite_mode,
            transmute(send as *const ()),
            Box::into_raw(Box::new(sender)) as *mut _,
        )
    }
    let result = receiver.recv().await.unwrap_or_default();
    result
}

pub async fn gnome_cmd_move_gfiles(
    parent_window: gtk::Window,
    src_uri_list: glib::List<gio::File>,
    to: Directory,
    dest_fn: Option<String>,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,
) -> bool {
    let (sender, receiver) = async_channel::bounded::<bool>(1);
    unsafe extern "C" fn send(succeess: glib::ffi::gboolean, user_data: glib::ffi::gpointer) {
        let sender = Box::<Sender<bool>>::from_raw(user_data as *mut Sender<bool>);
        let _ = sender.send_blocking(succeess != 0);
    }
    unsafe {
        ffi::gnome_cmd_move_gfiles_start(
            parent_window.to_glib_none().0,
            src_uri_list.into_raw(),
            to.to_glib_full(),
            dest_fn.to_glib_full(),
            copy_flags.into_glib(),
            overwrite_mode,
            transmute(send as *const ()),
            Box::into_raw(Box::new(sender)) as *mut _,
        )
    }
    let result = receiver.recv().await.unwrap_or_default();
    result
}

pub async fn gnome_cmd_link_gfiles(
    parent_window: gtk::Window,
    src_uri_list: glib::List<gio::File>,
    to: Directory,
    dest_fn: Option<String>,
    copy_flags: gio::FileCopyFlags,
    overwrite_mode: ConfirmOverwriteMode,
) -> bool {
    let (sender, receiver) = async_channel::bounded::<bool>(1);
    unsafe extern "C" fn send(succeess: glib::ffi::gboolean, user_data: glib::ffi::gpointer) {
        let sender = Box::<Sender<bool>>::from_raw(user_data as *mut Sender<bool>);
        let _ = sender.send_blocking(succeess != 0);
    }
    unsafe {
        ffi::gnome_cmd_link_gfiles_start(
            parent_window.to_glib_none().0,
            src_uri_list.into_raw(),
            to.to_glib_full(),
            dest_fn.to_glib_full(),
            copy_flags.into_glib(),
            overwrite_mode,
            transmute(send as *const ()),
            Box::into_raw(Box::new(sender)) as *mut _,
        )
    }
    let result = receiver.recv().await.unwrap_or_default();
    result
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
    Ok(
        gettext("<b>{name}</b>\n<span color='dimgray' size='smaller'>{size}, {time}</span>")
            .replace("{name}", &info.display_name())
            .replace("{size}", &nice_size(info.size() as u64, size_display_mode))
            .replace(
                "{time}",
                &info
                    .modification_date_time()
                    .and_then(|dt| time_to_string(dt, date_format).ok())
                    .unwrap_or_default(),
            ),
    )
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
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<ProblemAction, glib::Error> {
    let msg = gettext("Overwrite file:\n\n{dst}\n\nWith:\n\n{src}")
        .replace("{dst}", &file_details(dst, size_display_mode, date_format)?)
        .replace("{src}", &file_details(src, size_display_mode, date_format)?);

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
    size_display_mode: SizeDisplayMode,
    date_format: &str,
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

    Ok(run_simple_error_dialog(parent_window, &msg, &error).await)
}

async fn run_move_overwrite_dialog(
    parent_window: &gtk::Window,
    src: &gio::File,
    dst: &gio::File,
    many: bool,
    size_display_mode: SizeDisplayMode,
    date_format: &str,
) -> Result<ProblemAction, glib::Error> {
    let msg = gettext("Overwrite file:\n\n{dst}\n\nWith:\n\n{src}")
        .replace("{dst}", &file_details(dst, size_display_mode, date_format)?)
        .replace("{src}", &file_details(src, size_display_mode, date_format)?);

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
    size_display_mode: SizeDisplayMode,
    date_format: &str,
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
    let msg = gettext("Error while creating symlink “{file}”").replace(
        "{file}",
        &file_details(dst, size_display_mode, date_format)?,
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
    overwrite_mode: ConfirmOverwriteMode,
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

                win.set_msg(
                    &gettext("[file {index} of {count}] “{file}”")
                        .replace("{index}", &(current_file_number + 1).to_string())
                        .replace("{count}", &files_total.to_string())
                        .replace("{file}", &current_src_file_name),
                );
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
                        ErrorMessage::with_error(gettext("Unrecoverable error"), &error)
                            .show(win.upcast_ref())
                            .await;
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
    overwrite_mode: ConfirmOverwriteMode,
) -> *mut TransferControl {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window_ptr) };
    let title: String = unsafe { from_glib_none(title) };

    let options = GeneralOptions::new();

    let win = TransferProgressWindow::new(no_of_files, options.size_display_mode());
    win.set_transient_for(Some(&parent_window));
    win.set_title(Some(&title));

    let tc = Box::new(TransferControl::new(win.cancellable()));
    let tc1 = tc.clone();
    glib::spawn_future_local(async move {
        win.present();
        let success = transfer_gui_loop(
            &win,
            &tc1,
            no_of_files,
            transfer_type,
            overwrite_mode,
            options.size_display_mode(),
            &options.date_display_format(),
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
