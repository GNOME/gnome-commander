// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::debug::debug;
use memmap2::Mmap;
use std::{
    cell::RefCell,
    fs,
    io::{self, Read, Seek},
    path::{Path, PathBuf},
};

enum FileData {
    Buffer(RefCell<FileBuffer>),
    Mmap(Mmap),
    Static(&'static [u8]),
}

pub struct InputSource {
    path: PathBuf,
    data: FileData,
}

impl From<&'static [u8]> for InputSource {
    fn from(data: &'static [u8]) -> Self {
        Self {
            path: Default::default(),
            data: FileData::Static(data),
        }
    }
}

impl InputSource {
    pub fn open(path: &Path) -> io::Result<Self> {
        let file = fs::File::open(path)?;

        let mmap = unsafe { Mmap::map(&file) };
        Ok(Self {
            path: path.to_path_buf(),
            data: match mmap {
                Ok(mmap) => FileData::Mmap(mmap),
                Err(_error) => {
                    debug!('v', "Cannot mmap a file '{}': {}", path.display(), _error);
                    FileData::Buffer(RefCell::new(FileBuffer::new(file)))
                }
            },
        })
    }

    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn size(&self) -> u64 {
        match &self.data {
            FileData::Buffer(buffer) => buffer.borrow_mut().size().unwrap_or_default(),
            FileData::Mmap(mmap) => mmap.len() as u64,
            FileData::Static(data) => data.len() as u64,
        }
    }

    pub fn byte(&self, offset: u64) -> Option<u8> {
        match &self.data {
            FileData::Buffer(buffer) => buffer.borrow_mut().byte(offset).ok(),
            FileData::Mmap(mmap) => mmap.get(offset as usize).copied(),
            FileData::Static(data) => data.get(offset as usize).copied(),
        }
    }

    pub fn bytes(&self, offset: u64, length: usize) -> Vec<u8> {
        std::iter::successors(Some(offset), move |offset| offset.checked_add(1))
            .take(length)
            .map(|offset| self.byte(offset).unwrap_or_default())
            .collect()
    }

    pub fn bytes_backwards(&self, offset: u64, length: usize) -> Vec<u8> {
        std::iter::successors(Some(offset), move |offset| offset.checked_sub(1))
            .take(length)
            .map(|offset| self.byte(offset).unwrap_or_default())
            .collect()
    }
}

const PAGE_SIZE: u64 = 8192;

pub struct FileBuffer {
    file: fs::File,
    pages: Vec<Option<Vec<u8>>>,
}

impl FileBuffer {
    pub fn new(file: fs::File) -> Self {
        Self {
            file,
            pages: Default::default(),
        }
    }

    pub fn size(&mut self) -> io::Result<u64> {
        self.file.seek(io::SeekFrom::End(0))?;
        let position = self.file.stream_position()?;
        Ok(position)
    }

    pub fn byte(&mut self, offset: u64) -> io::Result<u8> {
        let page = self.page((offset / PAGE_SIZE) as usize)?;
        let byte = page
            .get((offset % PAGE_SIZE) as usize)
            .ok_or_else(|| io::Error::from(io::ErrorKind::UnexpectedEof))?;
        Ok(*byte)
    }

    fn page(&mut self, page: usize) -> io::Result<&[u8]> {
        let size = self.size()?;
        let total_pages = size.div_ceil(PAGE_SIZE);
        let pages = &mut self.pages;
        if pages.len() < page + 1 {
            pages.resize(page + 1, None);
        }
        if pages[page].is_none() {
            self.file
                .seek(io::SeekFrom::Start(page as u64 * PAGE_SIZE))?;

            let page_size = if page as u64 + 1 == total_pages {
                size % PAGE_SIZE
            } else {
                PAGE_SIZE
            } as usize;
            let mut buf = vec![0; page_size];
            self.file.read_exact(&mut buf)?;
            pages[page] = Some(buf);
        }
        Ok(pages[page].as_ref().unwrap())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn get_byte_does_read() {
        let file_path = PathBuf::from("../AUTHORS");
        let fops = InputSource::open(&file_path).unwrap();

        let end = fops.size();

        for current in 0..end {
            let value = fops.byte(current);
            assert!(value.is_some());
        }
    }
}
