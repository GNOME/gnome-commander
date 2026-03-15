// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    dir::Directory,
    file::{File, FileOps},
    main_win::MainWindow,
    transfer::copy_files,
    types::ConfirmOverwriteMode,
    utils::{NO_BUTTONS, SenderExt, WindowExt, dialog_button_box},
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
    dialog.add_css_class("dialog");

    let grid = gtk::Grid::builder().hexpand(true).vexpand(true).build();
    dialog.set_child(Some(&grid));

    let initial_value = f.name();

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
        .label(gettext("C_opy"))
        .use_underline(true)
        .receives_default(true)
        .build();
    ok_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));

    dialog.set_cancel_widget(&cancel_btn);

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
        dest_dir = Directory::new(&con, &con.create_uri(Path::new(parent)));
    } else {
        dest_dir = dir.clone();
        dest_fn = filename;
    }

    let src_files = vec![f.file().clone()];
    let success = copy_files(
        main_win.clone().upcast(),
        src_files,
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
