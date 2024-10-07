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

use crate::{
    dir::Directory,
    libgcmd::file_base::{FileBase, FileBaseExt},
};
use gettextrs::{gettext, ngettext};
use gtk::{gio, glib, prelude::*};

const FILES_PER_UPDATE: i32 = 50;

struct ProgressDialog {
    dialog: gtk::Dialog,
    label: gtk::Label,
    progress_bar: gtk::ProgressBar,
}

fn create_list_progress_dialog(parent_window: &gtk::Window) -> ProgressDialog {
    let dialog = gtk::Dialog::builder()
        .transient_for(parent_window)
        .resizable(false)
        .modal(true)
        .build();

    let content_area = dialog.content_area();
    content_area.set_margin_top(12);
    content_area.set_margin_bottom(12);
    content_area.set_margin_start(12);
    content_area.set_margin_end(12);
    content_area.set_spacing(12);

    let label = gtk::Label::builder()
        .label(gettext("Waiting for file list"))
        .build();
    content_area.append(&label);

    let progress_bar = gtk::ProgressBar::builder()
        .show_text(false)
        .pulse_step(0.2)
        .build();
    content_area.append(&progress_bar);

    let bbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .build();
    content_area.append(&bbox);

    let button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::Center)
        .build();
    bbox.append(&button);

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
    let file = dir.upcast_ref::<FileBase>().file();

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
            dlg.label.set_text(&ngettext!(
                "{} file listed",
                "{} files listed",
                files.len() as u32,
                files.len()
            ));
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
