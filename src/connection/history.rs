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

use std::cell::{Cell, RefCell};

pub struct History<T> {
    max: usize,
    is_locked: Cell<bool>,
    entries: RefCell<Vec<T>>,
    position: Cell<usize>,
}

impl<T: PartialEq + Clone> History<T> {
    pub fn new(max: usize) -> Self {
        Self {
            max,
            is_locked: Default::default(),
            entries: Default::default(),
            position: Default::default(),
        }
    }

    pub fn add(&self, item: T) {
        if self.is_locked() {
            return;
        }

        let mut entries = self.entries.borrow_mut();
        if let Some(position) = entries.iter().position(|i| *i == item) {
            // if the same value has been given before move it first in the list
            let item = entries.remove(position);
            entries.insert(0, item);
        } else {
            // or if its new just add it
            entries.insert(0, item);
        }
        // don't let the history get too long
        entries.truncate(self.max);
        self.position.set(0);
    }

    pub fn is_locked(&self) -> bool {
        self.is_locked.get()
    }

    pub fn lock(&self) {
        self.is_locked.set(true);
    }

    pub fn unlock(&self) {
        self.is_locked.set(false);
    }

    fn current(&self) -> Option<T> {
        self.entries.borrow().get(self.position.get()).cloned()
    }

    pub fn can_back(&self) -> bool {
        self.position.get() + 1 < self.entries.borrow().len()
    }

    pub fn can_forward(&self) -> bool {
        self.position.get() > 0
    }

    pub fn first(&self) -> Option<T> {
        let len = self.entries.borrow().len();
        if len == 0 {
            return None;
        }
        self.position.set(len - 1);
        self.current()
    }

    pub fn back(&self) -> Option<T> {
        let len = self.entries.borrow().len();
        if len == 0 {
            return None;
        }

        let position = self.position.get();
        if position + 1 < len {
            self.position.set(position + 1);
            self.current()
        } else {
            None
        }
    }

    pub fn forward(&self) -> Option<T> {
        if self.entries.borrow().is_empty() {
            return None;
        }

        let position = self.position.get();
        if position > 0 {
            self.position.set(position - 1);
            self.current()
        } else {
            None
        }
    }

    pub fn last(&self) -> Option<T> {
        if self.entries.borrow().is_empty() {
            return None;
        }
        self.position.set(0);
        self.current()
    }

    pub fn export(&self) -> Vec<T> {
        self.entries.borrow().clone()
    }
}
