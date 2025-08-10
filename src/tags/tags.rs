/*
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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

use super::file_metadata::FileMetadata;
use crate::{
    file::File,
    libgcmd::file_metadata_extractor::{FileMetadataExtractor, FileMetadataExtractorExt},
    plugin_manager::PluginManager,
};
use gtk::{
    gio,
    glib::{self, prelude::*, subclass::prelude::*},
};
use indexmap::IndexMap;
use std::borrow::Cow;

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct GnomeCmdTagClass(pub String);

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct GnomeCmdTag(pub Cow<'static, str>);

impl GnomeCmdTag {
    pub fn id(&self) -> &str {
        &self.0
    }

    pub fn class(&self) -> Option<GnomeCmdTagClass> {
        Some(GnomeCmdTagClass(self.0.split_once('.')?.0.to_owned()))
    }
}

mod imp {
    use super::*;
    use crate::{
        libgcmd::file_metadata_extractor::FileMetadataExtractor,
        tags::{basic::BasicMetadataExtractor, image::ImageMetadataExtractor},
    };
    use std::cell::{OnceCell, RefCell};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::FileMetadataService)]
    pub struct FileMetadataService {
        #[property(get, construct_only)]
        pub plugin_manager: OnceCell<PluginManager>,
        pub extractors: RefCell<Vec<FileMetadataExtractor>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileMetadataService {
        const NAME: &'static str = "GnomeCmdFileMetadataService";
        type Type = super::FileMetadataService;
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileMetadataService {
        fn constructed(&self) {
            self.parent_constructed();

            self.obj()
                .plugin_manager()
                .connect_plugins_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.plugins_changed()
                ));

            self.plugins_changed();
        }

        fn dispose(&self) {
            self.extractors.borrow_mut().clear();
        }
    }

    impl FileMetadataService {
        fn plugins_changed(&self) {
            let mut extractors = vec![
                BasicMetadataExtractor::default().upcast(),
                ImageMetadataExtractor::default().upcast(),
            ];

            extractors.extend(
                self.obj()
                    .plugin_manager()
                    .active_plugins()
                    .into_iter()
                    .filter_map(|(_, plugin)| plugin.downcast::<FileMetadataExtractor>().ok()),
            );

            self.extractors.replace(extractors);
        }
    }
}

glib::wrapper! {
    pub struct FileMetadataService(ObjectSubclass<imp::FileMetadataService>);
}

impl FileMetadataService {
    pub fn new(plugin_manager: &PluginManager) -> Self {
        glib::Object::builder()
            .property("plugin-manager", plugin_manager)
            .build()
    }

    pub fn supported_tags_map(
        &self,
    ) -> IndexMap<GnomeCmdTagClass, IndexMap<GnomeCmdTag, Vec<FileMetadataExtractor>>> {
        let mut result: IndexMap<
            GnomeCmdTagClass,
            IndexMap<GnomeCmdTag, Vec<FileMetadataExtractor>>,
        > = Default::default();
        for extractor in self.imp().extractors.borrow().iter() {
            for tag in extractor.supported_tags() {
                if let Some(class) = tag.class() {
                    result
                        .entry(class)
                        .or_default()
                        .entry(tag)
                        .or_default()
                        .push(extractor.clone());
                } else {
                    eprintln!("Invalid tag \"{}\".", tag.0);
                }
            }
        }
        result
    }

    pub fn extract_metadata(&self, file: &File) -> FileMetadata {
        let mut metadata = FileMetadata::default();
        for extractor in self.imp().extractors.borrow().iter() {
            extractor.extract_metadata(file, |tag, value| metadata.add(tag, value.as_deref()));
        }
        metadata
    }

    pub fn class_name(&self, class: &GnomeCmdTagClass) -> String {
        for extractor in self.imp().extractors.borrow().iter() {
            if let Some(name) = extractor.class_name(class) {
                return name;
            }
        }
        class.0.clone()
    }

    pub fn tag_name(&self, tag: &GnomeCmdTag) -> Option<String> {
        for extractor in self.imp().extractors.borrow().iter() {
            if let name @ Some(_) = extractor.tag_name(tag) {
                return name;
            }
        }
        None
    }

    pub fn tag_description(&self, tag: &GnomeCmdTag) -> Option<String> {
        for extractor in self.imp().extractors.borrow().iter() {
            if let description @ Some(_) = extractor.tag_description(tag) {
                return description;
            }
        }
        None
    }

    pub fn file_summary(&self, metadata: &FileMetadata) -> Vec<(String, String)> {
        let mut summary = Vec::new();
        for extractor in self.imp().extractors.borrow().iter() {
            for tag in extractor.summary_tags() {
                if let Some(value) = metadata.get_first(&tag) {
                    summary.push((
                        extractor
                            .tag_name(&tag)
                            .unwrap_or_else(|| tag.id().to_owned()),
                        value,
                    ));
                }
            }
        }
        summary
    }

    pub fn create_menu(&self, action_name: &str) -> gio::Menu {
        let menu = gio::Menu::new();
        for (class, tags) in self.supported_tags_map() {
            let submenu = gio::Menu::new();
            for (tag, extractors) in tags {
                let title = extractors.iter().find_map(|e| e.tag_name(&tag));
                let item = gio::MenuItem::new(title.as_deref(), None);
                item.set_action_and_target_value(
                    Some(action_name),
                    Some(&format!("$T({})", tag.0).to_variant()),
                );
                submenu.append_item(&item);
            }
            menu.append_submenu(Some(&self.class_name(&class)), &submenu);
        }
        menu
    }

    pub fn to_tsv(&self, fm: &FileMetadata) -> String {
        let mut tsv = String::new();
        for (tag, value) in fm.dump() {
            tsv.push_str(tag.id());
            tsv.push('\t');
            tsv.push_str(self.tag_name(&tag).as_deref().unwrap_or_default());
            tsv.push('\t');
            tsv.push_str(&value);
            tsv.push('\n');
        }
        tsv
    }
}
