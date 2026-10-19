// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::input_modes::{FileOffset, InputMode};
use std::{cell::RefCell, collections::VecDeque};

/// We don't allow really unwrapped lines in NoWrap mode for performance reasons, merely long ones.
const NOWRAP_LIMIT: u32 = 4096;

/// As long as the offset is within this difference to a known line we will fill up lines. For
/// larger differences we will reset the cache and build it from scratch.
const MAX_OFFSET_DIFF: f64 = 16384.0;

#[derive(Debug, Default)]
struct LineOffsetsCache {
    offsets: VecDeque<FileOffset>,
}

impl LineOffsetsCache {
    /// Maximum number of cache entries
    pub const MAX_SIZE: usize = 512;

    pub fn line_for(&self, offset: FileOffset) -> Result<(FileOffset, FileOffset), bool> {
        let index = match self.offsets.binary_search(&offset) {
            Ok(index) => index + 1,
            Err(index) => index,
        };
        if index >= self.offsets.len() {
            Err(true)
        } else {
            Ok((
                self.offsets[index.checked_sub(1).ok_or(false)?],
                self.offsets[index],
            ))
        }
    }

    pub fn first_line_start(&self) -> FileOffset {
        self.offsets.front().copied().unwrap_or_default()
    }

    pub fn last_line_start(&self) -> FileOffset {
        self.offsets.back().copied().unwrap_or_default()
    }

    pub fn push_front(&mut self, line_start: FileOffset) {
        self.offsets.push_front(line_start);
        while self.offsets.len() > Self::MAX_SIZE {
            self.offsets.pop_back();
        }
    }

    pub fn push_back(&mut self, line_start: FileOffset) {
        self.offsets.push_back(line_start);
        while self.offsets.len() > Self::MAX_SIZE {
            self.offsets.pop_front();
        }
    }

    pub fn clear(&mut self) {
        self.offsets.clear();
    }
}

#[derive(Debug)]
pub struct DataPresentation {
    mode: DataPresentationMode,
    wrap_limit: u32,
    tab_size: u32,
    fixed_count: u32,
    cache: RefCell<LineOffsetsCache>,
}

impl DataPresentation {
    pub fn new() -> Self {
        Self {
            mode: DataPresentationMode::NoWrap,
            wrap_limit: 80,
            tab_size: 8,
            fixed_count: 16,
            cache: Default::default(),
        }
    }

    /// To be called when input mode changes, ensures that all line offsets are recalculated.
    pub fn clear_cache(&self) {
        self.cache.borrow_mut().clear();
    }

    #[inline]
    pub fn mode(&self) -> DataPresentationMode {
        self.mode
    }

    #[inline]
    pub fn set_mode(&mut self, mode: DataPresentationMode) {
        self.mode = mode;
        self.clear_cache();
    }

    pub fn align_offset_to_line_start(
        &self,
        input_mode: &InputMode,
        offset: FileOffset,
    ) -> FileOffset {
        self.line_for(input_mode, offset)
            .map(|(start, _)| start)
            .unwrap_or(offset)
    }

    pub fn end_of_line_offset(&self, input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        self.line_for(input_mode, offset)
            .map(|(_, end)| end)
            .unwrap_or(offset)
    }

    pub fn scroll_lines(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
        delta: i32,
    ) -> FileOffset {
        let (mut start, mut end) = self
            .line_for(input_mode, current_offset)
            .unwrap_or((current_offset, current_offset));
        if delta < 0 {
            for _ in 0..-delta {
                (start, _) = match self.line_for(input_mode, input_mode.previous_char_offset(start))
                {
                    Some(line) => line,
                    None => return start,
                };
            }
        } else {
            for _ in 0..delta {
                (start, end) = match self.line_for(input_mode, end) {
                    Some(line) => line,
                    None => return end,
                }
            }
        }
        start
    }

    fn line_for(
        &self,
        input_mode: &InputMode,
        offset: FileOffset,
    ) -> Option<(FileOffset, FileOffset)> {
        let mut cache = self.cache.borrow_mut();
        match cache.line_for(offset) {
            Ok(line) => return Some(line),
            Err(forward) => {
                // Offset outside cached bounds
                let mut line_start = if forward {
                    cache.last_line_start()
                } else {
                    cache.first_line_start()
                };
                if (line_start - offset).abs() <= MAX_OFFSET_DIFF {
                    loop {
                        let (next_line, additional_offsets) = if forward {
                            (self.next_line(input_mode, line_start), None)
                        } else {
                            self.previous_line(input_mode, line_start)
                        };
                        if next_line == line_start {
                            // Reached file boundary
                            return None;
                        }
                        if forward {
                            cache.push_back(next_line);
                        } else {
                            if next_line == FileOffset::default()
                                && self.mode == DataPresentationMode::Fixed
                            {
                                // We’ve reached start of file in Fixed mode, but we might not have
                                // a complete line. Clear cache and recalculate next line offset.
                                cache.clear();
                                line_start = self.next_line(input_mode, next_line);
                                cache.push_front(line_start);
                            }
                            cache.push_front(next_line);
                            if let Some(additional_offsets) = additional_offsets {
                                // Limit number so that our current line isn't evicted from cache
                                for offset in additional_offsets
                                    .into_iter()
                                    .rev()
                                    .take(LineOffsetsCache::MAX_SIZE - 2)
                                {
                                    cache.push_front(offset);
                                }
                            }
                        }
                        if forward && offset < next_line {
                            return Some((line_start, next_line));
                        } else if !forward && offset >= next_line {
                            return Some((next_line, line_start));
                        }
                        line_start = next_line;
                    }
                }
            }
        }

        cache.clear();
        let start = self.align_offset_to_line_start_nocache(input_mode, offset);
        let end = self.next_line(input_mode, start);
        cache.push_back(start);
        cache.push_back(end);
        Some((start, end))
    }

    fn align_offset_to_line_start_nocache(
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

    fn next_line(&self, input_mode: &InputMode, current_offset: FileOffset) -> FileOffset {
        match self.mode() {
            DataPresentationMode::NoWrap => {
                self.wrap_next_line(input_mode, NOWRAP_LIMIT, current_offset)
            }
            DataPresentationMode::Wrap => {
                self.wrap_next_line(input_mode, self.wrap_limit, current_offset)
            }
            DataPresentationMode::Fixed => self.fixed_next_line(input_mode, current_offset),
            DataPresentationMode::Binary => self.bin_next_line(input_mode, current_offset),
        }
    }

    fn previous_line(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
    ) -> (FileOffset, Option<Vec<FileOffset>>) {
        match self.mode() {
            DataPresentationMode::NoWrap => {
                self.wrap_previous_line(input_mode, NOWRAP_LIMIT, current_offset)
            }
            DataPresentationMode::Wrap => {
                self.wrap_previous_line(input_mode, self.wrap_limit, current_offset)
            }
            DataPresentationMode::Fixed => {
                (self.fixed_previous_line(input_mode, current_offset), None)
            }
            DataPresentationMode::Binary => {
                (self.bin_previous_line(input_mode, current_offset), None)
            }
        }
    }

    pub fn set_tab_size(&mut self, tab_size: u32) {
        self.tab_size = tab_size;
        self.clear_cache();
    }

    pub fn set_fixed_count(&mut self, chars_per_line: u32) {
        self.fixed_count = chars_per_line;
        self.clear_cache();
    }

    pub fn set_wrap_limit(&mut self, chars_per_line: u32) {
        self.wrap_limit = chars_per_line;
        self.clear_cache();
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
            temp = self.wrap_next_line(input_mode, wrap_limit, temp);
            if temp == line_start {
                break;
            }
        }
        line_start
    }

    fn wrap_next_line(
        &self,
        input_mode: &InputMode,
        wrap_limit: u32,
        current_offset: FileOffset,
    ) -> FileOffset {
        // A single TAB character in the file translates to several displayable characters on the
        // screen. We need to take that into account when calculating number of characters before
        // wrapping the line.
        let mut char_count = 0;

        let mut offset = current_offset;

        while let Some(value) = input_mode.character(offset) {
            offset = input_mode.next_char_offset(offset);

            // break upon end of line
            if value == '\n' {
                break;
            } else if value == '\r' {
                // Treat \r\n combination as a single EOL
                if let Some(value) = input_mode.character(offset)
                    && value == '\n'
                {
                    offset = input_mode.next_char_offset(offset);
                }
                break;
            } else if value == '\t' {
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

    fn wrap_previous_line(
        &self,
        input_mode: &InputMode,
        wrap_limit: u32,
        current_offset: FileOffset,
    ) -> (FileOffset, Option<Vec<FileOffset>>) {
        // Step 1: find start offset of previous text line
        let mut offset = self
            .find_previous_crlf(input_mode, current_offset)
            .map(|t| self.nowrap_align_offset(input_mode, t))
            .unwrap_or_default();

        // Step 2: go through wrapped chunks of that line until we find the right one
        let mut additional_offsets = Vec::new();
        loop {
            let next_line_offset = self.wrap_next_line(input_mode, wrap_limit, offset);

            // this is the line we want: When the next line's offset is the current
            // offset ('current_offset' parameter), 'offset' will point to the previous
            // displayable line

            if next_line_offset >= current_offset {
                return (offset, Some(additional_offsets));
            } else {
                additional_offsets.push(offset);
            }

            offset = next_line_offset;
        }
    }

    fn fixed_align_offset(&self, _input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        // We cannot align the offset short of scanning the entire file, just take the current
        // offset as line start.
        offset
    }

    fn fixed_next_line(&self, input_mode: &InputMode, current_offset: FileOffset) -> FileOffset {
        let fixed_count = self.fixed_count.max(1);
        input_mode.move_forward_by(current_offset, fixed_count as u64)
    }

    fn fixed_previous_line(
        &self,
        input_mode: &InputMode,
        current_offset: FileOffset,
    ) -> FileOffset {
        let fixed_count = self.fixed_count.max(1);
        input_mode.move_backward_by(current_offset, fixed_count as u64)
    }

    fn bin_align_offset(&self, input_mode: &InputMode, offset: FileOffset) -> FileOffset {
        input_mode.align_offset_to_line_length(offset, self.fixed_count as u64)
    }

    fn bin_next_line(&self, input_mode: &InputMode, current_offset: FileOffset) -> FileOffset {
        let fixed_count = self.fixed_count.max(1);
        input_mode.move_forward_by(current_offset, fixed_count as u64)
    }

    fn bin_previous_line(&self, input_mode: &InputMode, current_offset: FileOffset) -> FileOffset {
        let fixed_count = self.fixed_count.max(1);
        input_mode.move_backward_by(current_offset, fixed_count as u64)
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

        assert_eq!(dp.align_offset_to_line_start(&imd, offset(0)), offset(0));
        assert_eq!(dp.align_offset_to_line_start(&imd, offset(1)), offset(1));
        assert_eq!(dp.align_offset_to_line_start(&imd, offset(2)), offset(1));
        assert_eq!(dp.align_offset_to_line_start(&imd, offset(3)), offset(1));
        assert_eq!(dp.align_offset_to_line_start(&imd, offset(4)), offset(4));
        assert_eq!(dp.align_offset_to_line_start(&imd, offset(6)), offset(4));
        assert_eq!(dp.align_offset_to_line_start(&imd, offset(6)), offset(4));
        assert_eq!(dp.align_offset_to_line_start(&imd, offset(7)), offset(7));

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
    fn wrap_backwards() {
        let mut imd = InputMode::new(ABRACADABRA2.into());
        imd.set_mode("UTF8", true);
        let mut dp = DataPresentation::new();
        dp.set_mode(DataPresentationMode::Wrap);
        dp.set_wrap_limit(5);

        assert_eq!(dp.scroll_lines(&imd, offset(27), -1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), -1), offset(13));
        assert_eq!(dp.scroll_lines(&imd, offset(13), -1), offset(10));
        assert_eq!(dp.scroll_lines(&imd, offset(10), -1), offset(5));
        assert_eq!(dp.scroll_lines(&imd, offset(5), -1), offset(0));
        assert_eq!(dp.scroll_lines(&imd, offset(0), -1), offset(0));

        assert_eq!(dp.scroll_lines(&imd, offset(0), 1), offset(5));
        assert_eq!(dp.scroll_lines(&imd, offset(5), 1), offset(10));
        assert_eq!(dp.scroll_lines(&imd, offset(10), 1), offset(13));
        assert_eq!(dp.scroll_lines(&imd, offset(13), 1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), 1), offset(27));
        assert_eq!(dp.scroll_lines(&imd, offset(27), 1), offset(27));
    }

    #[test]
    fn fixed() {
        let mut imd = InputMode::new(ABRACADABRA2.into());
        imd.set_mode("UTF8", true);
        let mut dp = DataPresentation::new();
        dp.set_mode(DataPresentationMode::Fixed);
        dp.set_fixed_count(6);

        assert_eq!(dp.scroll_lines(&imd, offset(0), 1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), 1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), 1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), 1), offset(27));
        assert_eq!(dp.scroll_lines(&imd, offset(27), 1), offset(27));

        assert_eq!(dp.scroll_lines(&imd, offset(27), -1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), -1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), -1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), -1), offset(0));
        assert_eq!(dp.scroll_lines(&imd, offset(0), -1), offset(0));
    }

    #[test]
    fn fixed_backwards() {
        let mut imd = InputMode::new(ABRACADABRA2.into());
        imd.set_mode("UTF8", true);
        let mut dp = DataPresentation::new();
        dp.set_mode(DataPresentationMode::Fixed);
        dp.set_fixed_count(6);

        assert_eq!(dp.scroll_lines(&imd, offset(27), -1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), -1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), -1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), -1), offset(0));
        assert_eq!(dp.scroll_lines(&imd, offset(0), -1), offset(0));

        assert_eq!(dp.scroll_lines(&imd, offset(0), 1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), 1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), 1), offset(23));
        assert_eq!(dp.scroll_lines(&imd, offset(23), 1), offset(27));
        assert_eq!(dp.scroll_lines(&imd, offset(27), 1), offset(27));
    }

    #[test]
    fn binary() {
        let mut imd = InputMode::new(ABRACADABRA2.into());
        imd.set_mode("ASCII", true);
        let mut dp = DataPresentation::new();
        dp.set_mode(DataPresentationMode::Binary);
        dp.set_fixed_count(6);

        assert_eq!(dp.scroll_lines(&imd, offset(0), 1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), 1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), 1), offset(18));
        assert_eq!(dp.scroll_lines(&imd, offset(18), 1), offset(24));
        assert_eq!(dp.scroll_lines(&imd, offset(24), 1), offset(27));
        assert_eq!(dp.scroll_lines(&imd, offset(27), 1), offset(27));

        assert_eq!(dp.scroll_lines(&imd, offset(27), -1), offset(24));
        assert_eq!(dp.scroll_lines(&imd, offset(24), -1), offset(18));
        assert_eq!(dp.scroll_lines(&imd, offset(18), -1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), -1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), -1), offset(0));
        assert_eq!(dp.scroll_lines(&imd, offset(0), -1), offset(0));
    }

    #[test]
    fn binary_backwards() {
        let mut imd = InputMode::new(ABRACADABRA2.into());
        imd.set_mode("ASCII", true);
        let mut dp = DataPresentation::new();
        dp.set_mode(DataPresentationMode::Binary);
        dp.set_fixed_count(6);

        assert_eq!(dp.scroll_lines(&imd, offset(27), -1), offset(24));
        assert_eq!(dp.scroll_lines(&imd, offset(24), -1), offset(18));
        assert_eq!(dp.scroll_lines(&imd, offset(18), -1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), -1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), -1), offset(0));
        assert_eq!(dp.scroll_lines(&imd, offset(0), -1), offset(0));

        assert_eq!(dp.scroll_lines(&imd, offset(0), 1), offset(6));
        assert_eq!(dp.scroll_lines(&imd, offset(6), 1), offset(12));
        assert_eq!(dp.scroll_lines(&imd, offset(12), 1), offset(18));
        assert_eq!(dp.scroll_lines(&imd, offset(18), 1), offset(24));
        assert_eq!(dp.scroll_lines(&imd, offset(24), 1), offset(27));
        assert_eq!(dp.scroll_lines(&imd, offset(27), 1), offset(27));
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
