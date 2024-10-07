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
    connection::{
        connection::{Connection, ConnectionExt},
        device::ConnectionDevice,
        list::ConnectionList,
    },
    dir::Directory,
    file::{File, GnomeCmdFileExt},
    file_selector::FileSelector,
    utils::{bold, pending, run_simple_dialog, show_error_message_future},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, prelude::*, subclass::prelude::*};
use std::path::{Path, PathBuf};

mod imp {
    use super::*;
    use crate::{dir::Directory, file_selector::FileSelector, utils::toggle_file_name_selection};
    use std::cell::OnceCell;

    pub struct PrepareTransferDialog {
        pub dst_label: gtk::Label,
        pub dst_entry: gtk::Entry,
        pub left_vbox: gtk::Box,
        pub right_vbox: gtk::Box,
        pub ok_button: gtk::Button,
        pub cancel_button: gtk::Button,
        pub src_files: OnceCell<glib::List<File>>,
        pub src_fs: OnceCell<FileSelector>,
        pub dst_fs: OnceCell<FileSelector>,
        pub default_dest_dir: OnceCell<Directory>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for PrepareTransferDialog {
        const NAME: &'static str = "GnomeCmndPrepareTransferDialog";
        type Type = super::PrepareTransferDialog;
        type ParentType = gtk::Dialog;

        fn new() -> Self {
            Self {
                dst_label: gtk::Label::builder()
                    .use_markup(true)
                    .halign(gtk::Align::Start)
                    .build(),
                dst_entry: gtk::Entry::builder().activates_default(true).build(),
                left_vbox: gtk::Box::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .spacing(6)
                    .hexpand(true)
                    .build(),
                right_vbox: gtk::Box::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .spacing(6)
                    .hexpand(true)
                    .build(),
                ok_button: gtk::Button::builder()
                    .label(gettext("_OK"))
                    .use_underline(true)
                    .hexpand(true)
                    .halign(gtk::Align::Start)
                    .receives_default(true)
                    .build(),
                cancel_button: gtk::Button::builder()
                    .label(gettext("_Cancel"))
                    .use_underline(true)
                    .hexpand(true)
                    .halign(gtk::Align::End)
                    .build(),
                src_files: OnceCell::new(),
                src_fs: OnceCell::new(),
                dst_fs: OnceCell::new(),
                default_dest_dir: OnceCell::new(),
            }
        }
    }

    impl ObjectImpl for PrepareTransferDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dlg = self.obj();

            dlg.set_width_request(500);
            dlg.set_resizable(false);

            let content_area = dlg.content_area();

            content_area.set_margin_top(12);
            content_area.set_margin_bottom(12);
            content_area.set_margin_start(12);
            content_area.set_margin_end(12);
            content_area.set_spacing(6);

            content_area.append(&self.dst_label);
            content_area.append(&self.dst_entry);

            self.dst_label.set_mnemonic_widget(Some(&self.dst_entry));

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, keyval, _, _| imp.dst_entry_key_pressed(keyval)
            ));
            self.dst_entry.add_controller(key_controller);

            let options_hbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .build();
            content_area.append(&options_hbox);

            options_hbox.append(&self.left_vbox);
            options_hbox.append(&self.right_vbox);

            let bbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .build();
            content_area.append(&bbox);
            let bbox_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Both);

            bbox.append(&self.cancel_button);
            bbox_size_group.add_widget(&self.cancel_button);

            bbox.append(&self.ok_button);
            bbox_size_group.add_widget(&self.ok_button);

            self.cancel_button.connect_clicked(glib::clone!(
                #[weak]
                dlg,
                move |_| dlg.response(gtk::ResponseType::Cancel)
            ));

            self.ok_button.connect_clicked(glib::clone!(
                #[weak]
                dlg,
                move |_| dlg.response(gtk::ResponseType::Ok)
            ));

            dlg.set_default_response(gtk::ResponseType::Ok);
        }
    }

    impl WidgetImpl for PrepareTransferDialog {}
    impl WindowImpl for PrepareTransferDialog {}
    impl DialogImpl for PrepareTransferDialog {}

    impl PrepareTransferDialog {
        fn dst_entry_key_pressed(&self, keyval: gdk::Key) -> glib::Propagation {
            if keyval == gdk::Key::Return || keyval == gdk::Key::KP_Enter {
                self.obj().response(gtk::ResponseType::Ok);
                glib::Propagation::Stop
            } else if keyval == gdk::Key::F5 || keyval == gdk::Key::F6 {
                toggle_file_name_selection(&self.dst_entry);
                glib::Propagation::Stop
            } else {
                glib::Propagation::Proceed
            }
        }
    }
}

glib::wrapper! {
    pub struct PrepareTransferDialog(ObjectSubclass<imp::PrepareTransferDialog>)
        @extends gtk::Dialog, gtk::Window, gtk::Widget;
}

fn prepend_slash(str: String) -> String {
    if !str.starts_with("/") {
        format!("/{}", str)
    } else {
        str
    }
}

fn con_device_has_path(fs: &FileSelector, path: &Path) -> Option<(Connection, PathBuf)> {
    let con = fs.connection()?;
    let mount_path = con.downcast_ref::<ConnectionDevice>()?.mountp_string()?;
    let relative_path = path.strip_prefix(&mount_path).ok()?;
    Some((con, relative_path.to_path_buf()))
}

fn path_points_at_directory(to: &FileSelector, dest_path: &Path) -> bool {
    let Some(con) = to.connection() else {
        return false;
    };
    let ftype = con.path_target_type(dest_path);
    ftype == Some(gio::FileType::Directory)
}

async fn ask_create_directory(parent_window: &gtk::Window, path: &Path) -> bool {
    let msg = gettext("The directory “{}” doesn’t exist, do you want to create it?").replace(
        "{}",
        &path
            .file_name()
            .unwrap_or(path.as_os_str())
            .to_string_lossy(),
    );
    let answer = run_simple_dialog(
        parent_window,
        true,
        gtk::MessageType::Question,
        &msg,
        "",
        None,
        &[&gettext("No"), &gettext("Yes")],
    )
    .await;
    answer == gtk::ResponseType::Other(1)
}

impl PrepareTransferDialog {
    pub fn new(from: &FileSelector, to: &FileSelector) -> Self {
        let dialog: Self = glib::Object::builder().build();

        let src_files = from.file_list().selected_files();
        let default_dest_dir = to.directory().unwrap();

        let single_source_file = if src_files.len() == 1 {
            src_files.front()
        } else {
            None
        };

        let mut dst: Option<String>;
        if !default_dest_dir.connection().is_local() {
            dst = single_source_file.map(|f| f.get_name());
        } else {
            let dst_path = default_dest_dir.upcast_ref::<File>().get_real_path();
            dst = Some(dst_path.to_string_lossy().to_string());

            if let Some(file) = single_source_file {
                let d = dst_path.join(file.get_name());
                if !path_points_at_directory(to, &d) {
                    dst = Some(d.to_string_lossy().to_string());
                }
            }
        }

        dialog.imp().src_files.set(src_files).ok().unwrap();
        dialog
            .imp()
            .default_dest_dir
            .set(default_dest_dir)
            .ok()
            .unwrap();
        dialog.imp().src_fs.set(from.clone()).ok().unwrap();
        dialog.imp().dst_fs.set(to.clone()).ok().unwrap();
        if let Some(dst) = dst {
            dialog.imp().dst_entry.set_text(&dst);
        }
        dialog.imp().dst_entry.grab_focus();

        dialog
    }

    pub fn set_dst_label(&self, label: &str) {
        self.imp().dst_label.set_markup(&bold(label));
    }

    pub fn append_to_left(&self, widget: &impl IsA<gtk::Widget>) {
        self.imp().left_vbox.append(widget);
    }

    pub fn append_to_right(&self, widget: &impl IsA<gtk::Widget>) {
        self.imp().right_vbox.append(widget);
    }

    pub async fn run(&self) -> Option<(Directory, Option<String>)> {
        let (sender, receiver) = async_channel::bounded::<gtk::ResponseType>(1);
        let response_handler = self.connect_response(move |_, response_type| {
            if let Err(error) = sender.send_blocking(response_type) {
                eprintln!("Failed to send a response {:?}: {}", response_type, error);
            }
        });

        self.present();
        self.set_default_widget(Some(&self.imp().ok_button));

        let result = loop {
            match receiver.recv().await {
                Ok(gtk::ResponseType::Ok) => {
                    self.imp().ok_button.set_sensitive(false);
                    self.imp().cancel_button.set_sensitive(false);

                    let dst = self.imp().dst_entry.text();
                    let dest = handle_user_input(
                        self.upcast_ref(),
                        self.imp().src_files.get().unwrap(),
                        self.imp().src_fs.get().unwrap(),
                        self.imp().dst_fs.get().unwrap(),
                        self.imp().default_dest_dir.get().unwrap(),
                        &dst,
                    )
                    .await;

                    self.imp().ok_button.set_sensitive(true);
                    self.imp().cancel_button.set_sensitive(true);

                    if dest.is_some() {
                        self.disconnect(response_handler);
                        break dest;
                    }
                }
                Ok(gtk::ResponseType::DeleteEvent) => {
                    break None;
                }
                Ok(_) => {
                    self.disconnect(response_handler);
                    break None;
                }
                Err(_) => {
                    break None;
                }
            }
        };

        self.close();

        pending().await;

        result
    }
}

pub async fn handle_user_input(
    parent_window: &gtk::Window,
    src_files: &glib::List<File>,
    src_fs: &FileSelector,
    dst_fs: &FileSelector,
    default_dest_dir: &Directory,
    user_path: &str,
) -> Option<(Directory, Option<String>)> {
    let Some(src_directory) = src_fs.directory() else {
        eprintln!("");
        return None;
    };

    let mut con = default_dest_dir.connection();

    let user_path = user_path.trim();
    if user_path.is_empty() {
        return None;
    }

    let mut dest_path = PathBuf::from(&user_path);

    // Make whatever the user entered into a valid path if possible
    // if user_path.len() > 2 {
    //     if let Some(stripped) = user_path.strip_suffix("/") {
    //         user_path = stripped.to_owned();
    //     }
    // }

    if dest_path.starts_with("/") {
        if dst_fs.connection().map_or(false, |c| c.is_local()) {
            if let Some((dev, path)) = con_device_has_path(&dst_fs, &dest_path)
                .or_else(|| con_device_has_path(&src_fs, &dest_path))
            {
                // if LEFT or RIGHT device (connection) points to user_path then adjust user_path and set con to the found device
                con = dev;
                dest_path = path;
            } else {
                // otherwise connection not present in any pane, use home connection instead
                con = ConnectionList::get().home();
            }
        }
    } else {
        if !default_dest_dir.upcast_ref::<File>().is_local() {
            dest_path = PathBuf::from(prepend_slash(
                default_dest_dir.path().child(&user_path).path(),
            ));
        } else {
            dest_path = PathBuf::from(
                src_directory
                    .upcast_ref::<File>()
                    .get_path_through_parent()
                    .child(&user_path)
                    .path(),
            );
        }
    }

    // Check if something exists at the given path and find out what it is

    let file_type = con.path_target_type(&dest_path);
    let dest_dir: Option<Directory>;
    let mut dest_fn: Option<String> = None;

    if src_files.len() == 1 {
        let single_source_file = src_files.front().unwrap();

        if file_type == Some(gio::FileType::Directory) {
            // There exists a directory, copy into it using the original filename
            dest_dir = Some(Directory::new(&con, con.create_path(&dest_path)));
            dest_fn = Some(single_source_file.get_name());
        } else if file_type.is_some() {
            // There exists something else, asume that the user wants to overwrite it for now
            dest_dir = Some(Directory::new(
                &con,
                con.create_path(dest_path.parent().unwrap()),
            ));
            dest_fn = dest_path
                .file_name()
                .map(|n| n.to_string_lossy().to_string());
        } else {
            // Nothing found, check if the parent dir exists
            let parent_dir = dest_path.parent().unwrap();
            let file_type = con.path_target_type(parent_dir);
            if file_type == Some(gio::FileType::Directory) {
                // yup, xfer to it
                dest_dir = Some(Directory::new(
                    &con,
                    con.create_path(Path::new(&parent_dir)),
                ));
                dest_fn = dest_path
                    .file_name()
                    .map(|n| n.to_string_lossy().to_string());
            } else if file_type.is_some() {
                // the parent dir was a file, abort!
                return None;
            } else {
                // Nothing exists, ask the user if a new directory might be suitable in the path that he specified
                if ask_create_directory(parent_window, parent_dir).await {
                    if let Err(error) = con.mkdir(parent_dir) {
                        show_error_message_future(
                            parent_window,
                            &gettext("Directory {} cannot be created.")
                                .replace("{}", &parent_dir.display().to_string()),
                            Some(&error.message()),
                        )
                        .await;
                        return None;
                    }
                } else {
                    return None;
                }

                dest_dir = Some(Directory::new(
                    &con,
                    con.create_path(Path::new(&parent_dir)),
                ));
                dest_fn = dest_path
                    .file_name()
                    .map(|n| n.to_string_lossy().to_string());
            }
        }
    } else {
        if file_type == Some(gio::FileType::Directory) {
            // There exists a directory, copy to it
            dest_dir = Some(Directory::new(&con, con.create_path(&dest_path)));
        } else if file_type.is_some() {
            // There exists something which is not a directory, abort!
            return None;
        } else {
            // Nothing exists, ask the user if a new directory might be suitable in the path that he specified
            if ask_create_directory(parent_window, &dest_path).await {
                if let Err(error) = con.mkdir(&dest_path) {
                    show_error_message_future(parent_window, error.message(), None).await;
                    return None;
                }
            } else {
                return None;
            }
            dest_dir = Some(Directory::new(&con, con.create_path(&dest_path)));
        }
    }

    let Some(dest_dir) = dest_dir else {
        return None;
    };

    Some((dest_dir, dest_fn))
}
