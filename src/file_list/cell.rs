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

use super::item::FileListItem;
use gtk::{glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        file::File,
        layout::ls_colors::{LsPalletteColor, ls_colors_get},
    };
    use std::cell::{Cell, RefCell};
    use strum::VariantArray;

    #[derive(glib::Properties, Default)]
    #[properties(wrapper_type = super::FileListCell)]
    pub struct FileListCell {
        #[property(get, set)]
        text: RefCell<String>,

        #[property(get, set = Self::set_item, nullable)]
        item: RefCell<Option<FileListItem>>,
        #[property(get, set)]
        xalign: Cell<f32>,
        #[property(get, set = Self::set_selected)]
        selected: Cell<bool>,
        #[property(get, set = Self::set_use_ls_colors)]
        use_ls_colors: Cell<bool>,

        pub label: gtk::Label,
        pub bindings: RefCell<Vec<glib::Binding>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileListCell {
        const NAME: &'static str = "GnomeCmdFileListCell";
        type Type = super::FileListCell;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileListCell {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_layout_manager(Some(gtk::BinLayout::new()));

            this.set_hexpand(true);
            this.set_vexpand(true);
            this.add_css_class("cell");

            self.label.set_parent(&*this);
            self.label.set_hexpand(true);
            self.label.set_vexpand(true);

            this.bind_property("text", &self.label, "label")
                .sync_create()
                .build();
            this.bind_property("xalign", &self.label, "xalign")
                .sync_create()
                .build();
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for FileListCell {}

    impl FileListCell {
        fn set_item(&self, item: Option<FileListItem>) {
            for binding in self.bindings.borrow_mut().drain(..) {
                binding.unbind();
            }
            *self.item.borrow_mut() = item.clone();
            if let Some(ref item) = item {
                self.bindings.borrow_mut().push(
                    item.bind_property("selected", self.obj().as_ref(), "selected")
                        .sync_create()
                        .build(),
                );
            }
        }

        fn set_selected(&self, value: bool) {
            self.selected.set(value);
            self.apply_css();
        }

        fn set_use_ls_colors(&self, value: bool) {
            self.use_ls_colors.set(value);
            self.apply_css();
        }

        fn file(&self) -> Option<File> {
            Some(self.obj().item()?.file())
        }

        fn apply_css(&self) {
            for c in LsPalletteColor::VARIANTS {
                self.obj().remove_css_class(&format!("fg-{}", c.as_ref()));
                self.obj().remove_css_class(&format!("bg-{}", c.as_ref()));
            }
            if self.selected.get() {
                self.obj().add_css_class("sel");
            } else {
                self.obj().remove_css_class("sel");
                if self.use_ls_colors.get() {
                    if let Some(colors) = self.file().and_then(|f| ls_colors_get(&f.file_info())) {
                        if let Some(class) = colors.fg.map(|fg| format!("fg-{}", fg.as_ref())) {
                            self.obj().add_css_class(&class);
                        }
                        if let Some(class) = colors.bg.map(|fg| format!("bg-{}", fg.as_ref())) {
                            self.obj().add_css_class(&class);
                        }
                    }
                }
            }
        }
    }
}

glib::wrapper! {
    pub struct FileListCell(ObjectSubclass<imp::FileListCell>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for FileListCell {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl FileListCell {
    pub fn bind(&self, item: &FileListItem) {
        self.set_item(Some(item));
    }

    pub fn unbind(&self) {
        self.set_item(None::<FileListItem>);
    }

    pub fn add_binding(&self, binding: glib::Binding) {
        self.imp().bindings.borrow_mut().push(binding);
    }
}
