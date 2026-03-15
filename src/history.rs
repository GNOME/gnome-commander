// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::cell::{Cell, RefCell};

pub struct History<T> {
    max: usize,
    entries: RefCell<Vec<T>>,
    position: Cell<usize>,
}

impl<T: PartialEq + Clone> History<T> {
    pub fn new(max: usize) -> Self {
        Self {
            max,
            entries: Default::default(),
            position: Default::default(),
        }
    }

    pub fn add(&self, item: T) {
        if self.current().as_ref() == Some(&item) {
            // ignore addition due to history navigation
            return;
        }

        let mut entries = self.entries.borrow_mut();
        if let Some(position) = entries.iter().position(|i| i == &item) {
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

    fn current(&self) -> Option<T> {
        self.entries.borrow().get(self.position.get()).cloned()
    }

    pub fn set_current(&self, item: T) {
        if let Some(position) = self.entries.borrow().iter().position(|i| i == &item) {
            self.position.set(position);
        }
    }

    pub fn set_current_position(&self, position: usize) {
        if position < self.entries.borrow().len() {
            self.position.set(position);
        }
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

    pub fn get(&self, index: usize) -> Option<T> {
        self.entries.borrow().get(index).cloned()
    }

    pub fn export(&self) -> Vec<T> {
        self.entries.borrow().clone()
    }
}
