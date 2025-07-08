/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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

use glob::{MatchOptions, Pattern};
use regex::{Regex, RegexBuilder};
use std::error::Error;

#[derive(Clone, Copy)]
#[repr(C)]
pub enum PatternType {
    Regex = 0,
    FnMatch,
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
