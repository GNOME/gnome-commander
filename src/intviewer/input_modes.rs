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

use crate::{debug::debug, intviewer::cp437::CP437};
use std::{
    num::NonZeroUsize,
    sync::{LazyLock, RwLock},
};

pub struct InputMode {
    source: Box<dyn InputSource + Send + Sync>,
    mode: RwLock<String>,
    ascii_charset_translation: RwLock<[char; 256]>,
}

impl InputMode {
    pub fn new<S: InputSource + Send + Sync + 'static>(source: S) -> Self {
        Self {
            source: Box::new(source),
            mode: RwLock::new("ASCII".to_owned()),
            ascii_charset_translation: RwLock::new(*DEFAULT_ASCII_TABLE),
        }
    }

    pub fn max_offset(&self) -> u64 {
        self.source.max_offset()
    }

    pub fn mode(&self) -> String {
        self.mode.read().unwrap().clone()
    }

    pub fn set_mode(&self, mode: &str) {
        *self.mode.write().unwrap() = mode.to_owned();
        if !mode.eq_ignore_ascii_case("UTF8") {
            self.inputmode_ascii_activate(mode)
        }
    }

    fn inputmode_ascii_activate(&self, encoding: &str) {
        *self.ascii_charset_translation.write().unwrap() = if encoding.eq_ignore_ascii_case("ASCII")
        {
            *DEFAULT_ASCII_TABLE
        } else if encoding.eq_ignore_ascii_case("CP437") {
            // Use our special translation table. (IConv does not work with CP437)
            CP437
        } else {
            // Build the translation table for the current charset
            create_encoding_table(encoding).unwrap_or_else(|| {
                eprintln!("Failed to load charset conversions, using ASCII fallback.");
                *DEFAULT_ASCII_TABLE
            })
        };
    }

    /// returns a UTF-8 character in the specified offset.
    ///
    /// 'offset' is ALWAYS BYTE OFFSET IN THE FILE. never logical offset.
    ///
    /// ## Implementors note:
    ///
    /// you must handle gracefully an 'offset' which is not on a character alignemnt.
    /// (e.g. in the second byte of a UTF-8 character)
    pub fn character(&self, offset: u64) -> Option<char> {
        if self.mode() == "UTF8" {
            self.inputmode_utf8_get_char(offset)
        } else {
            self.byte_to_char(self.raw_byte(offset)?)
        }
    }

    pub fn characters(&self, offset: u64, length: usize) -> Vec<char> {
        std::iter::successors(Some(offset), move |offset| {
            Some(self.next_char_offset(*offset))
        })
        .take(length)
        .map(|offset| self.character(offset).unwrap_or_default())
        .collect()
    }

    pub fn characters_backwards(&self, offset: u64, length: usize) -> Vec<char> {
        std::iter::successors(Some(offset), move |offset| {
            Some(self.previous_char_offset(*offset))
        })
        .take(length)
        .map(|offset| self.character(offset).unwrap_or_default())
        .collect()
    }

    /// returns the RAW Byte at 'offset'.
    /// Does no input mode translations.
    ///
    /// returns `None`` on failure or EOF.
    pub fn raw_byte(&self, offset: u64) -> Option<u8> {
        self.source.byte(offset)
    }

    pub fn bytes(&self, offset: u64, length: usize) -> Vec<u8> {
        std::iter::successors(Some(offset), move |offset| offset.checked_add(1))
            .take(length)
            .map(|offset| self.raw_byte(offset).unwrap_or_default())
            .collect()
    }

    pub fn bytes_backwards(&self, offset: u64, length: usize) -> Vec<u8> {
        std::iter::successors(Some(offset), move |offset| offset.checked_sub(1))
            .take(length)
            .map(|offset| self.raw_byte(offset).unwrap_or_default())
            .collect()
    }

    /// returns the BYTE offset of the next logical character.
    ///
    /// For ASCII input mode, each character is one byte, so the function only increments offset by 1.
    /// For UTF-8 input mode, a character is 1 to 6 bytes.
    /// Other input modes can return diferent values.
    pub fn next_char_offset(&self, current_offset: u64) -> u64 {
        if self.mode() == "UTF8" {
            self.inputmode_utf8_get_next_offset(current_offset)
        } else {
            (current_offset + 1).clamp(0, self.max_offset())
        }
    }

    /// returns the BYTE offset of the previous logical character.
    pub fn previous_char_offset(&self, current_offset: u64) -> u64 {
        if self.mode() == "UTF8" {
            self.inputmode_utf8_get_previous_offset(current_offset)
        } else {
            current_offset.saturating_sub(1)
        }
    }

    pub fn byte_offsets(&self, start: u64, end: u64) -> impl Iterator<Item = u64> + use<'_> {
        std::iter::successors(Some(start), move |offset| {
            (*offset < end)
                .then(|| *offset + 1)
                .filter(|next_offset| *next_offset < end)
        })
    }

    pub fn offsets(&self, start: u64, end: u64) -> impl Iterator<Item = u64> + use<'_> {
        std::iter::successors(Some(start), move |offset| {
            (*offset < end)
                .then(|| self.next_char_offset(*offset))
                .filter(|next_offset| *next_offset < end)
        })
    }

    pub fn byte_to_char(&self, data: u8) -> Option<char> {
        Some(self.ascii_charset_translation.read().ok()?[data as usize])
    }

    /// Used by highler layers (text-render) to update the translation table,
    /// filter out utf8 characters that IConv returned but Pango can't display
    pub fn update_utf8_translation(&self, index: u8, new_value: char) {
        self.ascii_charset_translation.write().unwrap()[index as usize] = new_value;
    }

    fn inputmode_utf8_get_next_offset(&self, offset: u64) -> u64 {
        let len = match self.get_utf8_value(offset) {
            Some((len, _)) => len as u64,
            None => 1,
        };
        (offset + len).clamp(0, self.max_offset())
    }

    fn inputmode_utf8_get_previous_offset(&self, offset: u64) -> u64 {
        for len in 1..=4 {
            if let Some(probe) = offset.checked_sub(len) {
                if self.get_utf8_value(probe).is_some() {
                    return probe;
                }
            }
        }
        offset.saturating_sub(1)
    }

    fn inputmode_utf8_get_char(&self, offset: u64) -> Option<char> {
        let b = self.raw_byte(offset)?;

        let Some((_len, value)) = self.get_utf8_value(offset) else {
            debug!('v', "invalid UTF character at offset {offset} ({b:02X})");
            return Some('.');
        };
        let Some(ch) = char::from_u32(value) else {
            debug!(
                'v',
                "invalid UTF character at offset {offset} ({value:08X})"
            );
            return Some('.');
        };
        Some(ch)
    }

    fn get_utf8_value(&self, offset: u64) -> Option<(usize, u32)> {
        let b = self.raw_byte(offset)?;

        let len: usize = utf8_get_char_len(b)?.into();

        if len == 1 {
            return Some((len, b as u32));
        }

        let b1 = self.raw_byte(offset + 1)?;
        if !is_utf8_trailer(b1) {
            return None;
        }

        if len == 2 {
            return Some((len, (b as u32 & 0x1F) << 6 | (b1 as u32 & 0x3F)));
        }

        let b2 = self.raw_byte(offset + 2)?;
        if !is_utf8_trailer(b2) {
            return None;
        }

        if len == 3 {
            return Some((
                len,
                (b as u32 & 0x0F) << 12 | (b1 as u32 & 0x3F) << 6 | (b2 as u32 & 0x3F),
            ));
        }

        let b3 = self.raw_byte(offset + 3)?;
        if !is_utf8_trailer(b3) {
            return None;
        }

        if len == 4 {
            return Some((
                len,
                (b as u32 & 0x03) << 18
                    | (b1 as u32 & 0x3F) << 12
                    | (b2 as u32 & 0x3F) << 6
                    | (b3 as u32 & 0x3F),
            ));
        }

        None
    }
}

pub trait InputSource {
    fn max_offset(&self) -> u64;
    fn byte(&self, offset: u64) -> Option<u8>;
}

fn utf8_get_char_len(b: u8) -> Option<NonZeroUsize> {
    if b & 0x80 == 0 {
        NonZeroUsize::new(1)
    } else if b & 0xE0 == 0xC0 {
        NonZeroUsize::new(2)
    } else if b & 0xF0 == 0xE0 {
        NonZeroUsize::new(3)
    } else if b & 0xF8 == 0xF0 {
        NonZeroUsize::new(4)
    } else {
        // this is an invalid UTF8 character
        None
    }
}

fn is_utf8_trailer(b: u8) -> bool {
    b & 0xC0 == 0x80
}

static DEFAULT_ASCII_TABLE: LazyLock<[char; 256]> = LazyLock::new(|| {
    let mut table = ['.'; 256];
    for b in 0..0x80_u8 {
        table[b as usize] = b as char
    }
    return table;
});

fn create_encoding_table(encoding: &str) -> Option<[char; 256]> {
    let mut icnv = glib::IConv::new("UTF8", encoding)?;
    let mut table = ['.'; 256];
    for i in 0..=255_u8 {
        table[i as usize] = icnv
            .convert(&[i])
            .ok()
            .as_ref()
            .and_then(|s| std::str::from_utf8(&s.0).ok())
            .and_then(|s| s.chars().next())
            .unwrap_or('.');
    }
    Some(table)
}

impl InputSource for &[u8] {
    fn max_offset(&self) -> u64 {
        self.len() as u64
    }

    fn byte(&self, offset: u64) -> Option<u8> {
        let index: usize = offset.try_into().ok()?;
        Some(*self.get(index)?)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn input_mode() {
        let data: &[u8] = &[];
        let imd = InputMode::new(data);
        for mode in ["ASCII", "UTF8", "CP437", "CP1251"] {
            imd.set_mode(mode);
            assert_eq!(imd.mode(), mode);
        }
    }

    #[test]
    fn test_decode_utf8() {
        let data: &[u8] = &[
            0x41, 0xC3, 0xB6, 0xD0, 0x96, 0xE2, 0x82, 0xAC, 0xF0, 0x9D, 0x84, 0x9E,
        ];
        let imd = InputMode::new(data);
        imd.set_mode("UTF8");
        assert_eq!(
            imd.characters(0, 5),
            vec!['\u{0041}', '\u{00f6}', '\u{0416}', '\u{20ac}', '\u{1d11e}',]
        );
    }
}
