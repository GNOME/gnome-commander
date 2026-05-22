// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    basic::BasicMetadataExtractor, file_metadata::FileMetadata, image::ImageMetadataExtractor,
    plugin::PluginMetadataExtractor,
};
use crate::{file::File, plugins::InactivePluginHostChannel};
use futures::future::join3;
use gtk::{gio, glib::prelude::*};
use std::{
    borrow::Cow,
    cmp::{Eq, Ord, PartialEq, PartialOrd},
    collections::BTreeMap,
};

pub trait Tag: std::fmt::Debug {
    fn id(&self) -> &str;
    fn class(&self) -> Cow<'_, str>;
    fn name(&self) -> Cow<'_, str>;
    fn description(&self) -> Cow<'_, str>;
    fn clone(&self) -> Box<dyn Tag>;
}

impl PartialEq for Box<dyn Tag> {
    fn eq(&self, other: &Self) -> bool {
        self.id() == other.id()
    }
}

impl Eq for Box<dyn Tag> {}

impl PartialOrd for Box<dyn Tag> {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Box<dyn Tag> {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.id().cmp(other.id())
    }
}

pub trait FileMetadataExtractor {
    async fn supported_tags(&self) -> Vec<(String, Vec<Box<dyn Tag>>)>;
    fn summary_tags(&self) -> Vec<Box<dyn Tag>>;
    async fn extract_metadata(&self, file: &File) -> Vec<(Box<dyn Tag>, String)>;
}

pub struct FileMetadataService {
    basic_extractor: BasicMetadataExtractor,
    image_extractor: ImageMetadataExtractor,
    plugin_extractor: PluginMetadataExtractor,
}

impl FileMetadataService {
    pub fn new(plugin_channel: InactivePluginHostChannel) -> Self {
        Self {
            basic_extractor: Default::default(),
            image_extractor: Default::default(),
            plugin_extractor: PluginMetadataExtractor::new(plugin_channel),
        }
    }

    pub async fn extract_metadata(&self, file: &File) -> FileMetadata {
        let mut metadata = FileMetadata::default();
        let (basic_tags, image_tags, plugin_tags) = join3(
            self.basic_extractor.extract_metadata(file),
            self.image_extractor.extract_metadata(file),
            self.plugin_extractor.extract_metadata(file),
        )
        .await;
        for tags in [basic_tags, image_tags, plugin_tags] {
            for (tag, value) in tags {
                metadata.add(tag, value)
            }
        }
        metadata
    }

    pub fn file_summary(&self, metadata: &FileMetadata) -> Vec<(String, String)> {
        let mut summary = Vec::new();
        for tags in [
            self.basic_extractor.summary_tags(),
            self.image_extractor.summary_tags(),
            self.plugin_extractor.summary_tags(),
        ] {
            for tag in tags {
                if let Some(value) = metadata.get_first(tag.id()) {
                    summary.push((tag.name().to_string(), value));
                }
            }
        }
        summary
    }

    pub async fn create_menu(&self, action_name: &str) -> gio::Menu {
        let menu = gio::Menu::new();
        let mut classes = BTreeMap::new();

        for supported in [
            self.basic_extractor.supported_tags().await,
            self.image_extractor.supported_tags().await,
            self.plugin_extractor.supported_tags().await,
        ] {
            for (class, tags) in supported {
                let submenu = classes.entry(class.clone()).or_insert_with(|| {
                    let submenu = gio::Menu::new();
                    menu.append_submenu(Some(&class), &submenu);
                    submenu
                });

                // TODO: Large lists cannot be handled, cut off at 256 items
                for tag in &tags[..std::cmp::min(tags.len(), 256)] {
                    let item = gio::MenuItem::new(Some(&tag.name()), None);
                    item.set_action_and_target_value(
                        Some(action_name),
                        Some(&format!("$T({})", tag.id()).to_variant()),
                    );
                    submenu.append_item(&item);
                }
            }
        }
        menu
    }
}
