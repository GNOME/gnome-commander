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
    chmod_component::ChmodComponent,
    dir::Directory,
    file::File,
    utils::{ErrorMessage, NO_BUTTONS, SenderExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum ChmodRecursiveMode {
    AllFiles = 0,
    DirectoriesOnly,
}

#[async_recursion::async_recursion(?Send)]
async fn chmod_recursively(
    parent_window: &gtk::Window,
    file: &File,
    permissions: u32,
    recursive: Option<ChmodRecursiveMode>,
) {
    if let Err(error) = file.chmod(permissions) {
        ErrorMessage::with_error(
            gettext("Could not chmod {}").replace("{}", &file.get_name()),
            &error,
        )
        .show(&parent_window)
        .await;
        return;
    }

    if let Some(mode) = recursive {
        if let Some(dir) = file.downcast_ref::<Directory>() {
            if let Err(error) = dir.list_files(parent_window, false).await {
                error.show(parent_window).await;
                return;
            }

            for child in dir
                .files()
                .iter::<File>()
                .flatten()
                .filter(|child| {
                    !child.is_dotdot()
                        && child.file_info().display_name() != "."
                        && !child.file_info().is_symlink()
                })
                .filter(|child| match mode {
                    ChmodRecursiveMode::AllFiles => true,
                    ChmodRecursiveMode::DirectoriesOnly => {
                        child.file_info().file_type() == gio::FileType::Directory
                    }
                })
            {
                chmod_recursively(parent_window, &child, permissions, recursive).await;
            }
        }
    }
}

async fn chmod_files(
    parent_window: &gtk::Window,
    files: &glib::List<File>,
    permissions: u32,
    recursive: Option<ChmodRecursiveMode>,
) {
    for file in files {
        chmod_recursively(parent_window, file, permissions, recursive).await;
    }
}

pub async fn show_chmod_dialog(parent_window: &gtk::Window, files: &glib::List<File>) -> bool {
    let Some(file) = files.front() else {
        return false;
    };
    let permissions = file
        .file_info()
        .attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE)
        & 0xFFF;

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

    let chmod_component = ChmodComponent::new(permissions);
    content_area.append(&chmod_component);

    content_area.append(&gtk::Separator::new(gtk::Orientation::Horizontal));

    let recurse_check = gtk::CheckButton::with_label(&gettext("Apply Recursively for"));
    content_area.append(&recurse_check);

    let recurse_model =
        gtk::StringList::new(&[&gettext("All files"), &gettext("Directories only")]);
    let recurse_combo = gtk::DropDown::builder()
        .sensitive(false)
        .model(&recurse_model)
        .build();
    recurse_combo.set_selected(0);
    content_area.append(&recurse_combo);

    recurse_check.connect_toggled(glib::clone!(
        #[weak]
        recurse_combo,
        move |toggle| {
            recurse_combo.set_sensitive(toggle.is_active());
        }
    ));

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
        let permissions = chmod_component.permissions();
        let recursive = recurse_check
            .is_active()
            .then(|| match recurse_combo.selected() {
                0 => ChmodRecursiveMode::AllFiles,
                _ => ChmodRecursiveMode::DirectoriesOnly,
            });

        chmod_files(dialog.upcast_ref(), &files, permissions, recursive).await;
    }

    dialog.close();

    result
}
