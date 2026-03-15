// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    dir::Directory,
    file::{File, FileOps},
    utils::WindowExt,
};
use gettextrs::{gettext, ngettext};
use gtk::{gio, glib, prelude::*};

const FILES_PER_UPDATE: i32 = 50;

struct ProgressDialog {
    dialog: gtk::Window,
    label: gtk::Label,
    progress_bar: gtk::ProgressBar,
}

fn create_list_progress_dialog(parent_window: &gtk::Window) -> ProgressDialog {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .resizable(false)
        .modal(true)
        .build();
    dialog.add_css_class("dialog");

    let grid = gtk::Grid::builder().build();
    dialog.set_child(Some(&grid));

    let label = gtk::Label::builder()
        .label(gettext("Waiting for file list"))
        .build();
    grid.attach(&label, 0, 0, 1, 1);

    let progress_bar = gtk::ProgressBar::builder()
        .show_text(false)
        .pulse_step(0.2)
        .build();
    grid.attach(&progress_bar, 0, 1, 1, 1);

    let button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::Center)
        .build();
    grid.attach(&button, 0, 2, 1, 1);
    dialog.set_cancel_widget(&button);

    button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| dialog.close()
    ));

    dialog.present();

    ProgressDialog {
        dialog,
        label,
        progress_bar,
    }
}

pub async fn list_directory(
    dir: &Directory,
    parent_window: Option<&gtk::Window>,
) -> Result<glib::List<gio::FileInfo>, glib::Error> {
    let file = dir.file().clone();

    let dialog = parent_window.map(create_list_progress_dialog);

    let mut files = Vec::new();

    let enumerator = file
        .enumerate_children_future(
            File::DEFAULT_ATTRIBUTES,
            gio::FileQueryInfoFlags::NONE,
            glib::Priority::DEFAULT,
        )
        .await?;

    let mut error = None;
    loop {
        match enumerator
            .next_files_future(FILES_PER_UPDATE, glib::Priority::DEFAULT)
            .await
        {
            Ok(chunk) if chunk.is_empty() => {
                break;
            }
            Ok(chunk) => {
                files.extend(chunk);
            }
            Err(e) => {
                error = Some(e);
                break;
            }
        }

        if let Some(ref dlg) = dialog {
            dlg.label.set_text(
                &ngettext("{} file listed", "{} files listed", files.len() as u32)
                    .replace("{}", &files.len().to_string()),
            );
            dlg.progress_bar.pulse();
            if !dlg.dialog.is_visible() {
                // cancelled
                break;
            }
        }
    }

    enumerator.close_future(glib::Priority::DEFAULT).await?;

    if let Some(ref dlg) = dialog {
        dlg.dialog.close();
    }

    match error {
        Some(error) => Err(error),
        None => Ok(files.into_iter().collect()),
    }
}
