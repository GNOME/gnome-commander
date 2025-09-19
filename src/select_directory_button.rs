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

use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use std::cell::RefCell;

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::DirectoryButton)]
    pub struct DirectoryButton {
        pub button: gtk::Button,
        #[property(get, set = Self::set_file, nullable)]
        pub file: RefCell<Option<gio::File>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for DirectoryButton {
        const NAME: &'static str = "GnomeCmdDirectoryButton";
        type Type = super::DirectoryButton;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for DirectoryButton {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_layout_manager(Some(gtk::BinLayout::new()));

            self.button.set_parent(&*this);

            self.button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.clicked().await });
                }
            ));

            self.set_file(None);
        }

        fn dispose(&self) {
            self.button.unparent();
        }
    }

    impl WidgetImpl for DirectoryButton {}

    impl DirectoryButton {
        async fn clicked(&self) {
            let parent = self.obj().root().and_downcast::<gtk::Window>();

            let dialog = gtk::FileDialog::builder()
                .title(gettext("Select Directory"))
                .modal(true)
                .build();

            let result = dialog.select_folder_future(parent.as_ref()).await;

            if let Ok(file) = result {
                self.set_file(Some(file));
            }
        }

        fn set_file(&self, file: Option<gio::File>) {
            if let Some(file_name) = file
                .as_ref()
                .and_then(|f| f.basename())
                .map(|f| f.to_string_lossy().to_string())
            {
                self.button.set_label(&file_name);
            } else {
                self.button.set_label(&gettext("(None)"));
            }
            *self.file.borrow_mut() = file;
        }
    }
}

glib::wrapper! {
    pub struct DirectoryButton(ObjectSubclass<imp::DirectoryButton>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for DirectoryButton {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}
