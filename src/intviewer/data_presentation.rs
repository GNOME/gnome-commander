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

use crate::intviewer::input_modes::InputMode;
use std::{cell::Cell, sync::Arc};

pub struct DataPresentation {
    input_mode: Arc<InputMode>,
    mode: Cell<DataPresentationMode>,
    wrap_limit: Cell<u32>,
    tab_size: Cell<u32>,
    fixed_count: Cell<u32>,
}

impl DataPresentation {
    pub fn new(input_mode: &Arc<InputMode>) -> Self {
        Self {
            input_mode: input_mode.clone(),
            mode: Cell::new(DataPresentationMode::NoWrap),
            wrap_limit: Cell::new(80),
            tab_size: Cell::new(8),
            fixed_count: Cell::new(16),
        }
    }

    pub fn mode(&self) -> DataPresentationMode {
        self.mode.get()
    }

    pub fn set_mode(&self, mode: DataPresentationMode) {
        self.mode.set(mode)
    }

    pub fn align_offset_to_line_start(&self, offset: u64) -> u64 {
        match self.mode() {
            DataPresentationMode::NoWrap => self.nowrap_align_offset(offset),
            DataPresentationMode::Wrap => self.wrap_align_offset(offset),
            DataPresentationMode::BinaryFixed => self.binfixed_align_offset(offset),
        }
    }

    pub fn scroll_lines(&self, current_offset: u64, delta: i32) -> u64 {
        match self.mode() {
            DataPresentationMode::NoWrap => self.nowrap_scroll_lines(current_offset, delta),
            DataPresentationMode::Wrap => self.wrap_scroll_lines(current_offset, delta),
            DataPresentationMode::BinaryFixed => self.binfixed_scroll_lines(current_offset, delta),
        }
    }

    pub fn end_of_line_offset(&self, start_of_line: u64) -> u64 {
        match self.mode() {
            DataPresentationMode::NoWrap => self.nowrap_get_eol(start_of_line),
            DataPresentationMode::Wrap => self.wrap_get_eol(start_of_line),
            DataPresentationMode::BinaryFixed => self.binfixed_get_eol(start_of_line),
        }
    }

    pub fn set_tab_size(&self, tab_size: u32) {
        self.tab_size.set(tab_size);
    }

    pub fn set_fixed_count(&self, chars_per_line: u32) {
        self.fixed_count.set(chars_per_line);
    }

    pub fn set_wrap_limit(&self, chars_per_line: u32) {
        self.wrap_limit.set(chars_per_line);
    }

    /// scans the file from offset "start" backwards, until a CR/LF is found.
    /// returns the offset of the previous CR/LF, or 0 (if we've reached the start of the file)
    fn find_previous_crlf(&self, start: u64) -> Option<u64> {
        let mut offset = start;
        while offset > 0 {
            offset = self.input_mode.previous_char_offset(offset);
            match self.input_mode.character(offset) {
                None => return None,
                Some('\n') | Some('\r') => return Some(offset),
                _ => {}
            }
        }
        None
    }

    fn nowrap_align_offset(&self, mut offset: u64) -> u64 {
        while offset > 0 {
            let prev_offset = self.input_mode.previous_char_offset(offset);
            match self.input_mode.character(prev_offset) {
                None => return 0,
                Some('\n') | Some('\r') => {
                    return offset;
                }
                _ => offset = prev_offset,
            }
        }
        0
    }

    fn nowrap_scroll_lines(&self, mut current_offset: u64, mut delta: i32) -> u64 {
        if delta == 0 {
            return current_offset;
        }

        let mut forward = true;
        if delta < 0 {
            delta = -delta;
            forward = false;
        }

        for _ in 0..delta {
            let temp;
            if forward {
                temp = self.nowrap_get_eol(current_offset);
            } else {
                let Some(temp1) = self
                    .find_previous_crlf(current_offset)
                    .map(|t| self.nowrap_align_offset(t))
                else {
                    break;
                };
                temp = temp1;
            }

            // Offset didn't changed ? we've reached eof
            if temp == current_offset {
                break;
            }

            current_offset = temp;
        }
        current_offset
    }

    fn nowrap_get_eol(&self, start_of_line: u64) -> u64 {
        let mut offset = start_of_line;
        loop {
            let Some(value) = self.input_mode.character(offset) else {
                break;
            };

            offset = self.input_mode.next_char_offset(offset);

            // break upon end of line
            if value == '\n' || value == '\r' {
                break;
            }
        }
        offset
    }

    /// returns the start offset of the previous line,
    /// with special handling for wrap mode.
    fn find_previous_wrapped_text_line(&self, start: u64) -> u64 {
        // step 1: find start offset of previous text line
        let Some(mut offset) = self
            .find_previous_crlf(start)
            .map(|t| self.nowrap_align_offset(t))
        else {
            return start;
        };

        // Step 2
        loop {
            let next_line_offset = self.wrap_get_eol(offset);

            // this is the line we want: When the next line's offset is the current
            // offset ('start' parameter), 'offset' will point to the previous
            // displayable line

            if next_line_offset >= start {
                return offset;
            }

            offset = next_line_offset;
        }
    }

    pub fn wrap_align_offset(&self, offset: u64) -> u64 {
        let mut line_start = self.nowrap_align_offset(offset);
        let mut temp = line_start;
        while temp <= offset {
            line_start = temp;
            temp = self.wrap_scroll_lines(temp, 1)
        }
        return line_start;
    }

    fn wrap_scroll_lines(&self, mut current_offset: u64, mut delta: i32) -> u64 {
        if delta == 0 {
            return current_offset;
        }

        let mut forward = true;
        if delta < 0 {
            delta = -delta;
            forward = false;
        }

        for _ in 0..delta {
            let temp = if forward {
                self.wrap_get_eol(current_offset)
            } else {
                self.find_previous_wrapped_text_line(current_offset)
            };

            // Offset didn't change? we've reached eof
            if temp == current_offset {
                break;
            }

            current_offset = temp;
        }
        current_offset
    }

    fn wrap_get_eol(&self, start_of_line: u64) -> u64 {
        /* A Single TAB character in the file,
        Translates to several displayable characters on the screen.
        We need to take that into account when calculating number of
        characters before wraping the line */
        let mut char_count = 0;

        let mut offset = start_of_line;

        loop {
            let Some(value) = self.input_mode.character(offset) else {
                break;
            };

            offset = self.input_mode.next_char_offset(offset);

            // break upon end of line
            if value == '\n' || value == '\r' {
                break;
            } else if value == '\t' {
                char_count = next_tab_position(char_count, self.tab_size.get());
            } else {
                char_count += 1;
            }

            if char_count >= self.wrap_limit.get() {
                break;
            }
        }
        offset
    }

    fn binfixed_align_offset(&self, offset: u64) -> u64 {
        let max_offset = self.input_mode.max_offset();
        let fixed_count = self.fixed_count.get().max(1) as u64;
        offset.clamp(0, max_offset) / fixed_count * fixed_count
    }

    fn binfixed_scroll_lines(&self, current_offset: u64, delta: i32) -> u64 {
        let fixed_count = self.fixed_count.get().max(1);
        (self
            .binfixed_align_offset(current_offset)
            .saturating_add_signed(fixed_count as i64 * delta as i64))
        .clamp(0, self.input_mode.max_offset())
    }

    fn binfixed_get_eol(&self, start_of_line: u64) -> u64 {
        self.binfixed_scroll_lines(start_of_line, 1)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, strum::FromRepr, Default)]
#[repr(C)]
pub enum DataPresentationMode {
    #[default]
    NoWrap = 0,
    Wrap,
    /// fixed number of binary characters per line.
    /// e.g. CHAR=BYTE, no UTF-8 or other translations
    BinaryFixed,
}

pub fn next_tab_position(column: u32, tab_size: u32) -> u32 {
    (column + tab_size) / tab_size * tab_size
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn set_data_presentation_mode() {
        let source: &[u8] = &[];
        let imd = Arc::new(InputMode::new(source));
        let dp = DataPresentation::new(&imd);
        for mode in [
            DataPresentationMode::Wrap,
            DataPresentationMode::NoWrap,
            DataPresentationMode::BinaryFixed,
        ] {
            dp.set_mode(mode);
            assert_eq!(dp.mode(), mode);
        }
    }

    const ABRACADABRA: &[u8] = br#"
a
ab
abra
abrac
abraca
abracad
abracada
abracadab
abracadabr
abracadabra
"#;

    #[test]
    fn nowrap() {
        let imd = Arc::new(InputMode::new(ABRACADABRA));
        let dp = DataPresentation::new(&imd);
        dp.set_mode(DataPresentationMode::NoWrap);

        assert_eq!(dp.nowrap_align_offset(0), 0);
        assert_eq!(dp.nowrap_align_offset(1), 1);
        assert_eq!(dp.nowrap_align_offset(2), 1);
        assert_eq!(dp.nowrap_align_offset(3), 3);
        assert_eq!(dp.nowrap_align_offset(4), 3);
        assert_eq!(dp.nowrap_align_offset(5), 3);
        assert_eq!(dp.nowrap_align_offset(6), 6);

        assert_eq!(dp.scroll_lines(0, 1), 1);
        assert_eq!(dp.scroll_lines(1, 1), 3);
        assert_eq!(dp.scroll_lines(3, 1), 6);

        assert_eq!(dp.scroll_lines(6, -1), 3);
        assert_eq!(dp.scroll_lines(3, -1), 1);
        assert_eq!(dp.scroll_lines(1, -1), 0);
    }

    #[test]
    fn test_next_tab_position() {
        assert_eq!(next_tab_position(0, 8), 8);
        assert_eq!(next_tab_position(1, 8), 8);
        assert_eq!(next_tab_position(7, 8), 8);
        assert_eq!(next_tab_position(8, 8), 16);
        assert_eq!(next_tab_position(9, 8), 16);
    }
}
