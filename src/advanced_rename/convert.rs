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

use unicode_segmentation::UnicodeSegmentation;

/// Quick roman numeral check (non-roman numerals may also return true)
///
/// (<http://home.hiwaay.net/~lkseitz/math/roman/numerals.shtml>)
///
/// ```text
///    I = 1    (one)
///    V = 5    (five)
///    X = 10   (ten)
///    L = 50   (fifty)
///    C = 100  (one hundred)
///    D = 500  (five hundred)
///    M = 1000 (one thousand)
/// ```
fn word_is_roman_numeral(text: &str) -> bool {
    const ROMAN: &[char] = &[
        'i', 'I', 'v', 'V', 'x', 'X', 'l', 'L', 'c', 'C', 'd', 'D', 'm', 'M',
    ];
    text.chars().all(|c| ROMAN.contains(&c))
}

/// Chicago Manual of Style "Heading caps" Capitalization Rules (CMS 1993, 282) (<http://www.docstyles.com/cmscrib.htm#Note2>)
pub fn sentence_case(string: &str) -> String {
    const EXEMPTS: &[&str] = &[
        "a", "against", "an", "and", "at", "between", "but", "for", "in", "nor", "of", "on", "or",
        "so", "the", "to", "with", "yet",
    ];

    let mut result = String::with_capacity(string.len());

    let first_word_index = string.unicode_word_indices().next().map(|p| p.0);
    let last_word_index = string.unicode_word_indices().rev().next().map(|p| p.0);

    for (index, word_or_space) in string.split_word_bound_indices() {
        if word_is_roman_numeral(word_or_space) {
            result.push_str(&word_or_space.to_uppercase());
        } else if Some(index) == first_word_index || Some(index) == last_word_index {
            // First or last word of the string: the first letter is always uppercase,
            // even if it's in the exempt list. This is a Chicago Manual of Style rule.
            // Last Word In String - Should Capitalize Regardless of Word (Chicago Manual of Style)
            push_capitalized(&mut result, word_or_space);
        } else {
            let lowercased = word_or_space.to_lowercase();
            if EXEMPTS.contains(&lowercased.as_str()) {
                result.push_str(&lowercased);
            } else {
                push_capitalized(&mut result, word_or_space);
            }
        }
    }
    result
}

pub fn initial_caps(string: &str) -> String {
    let mut result = String::with_capacity(string.len());
    for word_or_space in string.split_word_bounds() {
        push_capitalized(&mut result, word_or_space);
    }
    result
}

fn push_capitalized(result: &mut String, word: &str) {
    for (index, ch) in word.chars().enumerate() {
        if index == 0 {
            result.extend(ch.to_uppercase());
        } else {
            result.extend(ch.to_lowercase());
        }
    }
}

pub fn toggle_case(string: &str) -> String {
    let mut result = String::with_capacity(string.len());
    for c in string.chars() {
        if c.is_uppercase() {
            result.extend(c.to_lowercase());
        } else if c.is_lowercase() {
            result.extend(c.to_uppercase());
        } else {
            result.push(c);
        }
    }
    result
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_sentence_case() {
        assert_eq!(sentence_case("hello world!"), "Hello World!");
        assert_eq!(sentence_case(" hEllo _world! "), " Hello _world! ");
        assert_eq!(
            sentence_case("that's something, i like"),
            "That's Something, I Like"
        );
        assert_eq!(sentence_case("What A greaT day!"), "What a Great Day!");
        assert_eq!(
            sentence_case(" chapter xiv. not finished yet "),
            " Chapter XIV. Not Finished Yet "
        );
        assert_eq!(sentence_case("What A greaT day!"), "What a Great Day!");
        assert_eq!(
            sentence_case("CMS (chicago manual of style)"),
            "Cms (Chicago Manual of Style)"
        );
    }

    #[test]
    fn test_initial_caps() {
        assert_eq!(initial_caps("hello world!"), "Hello World!");
        assert_eq!(initial_caps(" hEllo _world! "), " Hello _world! ");
        assert_eq!(
            initial_caps("that'S something, i like"),
            "That's Something, I Like"
        );
    }

    #[test]
    fn test_toggle_case() {
        assert_eq!(toggle_case("Hello World"), "hELLO wORLD");
        assert_eq!(toggle_case("Große Straße"), "gROSSE sTRASSE");
        assert_eq!(toggle_case("gROSSE sTRASSE"), "Grosse Strasse");
    }
}
