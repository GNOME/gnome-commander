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

use super::{
    convert::{initial_caps, sentence_case, toggle_case},
    regex_dialog::RegexReplace,
};
use gettextrs::gettext;
use gtk::{gio, glib::subclass::prelude::*, prelude::*};
use std::fmt;

#[derive(
    Clone,
    Copy,
    PartialEq,
    Eq,
    Default,
    Debug,
    strum::VariantArray,
    strum::FromRepr,
    glib::Enum,
    glib::Variant,
)]
#[enum_type(name = "GnomeCmdCaseConversion")]
#[variant_enum(repr)]
#[repr(u32)]
pub enum CaseConversion {
    #[default]
    Unchanged,
    LowerCase,
    UpperCase,
    SentenceCase,
    InitialCaps,
    ToggleCase,
}

impl CaseConversion {
    pub fn apply(self, string: &str) -> String {
        match self {
            Self::Unchanged => string.to_owned(),
            Self::LowerCase => string.to_lowercase(),
            Self::UpperCase => string.to_uppercase(),
            Self::SentenceCase => sentence_case(&string),
            Self::InitialCaps => initial_caps(&string),
            Self::ToggleCase => toggle_case(&string),
        }
    }
}

impl fmt::Display for CaseConversion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let text = match self {
            Self::Unchanged => gettext("<unchanged>"),
            Self::LowerCase => gettext("lowercase"),
            Self::UpperCase => gettext("UPPERCASE"),
            Self::SentenceCase => gettext("Sentence case"),
            Self::InitialCaps => gettext("Initial Caps"),
            Self::ToggleCase => gettext("tOGGLE cASE"),
        };
        write!(f, "{}", text)
    }
}

#[derive(
    Clone,
    Copy,
    PartialEq,
    Eq,
    Default,
    Debug,
    strum::VariantArray,
    strum::FromRepr,
    glib::Enum,
    glib::Variant,
)]
#[enum_type(name = "GnomeCmdTrimBlanks")]
#[variant_enum(repr)]
#[repr(u32)]
pub enum TrimBlanks {
    #[default]
    None,
    Leading,
    Trailing,
    LeadingAndTrailing,
}

impl TrimBlanks {
    pub fn apply(self, string: &str) -> &str {
        match self {
            Self::None => string,
            Self::Leading => string.trim_start(),
            Self::Trailing => string.trim_end(),
            Self::LeadingAndTrailing => string.trim(),
        }
    }
}

impl fmt::Display for TrimBlanks {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let text = match self {
            Self::None => gettext("<none>"),
            Self::Leading => gettext("leading"),
            Self::Trailing => gettext("trailing"),
            Self::LeadingAndTrailing => gettext("leading and trailing"),
        };
        write!(f, "{}", text)
    }
}

mod imp {
    use super::*;
    use std::cell::{Cell, RefCell};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::AdvancedRenameProfile)]
    pub struct AdvancedRenameProfile {
        #[property(get, set)]
        name: RefCell<String>,

        #[property(get, set)]
        template_string: RefCell<String>,

        #[property(get, set)]
        counter_start: Cell<u32>,

        #[property(get, set)]
        counter_width: Cell<u32>,

        #[property(get, set)]
        counter_step: Cell<i32>,

        // #[property(get, set)]
        pub regexes: RefCell<Vec<RegexReplace>>,

        #[property(get, set, builder(CaseConversion::default()))]
        case_conversion: Cell<CaseConversion>,

        #[property(get, set, builder(TrimBlanks::default()))]
        trim_blanks: Cell<TrimBlanks>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for AdvancedRenameProfile {
        const NAME: &'static str = "GnomeCmdAdvancedRenameProfile";
        type Type = super::AdvancedRenameProfile;
    }

    #[glib::derived_properties]
    impl ObjectImpl for AdvancedRenameProfile {}
}

glib::wrapper! {
    pub struct AdvancedRenameProfile(ObjectSubclass<imp::AdvancedRenameProfile>);
}

impl Default for AdvancedRenameProfile {
    fn default() -> Self {
        let this: Self = glib::Object::builder().build();
        this.reset();
        this
    }
}

impl AdvancedRenameProfile {
    pub fn copy_from(&self, other: &Self) {
        self.set_name(other.name());
        self.set_template_string(other.template_string());
        self.set_counter_start(other.counter_start());
        self.set_counter_width(other.counter_width());
        self.set_counter_step(other.counter_step());
        self.set_case_conversion(other.case_conversion());
        self.set_trim_blanks(other.trim_blanks());
        self.set_patterns(other.imp().regexes.borrow().clone());
    }

    pub fn deep_clone(&self) -> Self {
        let copy: Self = glib::Object::builder().build();
        copy.copy_from(self);
        copy
    }

    pub fn reset(&self) {
        self.set_name("");
        self.set_template_string("$N");
        self.set_counter_start(1);
        self.set_counter_width(0);
        self.set_counter_step(1);
        self.imp().regexes.borrow_mut().clear();
        self.set_case_conversion(CaseConversion::Unchanged);
        self.set_trim_blanks(TrimBlanks::LeadingAndTrailing);
    }

    pub fn patterns(&self) -> Vec<RegexReplace> {
        self.imp().regexes.borrow().clone()
    }

    pub fn set_patterns(&self, patterns: Vec<RegexReplace>) {
        self.imp().regexes.replace(patterns);
    }

    pub fn example1() -> Self {
        let p = Self::default();
        p.set_name("Audio Files");
        p.set_template_string("$T(Audio.AlbumArtist) - $T(Audio.Title).$e");
        p.set_patterns(vec![
            RegexReplace {
                pattern: "[ _]+".to_owned(),
                replacement: " ".to_owned(),
                match_case: false,
            },
            RegexReplace {
                pattern: "[fF]eat\\.".to_owned(),
                replacement: "fr.".to_owned(),
                match_case: true,
            },
        ]);
        p.set_counter_width(1);
        p
    }

    pub fn example2() -> Self {
        let p = Self::default();
        p.set_name("CamelCase");
        p.set_patterns(vec![
            RegexReplace {
                pattern: "\\s*\\b(\\w)(\\w*)\\b".to_owned(),
                replacement: "\\u\\1\\L\\2\\E".to_owned(),
                match_case: false,
            },
            RegexReplace {
                pattern: "\\.(.+)$".to_owned(),
                replacement: ".\\L\\1".to_owned(),
                match_case: false,
            },
        ]);
        p
    }
}

#[derive(glib::Variant)]
pub struct AdvancedRenameProfileVariant {
    pub name: String,
    pub template_string: String,
    pub counter_start: u32,
    pub counter_step: i32,
    pub counter_width: u32,
    pub case_conversion: CaseConversion,
    pub trim_blanks: TrimBlanks,
    pub patterns: Vec<String>,
    pub replacements: Vec<String>,
    pub match_cases: Vec<bool>,
}

impl From<&AdvancedRenameProfile> for AdvancedRenameProfileVariant {
    fn from(value: &AdvancedRenameProfile) -> Self {
        let patterns = value.patterns();
        Self {
            name: value.name(),
            template_string: value.template_string(),
            counter_start: value.counter_start(),
            counter_step: value.counter_step(),
            counter_width: value.counter_width(),
            case_conversion: value.case_conversion(),
            trim_blanks: value.trim_blanks(),
            patterns: patterns.iter().map(|p| p.pattern.clone()).collect(),
            replacements: patterns.iter().map(|p| p.replacement.clone()).collect(),
            match_cases: patterns.iter().map(|p| p.match_case).collect(),
        }
    }
}

impl Into<AdvancedRenameProfile> for AdvancedRenameProfileVariant {
    fn into(self) -> AdvancedRenameProfile {
        let profile = AdvancedRenameProfile::default();
        profile.set_name(self.name);
        profile.set_template_string(self.template_string);
        profile.set_counter_start(self.counter_start);
        profile.set_counter_step(self.counter_step);
        profile.set_counter_width(self.counter_width);
        profile.set_case_conversion(self.case_conversion);
        profile.set_trim_blanks(self.trim_blanks);
        profile.set_patterns(
            self.patterns
                .into_iter()
                .zip(self.replacements.into_iter())
                .zip(self.match_cases.into_iter())
                .map(|((pattern, replacement), match_case)| RegexReplace {
                    pattern,
                    replacement,
                    match_case,
                })
                .collect(),
        );
        profile
    }
}

pub fn load_advrename_profiles(
    profile_variants: Vec<AdvancedRenameProfileVariant>,
    default_profile: &AdvancedRenameProfile,
    profiles: &gio::ListStore,
) {
    let mut loaded_profiles: Vec<AdvancedRenameProfile> =
        profile_variants.into_iter().map(|p| p.into()).collect();
    if loaded_profiles.is_empty() {
        default_profile.reset();
        // Add two sample profiles for new users
        profiles.remove_all();
        profiles.append(&AdvancedRenameProfile::example1());
        profiles.append(&AdvancedRenameProfile::example2());
    } else {
        default_profile.copy_from(&loaded_profiles.remove(0));
        profiles.remove_all();
        for p in loaded_profiles {
            profiles.append(&p);
        }
    }
}

pub fn save_advrename_profiles(
    default_profile: &AdvancedRenameProfile,
    profiles: &gio::ListModel,
) -> Vec<AdvancedRenameProfileVariant> {
    let mut profile_variants = Vec::<AdvancedRenameProfileVariant>::new();
    profile_variants.push(default_profile.into());
    for profile in profiles.iter::<AdvancedRenameProfile>().flatten() {
        profile_variants.push((&profile).into());
    }
    profile_variants
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_type() {
        assert_eq!(
            AdvancedRenameProfileVariant::static_variant_type().as_str(),
            "(ssuiuuuasasab)"
        );
    }
}
