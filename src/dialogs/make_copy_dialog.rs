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
    dir::Directory,
    file::File,
    main_win::MainWindow,
    transfer::copy_files,
    types::ConfirmOverwriteMode,
    utils::{NO_BUTTONS, SenderExt, channel_send_action, dialog_button_box, handle_escape_key},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};
use std::path::{Path, PathBuf};

pub async fn make_copy_dialog(f: &File, dir: &Directory, main_win: &MainWindow) {
    let dialog = gtk::Window::builder()
        .resizable(false)
        .title(gettext("Copy File"))
        .modal(true)
        .transient_for(main_win)
        .build();

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
    dialog.set_child(Some(&grid));

    let initial_value = f.file_info().display_name();

    let label = gtk::Label::builder()
        .label(gettext("Copy “{}” to").replace("{}", &initial_value))
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

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    handle_escape_key(&dialog, &channel_send_action(&sender, false));

    let cancel_btn = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    cancel_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(false)
    ));

    let ok_btn = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .receives_default(true)
        .build();
    ok_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_btn, &ok_btn]),
        0,
        2,
        1,
        1,
    );

    entry.connect_changed(glib::clone!(
        #[weak]
        ok_btn,
        move |entry| ok_btn.set_sensitive(!entry.text().is_empty())
    ));

    entry.grab_focus();
    dialog.present();

    let response = receiver.recv().await;
    let filename = entry.text();
    let filename = filename.trim_end_matches("/");
    dialog.close();

    if response != Ok(true) {
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

    let success = copy_files(
        main_win.clone().upcast(),
        [f.file().clone()].into_iter().collect(),
        dest_dir,
        Some(PathBuf::from(dest_fn)),
        gio::FileCopyFlags::NONE,
        ConfirmOverwriteMode::Query,
    )
    .await;

    if success {
        if let Err(error) = dir.relist_files(main_win.upcast_ref(), false).await {
            error.show(main_win.upcast_ref()).await;
        }
        main_win.focus_file_lists();
    }
}
