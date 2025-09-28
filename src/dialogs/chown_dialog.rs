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
    chown_component::ChownComponent,
    dir::Directory,
    file::File,
    pwd::{gid_t, uid, uid_t},
    utils::{ErrorMessage, NO_BUTTONS, SenderExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

#[async_recursion::async_recursion(?Send)]
async fn chown_recursively(
    parent_window: &gtk::Window,
    file: &File,
    uid: Option<uid_t>,
    gid: gid_t,
    recurse: bool,
) {
    if let Err(error) = file.chown(uid, gid) {
        ErrorMessage::with_error(
            gettext("Could not chown {}").replace("{}", &file.get_name()),
            &error,
        )
        .show(&parent_window)
        .await;
    }

    if !recurse {
        return;
    }

    if let Some(dir) = file.downcast_ref::<Directory>() {
        if let Err(error) = dir.list_files(parent_window, false).await {
            error.show(parent_window).await;
            return;
        }

        for child in dir.files().iter::<File>().flatten().filter(|child| {
            !child.is_dotdot()
                && child.file_info().display_name() != "."
                && !child.file_info().is_symlink()
        }) {
            chown_recursively(parent_window, &child, uid, gid, recurse).await;
        }
    }
}

async fn chown_files(
    parent_window: &gtk::Window,
    files: &glib::List<File>,
    uid: Option<uid_t>,
    gid: gid_t,
    recursive: bool,
) {
    for file in files {
        chown_recursively(parent_window, file, uid, gid, recursive).await;
    }
}

pub async fn show_chown_dialog(parent_window: &gtk::Window, files: &glib::List<File>) -> bool {
    let Some(file_info) = files.front().map(|f| f.file_info()) else {
        return false;
    };
    let owner = file_info.attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_UID) as uid_t;
    let group = file_info.attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_GID) as gid_t;

    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .title(gettext("Access Permissions"))
        .resizable(false)
        .build();

    let content_area = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .spacing(6)
        .build();
    dialog.set_child(Some(&content_area));

    let chown_component = ChownComponent::new();
    chown_component.set_ownership(owner, group);
    content_area.append(&chown_component);

    content_area.append(&gtk::Separator::new(gtk::Orientation::Horizontal));

    let recurse_check = gtk::CheckButton::with_label(&gettext("Apply Recursively"));
    content_area.append(&recurse_check);

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
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    ok_btn.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));

    content_area.append(&dialog_button_box(NO_BUTTONS, &[&cancel_btn, &ok_btn]));

    dialog.set_default_widget(Some(&ok_btn));
    dialog.present();

    let result = receiver.recv().await == Ok(true);

    if result {
        let (owner, group) = chown_component.ownership();
        let owner = (uid() == 0).then_some(owner);

        let recursive = recurse_check.is_active();

        chown_files(dialog.upcast_ref(), &files, owner, group, recursive).await;
    }

    dialog.close();

    result
}
