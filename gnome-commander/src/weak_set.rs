// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::weak_ref::IdentityWeakRef;
use gtk::glib::prelude::*;
use std::{cell::RefCell, collections::HashSet};

pub struct WeakSet<T: ObjectType> {
    weak_refs: RefCell<HashSet<IdentityWeakRef<T>>>,
}

impl<T: ObjectType> WeakSet<T> {
    pub fn contains(&self, fl: &T) -> bool {
        self.weak_refs.borrow().iter().any(|t| t == fl)
    }

    pub fn insert(&self, obj: &T) {
        self.clean_expired();
        self.weak_refs
            .borrow_mut()
            .insert(IdentityWeakRef::new(obj));
    }

    pub fn remove(&self, obj: &T) {
        self.clean_expired();
        self.weak_refs.borrow_mut().retain(|t| t != obj);
    }

    fn clean_expired(&self) {
        self.weak_refs
            .borrow_mut()
            .retain(|t| t.upgrade().is_some());
    }
}

impl<T: ObjectType> Default for WeakSet<T> {
    fn default() -> Self {
        Self {
            weak_refs: Default::default(),
        }
    }
}
