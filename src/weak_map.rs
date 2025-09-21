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

use bimap::BiHashMap;
use gtk::glib::prelude::*;
use std::cell::RefCell;

use crate::weak_ref::IdentityWeakRef;

pub struct WeakMap<K: ObjectType, V: ObjectType> {
    mappings: RefCell<BiHashMap<IdentityWeakRef<K>, IdentityWeakRef<V>>>,
}

impl<K: ObjectType, V: ObjectType> Default for WeakMap<K, V> {
    fn default() -> Self {
        Self {
            mappings: Default::default(),
        }
    }
}

impl<K: ObjectType, V: ObjectType> WeakMap<K, V> {
    pub fn add(&self, key: &K, value: &V) {
        self.clean_expired();
        let present = self
            .mappings
            .borrow()
            .iter()
            .any(|p| p.0 == key && p.1 == value);
        if !present {
            self.mappings
                .borrow_mut()
                .insert(IdentityWeakRef::new(key), IdentityWeakRef::new(value));
        }
    }

    pub fn remove_key(&self, key: &K) {
        self.clean_expired();
        self.mappings
            .borrow_mut()
            .remove_by_left(&IdentityWeakRef::new(key));
    }

    pub fn remove_value(&self, value: &V) {
        self.clean_expired();
        self.mappings
            .borrow_mut()
            .remove_by_right(&IdentityWeakRef::new(value));
    }

    pub fn find_by_key(&self, key: &K) -> Option<V> {
        self.clean_expired();
        self.mappings
            .borrow()
            .get_by_left(&IdentityWeakRef::new(key))
            .and_then(|p| p.upgrade())
    }

    pub fn find_by_value(&self, value: &V) -> Option<K> {
        self.clean_expired();
        self.mappings
            .borrow()
            .get_by_right(&IdentityWeakRef::new(value))
            .and_then(|p| p.upgrade())
    }

    fn clean_expired(&self) {
        self.mappings
            .borrow_mut()
            .retain(|k, v| k.upgrade().is_some() && v.upgrade().is_some());
    }
}
