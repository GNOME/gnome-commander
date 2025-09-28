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
    file::File,
    utils::{ErrorMessage, NO_BUTTONS, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, prelude::*, subclass::prelude::*};
use std::path::Path;

mod imp {
    use super::*;
    use crate::utils::SenderExt;

    pub struct CreateSymlinkDialog {
        pub link_name_entry: gtk::Entry,
        cancel_button: gtk::Button,
        ok_button: gtk::Button,
        sender: async_channel::Sender<Option<glib::GString>>,
        pub receiver: async_channel::Receiver<Option<glib::GString>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for CreateSymlinkDialog {
        const NAME: &'static str = "GnomeCmdCreateSymlinkDialog";
        type Type = super::CreateSymlinkDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            let (sender, receiver) = async_channel::bounded(1);
            Self {
                link_name_entry: gtk::Entry::builder().activates_default(true).build(),
                cancel_button: gtk::Button::builder()
                    .label(gettext("_Cancel"))
                    .use_underline(true)
                    .build(),
                ok_button: gtk::Button::builder()
                    .label(gettext("_OK"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                sender,
                receiver,
            }
        }
    }

    impl ObjectImpl for CreateSymlinkDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dialog = self.obj();

            dialog.set_title(Some(&gettext("Create Symbolic Link")));
            dialog.set_resizable(false);
            dialog.set_modal(true);

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .column_spacing(12)
                .row_spacing(6)
                .build();
            dialog.set_child(Some(&grid));

            let label = gtk::Label::builder()
                .label(gettext("Symbolic link name:"))
                .mnemonic_widget(&self.link_name_entry)
                .build();
            grid.attach(&label, 0, 0, 1, 1);
            grid.attach(&self.link_name_entry, 1, 0, 1, 1);

            grid.attach(
                &dialog_button_box(NO_BUTTONS, &[&self.cancel_button, &self.ok_button]),
                0,
                1,
                2,
                1,
            );

            self.link_name_entry.connect_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.link_name_changed()
            ));

            self.cancel_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.sender.toss(None)
            ));

            self.ok_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.ok_clicked()
            ));

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, modifier| {
                    if key == gdk::Key::Escape && modifier.is_empty() {
                        imp.sender.toss(None);
                        glib::Propagation::Stop
                    } else {
                        glib::Propagation::Proceed
                    }
                }
            ));
            dialog.add_controller(key_controller);

            dialog.set_default_widget(Some(&self.ok_button));
        }
    }

    impl WidgetImpl for CreateSymlinkDialog {}
    impl WindowImpl for CreateSymlinkDialog {}

    impl CreateSymlinkDialog {
        fn link_name_changed(&self) {
            self.ok_button
                .set_sensitive(!self.link_name_entry.text().is_empty());
        }

        fn ok_clicked(&self) {
            let link_name = self.link_name_entry.text();
            if !link_name.is_empty() {
                self.sender.toss(Some(link_name));
            }
        }
    }
}

glib::wrapper! {
    pub struct CreateSymlinkDialog(ObjectSubclass<imp::CreateSymlinkDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl Default for CreateSymlinkDialog {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

pub async fn show_create_symlink_dialog(
    parent_window: &gtk::Window,
    file: &File,
    directory: &Directory,
    link_name: &str,
) {
    let dialog = CreateSymlinkDialog::default();
    dialog.set_transient_for(Some(parent_window));
    dialog.imp().link_name_entry.set_text(link_name);

    dialog.present();

    loop {
        let response = dialog.imp().receiver.recv().await;
        let Ok(Some(link_name)) = response else {
            break;
        };

        let symlink_file: gio::File = if link_name.starts_with('/') {
            let con = directory.connection();
            let path = con.create_path(&Path::new(&link_name));
            con.create_gfile(&path)
        } else {
            directory.get_child_gfile(&Path::new(&link_name))
        };
        let absolute_path = file.file().parse_name();

        match symlink_file.make_symbolic_link(absolute_path, gio::Cancellable::NONE) {
            Ok(_) => {
                if symlink_file.parent().map_or(false, |parent| {
                    parent.equal(&directory.upcast_ref::<File>().file())
                }) {
                    directory.file_created(&symlink_file.uri());
                }
                break;
            }
            Err(error) => {
                ErrorMessage::with_error(gettext("Making a symbolic link failed"), &error)
                    .show(dialog.upcast_ref())
                    .await;
            }
        }
    }
    dialog.close();
}
