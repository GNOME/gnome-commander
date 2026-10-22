// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::{gio, gio::prelude::*};
use std::cell::RefCell;

enum FileData {
    Buffer(RefCell<FileBuffer>),
    Static(&'static [u8]),
}

pub struct InputSource {
    file: gio::File,
    data: FileData,
}

impl From<&'static [u8]> for InputSource {
    fn from(data: &'static [u8]) -> Self {
        Self {
            file: gio::File::for_uri(""),
            data: FileData::Static(data),
        }
    }
}

impl InputSource {
    pub fn open(file: &gio::File) -> Result<Self, glib::Error> {
        Ok(Self {
            file: file.clone(),
            data: FileData::Buffer(RefCell::new(FileBuffer::new(file)?)),
        })
    }

    pub fn file(&self) -> &gio::File {
        &self.file
    }

    pub fn size(&self) -> i64 {
        match &self.data {
            FileData::Buffer(buffer) => buffer.borrow_mut().size().unwrap_or_default(),
            FileData::Static(data) => data.len() as i64,
        }
    }

    pub fn byte(&self, offset: i64) -> Option<u8> {
        match &self.data {
            FileData::Buffer(buffer) => buffer.borrow_mut().byte(offset).ok(),
            FileData::Static(data) => data.get(offset as usize).copied(),
        }
    }

    pub fn bytes(&self, offset: i64, length: usize) -> Vec<u8> {
        std::iter::successors(Some(offset), move |offset| offset.checked_add(1))
            .take(length)
            .map(|offset| self.byte(offset).unwrap_or_default())
            .collect()
    }

    pub fn bytes_backwards(&self, offset: i64, length: usize) -> Vec<u8> {
        std::iter::successors(Some(offset), move |offset| offset.checked_sub(1))
            .take(length)
            .map(|offset| self.byte(offset).unwrap_or_default())
            .collect()
    }
}

pub struct FileBuffer {
    stream: gio::FileInputStream,
    cache_offset: i64,
    cache: Vec<u8>,
}

impl FileBuffer {
    const PAGE_SIZE: usize = 0x2000;
    const MAX_EXTRA_PAGES: usize = 2;

    pub fn new(file: &gio::File) -> Result<Self, glib::Error> {
        Ok(Self {
            stream: file.read(gio::Cancellable::NONE)?,
            cache_offset: 0,
            cache: Vec::new(),
        })
    }

    pub fn size(&self) -> Result<i64, glib::Error> {
        self.stream
            .seek(0, glib::SeekType::End, gio::Cancellable::NONE)?;
        Ok(self.stream.tell())
    }

    pub fn byte(&mut self, offset: i64) -> Result<u8, glib::Error> {
        self.ensure_cached(offset)?;
        Ok(self.cache[(offset - self.cache_offset) as usize])
    }

    fn ensure_cached(&mut self, offset: i64) -> Result<(), glib::Error> {
        if offset < self.cache_offset || offset >= self.cache_offset + self.cache.len() as i64 {
            let page_start = offset / Self::PAGE_SIZE as i64 * Self::PAGE_SIZE as i64;
            if page_start >= self.cache_offset
                && page_start < self.cache_offset + self.cache.len() as i64
            {
                // We should have that offset in cache already but last page is incomplete - EOF
                return Err(glib::Error::new(glib::FileError::Failed, "EOF"));
            }

            self.stream
                .seek(page_start, glib::SeekType::Set, gio::Cancellable::NONE)?;
            let mut page = self
                .stream
                .read_bytes(Self::PAGE_SIZE, gio::Cancellable::NONE)?
                .to_vec();
            if page_start + page.len() as i64 == self.cache_offset {
                self.cache.truncate(Self::PAGE_SIZE * Self::MAX_EXTRA_PAGES);
                page.append(&mut self.cache);
                self.cache = page;
                self.cache_offset = page_start;
            } else if self.cache_offset + self.cache.len() as i64 == page_start {
                if self.cache.len() > Self::PAGE_SIZE * Self::MAX_EXTRA_PAGES {
                    let extra_elements = self.cache.len() - Self::PAGE_SIZE * Self::MAX_EXTRA_PAGES;
                    self.cache.drain(0..extra_elements);
                    self.cache_offset += extra_elements as i64;
                }
                self.cache.append(&mut page);
            } else {
                self.cache = page;
                self.cache_offset = page_start;
            }

            if offset >= self.cache_offset + self.cache.len() as i64 {
                // We read the page but it is incomplete - EOF
                return Err(glib::Error::new(glib::FileError::Failed, "EOF"));
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn get_byte_does_read() {
        let path = std::fs::canonicalize("../AUTHORS").unwrap();
        let fops = InputSource::open(&gio::File::for_path(path)).unwrap();

        let end = fops.size();

        for current in 0..end {
            let value = fops.byte(current);
            assert!(value.is_some());
        }
    }
}
