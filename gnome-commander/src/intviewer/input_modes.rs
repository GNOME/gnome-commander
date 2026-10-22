// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::input_source::InputSource;
use gtk::gio;
use std::borrow::Cow;

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct FileOffset {
    offset: i64,
}

impl FileOffset {
    fn new(offset: i64) -> Self {
        Self { offset }
    }

    fn add(self, n: i64, size: FileOffset) -> Self {
        Self {
            offset: self.offset.saturating_add(n).clamp(0, size.offset),
        }
    }

    fn inc(self, size: FileOffset) -> Self {
        self.add(1, size)
    }

    fn sub(self, n: i64) -> Self {
        Self {
            offset: (self.offset - n).max(0),
        }
    }

    fn dec(self) -> Self {
        self.sub(1)
    }

    /// This converts the offset to a number.
    ///
    /// Using this method should normally not be necessary!
    pub fn to_raw_offset(self) -> usize {
        self.offset as usize
    }

    /// This converts a number into a file offset.
    ///
    /// Using this method should normally not be necessary!
    pub fn from_raw_offset(offset: usize) -> Self {
        Self::new(offset as i64)
    }
}

impl From<FileOffset> for f64 {
    fn from(value: FileOffset) -> Self {
        value.offset as Self
    }
}

impl From<f64> for FileOffset {
    fn from(value: f64) -> Self {
        FileOffset {
            offset: value as i64,
        }
    }
}

impl std::ops::Sub for FileOffset {
    type Output = f64;

    fn sub(self, rhs: Self) -> Self::Output {
        f64::from(self) - f64::from(rhs)
    }
}

impl std::fmt::Display for FileOffset {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        std::fmt::Display::fmt(&self.offset, f)
    }
}

impl std::fmt::UpperHex for FileOffset {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        std::fmt::UpperHex::fmt(&self.offset, f)
    }
}

pub struct InputMode {
    source: InputSource,
    encoding: Box<dyn CharacterEncoding + Send>,
}

impl InputMode {
    pub fn new(source: InputSource) -> Self {
        Self {
            source,
            encoding: Box::new(SingleByteEncoding::new("ASCII")),
        }
    }

    pub fn file(&self) -> &gio::File {
        self.source.file()
    }

    pub fn size(&self) -> FileOffset {
        FileOffset::new(self.source.size())
    }

    pub fn max_offset(&self) -> FileOffset {
        self.size().dec()
    }

    pub fn set_mode(&mut self, encoding: &str, allow_newlines: bool) {
        self.encoding = if encoding.eq_ignore_ascii_case("UTF8") {
            Box::new(UTF8Encoding)
        } else {
            let mut encoding = SingleByteEncoding::new(encoding);
            if allow_newlines {
                encoding.allow_newlines();
            }
            Box::new(encoding)
        };
    }

    /// Returns a Unicode character at the specified offset.
    pub fn character(&self, offset: FileOffset) -> Option<char> {
        self.encoding.char_at(self, offset)
    }

    pub fn characters(&self, offset: FileOffset, length: usize) -> Vec<char> {
        std::iter::successors(Some(offset), move |offset| {
            Some(self.next_char_offset(*offset))
        })
        .take(length)
        .map(|offset| self.character(offset).unwrap_or_default())
        .collect()
    }

    pub fn characters_backwards(&self, offset: FileOffset, length: usize) -> Vec<char> {
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
    pub fn raw_byte(&self, offset: FileOffset) -> Option<u8> {
        self.source.byte(offset.offset)
    }

    pub fn bytes(&self, offset: FileOffset, length: usize) -> Vec<u8> {
        self.source.bytes(offset.offset, length)
    }

    pub fn bytes_backwards(&self, offset: FileOffset, length: usize) -> Vec<u8> {
        self.source.bytes_backwards(offset.offset, length)
    }

    /// Returns the offset of the next logical character.
    ///
    /// The size of a character depends on the chosen encoding and can be non-constant.
    pub fn next_char_offset(&self, current_offset: FileOffset) -> FileOffset {
        self.encoding.next_char_offset(self, current_offset)
    }

    /// Returns the offset of the previous logical character.
    ///
    /// The size of a character depends on the chosen encoding and can be non-constant.
    pub fn previous_char_offset(&self, current_offset: FileOffset) -> FileOffset {
        self.encoding.previous_char_offset(self, current_offset)
    }

    /// Moves the offset forward by the given number of logical characters.
    pub fn move_forward_by(&self, current_offset: FileOffset, n: i64) -> FileOffset {
        self.encoding.move_forward_by(self, current_offset, n)
    }

    /// Moves the offset backward by the given number of logical characters.
    pub fn move_backward_by(&self, current_offset: FileOffset, n: i64) -> FileOffset {
        self.encoding.move_backward_by(self, current_offset, n)
    }

    pub fn offsets(&self, start: FileOffset, end: FileOffset) -> impl Iterator<Item = FileOffset> {
        std::iter::successors(Some(start), move |offset| {
            (*offset < end)
                .then(|| self.next_char_offset(*offset))
                .filter(|next_offset| *next_offset < end)
        })
    }

    /// Returns the character size if an encoding with a fixed character size is selected.
    pub fn char_size(&self) -> Option<u32> {
        self.encoding.char_size()
    }

    /// Checks whether a character starts at the given offset. This will always return `true` for
    /// single-byte encodings.
    pub fn is_at_char_boundary(&self, offset: FileOffset) -> bool {
        self.encoding.is_at_char_boundary(self, offset)
    }

    /// If not currently at a character start, finds next character start. Otherwise returns the
    /// offset unchanged.
    pub fn next_char_boundary(&self, offset: FileOffset) -> FileOffset {
        self.encoding.next_char_boundary(self, offset)
    }

    /// If not currently at a character start, finds previous character start. Otherwise returns the
    /// offset unchanged.
    pub fn previous_char_boundary(&self, offset: FileOffset) -> FileOffset {
        self.encoding.previous_char_boundary(self, offset)
    }

    /// For fixed-width encodings this shifts the offset backward towards a line start for lines of
    /// given fixed number of characters. For variable-width encoding the offset is left unchanged.
    pub fn align_offset_to_line_length(&self, offset: FileOffset, line_length: i64) -> FileOffset {
        self.encoding
            .align_offset_to_line_length(offset, line_length)
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
        const DATA: &[u8] = &[];
        InputMode::new(DATA.into())
    }
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

trait CharacterEncoding {
    /// For fixed-width encodings, this should return the size of a character in bytes. For
    /// variable-width encodings `None`` is returned.
    fn char_size(&self) -> Option<u32>;

    /// Decodes the character at the given offset.
    fn char_at(&self, input_mode: &InputMode, offset: FileOffset) -> Option<char>;

    /// Determines the offset of the character after the given offset. Note: the offset isn’t
    /// necessarily aligned with the character boundary.
    fn next_char_offset(&self, input_mode: &InputMode, current_offset: FileOffset) -> FileOffset;

    /// Determines the offset of the character before the given offset. Note: the offset isn’t
    /// necessarily aligned with the character boundary.
    fn previous_char_offset(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
    ) -> FileOffset;

    /// Moves offset forward by the given number of characters.
    fn move_forward_by(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
        n: i64,
    ) -> FileOffset;

    /// Moves offset backward by the given number of characters.
    fn move_backward_by(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
        n: i64,
    ) -> FileOffset;

    /// Checks whether the offset is aligned with the character boundary.
    fn is_at_char_boundary(&self, input_mode: &InputMode, offset: FileOffset) -> bool;

    /// Aligns the offset to character boundary, going forward.
    fn next_char_boundary(&self, input_mode: &InputMode, offset: FileOffset) -> FileOffset;

    /// Aligns the offset to character boundary, going backwards.
    fn previous_char_boundary(&self, input_mode: &InputMode, offset: FileOffset) -> FileOffset;

    /// If possible, shifts the offset backward to align it with the line start for lines with given
    /// fixed number of characters.
    fn align_offset_to_line_length(&self, offset: FileOffset, line_length: i64) -> FileOffset;

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

    pub fn allow_newlines(&mut self) {
        self.translation_table[b'\n' as usize] = '\n';
        self.translation_table[b'\r' as usize] = '\r';
    }
}

impl CharacterEncoding for SingleByteEncoding {
    fn char_size(&self) -> Option<u32> {
        Some(1)
    }

    fn char_at(&self, input_mode: &InputMode, offset: FileOffset) -> Option<char> {
        input_mode
            .raw_byte(offset)
            .map(|byte| self.translation_table[byte as usize])
    }

    fn next_char_offset(&self, input_mode: &InputMode, current_offset: FileOffset) -> FileOffset {
        self.move_forward_by(input_mode, current_offset, 1)
    }

    fn previous_char_offset(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
    ) -> FileOffset {
        self.move_backward_by(input_mode, current_offset, 1)
    }

    fn move_forward_by(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
        n: i64,
    ) -> FileOffset {
        current_offset.add(n, input_mode.size())
    }

    fn move_backward_by(
        &self,
        _input_mode: &InputMode,
        current_offset: FileOffset,
        n: i64,
    ) -> FileOffset {
        current_offset.sub(n)
    }

    fn is_at_char_boundary(&self, _input_mode: &InputMode, _offset: FileOffset) -> bool {
        true
    }

    fn next_char_boundary(&self, _input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        offset
    }

    fn previous_char_boundary(&self, _input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        offset
    }

    fn align_offset_to_line_length(&self, offset: FileOffset, line_length: i64) -> FileOffset {
        if line_length <= 1 {
            offset
        } else {
            FileOffset::new(offset.offset / line_length * line_length)
        }
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
    fn char_size(&self) -> Option<u32> {
        None
    }

    fn char_at(&self, input_mode: &InputMode, offset: FileOffset) -> Option<char> {
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

    fn next_char_offset(&self, input_mode: &InputMode, current_offset: FileOffset) -> FileOffset {
        // Rust supports at most four bytes long UTF-8 sequences, try decoding.
        let bytes = input_mode.bytes(current_offset, 4);
        for len in 1..=4 {
            if std::str::from_utf8(&bytes[..len]).is_ok() {
                return current_offset.add(len as i64, input_mode.size());
            }
        }
        current_offset.inc(input_mode.size())
    }

    fn previous_char_offset(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
    ) -> FileOffset {
        // Rust supports at most four bytes long UTF-8 sequences, try decoding.
        let mut bytes = input_mode.bytes_backwards(current_offset, 5);
        while bytes.len() < 5 {
            bytes.push(0);
        }
        bytes.reverse();
        bytes.pop();
        for len in 1..=4 {
            if std::str::from_utf8(&bytes[4 - len..4]).is_ok() {
                return current_offset.sub(len as i64);
            }
        }
        current_offset.dec()
    }

    fn move_forward_by(
        &self,
        input_mode: &InputMode,
        mut current_offset: FileOffset,
        n: i64,
    ) -> FileOffset {
        for _ in 0..n {
            current_offset = self.next_char_offset(input_mode, current_offset);
        }
        current_offset
    }

    fn move_backward_by(
        &self,
        input_mode: &InputMode,
        mut current_offset: FileOffset,
        n: i64,
    ) -> FileOffset {
        for _ in 0..n {
            current_offset = self.previous_char_offset(input_mode, current_offset);
        }
        current_offset
    }

    fn next_char_boundary(&self, input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        // Skip at most 3 continuation bytes
        let mut result = offset;
        for _ in 0..3 {
            if input_mode
                .raw_byte(result)
                .is_none_or(|byte| !Self::is_continuation(byte))
            {
                break;
            }
            result = result.inc(input_mode.size())
        }
        result
    }

    fn previous_char_boundary(&self, input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        // Skip at most 3 continuation bytes
        let mut result = offset;
        for _ in 0..3 {
            if input_mode
                .raw_byte(result)
                .is_none_or(|byte| !Self::is_continuation(byte))
            {
                break;
            }
            result = result.dec();
        }
        result
    }

    fn is_at_char_boundary(&self, input_mode: &InputMode, offset: FileOffset) -> bool {
        !input_mode
            .raw_byte(offset)
            .is_some_and(Self::is_continuation)
    }

    fn align_offset_to_line_length(&self, offset: FileOffset, _line_length: i64) -> FileOffset {
        offset
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn offset(value: i64) -> FileOffset {
        FileOffset::new(value)
    }

    #[test]
    fn test_decode_cp437() {
        let data: &[u8] = &[0x00, 0x03, 0x0A, 0x20, 0x41, 0x80, 0xFE];
        let mut imd = InputMode::new(data.into());
        imd.set_mode("CP437", false);
        assert_eq!(
            String::from_iter(imd.characters(offset(0), 7)),
            "\x00♥◙ AÇ■"
        );
    }

    #[test]
    fn test_decode_windows1251() {
        let data: &[u8] = &[0x00, 0x03, 0x0A, 0x20, 0x41, 0x80, 0xFE];
        let mut imd = InputMode::new(data.into());
        imd.set_mode("CP1251", false);
        assert_eq!(
            String::from_iter(imd.characters(offset(0), 7)),
            "\x00\x03\n AЂю"
        );
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
        let mut imd = InputMode::new(data.into());
        imd.set_mode("ASCII", true);

        assert_eq!(imd.char_size(), Some(1));

        assert_eq!(imd.character(offset(0)), Some('\0'));
        assert_eq!(imd.character(offset(1)), Some('\x03'));
        assert_eq!(imd.character(offset(2)), Some('\n'));
        assert_eq!(imd.character(offset(3)), Some(' '));
        assert_eq!(imd.character(offset(4)), Some('A'));
        assert_eq!(imd.character(offset(5)), Some('.'));
        assert_eq!(imd.character(offset(6)), Some('.'));
        assert_eq!(imd.character(offset(7)), None);

        assert_eq!(imd.next_char_offset(offset(0)), offset(1));
        assert_eq!(imd.next_char_offset(offset(5)), offset(6));
        assert_eq!(imd.next_char_offset(offset(6)), offset(7));
        assert_eq!(imd.next_char_offset(offset(7)), offset(7));

        assert_eq!(imd.previous_char_offset(offset(0)), offset(0));
        assert_eq!(imd.previous_char_offset(offset(5)), offset(4));
        assert_eq!(imd.previous_char_offset(offset(6)), offset(5));
        assert_eq!(imd.previous_char_offset(offset(7)), offset(6));

        assert!(imd.is_at_char_boundary(offset(0)));
        assert!(imd.is_at_char_boundary(offset(5)));
        assert!(imd.is_at_char_boundary(offset(6)));
        assert!(imd.is_at_char_boundary(offset(7)));

        assert_eq!(imd.next_char_boundary(offset(0)), offset(0));
        assert_eq!(imd.next_char_boundary(offset(5)), offset(5));
        assert_eq!(imd.next_char_boundary(offset(6)), offset(6));
        assert_eq!(imd.next_char_boundary(offset(7)), offset(7));

        assert_eq!(imd.previous_char_boundary(offset(0)), offset(0));
        assert_eq!(imd.previous_char_boundary(offset(5)), offset(5));
        assert_eq!(imd.previous_char_boundary(offset(6)), offset(6));
        assert_eq!(imd.previous_char_boundary(offset(7)), offset(7));
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
        let mut imd = InputMode::new(data.into());
        imd.set_mode("UTF8", false);

        assert_eq!(imd.char_size(), None);

        assert_eq!(imd.character(offset(0)), Some('\0'));
        assert_eq!(imd.character(offset(1)), Some('\n'));
        assert_eq!(imd.character(offset(2)), Some(' '));
        assert_eq!(imd.character(offset(3)), Some('A'));
        assert_eq!(imd.character(offset(4)), Some('\x7f'));
        assert_eq!(imd.character(offset(5)), Some('.'));
        assert_eq!(imd.character(offset(6)), Some('т'));
        assert_eq!(imd.character(offset(7)), Some('.'));
        assert_eq!(imd.character(offset(8)), Some('\u{D7FF}'));
        assert_eq!(imd.character(offset(9)), Some('.'));
        assert_eq!(imd.character(offset(10)), Some('.'));
        assert_eq!(imd.character(offset(11)), Some('\u{10FFFF}'));
        assert_eq!(imd.character(offset(12)), Some('.'));
        assert_eq!(imd.character(offset(13)), Some('.'));
        assert_eq!(imd.character(offset(14)), Some('.'));
        assert_eq!(imd.character(offset(15)), Some('.'));
        assert_eq!(imd.character(offset(16)), Some('.'));
        assert_eq!(imd.character(offset(17)), Some('.'));
        assert_eq!(imd.character(offset(18)), Some('"'));
        assert_eq!(imd.character(offset(19)), None); // out of bounds

        assert_eq!(imd.next_char_offset(offset(0)), offset(1));
        assert_eq!(imd.next_char_offset(offset(1)), offset(2));
        assert_eq!(imd.next_char_offset(offset(2)), offset(3));
        assert_eq!(imd.next_char_offset(offset(3)), offset(4));
        assert_eq!(imd.next_char_offset(offset(4)), offset(5));
        assert_eq!(imd.next_char_offset(offset(5)), offset(6));
        assert_eq!(imd.next_char_offset(offset(6)), offset(8));
        assert_eq!(imd.next_char_offset(offset(8)), offset(11));
        assert_eq!(imd.next_char_offset(offset(11)), offset(15));
        assert_eq!(imd.next_char_offset(offset(15)), offset(16));
        assert_eq!(imd.next_char_offset(offset(16)), offset(17));
        assert_eq!(imd.next_char_offset(offset(17)), offset(18));
        assert_eq!(imd.next_char_offset(offset(18)), offset(19));
        assert_eq!(imd.next_char_offset(offset(19)), offset(19));

        assert_eq!(imd.previous_char_offset(offset(0)), offset(0));
        assert_eq!(imd.previous_char_offset(offset(1)), offset(0));
        assert_eq!(imd.previous_char_offset(offset(2)), offset(1));
        assert_eq!(imd.previous_char_offset(offset(3)), offset(2));
        assert_eq!(imd.previous_char_offset(offset(4)), offset(3));
        assert_eq!(imd.previous_char_offset(offset(5)), offset(4));
        assert_eq!(imd.previous_char_offset(offset(6)), offset(5));
        assert_eq!(imd.previous_char_offset(offset(8)), offset(6));
        assert_eq!(imd.previous_char_offset(offset(11)), offset(8));
        assert_eq!(imd.previous_char_offset(offset(15)), offset(11));
        assert_eq!(imd.previous_char_offset(offset(16)), offset(15));
        assert_eq!(imd.previous_char_offset(offset(17)), offset(16));
        assert_eq!(imd.previous_char_offset(offset(18)), offset(17));
        assert_eq!(imd.previous_char_offset(offset(19)), offset(18));

        assert!(imd.is_at_char_boundary(offset(0)));
        assert!(imd.is_at_char_boundary(offset(1)));
        assert!(imd.is_at_char_boundary(offset(2)));
        assert!(imd.is_at_char_boundary(offset(3)));
        assert!(imd.is_at_char_boundary(offset(4)));
        assert!(!imd.is_at_char_boundary(offset(5)));
        assert!(imd.is_at_char_boundary(offset(6)));
        assert!(!imd.is_at_char_boundary(offset(7)));
        assert!(imd.is_at_char_boundary(offset(8)));
        assert!(!imd.is_at_char_boundary(offset(9)));
        assert!(!imd.is_at_char_boundary(offset(10)));
        assert!(imd.is_at_char_boundary(offset(11)));
        assert!(!imd.is_at_char_boundary(offset(12)));
        assert!(!imd.is_at_char_boundary(offset(13)));
        assert!(!imd.is_at_char_boundary(offset(14)));
        assert!(imd.is_at_char_boundary(offset(15)));
        assert!(!imd.is_at_char_boundary(offset(16)));
        assert!(!imd.is_at_char_boundary(offset(17)));
        assert!(imd.is_at_char_boundary(offset(18)));
        assert!(imd.is_at_char_boundary(offset(19)));

        assert_eq!(imd.next_char_boundary(offset(0)), offset(0));
        assert_eq!(imd.next_char_boundary(offset(1)), offset(1));
        assert_eq!(imd.next_char_boundary(offset(2)), offset(2));
        assert_eq!(imd.next_char_boundary(offset(3)), offset(3));
        assert_eq!(imd.next_char_boundary(offset(4)), offset(4));
        assert_eq!(imd.next_char_boundary(offset(5)), offset(6));
        assert_eq!(imd.next_char_boundary(offset(6)), offset(6));
        assert_eq!(imd.next_char_boundary(offset(7)), offset(8));
        assert_eq!(imd.next_char_boundary(offset(8)), offset(8));
        assert_eq!(imd.next_char_boundary(offset(9)), offset(11));
        assert_eq!(imd.next_char_boundary(offset(10)), offset(11));
        assert_eq!(imd.next_char_boundary(offset(11)), offset(11));
        assert_eq!(imd.next_char_boundary(offset(12)), offset(15));
        assert_eq!(imd.next_char_boundary(offset(13)), offset(15));
        assert_eq!(imd.next_char_boundary(offset(14)), offset(15));
        assert_eq!(imd.next_char_boundary(offset(15)), offset(15));
        assert_eq!(imd.next_char_boundary(offset(16)), offset(18));
        assert_eq!(imd.next_char_boundary(offset(17)), offset(18));
        assert_eq!(imd.next_char_boundary(offset(18)), offset(18));
        assert_eq!(imd.next_char_boundary(offset(19)), offset(19));

        assert_eq!(imd.previous_char_boundary(offset(0)), offset(0));
        assert_eq!(imd.previous_char_boundary(offset(1)), offset(1));
        assert_eq!(imd.previous_char_boundary(offset(2)), offset(2));
        assert_eq!(imd.previous_char_boundary(offset(3)), offset(3));
        assert_eq!(imd.previous_char_boundary(offset(4)), offset(4));
        assert_eq!(imd.previous_char_boundary(offset(5)), offset(4));
        assert_eq!(imd.previous_char_boundary(offset(6)), offset(6));
        assert_eq!(imd.previous_char_boundary(offset(7)), offset(6));
        assert_eq!(imd.previous_char_boundary(offset(8)), offset(8));
        assert_eq!(imd.previous_char_boundary(offset(9)), offset(8));
        assert_eq!(imd.previous_char_boundary(offset(10)), offset(8));
        assert_eq!(imd.previous_char_boundary(offset(11)), offset(11));
        assert_eq!(imd.previous_char_boundary(offset(12)), offset(11));
        assert_eq!(imd.previous_char_boundary(offset(13)), offset(11));
        assert_eq!(imd.previous_char_boundary(offset(14)), offset(11));
        assert_eq!(imd.previous_char_boundary(offset(15)), offset(15));
        assert_eq!(imd.previous_char_boundary(offset(16)), offset(15));
        assert_eq!(imd.previous_char_boundary(offset(17)), offset(15));
        assert_eq!(imd.previous_char_boundary(offset(18)), offset(18));
        assert_eq!(imd.previous_char_boundary(offset(19)), offset(19));
    }
}
