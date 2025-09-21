/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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

use crate::file::File;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use std::cell::{Cell, OnceCell, RefCell};

    #[derive(glib::Properties, Default)]
    #[properties(wrapper_type = super::FileListItem)]
    pub struct FileListItem {
        #[property(get, construct_only)]
        file: OnceCell<File>,
        #[property(get, construct_only, nullable)]
        base_dir: OnceCell<Option<gio::File>>,

        #[property(get, set)]
        name: RefCell<String>,
        #[property(get, set)]
        stem: RefCell<String>,
        #[property(get, set)]
        extension: RefCell<String>,
        #[property(get, set)]
        directory: RefCell<String>,
        #[property(get, set)]
        size: Cell<i64>,
        #[property(get, set, nullable)]
        modification_time: RefCell<Option<glib::DateTime>>,
        #[property(get, set)]
        permissions: Cell<u32>,
        #[property(get, set, nullable)]
        owner: RefCell<Option<String>>,
        #[property(get, set, nullable)]
        group: RefCell<Option<String>>,

        #[property(get, set)]
        selected: Cell<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileListItem {
        const NAME: &'static str = "GnomeCmdFileListItem";
        type Type = super::FileListItem;
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileListItem {
        fn constructed(&self) {
            self.parent_constructed();
            self.obj().update();
        }
    }
}

glib::wrapper! {
    pub struct FileListItem(ObjectSubclass<imp::FileListItem>);
}

impl FileListItem {
    pub fn new(file: &File) -> Self {
        glib::Object::builder().property("file", file).build()
    }

    pub fn copy(&self) -> Self {
        glib::Object::builder()
            .property("file", self.file())
            .property("size", self.size())
            .property("selected", self.selected())
            .build()
    }

    pub fn update(&self) {
        let is_dotdot = self.file().is_dotdot();
        let file_info = self.file().file_info();

        self.set_name(file_info.display_name());

        if file_info.file_type() == gio::FileType::Regular {
            let name = file_info.name();
            self.set_stem({
                name.file_stem()
                    .map(|n| n.to_string_lossy().to_string())
                    .unwrap_or_default()
            });
            self.set_extension(
                name.extension()
                    .map(|n| n.to_string_lossy().to_string())
                    .unwrap_or_default(),
            );
        } else {
            self.set_stem({
                file_info
                    .name()
                    .file_name()
                    .map(|n| n.to_string_lossy().to_string())
                    .unwrap_or_default()
            });
            self.set_extension("");
        }

        self.set_directory(if let Some(ref base) = self.base_dir() {
            base.relative_path(&self.file().file())
                .map(|p| format!(".{}", p.display()))
                .unwrap_or_default()
        } else {
            let path = self.file().get_path_string_through_parent();
            path.parent()
                .map(|p| p.display().to_string())
                .unwrap_or_default()
        });

        if file_info.file_type() == gio::FileType::Directory {
            self.set_size(-1);
        } else {
            self.set_size(file_info.size());
        }

        self.set_modification_time(if is_dotdot {
            None
        } else {
            file_info.modification_date_time()
        });

        self.set_permissions(if is_dotdot {
            u32::MAX
        } else {
            self.file().permissions()
        });

        self.set_owner(if is_dotdot {
            None
        } else {
            Some(self.file().owner())
        });

        self.set_group(if is_dotdot {
            None
        } else {
            Some(self.file().group())
        });
    }

    pub fn toggle_selected(&self) {
        self.set_selected(!self.selected());
    }
}
