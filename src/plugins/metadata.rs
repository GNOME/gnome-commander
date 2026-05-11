// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use glib::prelude::*;
use std::{borrow::Cow, collections::BTreeMap};

#[derive(Debug, Default, Clone)]
pub struct PluginMetadata(BTreeMap<String, glib::Variant>);

impl ToVariant for PluginMetadata {
    fn to_variant(&self) -> glib::Variant {
        self.0.to_variant()
    }
}

impl FromVariant for PluginMetadata {
    fn from_variant(variant: &glib::Variant) -> Option<Self> {
        BTreeMap::from_variant(variant).map(Self)
    }
}

impl StaticVariantType for PluginMetadata {
    fn static_variant_type() -> Cow<'static, glib::VariantTy> {
        BTreeMap::<String, glib::Variant>::static_variant_type()
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

    pub fn name(&self) -> Option<String> {
        self.0
            .get(Self::SETTING_NAME)
            .and_then(String::from_variant)
    }

    pub fn set_name(&mut self, value: Option<&str>) {
        if let Some(value) = value {
            self.0.insert(Self::SETTING_NAME.to_string(), value.into());
        } else {
            self.0.remove(Self::SETTING_NAME);
        }
    }

    pub fn version(&self) -> Option<String> {
        self.0
            .get(Self::SETTING_VERSION)
            .and_then(String::from_variant)
    }

    pub fn set_version(&mut self, value: Option<&str>) {
        if let Some(value) = value {
            self.0
                .insert(Self::SETTING_VERSION.to_string(), value.into());
        } else {
            self.0.remove(Self::SETTING_VERSION);
        }
    }

    pub fn copyright(&self) -> Option<String> {
        self.0
            .get(Self::SETTING_COPYRIGHT)
            .and_then(String::from_variant)
    }

    pub fn set_copyright(&mut self, value: Option<&str>) {
        if let Some(value) = value {
            self.0
                .insert(Self::SETTING_COPYRIGHT.to_string(), value.into());
        } else {
            self.0.remove(Self::SETTING_COPYRIGHT);
        }
    }

    pub fn comments(&self) -> Option<String> {
        self.0
            .get(Self::SETTING_COMMENTS)
            .and_then(String::from_variant)
    }

    pub fn set_comments(&mut self, value: Option<&str>) {
        if let Some(value) = value {
            self.0
                .insert(Self::SETTING_COMMENTS.to_string(), value.into());
        } else {
            self.0.remove(Self::SETTING_COMMENTS);
        }
    }

    pub fn authors(&self) -> Vec<String> {
        self.0
            .get(Self::SETTING_AUTHORS)
            .and_then(Vec::<String>::from_variant)
            .unwrap_or_default()
    }

    pub fn set_authors(&mut self, value: &[String]) {
        self.0
            .insert(Self::SETTING_AUTHORS.to_string(), value.into());
    }

    pub fn documenters(&self) -> Vec<String> {
        self.0
            .get(Self::SETTING_DOCUMENTERS)
            .and_then(Vec::<String>::from_variant)
            .unwrap_or_default()
    }

    pub fn set_documenters(&mut self, value: &[String]) {
        self.0
            .insert(Self::SETTING_DOCUMENTERS.to_string(), value.into());
    }

    pub fn translators(&self) -> Vec<String> {
        self.0
            .get(Self::SETTING_TRANSLATORS)
            .and_then(Vec::<String>::from_variant)
            .unwrap_or_default()
    }

    pub fn set_translators(&mut self, value: &[String]) {
        self.0
            .insert(Self::SETTING_TRANSLATORS.to_string(), value.into());
    }

    pub fn webpage(&self) -> Option<String> {
        self.0
            .get(Self::SETTING_WEBPAGE)
            .and_then(String::from_variant)
    }

    pub fn set_webpage(&mut self, value: Option<&str>) {
        if let Some(value) = value {
            self.0
                .insert(Self::SETTING_WEBPAGE.to_string(), value.into());
        } else {
            self.0.remove(Self::SETTING_WEBPAGE);
        }
    }

    pub fn enabled(&self) -> bool {
        self.0
            .get(Self::SETTING_ENABLED)
            .and_then(bool::from_variant)
            .unwrap_or_default()
    }

    pub fn set_enabled(&mut self, value: bool) {
        self.0
            .insert(Self::SETTING_ENABLED.to_string(), value.into());
    }
}
