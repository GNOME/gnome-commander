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

use crate::config::PIXMAPS_DIR;
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::path::PathBuf;

mod imp {
    use super::*;
    use std::cell::RefCell;

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::IconButton)]
    pub struct IconButton {
        pub button: gtk::Button,
        #[property(get, set = Self::set_path, nullable)]
        pub path: RefCell<Option<PathBuf>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for IconButton {
        const NAME: &'static str = "GnomeCmdIconButton";
        type Type = super::IconButton;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for IconButton {
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

            self.set_path(None);
        }

        fn dispose(&self) {
            self.button.unparent();
        }
    }

    impl WidgetImpl for IconButton {}

    impl IconButton {
        fn directory(&self) -> Option<PathBuf> {
            let path = self.path.borrow();
            let dir = path.as_ref().and_then(|p| p.parent())?;
            Some(dir.to_path_buf())
        }

        async fn clicked(&self) {
            let parent = self.obj().root().and_downcast::<gtk::Window>();
            let directory = self
                .directory()
                .unwrap_or_else(|| PathBuf::from(&PIXMAPS_DIR));

            let filter = gtk::FileFilter::new();
            filter.add_pixbuf_formats();

            let dialog = gtk::FileDialog::builder()
                .title(gettext("Select an Image File"))
                .modal(true)
                .initial_folder(&gio::File::for_path(directory))
                .default_filter(&filter)
                .build();

            let result = dialog.open_future(parent.as_ref()).await;

            if let Ok(icon_file) = result {
                self.obj().set_path(icon_file.path().as_deref());
            }
        }

        fn set_path(&self, path: Option<PathBuf>) {
            if let Some(ref path) = path {
                self.button.set_child(Some(&gtk::Image::from_file(path)));
            } else {
                self.button
                    .set_child(Some(&gtk::Label::new(Some(&gettext("Choose Icon")))));
            }
            *self.path.borrow_mut() = path;
        }
    }
}

glib::wrapper! {
    pub struct IconButton(ObjectSubclass<imp::IconButton>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for IconButton {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}
