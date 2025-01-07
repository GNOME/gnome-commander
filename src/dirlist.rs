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

use crate::{dir::Directory, libgcmd::file_descriptor::FileDescriptorExt};
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

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
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

    button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| dialog.close()
    ));

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
    let file = dir.file();

    let dialog = if let Some(parent_window) = parent_window {
        Some(create_list_progress_dialog(parent_window))
    } else {
        None
    };

    let mut files = Vec::new();

    let enumerator = file
        .enumerate_children_future("*", gio::FileQueryInfoFlags::NONE, glib::Priority::DEFAULT)
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
