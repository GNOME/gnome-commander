// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::borrow::Cow;

pub struct InputMode {
    source: Box<dyn InputSource + Send + Sync>,
    encoding: Box<dyn CharacterEncoding + Send + Sync>,
}

impl InputMode {
    pub fn new<S: InputSource + Send + Sync + 'static>(source: S) -> Self {
        Self {
            source: Box::new(source),
            encoding: Box::new(SingleByteEncoding::new("ASCII")),
        }
    }

    pub fn max_offset(&self) -> u64 {
        self.source.max_offset()
    }

    pub fn set_mode(&mut self, encoding: &str) {
        self.encoding = if encoding.eq_ignore_ascii_case("UTF8") {
            Box::new(UTF8Encoding)
        } else {
            Box::new(SingleByteEncoding::new(encoding))
        };
    }

    /// Returns a Unicode character at the specified offset.
    ///
    /// Note: `offset`` is ALWAYS THE BYTE OFFSET IN THE FILE, never logical offset.
    pub fn character(&self, offset: u64) -> Option<char> {
        self.encoding.char_at(self, offset)
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

    /// Returns the raw untranslated byte at `offset`.
    ///
    /// Will return `None` on failure or EOF.
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

    /// Returns the BYTE offset of the next logical character.
    ///
    /// The size of a character depends on the chosen encoding and can be non-constant.
    pub fn next_char_offset(&self, current_offset: u64) -> u64 {
        self.encoding
            .next_char_offset(self, current_offset)
            .clamp(0, self.max_offset())
    }

    /// Returns the BYTE offset of the previous logical character.
    ///
    /// The size of a character depends on the chosen encoding and can be non-constant.
    pub fn previous_char_offset(&self, current_offset: u64) -> u64 {
        self.encoding
            .previous_char_offset(self, current_offset)
            .clamp(0, self.max_offset())
    }

    pub fn byte_offsets(&self, start: u64, end: u64) -> impl Iterator<Item = u64> {
        std::iter::successors(Some(start), move |offset| {
            (*offset < end)
                .then(|| *offset + 1)
                .filter(|next_offset| *next_offset < end)
        })
    }

    pub fn offsets(&self, start: u64, end: u64) -> impl Iterator<Item = u64> {
        std::iter::successors(Some(start), move |offset| {
            (*offset < end)
                .then(|| self.next_char_offset(*offset))
                .filter(|next_offset| *next_offset < end)
        })
    }

    /// Returns the character size if an encoding with a fixed character size is selected.
    pub fn char_size(&self) -> Option<u64> {
        self.encoding.char_size()
    }

    /// Checks whether a character starts at the given offset. This will always return `true` for
    /// single-byte encodings.
    pub fn is_at_char_boundary(&self, offset: u64) -> bool {
        self.encoding.is_at_char_boundary(self, offset)
    }

    /// If not currently at a character start, finds next character start. Otherwise returns the
    /// offset unchanged.
    pub fn next_char_boundary(&self, offset: u64) -> u64 {
        self.encoding.next_char_boundary(self, offset)
    }

    /// If not currently at a character start, finds previous character start. Otherwise returns the
    /// offset unchanged.
    pub fn previous_char_boundary(&self, offset: u64) -> u64 {
        self.encoding.previous_char_boundary(self, offset)
    }

    /// Used by higher layers (text-render) to update the translation table,
    /// filter out utf8 characters that IConv returned but Pango can't display.
    pub fn adjust_invisible_characters(
        &mut self,
        check_visibility: impl Fn(char) -> bool + 'static,
    ) {
        self.encoding
            .adjust_invisible_characters(Box::new(check_visibility))
    }
}

impl Default for InputMode {
    fn default() -> Self {
        InputMode::new(&[0_u8][0..0])
    }
}

pub trait InputSource {
    fn max_offset(&self) -> u64;
    fn byte(&self, offset: u64) -> Option<u8>;
}

const CP437_SPECIAL_CHARS: &str = "☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼⌂";

pub fn preprocess_for_cp437_conversion(text: &str) -> Cow<'_, str> {
    let mut current = Cow::Borrowed(text);
    for (i, char) in CP437_SPECIAL_CHARS.chars().enumerate() {
        if current.contains(char) {
            let replacement = (if i < 0x1f { i as u8 + 1 } else { 0x7f } as char).to_string();
            current = Cow::Owned(current.replace(char, &replacement));
        }
    }
    current
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

trait CharacterEncoding {
    /// For fixed-width encodings, this should return the size of a character in bytes. For
    /// variable-width encodings `None`` is returned.
    fn char_size(&self) -> Option<u64>;

    /// Decodes the character at the given offset.
    fn char_at(&self, input_mode: &InputMode, offset: u64) -> Option<char>;

    /// Determines the byte offset of the character after the given offset. Note: the offset isn’t
    /// necessarily aligned with the character boundary.
    fn next_char_offset(&self, input_mode: &InputMode, current_offset: u64) -> u64;

    /// Determines the byte offset of the character before the given offset. Note: the offset isn’t
    /// necessarily aligned with the character boundary.
    fn previous_char_offset(&self, input_mode: &InputMode, current_offset: u64) -> u64;

    /// Checks whether the offset is aligned with the character boundary.
    fn is_at_char_boundary(&self, input_mode: &InputMode, offset: u64) -> bool;

    /// Aligns the offset to character boundary, going forward.
    fn next_char_boundary(&self, input_mode: &InputMode, offset: u64) -> u64;

    /// Aligns the offset to character boundary, going backwards.
    fn previous_char_boundary(&self, input_mode: &InputMode, offset: u64) -> u64;

    /// Will be called with a callback that can determine whether a character can be rendered. This
    /// can be used to adjust the character's presentation.
    fn adjust_invisible_characters(&mut self, _check_visibility: Box<dyn Fn(char) -> bool>) {}
}

#[derive(Debug)]
struct SingleByteEncoding {
    translation_table: [char; 256],
}

impl SingleByteEncoding {
    const DEFAULT_ASCII_TABLE: [char; 256] = {
        let mut table = ['.'; 256];
        let mut b = 0_u8;
        while b < 0x80 {
            table[b as usize] = b as char;
            b += 1;
        }
        table
    };

    pub fn new(encoding: &str) -> Self {
        let translation_table = if encoding.eq_ignore_ascii_case("ASCII") {
            Self::DEFAULT_ASCII_TABLE
        } else {
            // Build the translation table for the current charset
            Self::create_encoding_table(encoding).unwrap_or_else(|| {
                eprintln!("Failed to load charset conversions, using ASCII fallback.");
                Self::DEFAULT_ASCII_TABLE
            })
        };
        Self { translation_table }
    }

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
        if encoding.eq_ignore_ascii_case("CP437") {
            // CP437 characters 01-1F and 7F aren't handled by iconv
            for (i, char) in CP437_SPECIAL_CHARS.chars().enumerate() {
                if i < 0x1f {
                    table[i + 1] = char;
                } else {
                    table[0x7f] = char;
                }
            }
        }
        Some(table)
    }
}

impl CharacterEncoding for SingleByteEncoding {
    fn char_size(&self) -> Option<u64> {
        Some(1)
    }

    fn char_at(&self, input_mode: &InputMode, offset: u64) -> Option<char> {
        input_mode
            .raw_byte(offset)
            .map(|byte| self.translation_table[byte as usize])
    }

    fn next_char_offset(&self, input_mode: &InputMode, current_offset: u64) -> u64 {
        current_offset
            .saturating_add(1)
            .clamp(0, input_mode.max_offset())
    }

    fn previous_char_offset(&self, _input_mode: &InputMode, current_offset: u64) -> u64 {
        current_offset.saturating_sub(1)
    }

    fn is_at_char_boundary(&self, _input_mode: &InputMode, _offset: u64) -> bool {
        true
    }

    fn next_char_boundary(&self, _input_mode: &InputMode, offset: u64) -> u64 {
        offset
    }

    fn previous_char_boundary(&self, _input_mode: &InputMode, offset: u64) -> u64 {
        offset
    }

    fn adjust_invisible_characters(&mut self, check_visibility: Box<dyn Fn(char) -> bool>) {
        for byte in 0..256 {
            let ch = self.translation_table[byte];
            if !check_visibility(ch) {
                self.translation_table[byte] = '.';
            }
        }
    }
}

struct UTF8Encoding;

impl UTF8Encoding {
    fn char_len(b: u8) -> Option<usize> {
        match b.leading_ones() {
            0 => Some(1),
            1 => None, // continuation byte
            leading_ones if (2..=4).contains(&leading_ones) => Some(leading_ones as usize),
            _ => None, // invalid character
        }
    }

    fn is_continuation(b: u8) -> bool {
        b.leading_ones() == 1
    }
}

impl CharacterEncoding for UTF8Encoding {
    fn char_size(&self) -> Option<u64> {
        None
    }

    fn char_at(&self, input_mode: &InputMode, offset: u64) -> Option<char> {
        let b = input_mode.raw_byte(offset)?;
        Some(
            Self::char_len(b)
                .and_then(|len| {
                    str::from_utf8(&input_mode.bytes(offset, len))
                        .ok()
                        .and_then(|str| str.chars().next())
                })
                .unwrap_or('.'),
        )
    }

    fn next_char_offset(&self, input_mode: &InputMode, current_offset: u64) -> u64 {
        // Rust supports at most four bytes long UTF-8 sequences, try decoding.
        let bytes = input_mode.bytes(current_offset, 4);
        for len in 1..=4 {
            if std::str::from_utf8(&bytes[..len]).is_ok() {
                return current_offset.saturating_add(len as u64);
            }
        }
        current_offset.saturating_add(1)
    }

    fn previous_char_offset(&self, input_mode: &InputMode, current_offset: u64) -> u64 {
        // Rust supports at most four bytes long UTF-8 sequences, try decoding.
        let mut bytes = input_mode.bytes(
            current_offset.saturating_sub(4),
            std::cmp::min(4, current_offset) as usize,
        );
        while bytes.len() < 4 {
            bytes.insert(0, 0);
        }
        for len in 1..=4 {
            if std::str::from_utf8(&bytes[4 - len..4]).is_ok() {
                return current_offset.saturating_sub(len as u64);
            }
        }
        current_offset.saturating_sub(1)
    }

    fn next_char_boundary(&self, input_mode: &InputMode, offset: u64) -> u64 {
        // Skip at most 3 continuation bytes
        let mut result = offset;
        for distance in 1..=3 {
            if input_mode
                .raw_byte(result)
                .is_none_or(|byte| !Self::is_continuation(byte))
            {
                break;
            }
            if let Some(offset) = offset.checked_add(distance) {
                result = offset;
            } else {
                break;
            };
        }
        result
    }

    fn previous_char_boundary(&self, input_mode: &InputMode, offset: u64) -> u64 {
        // Skip at most 3 continuation bytes
        let mut result = offset;
        for distance in 1..=3 {
            if input_mode
                .raw_byte(result)
                .is_none_or(|byte| !Self::is_continuation(byte))
            {
                break;
            }
            if let Some(offset) = offset.checked_sub(distance) {
                result = offset;
            } else {
                break;
            };
        }
        result
    }

    fn is_at_char_boundary(&self, input_mode: &InputMode, offset: u64) -> bool {
        !input_mode
            .raw_byte(offset)
            .is_some_and(Self::is_continuation)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_decode_cp437() {
        let data: &[u8] = &[0x00, 0x03, 0x0A, 0x20, 0x41, 0x80, 0xFE];
        let mut imd = InputMode::new(data);
        imd.set_mode("CP437");
        assert_eq!(String::from_iter(imd.characters(0, 7)), "\x00♥◙ AÇ■");
    }

    #[test]
    fn test_decode_windows1251() {
        let data: &[u8] = &[0x00, 0x03, 0x0A, 0x20, 0x41, 0x80, 0xFE];
        let mut imd = InputMode::new(data);
        imd.set_mode("CP1251");
        assert_eq!(String::from_iter(imd.characters(0, 7)), "\x00\x03\n AЂю");
    }

    #[test]
    fn test_preprocess_cp437() {
        let data = "a\x00b☻♥cd▬↨e f▲▼g~h⌂";
        assert_eq!(
            preprocess_for_cp437_conversion(data),
            "a\x00b\x02\x03cd\x16\x17e f\x1e\x1fg~h\x7f"
        );
    }

    #[test]
    fn test_ascii_encoding() {
        let data: &[u8] = &[0x00, 0x03, 0x0A, 0x20, 0x41, 0x80, 0xFE];
        let mut imd = InputMode::new(data);
        imd.set_mode("ASCII");

        assert_eq!(imd.char_size(), Some(1));

        assert_eq!(imd.character(0), Some('\0'));
        assert_eq!(imd.character(1), Some('\x03'));
        assert_eq!(imd.character(2), Some('\n'));
        assert_eq!(imd.character(3), Some(' '));
        assert_eq!(imd.character(4), Some('A'));
        assert_eq!(imd.character(5), Some('.'));
        assert_eq!(imd.character(6), Some('.'));
        assert_eq!(imd.character(7), None);

        assert_eq!(imd.next_char_offset(0), 1);
        assert_eq!(imd.next_char_offset(5), 6);
        assert_eq!(imd.next_char_offset(6), 7);
        assert_eq!(imd.next_char_offset(7), 7);

        assert_eq!(imd.previous_char_offset(0), 0);
        assert_eq!(imd.previous_char_offset(5), 4);
        assert_eq!(imd.previous_char_offset(6), 5);
        assert_eq!(imd.previous_char_offset(7), 6);

        assert_eq!(imd.is_at_char_boundary(0), true);
        assert_eq!(imd.is_at_char_boundary(5), true);
        assert_eq!(imd.is_at_char_boundary(6), true);
        assert_eq!(imd.is_at_char_boundary(7), true);

        assert_eq!(imd.next_char_boundary(0), 0);
        assert_eq!(imd.next_char_boundary(5), 5);
        assert_eq!(imd.next_char_boundary(6), 6);
        assert_eq!(imd.next_char_boundary(7), 7);

        assert_eq!(imd.previous_char_boundary(0), 0);
        assert_eq!(imd.previous_char_boundary(5), 5);
        assert_eq!(imd.previous_char_boundary(6), 6);
        assert_eq!(imd.previous_char_boundary(7), 7);
    }

    #[test]
    fn test_utf8_encoding() {
        let data: &[u8] = &[
            0x00, 0x0A, 0x20, 0x41, 0x7F, // single-byte sequences
            0x80, // Invalid
            0xD1, 0x82, // Cyrillic т
            0xED, 0x9F, 0xBF, // \u{D7FF}
            0xF4, 0x8F, 0xBF, 0xBF, // \u{10FFFF}
            0xF4, 0x8F, 0xBF, 0x22, // incomplete sequence
        ];
        let mut imd = InputMode::new(data);
        imd.set_mode("UTF8");

        assert_eq!(imd.char_size(), None);

        assert_eq!(imd.character(0), Some('\0'));
        assert_eq!(imd.character(1), Some('\n'));
        assert_eq!(imd.character(2), Some(' '));
        assert_eq!(imd.character(3), Some('A'));
        assert_eq!(imd.character(4), Some('\x7f'));
        assert_eq!(imd.character(5), Some('.'));
        assert_eq!(imd.character(6), Some('т'));
        assert_eq!(imd.character(7), Some('.'));
        assert_eq!(imd.character(8), Some('\u{D7FF}'));
        assert_eq!(imd.character(9), Some('.'));
        assert_eq!(imd.character(10), Some('.'));
        assert_eq!(imd.character(11), Some('\u{10FFFF}'));
        assert_eq!(imd.character(12), Some('.'));
        assert_eq!(imd.character(13), Some('.'));
        assert_eq!(imd.character(14), Some('.'));
        assert_eq!(imd.character(15), Some('.'));
        assert_eq!(imd.character(16), Some('.'));
        assert_eq!(imd.character(17), Some('.'));
        assert_eq!(imd.character(18), Some('"'));
        assert_eq!(imd.character(19), None); // out of bounds

        assert_eq!(imd.next_char_offset(0), 1);
        assert_eq!(imd.next_char_offset(1), 2);
        assert_eq!(imd.next_char_offset(2), 3);
        assert_eq!(imd.next_char_offset(3), 4);
        assert_eq!(imd.next_char_offset(4), 5);
        assert_eq!(imd.next_char_offset(5), 6);
        assert_eq!(imd.next_char_offset(6), 8);
        assert_eq!(imd.next_char_offset(8), 11);
        assert_eq!(imd.next_char_offset(11), 15);
        assert_eq!(imd.next_char_offset(15), 16);
        assert_eq!(imd.next_char_offset(16), 17);
        assert_eq!(imd.next_char_offset(17), 18);
        assert_eq!(imd.next_char_offset(18), 19);
        assert_eq!(imd.next_char_offset(19), 19);

        assert_eq!(imd.previous_char_offset(0), 0);
        assert_eq!(imd.previous_char_offset(1), 0);
        assert_eq!(imd.previous_char_offset(2), 1);
        assert_eq!(imd.previous_char_offset(3), 2);
        assert_eq!(imd.previous_char_offset(4), 3);
        assert_eq!(imd.previous_char_offset(5), 4);
        assert_eq!(imd.previous_char_offset(6), 5);
        assert_eq!(imd.previous_char_offset(8), 6);
        assert_eq!(imd.previous_char_offset(11), 8);
        assert_eq!(imd.previous_char_offset(15), 11);
        assert_eq!(imd.previous_char_offset(16), 15);
        assert_eq!(imd.previous_char_offset(17), 16);
        assert_eq!(imd.previous_char_offset(18), 17);
        assert_eq!(imd.previous_char_offset(19), 18);

        assert_eq!(imd.is_at_char_boundary(0), true);
        assert_eq!(imd.is_at_char_boundary(1), true);
        assert_eq!(imd.is_at_char_boundary(2), true);
        assert_eq!(imd.is_at_char_boundary(3), true);
        assert_eq!(imd.is_at_char_boundary(4), true);
        assert_eq!(imd.is_at_char_boundary(5), false);
        assert_eq!(imd.is_at_char_boundary(6), true);
        assert_eq!(imd.is_at_char_boundary(7), false);
        assert_eq!(imd.is_at_char_boundary(8), true);
        assert_eq!(imd.is_at_char_boundary(9), false);
        assert_eq!(imd.is_at_char_boundary(10), false);
        assert_eq!(imd.is_at_char_boundary(11), true);
        assert_eq!(imd.is_at_char_boundary(12), false);
        assert_eq!(imd.is_at_char_boundary(13), false);
        assert_eq!(imd.is_at_char_boundary(14), false);
        assert_eq!(imd.is_at_char_boundary(15), true);
        assert_eq!(imd.is_at_char_boundary(16), false);
        assert_eq!(imd.is_at_char_boundary(17), false);
        assert_eq!(imd.is_at_char_boundary(18), true);
        assert_eq!(imd.is_at_char_boundary(19), true);

        assert_eq!(imd.next_char_boundary(0), 0);
        assert_eq!(imd.next_char_boundary(1), 1);
        assert_eq!(imd.next_char_boundary(2), 2);
        assert_eq!(imd.next_char_boundary(3), 3);
        assert_eq!(imd.next_char_boundary(4), 4);
        assert_eq!(imd.next_char_boundary(5), 6);
        assert_eq!(imd.next_char_boundary(6), 6);
        assert_eq!(imd.next_char_boundary(7), 8);
        assert_eq!(imd.next_char_boundary(8), 8);
        assert_eq!(imd.next_char_boundary(9), 11);
        assert_eq!(imd.next_char_boundary(10), 11);
        assert_eq!(imd.next_char_boundary(11), 11);
        assert_eq!(imd.next_char_boundary(12), 15);
        assert_eq!(imd.next_char_boundary(13), 15);
        assert_eq!(imd.next_char_boundary(14), 15);
        assert_eq!(imd.next_char_boundary(15), 15);
        assert_eq!(imd.next_char_boundary(16), 18);
        assert_eq!(imd.next_char_boundary(17), 18);
        assert_eq!(imd.next_char_boundary(18), 18);
        assert_eq!(imd.next_char_boundary(19), 19);

        assert_eq!(imd.previous_char_boundary(0), 0);
        assert_eq!(imd.previous_char_boundary(1), 1);
        assert_eq!(imd.previous_char_boundary(2), 2);
        assert_eq!(imd.previous_char_boundary(3), 3);
        assert_eq!(imd.previous_char_boundary(4), 4);
        assert_eq!(imd.previous_char_boundary(5), 4);
        assert_eq!(imd.previous_char_boundary(6), 6);
        assert_eq!(imd.previous_char_boundary(7), 6);
        assert_eq!(imd.previous_char_boundary(8), 8);
        assert_eq!(imd.previous_char_boundary(9), 8);
        assert_eq!(imd.previous_char_boundary(10), 8);
        assert_eq!(imd.previous_char_boundary(11), 11);
        assert_eq!(imd.previous_char_boundary(12), 11);
        assert_eq!(imd.previous_char_boundary(13), 11);
        assert_eq!(imd.previous_char_boundary(14), 11);
        assert_eq!(imd.previous_char_boundary(15), 15);
        assert_eq!(imd.previous_char_boundary(16), 15);
        assert_eq!(imd.previous_char_boundary(17), 15);
        assert_eq!(imd.previous_char_boundary(18), 18);
        assert_eq!(imd.previous_char_boundary(19), 19);
    }
}
