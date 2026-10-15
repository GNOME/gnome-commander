// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::input_modes::{FileOffset, InputMode};

// We don't allow really unwrapped lines in NoWrap mode for performance reasons, merely long ones.
const NOWRAP_LIMIT: u32 = 4096;

pub struct DataPresentation {
    mode: DataPresentationMode,
    wrap_limit: u32,
    tab_size: u32,
    fixed_count: u32,
}

impl DataPresentation {
    pub fn new() -> Self {
        Self {
            mode: DataPresentationMode::NoWrap,
            wrap_limit: 80,
            tab_size: 8,
            fixed_count: 16,
        }
    }

    #[inline]
    pub fn mode(&self) -> DataPresentationMode {
        self.mode
    }

    #[inline]
    pub fn set_mode(&mut self, mode: DataPresentationMode) {
        self.mode = mode;
    }

    pub fn align_offset_to_line_start(
        &self,
        input_mode: &InputMode,
        offset: FileOffset,
    ) -> FileOffset {
        match self.mode() {
            DataPresentationMode::NoWrap => {
                self.wrap_align_offset(input_mode, NOWRAP_LIMIT, offset)
            }
            DataPresentationMode::Wrap => {
                self.wrap_align_offset(input_mode, self.wrap_limit, offset)
            }
            DataPresentationMode::Fixed => self.fixed_align_offset(input_mode, offset),
            DataPresentationMode::Binary => self.bin_align_offset(input_mode, offset),
        }
    }

    pub fn scroll_lines(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
        delta: i32,
    ) -> FileOffset {
        match self.mode() {
            DataPresentationMode::NoWrap => {
                self.wrap_scroll_lines(input_mode, NOWRAP_LIMIT, current_offset, delta)
            }
            DataPresentationMode::Wrap => {
                self.wrap_scroll_lines(input_mode, self.wrap_limit, current_offset, delta)
            }
            DataPresentationMode::Fixed => {
                self.fixed_scroll_lines(input_mode, current_offset, delta, false)
            }
            DataPresentationMode::Binary => {
                self.bin_scroll_lines(input_mode, current_offset, delta)
            }
        }
    }

    pub fn end_of_line_offset(
        &self,
        input_mode: &InputMode,
        start_of_line: FileOffset,
    ) -> FileOffset {
        match self.mode() {
            DataPresentationMode::NoWrap => {
                self.wrap_get_eol(input_mode, NOWRAP_LIMIT, start_of_line)
            }
            DataPresentationMode::Wrap => {
                self.wrap_get_eol(input_mode, self.wrap_limit, start_of_line)
            }
            DataPresentationMode::Fixed => self.fixed_get_eol(input_mode, start_of_line),
            DataPresentationMode::Binary => self.bin_get_eol(input_mode, start_of_line),
        }
    }

    pub fn set_tab_size(&mut self, tab_size: u32) {
        self.tab_size = tab_size;
    }

    pub fn set_fixed_count(&mut self, chars_per_line: u32) {
        self.fixed_count = chars_per_line;
    }

    pub fn set_wrap_limit(&mut self, chars_per_line: u32) {
        self.wrap_limit = chars_per_line;
    }

    /// Scans the file from offset "start" backwards until a CR/LF/CRLF is found.
    /// Returns the offset of the previous CR/LF/CRLF, or None if start of the file is reached.
    fn find_previous_crlf(&self, input_mode: &InputMode, start: FileOffset) -> Option<FileOffset> {
        let mut offset = input_mode.previous_char_offset(start);
        loop {
            match input_mode.character(offset) {
                None => return None,
                Some('\r') => return Some(offset),
                Some('\n') => {
                    let prev_offset = input_mode.previous_char_offset(offset);
                    if input_mode.character(prev_offset) == Some('\r') {
                        // Treat CRLF combination as a single EOL
                        return Some(prev_offset);
                    } else {
                        return Some(offset);
                    }
                }
                _ => {}
            }
            if offset == FileOffset::default() {
                return None;
            }
            offset = input_mode.previous_char_offset(offset);
        }
    }

    /// Scans the file backwards starting from offset to find the start of the line.
    fn nowrap_align_offset(&self, input_mode: &InputMode, start: FileOffset) -> FileOffset {
        let mut offset = start;
        while offset != FileOffset::default() {
            let prev_offset = input_mode.previous_char_offset(offset);
            match input_mode.character(prev_offset) {
                None => break,
                Some('\n') => return offset,
                // Treat CRLF combination as a single EOL
                Some('\r') if input_mode.character(offset) != Some('\n') => {
                    return offset;
                }
                _ => {}
            }
            offset = prev_offset;
        }
        offset
    }

    /// returns the start offset of the previous line,
    /// with special handling for wrap mode.
    fn find_previous_wrapped_text_line(
        &self,
        input_mode: &InputMode,
        wrap_limit: u32,
        start: FileOffset,
    ) -> FileOffset {
        // step 1: find start offset of previous text line
        let mut offset = self
            .find_previous_crlf(input_mode, start)
            .map(|t| self.nowrap_align_offset(input_mode, t))
            .unwrap_or_default();

        // Step 2
        loop {
            let next_line_offset = self.wrap_get_eol(input_mode, wrap_limit, offset);

            // this is the line we want: When the next line's offset is the current
            // offset ('start' parameter), 'offset' will point to the previous
            // displayable line

            if next_line_offset >= start {
                return offset;
            }

            offset = next_line_offset;
        }
    }

    pub fn wrap_align_offset(
        &self,
        input_mode: &InputMode,
        wrap_limit: u32,
        offset: FileOffset,
    ) -> FileOffset {
        let mut line_start = self.nowrap_align_offset(input_mode, offset);
        let mut temp = line_start;
        while temp <= offset {
            line_start = temp;
            temp = self.wrap_scroll_lines(input_mode, wrap_limit, temp, 1);
            if temp == line_start {
                break;
            }
        }
        line_start
    }

    fn wrap_scroll_lines(
        &self,
        input_mode: &InputMode,
        wrap_limit: u32,
        mut current_offset: FileOffset,
        mut delta: i32,
    ) -> FileOffset {
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
                self.wrap_get_eol(input_mode, wrap_limit, current_offset)
            } else {
                self.find_previous_wrapped_text_line(input_mode, wrap_limit, current_offset)
            };

            // Offset didn't change? we've reached eof
            if temp == current_offset {
                break;
            }

            current_offset = temp;
        }
        current_offset
    }

    fn wrap_get_eol(
        &self,
        input_mode: &InputMode,
        wrap_limit: u32,
        start_of_line: FileOffset,
    ) -> FileOffset {
        /* A Single TAB character in the file,
        Translates to several displayable characters on the screen.
        We need to take that into account when calculating number of
        characters before wraping the line */
        let mut char_count = 0;

        let mut offset = start_of_line;

        while let Some(value) = input_mode.raw_byte(offset) {
            offset = input_mode.next_char_offset(offset);

            // break upon end of line
            if value == b'\n' {
                break;
            } else if value == b'\r' {
                // Treat \r\n combination as a single EOL
                if let Some(value) = input_mode.raw_byte(offset)
                    && value == b'\n'
                {
                    offset = input_mode.next_char_offset(offset);
                }
                break;
            } else if value == b'\t' {
                char_count = next_tab_position(char_count, self.tab_size);
            } else {
                char_count += 1;
            }

            if char_count >= wrap_limit {
                break;
            }
        }
        offset
    }

    fn fixed_align_offset(&self, _input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        // We cannot align the offset short of scanning the entire file, just take the current
        // offset as line start.
        offset
    }

    fn fixed_scroll_lines(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
        delta: i32,
        past_last_line: bool,
    ) -> FileOffset {
        let fixed_count = self.fixed_count.max(1);
        if delta < 0 {
            input_mode.move_backward_by(current_offset, fixed_count as u64 * (-delta) as u64)
        } else {
            // Slightly more complicated logic when scrolling forward because we usually don't
            // want to "scroll" past the start of the last line: it will cause the content to
            // shift around.
            let size = input_mode.size();
            let mut line_offset = current_offset;
            for _ in 0..delta {
                let next_line = input_mode.move_forward_by(line_offset, fixed_count as u64);
                if !past_last_line && next_line >= size {
                    break;
                }
                line_offset = next_line;
            }
            line_offset
        }
    }

    fn fixed_get_eol(&self, input_mode: &InputMode, start_of_line: FileOffset) -> FileOffset {
        self.fixed_scroll_lines(input_mode, start_of_line, 1, true)
    }

    fn bin_align_offset(&self, input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        input_mode.align_offset_to_line_length(offset, self.fixed_count as u64)
    }

    fn bin_scroll_lines(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
        delta: i32,
    ) -> FileOffset {
        let fixed_count = self.fixed_count.max(1);
        let offset = self.bin_align_offset(input_mode, current_offset);
        if delta > 0 {
            input_mode.move_forward_by(offset, fixed_count as u64 * delta as u64)
        } else {
            input_mode.move_backward_by(offset, fixed_count as u64 * (-delta) as u64)
        }
    }

    fn bin_get_eol(&self, input_mode: &InputMode, start_of_line: FileOffset) -> FileOffset {
        self.bin_scroll_lines(input_mode, start_of_line, 1)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum DataPresentationMode {
    #[default]
    NoWrap,
    Wrap,
    /// fixed number of characters per line.
    Fixed,
    /// fixed number of binary characters per line.
    /// e.g. CHAR=BYTE, no UTF-8 or other translations
    Binary,
}

pub fn next_tab_position(column: u32, tab_size: u32) -> u32 {
    (column + tab_size) / tab_size * tab_size
}

#[cfg(test)]
mod data_presentation_test {
    use super::super::{input_modes::InputSourceExt, input_source::InputSource};
    use super::*;

    #[test]
    fn set_data_presentation_mode() {
        let mut dp = DataPresentation::new();
        for mode in [
            DataPresentationMode::Wrap,
            DataPresentationMode::NoWrap,
            DataPresentationMode::Fixed,
            DataPresentationMode::Binary,
        ] {
            dp.set_mode(mode);
            assert_eq!(dp.mode(), mode);
        }
    }

    const ABRACADABRA: &[u8] = b"\na\r\nab\nabra\r\nabrac\nabraca\r\nabracad\nabracada\r\nabracadab\nabracadabr\r\nabracadabra\n";
    const ABRACADABRA2: &[u8] = "abracardabra\nкадабра".as_bytes();

    fn offset(value: u64) -> FileOffset {
        InputSource::from(ABRACADABRA).get_file_offset(value)
    }

    #[test]
    fn nowrap() {
        let mut imd = InputMode::new(ABRACADABRA.into());
        imd.set_mode("CP437", true);
        let mut dp = DataPresentation::new();
        dp.set_mode(DataPresentationMode::NoWrap);

        assert_eq!(dp.nowrap_align_offset(&imd, offset(0)), offset(0));
        assert_eq!(dp.nowrap_align_offset(&imd, offset(1)), offset(1));
        assert_eq!(dp.nowrap_align_offset(&imd, offset(2)), offset(1));
        assert_eq!(dp.nowrap_align_offset(&imd, offset(3)), offset(1));
        assert_eq!(dp.nowrap_align_offset(&imd, offset(4)), offset(4));
        assert_eq!(dp.nowrap_align_offset(&imd, offset(6)), offset(4));
        assert_eq!(dp.nowrap_align_offset(&imd, offset(6)), offset(4));
        assert_eq!(dp.nowrap_align_offset(&imd, offset(7)), offset(7));

        assert_eq!(dp.scroll_lines(&imd, offset(0), 1), offset(1));
        assert_eq!(dp.scroll_lines(&imd, offset(1), 1), offset(4));
        assert_eq!(dp.scroll_lines(&imd, offset(4), 1), offset(7));

        assert_eq!(dp.scroll_lines(&imd, offset(7), -1), offset(4));
        assert_eq!(dp.scroll_lines(&imd, offset(4), -1), offset(1));
        assert_eq!(dp.scroll_lines(&imd, offset(1), -1), offset(0));
    }

    #[test]
    fn wrap() {
        let mut imd = InputMode::new(ABRACADABRA2.into());
        imd.set_mode("UTF8", true);
        let mut dp = DataPresentation::new();
        dp.set_mode(DataPresentationMode::Wrap);
        dp.set_wrap_limit(5);

        assert_eq!(dp.scroll_lines(&imd, offset(0), 1), offset(5));
        assert_eq!(dp.scroll_lines(&imd, offset(5), 1), offset(10));
        assert_eq!(dp.scroll_lines(&imd, offset(10), 1), offset(13));
        assert_eq!(dp.scroll_lines(&imd, offset(13), 1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), 1), offset(27));
        assert_eq!(dp.scroll_lines(&imd, offset(27), 1), offset(27));

        assert_eq!(dp.scroll_lines(&imd, offset(27), -1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), -1), offset(13));
        assert_eq!(dp.scroll_lines(&imd, offset(13), -1), offset(10));
        assert_eq!(dp.scroll_lines(&imd, offset(10), -1), offset(5));
        assert_eq!(dp.scroll_lines(&imd, offset(5), -1), offset(0));
        assert_eq!(dp.scroll_lines(&imd, offset(0), -1), offset(0));
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
