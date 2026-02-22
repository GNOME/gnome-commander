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

use crate::filter::PatternType;
use gtk::{glib, prelude::*, subclass::prelude::*};
use std::collections::HashMap;

mod imp {
    use super::*;
    use std::cell::{Cell, RefCell};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::SearchProfile)]
    pub struct SearchProfile {
        #[property(get, set)]
        pub name: RefCell<String>,
        #[property(get, set)]
        pub filename_pattern: RefCell<String>,
        #[property(get, set)]
        pub syntax: Cell<i32>,
        #[property(get, set)]
        pub max_depth: Cell<i32>,
        #[property(get, set)]
        pub text_pattern: RefCell<String>,
        #[property(get, set)]
        pub content_search: Cell<bool>,
        #[property(get, set)]
        pub match_case: Cell<bool>,
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
        self.set_filename_pattern("");
        self.set_syntax(i32::from(PatternType::default()));
        self.set_max_depth(-1);
        self.set_text_pattern("");
        self.set_content_search(false);
        self.set_match_case(false);
    }

    pub fn copy_from(&self, other: &Self) {
        self.set_name(other.name());
        self.set_filename_pattern(other.filename_pattern());
        self.set_syntax(other.syntax());
        self.set_max_depth(other.max_depth());
        self.set_text_pattern(other.text_pattern());
        self.set_content_search(other.content_search());
        self.set_match_case(other.match_case());
    }

    pub fn deep_clone(&self) -> Self {
        let clone = Self::default();
        clone.copy_from(self);
        clone
    }

    pub fn pattern_type(&self) -> PatternType {
        self.syntax().try_into().unwrap_or_default()
    }

    pub fn set_pattern_type(&self, pattern_type: PatternType) {
        self.set_syntax(i32::from(pattern_type));
    }

    const SETTING_NAME: &str = "name";
    const SETTING_MAX_DEPTH: &str = "max-depth";
    const SETTING_PATH_SYNTAX: &str = "path-syntax";
    const SETTING_PATH_PATTERN: &str = "path-pattern";
    const SETTING_CONTENT_SEARCH: &str = "content-search";
    const SETTING_CONTENT_MATCH_CASE: &str = "content-match-case";
    const SETTING_CONTENT_PATTERN: &str = "content-pattern";

    pub fn save(&self) -> SearchProfileVariant {
        let mut v = SearchProfileVariant::new();
        v.insert(Self::SETTING_NAME.to_string(), self.name().into());
        v.insert(Self::SETTING_MAX_DEPTH.to_string(), self.max_depth().into());
        v.insert(Self::SETTING_PATH_SYNTAX.to_string(), self.syntax().into());
        v.insert(
            Self::SETTING_PATH_PATTERN.to_string(),
            self.filename_pattern().into(),
        );
        v.insert(
            Self::SETTING_CONTENT_SEARCH.to_string(),
            self.content_search().into(),
        );
        v.insert(
            Self::SETTING_CONTENT_MATCH_CASE.to_string(),
            self.match_case().into(),
        );
        v.insert(
            Self::SETTING_CONTENT_PATTERN.to_string(),
            self.text_pattern().into(),
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
            .and_then(i32::from_variant)
        {
            self.set_syntax(path_syntax);
        }
        if let Some(path_pattern) = variant
            .get(Self::SETTING_PATH_PATTERN)
            .and_then(String::from_variant)
        {
            self.set_filename_pattern(path_pattern);
        }
        if let Some(content_search) = variant
            .get(Self::SETTING_CONTENT_SEARCH)
            .and_then(bool::from_variant)
        {
            self.set_content_search(content_search);
        }
        if let Some(content_match_case) = variant
            .get(Self::SETTING_CONTENT_MATCH_CASE)
            .and_then(bool::from_variant)
        {
            self.set_match_case(content_match_case);
        }
        if let Some(content_pattern) = variant
            .get(Self::SETTING_CONTENT_PATTERN)
            .and_then(String::from_variant)
        {
            self.set_text_pattern(content_pattern);
        }
    }

    pub fn load_legacy(&self, variant: LegacySearchProfileVariant) {
        self.set_name(variant.name);
        self.set_max_depth(variant.max_depth);
        self.set_syntax(variant.syntax);
        self.set_filename_pattern(variant.filename_pattern);
        self.set_content_search(variant.content_search);
        self.set_match_case(variant.match_case);
        self.set_text_pattern(variant.text_pattern);
    }
}

pub type SearchProfileVariant = HashMap<String, glib::Variant>;

#[derive(glib::Variant)]
pub struct LegacySearchProfileVariant {
    pub name: String,
    pub max_depth: i32,
    pub syntax: i32,
    pub filename_pattern: String,
    pub content_search: bool,
    pub match_case: bool,
    pub text_pattern: String,
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
        profile.set_syntax(1);
        profile.set_filename_pattern("path");
        profile.set_content_search(true);
        profile.set_match_case(true);
        profile.set_text_pattern("text");

        let variant = profile.save();

        let profile = SearchProfile::default();
        profile.load(variant);

        assert_eq!(profile.name(), "name");
        assert_eq!(profile.max_depth(), 3);
        assert_eq!(profile.syntax(), 1);
        assert_eq!(profile.filename_pattern(), "path");
        assert_eq!(profile.content_search(), true);
        assert_eq!(profile.match_case(), true);
        assert_eq!(profile.text_pattern(), "text");
    }
}
