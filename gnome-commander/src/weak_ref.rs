// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::{glib, glib::prelude::*};
use std::sync::{
    LazyLock,
    atomic::{AtomicU64, Ordering},
};

#[derive(Clone, Debug)]
pub struct IdentityWeakRef<T: ObjectType> {
    id: u64,
    weak_ref: glib::WeakRef<T>,
}

static ID_QUARK: LazyLock<glib::Quark> = LazyLock::new(|| glib::Quark::from_str("WeakRefId"));

fn object_id<T: ObjectType>(obj: &T) -> Option<u64> {
    unsafe {
        let existing_id = obj.qdata::<u64>(*ID_QUARK)?;
        Some(*(existing_id.as_ref()))
    }
}

fn set_object_id<T: ObjectType>(obj: &T) -> u64 {
    static SEQ: AtomicU64 = AtomicU64::new(1);

    unsafe {
        match obj.qdata::<u64>(*ID_QUARK) {
            Some(existing_id) => *(existing_id.as_ref()),
            None => {
                let new_id = SEQ.fetch_add(1, Ordering::SeqCst);
                obj.set_qdata(*ID_QUARK, new_id);
                new_id
            }
        }
    }
}

impl<T: ObjectType> IdentityWeakRef<T> {
    pub fn new(obj: &T) -> Self {
        let id = set_object_id(obj);
        Self {
            id,
            weak_ref: obj.downgrade(),
        }
    }

    pub fn upgrade(&self) -> Option<T> {
        self.weak_ref.upgrade()
    }
}

impl<T: ObjectType> PartialEq for IdentityWeakRef<T> {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T: ObjectType> PartialEq<T> for IdentityWeakRef<T> {
    fn eq(&self, other: &T) -> bool {
        Some(self.id) == object_id(other)
    }
}

impl<T: ObjectType> Eq for IdentityWeakRef<T> {}

impl<T: ObjectType> std::hash::Hash for IdentityWeakRef<T> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.id.hash(state);
    }
}
