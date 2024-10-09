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
    connection::connection::ConnectionExt,
    dir::{ffi::GnomeCmdDir, Directory},
    file::{ffi::GnomeCmdFile, File, GnomeCmdFileExt},
    libgcmd::file_base::FileBaseExt,
    main_win::{ffi::GnomeCmdMainWin, MainWindow},
    transfer::gnome_cmd_copy_gfiles,
    types::GnomeCmdConfirmOverwriteMode,
    utils::close_dialog_with_escape_key,
};
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{self, translate::from_glib_none},
    prelude::*,
};
use std::path::Path;

pub async fn make_copy_dialog(f: &File, dir: &Directory, main_win: &MainWindow) {
    let dialog = gtk::Dialog::builder()
        .resizable(false)
        .title(gettext("Copy File"))
        .build();
    close_dialog_with_escape_key(&dialog);

    let grid = gtk::Grid::builder()
        .hexpand(true)
        .vexpand(true)
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
    dialog.content_area().append(&grid);

    let initial_value = f.file_info().display_name();

    let label = gtk::Label::builder()
        .label(gettext!("Copy “{}” to", initial_value))
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Center)
        .build();
    grid.attach(&label, 0, 0, 1, 1);

    let entry = gtk::Entry::builder()
        .activates_default(true)
        .hexpand(true)
        .text(initial_value)
        .build();
    grid.attach(&entry, 0, 1, 1, 1);

    let buttonbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .hexpand(false)
        .halign(gtk::Align::End)
        .build();
    let buttonbox_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);
    grid.attach(&buttonbox, 0, 2, 1, 1);

    let cancel_btn = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_btn.connect_clicked(
        glib::clone!(@weak dialog => move |_| dialog.response(gtk::ResponseType::Cancel)),
    );
    buttonbox.append(&cancel_btn);
    buttonbox_size_group.add_widget(&cancel_btn);

    let ok_btn = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .receives_default(true)
        .build();
    ok_btn.connect_clicked(
        glib::clone!(@weak dialog => move |_| dialog.response(gtk::ResponseType::Ok)),
    );
    buttonbox.append(&ok_btn);
    buttonbox_size_group.add_widget(&ok_btn);

    entry.connect_changed(glib::clone!(@weak ok_btn => move|entry| {
        ok_btn.set_sensitive(!entry.text().is_empty());
    }));

    entry.grab_focus();
    dialog.present();

    let response = dialog.run_future().await;
    let filename = entry.text();
    let filename = filename.trim_end_matches("/");
    dialog.close();

    if response != gtk::ResponseType::Ok {
        return;
    }

    let dest_dir;
    let dest_fn;
    if filename.starts_with('/') {
        let (parent, base) = filename.rsplit_once('/').unwrap();
        dest_fn = base;

        let con = dir.connection();
        let con_path = con.create_path(&Path::new(parent));
        dest_dir = Directory::new(&con, con_path);
    } else {
        dest_dir = dir.clone();
        dest_fn = filename;
    }

    let success = gnome_cmd_copy_gfiles(
        main_win.clone().upcast(),
        [f.file().clone()].into_iter().collect(),
        dest_dir,
        Some(dest_fn.to_owned()),
        gio::FileCopyFlags::NONE,
        GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
    )
    .await;

    if success {
        dir.relist_files(main_win.upcast_ref(), false).await;
        main_win.focus_file_lists();
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_make_copy_dialog_run_r(
    file: *mut GnomeCmdFile,
    dir: *mut GnomeCmdDir,
    main_win: *mut GnomeCmdMainWin,
) {
    let file: File = unsafe { from_glib_none(file) };
    let dir: Directory = unsafe { from_glib_none(dir) };
    let main_win: MainWindow = unsafe { from_glib_none(main_win) };
    glib::MainContext::default().spawn_local(async move {
        make_copy_dialog(&file, &dir, &main_win).await;
    });
}
