// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::file::{File, FileOps};
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
        let file = self.file();
        let is_dotdot = file.is_dotdot();

        self.set_name(file.name());

        if file.is_regular() {
            let name = file.path_name();
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
            self.set_stem(file.name());
            self.set_extension("");
        }

        self.set_directory(if let Some(ref base) = self.base_dir() {
            base.relative_path(&*self.file().file())
                .map(|p| format!(".{}", p.display()))
                .unwrap_or_default()
        } else {
            let path = self.file().path_from_root();
            path.parent()
                .map(|p| p.display().to_string())
                .unwrap_or_default()
        });

        self.set_size(
            file.size()
                .and_then(|size| i64::try_from(size).ok())
                .unwrap_or(-1),
        );

        self.set_modification_time(file.modification_date());

        self.set_permissions(file.permissions());

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
