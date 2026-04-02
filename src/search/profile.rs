// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::filter::PatternType;
use gtk::{glib, prelude::*, subclass::prelude::*};
use std::collections::BTreeMap;

mod imp {
    use super::*;
    use std::cell::{Cell, RefCell};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::SearchProfile)]
    pub struct SearchProfile {
        #[property(get, set)]
        pub name: RefCell<String>,
        #[property(get, set)]
        pub path_pattern: RefCell<String>,
        #[property(get, set)]
        pub path_syntax: Cell<PatternType>,
        #[property(get, set)]
        pub path_match_case: Cell<bool>,
        #[property(get, set)]
        pub max_depth: Cell<i32>,
        #[property(get, set)]
        pub content_pattern: RefCell<String>,
        #[property(get, set)]
        pub content_match_case: Cell<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for SearchProfile {
        const NAME: &'static str = "GnomeCmdSearchProfile";
        type Type = super::SearchProfile;
    }

    #[glib::derived_properties]
    impl ObjectImpl for SearchProfile {}
}

glib::wrapper! {
    pub struct SearchProfile(ObjectSubclass<imp::SearchProfile>);
}

impl Default for SearchProfile {
    fn default() -> Self {
        // Builder seems to ignore default property values, reset before returning.
        let profile: Self = glib::Object::builder().build();
        profile.reset();
        profile
    }
}

impl SearchProfile {
    pub fn reset(&self) {
        self.set_name("");
        self.set_path_pattern("");
        self.set_path_syntax(PatternType::default());
        self.set_path_match_case(false);
        self.set_max_depth(-1);
        self.set_content_pattern("");
        self.set_content_match_case(false);
    }

    pub fn copy_from(&self, other: &Self) {
        self.set_name(other.name());
        self.set_path_pattern(other.path_pattern());
        self.set_path_syntax(other.path_syntax());
        self.set_path_match_case(other.path_match_case());
        self.set_max_depth(other.max_depth());
        self.set_content_pattern(other.content_pattern());
        self.set_content_match_case(other.content_match_case());
    }

    pub fn deep_clone(&self) -> Self {
        let clone = Self::default();
        clone.copy_from(self);
        clone
    }

    const SETTING_NAME: &str = "name";
    const SETTING_MAX_DEPTH: &str = "max-depth";
    const SETTING_PATH_SYNTAX: &str = "path-syntax";
    const SETTING_PATH_MATCH_CASE: &str = "path-match-case";
    const SETTING_PATH_PATTERN: &str = "path-pattern";
    const SETTING_CONTENT_MATCH_CASE: &str = "content-match-case";
    const SETTING_CONTENT_PATTERN: &str = "content-pattern";

    pub fn save(&self) -> SearchProfileVariant {
        let mut v = SearchProfileVariant::new();
        v.insert(Self::SETTING_NAME.to_string(), self.name().into());
        v.insert(Self::SETTING_MAX_DEPTH.to_string(), self.max_depth().into());
        v.insert(
            Self::SETTING_PATH_SYNTAX.to_string(),
            u32::from(self.path_syntax()).into(),
        );
        v.insert(
            Self::SETTING_PATH_MATCH_CASE.to_string(),
            self.path_match_case().into(),
        );
        v.insert(
            Self::SETTING_PATH_PATTERN.to_string(),
            self.path_pattern().into(),
        );
        v.insert(
            Self::SETTING_CONTENT_MATCH_CASE.to_string(),
            self.content_match_case().into(),
        );
        v.insert(
            Self::SETTING_CONTENT_PATTERN.to_string(),
            self.content_pattern().into(),
        );
        v
    }

    pub fn load(&self, variant: SearchProfileVariant) {
        if let Some(name) = variant
            .get(Self::SETTING_NAME)
            .and_then(String::from_variant)
        {
            self.set_name(name);
        }
        if let Some(max_depth) = variant
            .get(Self::SETTING_MAX_DEPTH)
            .and_then(i32::from_variant)
        {
            self.set_max_depth(max_depth);
        }
        if let Some(path_syntax) = variant
            .get(Self::SETTING_PATH_SYNTAX)
            .and_then(u32::from_variant)
        {
            self.set_path_syntax(PatternType::from(path_syntax));
        }
        if let Some(path_match_case) = variant
            .get(Self::SETTING_PATH_MATCH_CASE)
            .and_then(bool::from_variant)
        {
            self.set_path_match_case(path_match_case);
        }
        if let Some(path_pattern) = variant
            .get(Self::SETTING_PATH_PATTERN)
            .and_then(String::from_variant)
        {
            self.set_path_pattern(path_pattern);
        }
        if let Some(content_match_case) = variant
            .get(Self::SETTING_CONTENT_MATCH_CASE)
            .and_then(bool::from_variant)
        {
            self.set_content_match_case(content_match_case);
        }
        if let Some(content_pattern) = variant
            .get(Self::SETTING_CONTENT_PATTERN)
            .and_then(String::from_variant)
        {
            self.set_content_pattern(content_pattern);
        }
    }

    pub fn load_legacy(&self, variant: LegacySearchProfileVariant) {
        self.set_name(variant.name);
        self.set_max_depth(variant.max_depth);
        self.set_path_syntax(PatternType::from(variant.path_syntax as u32));
        self.set_path_pattern(variant.path_pattern);
        self.set_content_match_case(variant.content_match_case);
        self.set_content_pattern(variant.content_pattern);
    }
}

pub type SearchProfileVariant = BTreeMap<String, glib::Variant>;

#[derive(glib::Variant)]
pub struct LegacySearchProfileVariant {
    pub name: String,
    pub max_depth: i32,
    pub path_syntax: i32,
    pub path_pattern: String,
    pub content_search: bool,
    pub content_match_case: bool,
    pub content_pattern: String,
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_type() {
        assert_eq!(&*SearchProfileVariant::static_variant_type(), "a{sv}");
        assert_eq!(
            &*LegacySearchProfileVariant::static_variant_type(),
            "(siisbbs)"
        );
    }

    #[test]
    fn test_variant_roundtrip() {
        let profile = SearchProfile::default();
        profile.set_name("name");
        profile.set_max_depth(3);
        profile.set_path_syntax(PatternType::Regex);
        profile.set_path_match_case(true);
        profile.set_path_pattern("path");
        profile.set_content_match_case(true);
        profile.set_content_pattern("text");

        let variant = profile.save();

        let profile = SearchProfile::default();
        profile.load(variant);

        assert_eq!(profile.name(), "name");
        assert_eq!(profile.max_depth(), 3);
        assert_eq!(profile.path_syntax(), PatternType::Regex);
        assert_eq!(profile.path_match_case(), true);
        assert_eq!(profile.path_pattern(), "path");
        assert_eq!(profile.content_match_case(), true);
        assert_eq!(profile.content_pattern(), "text");
    }
}
