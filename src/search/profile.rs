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
        #[property(get, set, default_value=-1)]
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
        glib::Object::builder().build()
    }
}

impl SearchProfile {
    pub fn reset(&self) {
        self.set_name("");
        self.set_filename_pattern("");
        self.set_syntax(0);
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
        match self.syntax() {
            0 => PatternType::Regex,
            _ => PatternType::FnMatch,
        }
    }

    pub fn set_pattern_type(&self, pattern_type: PatternType) {
        self.set_syntax(match pattern_type {
            PatternType::Regex => 0,
            PatternType::FnMatch => 1,
        });
    }

    pub fn save(&self) -> SearchProfileVariant {
        SearchProfileVariant {
            name: self.name(),
            max_depth: self.max_depth(),
            syntax: self.syntax(),
            filename_pattern: self.filename_pattern(),
            content_search: self.content_search(),
            match_case: self.match_case(),
            text_pattern: self.text_pattern(),
        }
    }

    pub fn load(&self, variant: SearchProfileVariant) {
        self.set_name(variant.name);
        self.set_max_depth(variant.max_depth);
        self.set_syntax(variant.syntax);
        self.set_filename_pattern(variant.filename_pattern);
        self.set_content_search(variant.content_search);
        self.set_match_case(variant.match_case);
        self.set_text_pattern(variant.text_pattern);
    }
}

#[derive(glib::Variant)]
pub struct SearchProfileVariant {
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
        assert_eq!(&*SearchProfileVariant::static_variant_type(), "(siisbbs)");
    }
}
