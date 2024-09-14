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
    data::{ConfirmOptions, ConfirmOptionsRead, DeleteDefault, GeneralOptions, GeneralOptionsRead},
    dir::Directory,
    file::File,
    libgcmd::file_base::FileBaseExt,
    utils::{run_simple_dialog, show_error, Gtk3to4BoxCompat},
};
use gettextrs::{gettext, ngettext};
use glib::{
    ffi::{gboolean, GList},
    translate::from_glib_none,
};
use gtk::{ffi::GtkWindow, gio, glib, prelude::*, subclass::prelude::*};

#[derive(Clone, Copy)]
enum DeleteAction {
    DeleteToTrash,
    DeletePermanently,
}

enum DeleteNonEmpty {
    Cancel,
    Skip,
    DeleteAll,
    Delete,
}

enum DeleteErrorAction {
    Abort,
    Retry,
    Skip,
}

struct DeleteData {
    parent_window: gtk::Window,
    progress_dialog: Option<DeleteProgressDialog>,
    /// this is the real list of deleted files
    deleted_files: glib::List<File>,
    delete_action: DeleteAction,
    cancellable: gio::Cancellable,
}

mod imp {
    use super::*;
    use std::cell::Cell;

    pub struct DeleteProgressDialog {
        pub label: gtk::Label,
        pub progress_bar: gtk::ProgressBar,
        pub total: Cell<u64>,
        pub stop_button: gtk::Button,
        pub cancellable: gio::Cancellable,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for DeleteProgressDialog {
        const NAME: &'static str = "GnomeCmdDeleteProgressDialog";
        type Type = super::DeleteProgressDialog;
        type ParentType = gtk::Dialog;

        fn new() -> Self {
            Self {
                label: gtk::Label::builder().build(),
                progress_bar: gtk::ProgressBar::builder()
                    .text("0.0")
                    .fraction(0.0)
                    .build(),
                total: Default::default(),
                stop_button: gtk::Button::builder()
                    .label(gettext("_Cancel"))
                    .use_underline(true)
                    .hexpand(true)
                    .halign(gtk::Align::Center)
                    .build(),
                cancellable: gio::Cancellable::new(),
            }
        }
    }

    impl ObjectImpl for DeleteProgressDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dlg = self.obj();
            dlg.set_title(&gettext("Deleting…"));
            dlg.set_modal(true);
            dlg.set_resizable(false);

            let content_area = dlg.content_area();

            content_area.set_margin_top(12);
            content_area.set_margin_bottom(12);
            content_area.set_margin_start(12);
            content_area.set_margin_end(12);
            content_area.set_spacing(6);

            content_area.append(&self.label);
            content_area.append(&self.progress_bar);

            let bbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .build();
            content_area.append(&bbox);

            bbox.append(&self.stop_button);
            self.stop_button
                .connect_clicked(glib::clone!(@weak dlg => move |_| {
                    dlg.imp().cancellable.cancel();
                    dlg.response(gtk::ResponseType::Cancel);
                }));

            dlg.connect_close(glib::clone!(@weak dlg => move |_| {
                dlg.imp().cancellable.cancel();
                dlg.response(gtk::ResponseType::Cancel);
            }));

            content_area.show_all();
        }
    }

    impl WidgetImpl for DeleteProgressDialog {}
    impl ContainerImpl for DeleteProgressDialog {}
    impl BinImpl for DeleteProgressDialog {}
    impl WindowImpl for DeleteProgressDialog {}
    impl DialogImpl for DeleteProgressDialog {}
}

glib::wrapper! {
    pub struct DeleteProgressDialog(ObjectSubclass<imp::DeleteProgressDialog>)
        @extends gtk::Dialog, gtk::Window, gtk::Widget;
}

impl DeleteProgressDialog {
    pub fn new(parent: &gtk::Window) -> Self {
        let this: Self = glib::Object::builder().build();
        this.set_transient_for(Some(parent));
        this
    }

    pub fn connect_stop<F: Fn() + 'static>(&self, f: F) -> glib::SignalHandlerId {
        self.connect_response(move |_, _| f())
    }

    pub fn set_total(&self, total: u64) {
        self.imp().total.set(total);
    }

    pub fn set_progress(&self, deleted: usize) {
        let total = self.imp().total.get();
        if deleted > 0 && total > 0 {
            let f = (deleted as f64) / (total as f64);
            self.imp().label.set_text(&ngettext!(
                "Deleted {} of {} file",
                "Deleted {} of {} files",
                total as u32,
                deleted,
                total
            ));
            self.imp().progress_bar.set_fraction(f);
        }
    }

    pub fn cancellable(&self) -> &gio::Cancellable {
        &self.imp().cancellable
    }
}

async fn confirm_delete_directory(
    parent_window: &gtk::Window,
    file: &File,
    can_measure: bool,
    first_confirmation: bool,
    options: &dyn ConfirmOptionsRead,
) -> DeleteNonEmpty {
    let msg = if can_measure {
        gettext!(
            "The directory “{}” is not empty. Do you really want to delete it?",
            file.get_name()
        )
    } else {
        gettext!("Do you really want to delete “{}”?", file.get_name())
    };

    let answer = run_simple_dialog(
        parent_window,
        false,
        gtk::MessageType::Warning,
        &msg,
        &gettext("Delete"),
        match options.confirm_delete_default() {
            DeleteDefault::Cancel => Some(0),
            DeleteDefault::Delete => Some(1),
        },
        &[
            &gettext("Cancel"),
            &gettext("Skip"),
            &if first_confirmation {
                gettext("Delete All")
            } else {
                gettext("Delete Remaining")
            },
            &gettext("Delete"),
        ],
    )
    .await;

    match answer {
        gtk::ResponseType::Other(1) => DeleteNonEmpty::Skip,
        gtk::ResponseType::Other(2) => DeleteNonEmpty::DeleteAll,
        gtk::ResponseType::Other(3) => DeleteNonEmpty::Delete,
        _ => DeleteNonEmpty::Cancel,
    }
}

async fn ask_delete_problem_action(
    parent_window: &gtk::Window,
    filename: &str,
    error: &glib::Error,
) -> DeleteErrorAction {
    let msg = gettext!("Error while deleting “{}”\n\n{}", filename, error.message());

    let action = run_simple_dialog(
        parent_window,
        true,
        gtk::MessageType::Error,
        &msg,
        &gettext("Delete problem"),
        None,
        &[&gettext("Abort"), &gettext("Retry"), &gettext("Skip")],
    )
    .await;

    match action {
        gtk::ResponseType::Other(1) => DeleteErrorAction::Retry,
        gtk::ResponseType::Other(2) => DeleteErrorAction::Skip,
        _ => DeleteErrorAction::Abort,
    }
}

enum DeleteBreak {
    Abort,
    Error(glib::Error),
}

type DeleteResult = std::ops::ControlFlow<DeleteBreak, ()>;

async fn perform_delete_operation_one(delete_data: &mut DeleteData, file: &File) -> DeleteResult {
    if delete_data.cancellable.is_cancelled() {
        return DeleteResult::Break(DeleteBreak::Abort);
    }

    let filename = file.get_name();

    if file.is_dotdot() || filename == "." {
        return DeleteResult::Continue(());
    }

    let result = match delete_data.delete_action {
        DeleteAction::DeleteToTrash => file.file().trash(Some(&delete_data.cancellable)),
        DeleteAction::DeletePermanently => file.file().delete(Some(&delete_data.cancellable)),
    };

    match result {
        Ok(_) => {
            delete_data.deleted_files.push_back(file.clone());

            if let Some(ref progress_window) = delete_data.progress_dialog {
                progress_window.set_progress(delete_data.deleted_files.len());
            }

            return DeleteResult::Continue(());
        }
        Err(error) if error.matches(gio::IOErrorEnum::NotEmpty) => {
            let dir = file.downcast_ref::<Directory>().unwrap();
            dir.list_files(&delete_data.parent_window, false).await;

            for child in dir.files() {
                Box::pin(perform_delete_operation_one(delete_data, &child)).await?;
            }
            // Now remove the directory itself, as it is finally empty
            return Box::pin(perform_delete_operation_one(delete_data, file)).await;
        }
        Err(error) if error.matches(gio::IOErrorEnum::Cancelled) => {
            return DeleteResult::Break(DeleteBreak::Abort);
        }
        Err(error) => {
            eprintln!("Failed to delete {}: {}", filename, error.message());

            let problem_action =
                ask_delete_problem_action(&delete_data.parent_window, &filename, &error).await;

            return match problem_action {
                DeleteErrorAction::Abort => DeleteResult::Break(DeleteBreak::Abort),
                DeleteErrorAction::Retry => {
                    Box::pin(perform_delete_operation_one(delete_data, file)).await
                }
                DeleteErrorAction::Skip => DeleteResult::Break(DeleteBreak::Error(error)),
            };
        }
    }
}

async fn perform_delete_operation(
    delete_data: &mut DeleteData,
    files: &glib::List<File>,
) -> DeleteResult {
    for file in files {
        perform_delete_operation_one(delete_data, file).await?;
    }
    return DeleteResult::Continue(());
}

async fn count_total_items(files: &glib::List<File>) -> Option<u64> {
    let mut total = 0;
    for file in files {
        let (_, num_dirs, num_files) = file
            .file()
            .measure_disk_usage_future(gio::FileMeasureFlags::NONE, glib::Priority::DEFAULT)
            .0
            .await
            .ok()?;
        total += num_files + num_dirs;
    }
    Some(total)
}

async fn do_delete(
    parent_window: &gtk::Window,
    delete_action: DeleteAction,
    files: &glib::List<File>,
    mut show_progress: bool,
) {
    if files.is_empty() {
        return;
    }

    let mut items_total = 0;
    if show_progress {
        match count_total_items(files).await {
            Some(v) => items_total = v,
            None => {
                // If we cannot determine if gFile is empty or not, don't show progress dialog
                items_total = 0;
                show_progress = false;
            }
        }
    }

    let (progress_dialog, cancellable) = if show_progress {
        let progress_dialog = DeleteProgressDialog::new(parent_window);
        progress_dialog.set_total(items_total);
        let cancellable = progress_dialog.cancellable().clone();
        (Some(progress_dialog), cancellable)
    } else {
        (None, gio::Cancellable::new())
    };

    let mut delete_data = DeleteData {
        parent_window: progress_dialog
            .as_ref()
            .map_or(parent_window, |p| p.upcast_ref())
            .clone(),
        progress_dialog,
        deleted_files: glib::List::new(),
        delete_action,
        cancellable,
    };

    let result = perform_delete_operation(&mut delete_data, files).await;

    match result {
        DeleteResult::Break(DeleteBreak::Error(error)) => {
            show_error(
                &delete_data.parent_window,
                &gettext("Deletion failed"),
                &error,
            );
        }
        DeleteResult::Break(DeleteBreak::Abort) => {}
        DeleteResult::Continue(()) => {}
    }

    for file in delete_data.deleted_files {
        if !file.file().query_exists(gio::Cancellable::NONE) {
            file.set_deleted();
        }
    }

    if let Some(progress_dialog) = delete_data.progress_dialog {
        progress_dialog.close();
    }
}

async fn measure_directory(file: &File) -> Result<(bool, bool), glib::Error> {
    match file
        .file()
        .measure_disk_usage_future(gio::FileMeasureFlags::NONE, glib::Priority::DEFAULT)
        .0
        .await
    {
        Ok((_, num_dirs, num_files)) => {
            // num_dirs = 1 -> this is the folder to be deleted
            Ok((num_dirs != 1 || num_files != 0, true))
        }
        Err(error) if error.matches(gio::IOErrorEnum::NotSupported) => {
            // If we cannot determine if directory is empty or not, we have to assume it is not empty
            Ok((true, false))
        }
        Err(error) => Err(error),
    }
}

/// Remove a directory from the list of files to be deleted.
/// This happens as of user interaction.
async fn remove_items_from_list_to_be_deleted(
    parent_window: &gtk::Window,
    files: glib::List<File>,
    options: &dyn ConfirmOptionsRead,
) -> Result<glib::List<File>, glib::Error> {
    if !options.confirm_delete() {
        return Ok(files);
    };

    let mut files_to_delete = glib::List::new();

    let mut delete_all_confirmed = false;
    let mut first_confirmation = true;
    for file in files {
        if delete_all_confirmed {
            files_to_delete.push_back(file);
            continue;
        }

        if !file.file_info().is_symlink()
            && file.file_info().file_type() == gio::FileType::Directory
        {
            let (confirm_needed, can_measure) = measure_directory(&file).await?;
            if confirm_needed {
                let response = confirm_delete_directory(
                    parent_window,
                    &file,
                    can_measure,
                    first_confirmation,
                    options,
                )
                .await;
                first_confirmation = false;

                match response {
                    DeleteNonEmpty::Cancel => {
                        return Ok(glib::List::new());
                    }
                    DeleteNonEmpty::DeleteAll => {
                        files_to_delete.push_back(file);
                        delete_all_confirmed = true;
                    }
                    DeleteNonEmpty::Skip => {}
                    DeleteNonEmpty::Delete => {
                        files_to_delete.push_back(file);
                    }
                }
            }
        } else {
            files_to_delete.push_back(file);
        }
    }

    Ok(files_to_delete)
}

async fn confirm_delete(
    parent_window: &gtk::Window,
    files: &glib::List<File>,
    delete_action: DeleteAction,
    options: &dyn ConfirmOptionsRead,
) -> bool {
    if !options.confirm_delete() {
        return true;
    }

    let n_files = files.len();
    let text = if n_files == 1 {
        let file = files.front().unwrap();
        match delete_action {
            DeleteAction::DeletePermanently => {
                gettext!("Do you want to permanently delete “{}”?", file.get_name())
            }
            DeleteAction::DeleteToTrash => gettext!(
                "Do you want to move “{}” to the trash can?",
                file.get_name()
            ),
        }
    } else {
        match delete_action {
            DeleteAction::DeletePermanently => ngettext(
                "Do you want to permanently delete the selected file?",
                "Do you want to permanently delete the %d selected files?",
                n_files as u32,
            )
            .replace("%d", &n_files.to_string()),
            DeleteAction::DeleteToTrash => ngettext(
                "Do you want to move the selected file to the trash can?",
                "Do you want to move the %d selected files to the trash can?",
                n_files as u32,
            )
            .replace("%d", &n_files.to_string()),
        }
    };

    run_simple_dialog(
        parent_window,
        false,
        match delete_action {
            DeleteAction::DeletePermanently => gtk::MessageType::Warning,
            DeleteAction::DeleteToTrash => gtk::MessageType::Question,
        },
        &text,
        &gettext("Delete"),
        match options.confirm_delete_default() {
            DeleteDefault::Cancel => Some(0),
            DeleteDefault::Delete => Some(1),
        },
        &[&gettext("Cancel"), &gettext("Delete")],
    )
    .await
        == gtk::ResponseType::Other(1)
}

pub async fn show_delete_dialog(
    parent_window: &gtk::Window,
    files: &glib::List<File>,
    force_delete: bool,
    general_options: &dyn GeneralOptionsRead,
    confirm_options: &dyn ConfirmOptionsRead,
) {
    let files: glib::List<File> = files.iter().filter(|f| !f.is_dotdot()).cloned().collect();
    if files.is_empty() {
        return;
    }

    let permanent_delete = force_delete || !general_options.use_trash();
    let delete_action = if permanent_delete {
        DeleteAction::DeletePermanently
    } else {
        DeleteAction::DeleteToTrash
    };

    if !confirm_delete(parent_window, &files, delete_action, confirm_options).await {
        return;
    }

    let files =
        match remove_items_from_list_to_be_deleted(parent_window, files, confirm_options).await {
            Ok(files) => files,
            Err(error) => {
                run_simple_dialog(
                    parent_window,
                    false,
                    gtk::MessageType::Error,
                    &gettext!("Error while deleting: \n\n{}", error.message()),
                    &gettext("Delete problem"),
                    Some(0),
                    &[&gettext("Abort")],
                )
                .await;
                return;
            }
        };

    if files.is_empty() {
        return;
    }

    do_delete(parent_window, delete_action, &files, true).await;
}

#[no_mangle]
pub extern "C" fn gnome_cmd_delete_dialog_show(
    parent_window: *mut GtkWindow,
    files: *mut GList,
    force_delete: gboolean,
) {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window) };
    let files: glib::List<File> = unsafe { glib::List::from_glib_none(files) };
    let general_options = GeneralOptions::new();
    let confirm_options = ConfirmOptions::new();

    glib::MainContext::default().spawn_local(async move {
        show_delete_dialog(
            &parent_window,
            &files,
            force_delete != 0,
            &general_options,
            &confirm_options,
        )
        .await;
    });
}

#[no_mangle]
pub extern "C" fn do_delete_files_for_move(
    parent_window: *mut GtkWindow,
    files: *mut GList,
    show_progress: gboolean,
) {
    let parent_window: gtk::Window = unsafe { from_glib_none(parent_window) };
    let files: glib::List<File> = unsafe { glib::List::from_glib_none(files) };

    glib::MainContext::default().spawn_local(async move {
        do_delete(
            &parent_window,
            DeleteAction::DeletePermanently,
            &files,
            show_progress != 0,
        )
        .await;
    });
}
