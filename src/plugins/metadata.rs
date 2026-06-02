// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use glib::prelude::*;
use std::{borrow::Cow, collections::HashMap};

#[derive(Debug, Default, Clone)]
pub struct PluginMetadata {
    pub name: Option<String>,
    pub version: Option<String>,
    pub copyright: Option<String>,
    pub comments: Option<String>,
    pub authors: Vec<String>,
    pub documenters: Vec<String>,
    pub translators: Vec<String>,
    pub webpage: Option<String>,
    pub enabled: bool,
}

type VariantType = HashMap<String, glib::Variant>;

impl ToVariant for PluginMetadata {
    fn to_variant(&self) -> glib::Variant {
        let mut map = VariantType::new();
        for (name, value) in [
            (Self::SETTING_NAME, self.name.as_ref()),
            (Self::SETTING_VERSION, self.version.as_ref()),
            (Self::SETTING_COPYRIGHT, self.copyright.as_ref()),
            (Self::SETTING_COMMENTS, self.comments.as_ref()),
            (Self::SETTING_WEBPAGE, self.webpage.as_ref()),
        ] {
            if let Some(value) = value {
                map.insert(name.to_owned(), value.to_variant());
            }
        }

        for (name, value) in [
            (Self::SETTING_AUTHORS, &self.authors),
            (Self::SETTING_DOCUMENTERS, &self.documenters),
            (Self::SETTING_TRANSLATORS, &self.translators),
        ] {
            map.insert(name.to_owned(), value.to_variant());
        }

        map.insert(Self::SETTING_ENABLED.to_owned(), self.enabled.to_variant());

        map.to_variant()
    }
}

impl FromVariant for PluginMetadata {
    fn from_variant(variant: &glib::Variant) -> Option<Self> {
        let map = VariantType::from_variant(variant)?;
        let mut result = Self::default();
        for (key, value) in map.into_iter() {
            match key.as_str() {
                Self::SETTING_NAME => result.name = String::from_variant(&value),
                Self::SETTING_VERSION => result.version = String::from_variant(&value),
                Self::SETTING_COPYRIGHT => result.copyright = String::from_variant(&value),
                Self::SETTING_COMMENTS => result.comments = String::from_variant(&value),
                Self::SETTING_AUTHORS => {
                    result.authors = Vec::<String>::from_variant(&value).unwrap_or_default()
                }
                Self::SETTING_DOCUMENTERS => {
                    result.documenters = Vec::<String>::from_variant(&value).unwrap_or_default()
                }
                Self::SETTING_TRANSLATORS => {
                    result.translators = Vec::<String>::from_variant(&value).unwrap_or_default()
                }
                Self::SETTING_WEBPAGE => result.webpage = String::from_variant(&value),
                Self::SETTING_ENABLED => {
                    result.enabled = bool::from_variant(&value).unwrap_or_default()
                }
                _ => {}
            }
        }
        Some(result)
    }
}

impl StaticVariantType for PluginMetadata {
    fn static_variant_type() -> Cow<'static, glib::VariantTy> {
        VariantType::static_variant_type()
    }
}

impl PluginMetadata {
    const SETTING_NAME: &str = "name";
    const SETTING_VERSION: &str = "version";
    const SETTING_COPYRIGHT: &str = "copyright";
    const SETTING_COMMENTS: &str = "comments";
    const SETTING_AUTHORS: &str = "authors";
    const SETTING_DOCUMENTERS: &str = "documenters";
    const SETTING_TRANSLATORS: &str = "translators";
    const SETTING_WEBPAGE: &str = "webpage";
    const SETTING_ENABLED: &str = "enabled";
}
