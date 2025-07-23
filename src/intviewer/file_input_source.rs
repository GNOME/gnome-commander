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

use crate::{debug::debug, intviewer::input_modes::InputSource};
use memmap2::Mmap;
use std::{
    fs,
    io::{self, Read, Seek},
    path::Path,
    sync::Mutex,
};

pub enum FileInputSource {
    Buffer(Mutex<FileBuffer>),
    Mmap(Mmap),
}

impl FileInputSource {
    pub fn open(path: &Path) -> io::Result<Self> {
        let file = fs::File::open(path)?;

        let mmap = unsafe { Mmap::map(&file) };
        Ok(match mmap {
            Ok(mmap) => Self::Mmap(mmap),
            Err(error) => {
                debug!('v', "Cannot mmap a file '{}': {}", path.display(), error);
                Self::Buffer(Mutex::new(FileBuffer::new(file)))
            }
        })
    }
}

impl InputSource for FileInputSource {
    fn max_offset(&self) -> u64 {
        match self {
            Self::Buffer(buffer) => buffer.lock().unwrap().size().unwrap_or_default(),
            Self::Mmap(mmap) => mmap.len() as u64,
        }
    }

    fn byte(&self, offset: u64) -> Option<u8> {
        match self {
            Self::Buffer(buffer) => buffer.lock().unwrap().byte(offset).ok(),
            Self::Mmap(mmap) => mmap.get(offset as usize).copied(),
        }
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
        let total_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
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
        let file_path = PathBuf::from("./TODO");
        let fops = FileInputSource::open(&file_path).unwrap();

        let end = fops.max_offset();

        for current in 0..end {
            let value = fops.byte(current);
            assert!(value.is_some());
        }
    }
}
