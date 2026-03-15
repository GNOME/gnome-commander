// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::utils::u32_enum;
use glob::{MatchOptions, Pattern};
use regex::{Regex, RegexBuilder};
use std::error::Error;

u32_enum! {
    pub enum PatternType {
        Regex,
        #[default]
        FnMatch,
    }
}

pub enum Filter {
    Regex {
        regex: Regex,
    },
    FnMatch {
        pattern: Pattern,
        options: MatchOptions,
    },
}

impl Filter {
    pub fn new(
        expression: &str,
        case_sensitive: bool,
        pattern_type: PatternType,
    ) -> Result<Self, Box<dyn Error>> {
        match pattern_type {
            PatternType::Regex => Ok(Self::Regex {
                regex: RegexBuilder::new(expression)
                    .case_insensitive(!case_sensitive)
                    .build()?,
            }),
            PatternType::FnMatch => Ok(Self::FnMatch {
                pattern: Pattern::new(expression)?,
                options: MatchOptions {
                    case_sensitive,
                    ..Default::default()
                },
            }),
        }
    }

    pub fn matches(&self, text: &str) -> bool {
        match self {
            Self::Regex { regex } => regex.is_match(text),
            Self::FnMatch { pattern, options } => pattern.matches_with(text, *options),
        }
    }
}

pub fn fnmatch(pattern: &str, string: &str, case_sensitive: bool) -> bool {
    let Ok(pattern) = Pattern::new(pattern) else {
        return false;
    };
    let options = MatchOptions {
        case_sensitive,
        ..Default::default()
    };
    pattern.matches_with(string, options)
}
