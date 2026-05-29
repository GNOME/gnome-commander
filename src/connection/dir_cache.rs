// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{debug::debug, dir::Directory, file::FileOps};
use std::collections::HashMap;

#[derive(Debug)]
struct DirCacheEntry {
    directory: Directory,
    pinned: usize,
}

#[derive(Debug)]
pub struct DirCache {
    unpinned_capacity: usize,
    unpinned_capacity_used: usize,
    map: HashMap<String, DirCacheEntry>,
    order: Vec<String>,
}

impl Default for DirCache {
    fn default() -> Self {
        let unpinned_capacity = Self::DEFAULT_CAPACITY;
        Self {
            unpinned_capacity,
            unpinned_capacity_used: 0,
            map: HashMap::with_capacity(unpinned_capacity),
            order: Vec::with_capacity(unpinned_capacity),
        }
    }
}

impl DirCache {
    const DEFAULT_CAPACITY: usize = 16;

    pub fn get(&self, uri: &str) -> Option<&Directory> {
        match self.map.get(uri) {
            Some(entry) => {
                debug!(
                    'k',
                    "FOUND {:?} {uri} in the cache, reusing it!", entry.directory
                );
                Some(&entry.directory)
            }
            None => {
                debug!('k', "FAILED to find {uri} in the cache");
                None
            }
        }
    }

    fn evict_oldest(&mut self) -> bool {
        for (i, key) in self.order.iter().enumerate() {
            if let Some(entry) = self.map.get(key)
                && entry.pinned == 0
            {
                debug!('k', "EVICTING {:?} {key} from the cache", entry.directory);
                self.map.remove(key);
                self.order.remove(i);
                self.unpinned_capacity_used -= 1;
                return true;
            }
        }
        false
    }

    pub fn insert(&mut self, directory: &Directory) {
        let uri = directory.uri();
        assert!(!self.map.contains_key(&uri));

        if self.unpinned_capacity == 0 {
            // We could add and immediately evict it again but let’s take a shortcut.
            return;
        }

        debug!('k', "ADDING {directory:?} {uri} to the cache");
        if self.unpinned_capacity_used >= self.unpinned_capacity {
            self.evict_oldest();
        }

        self.map.insert(
            uri.clone(),
            DirCacheEntry {
                directory: directory.clone(),
                pinned: 0,
            },
        );
        self.order.push(uri);
        self.unpinned_capacity_used += 1;
    }

    pub fn remove(&mut self, uri: &str) {
        if let Some(entry) = self.map.remove(uri) {
            debug!('k', "REMOVING {:?} {uri} from the cache", entry.directory);
            self.order.retain(|key| key != uri);
            if entry.pinned == 0 {
                self.unpinned_capacity_used -= 1;
            } else {
                // Processing of this signal will need a reference to the cache, yet we are already
                // running from a mutable reference. Run this asynchronously to prevent a conflict.
                let directory = entry.directory.clone();
                glib::spawn_future_local(async move {
                    directory.emit_deleted();
                });
            }
        }
    }

    pub fn remove_with_children(&mut self, uri: &str) {
        let removed: Vec<_> = self
            .order
            .iter()
            .filter(|key| key.starts_with(uri))
            .cloned()
            .collect();
        for key in removed {
            self.remove(&key);
        }
    }

    pub fn invalidate_all(&mut self) {
        for entry in self.map.values() {
            entry.directory.invalidate();
        }
    }

    pub fn pin(&mut self, directory: &Directory) {
        let uri = directory.uri();
        if let Some(entry) = self.map.get_mut(&uri) {
            debug!('k', "PINNING {:?} {uri} to the cache", entry.directory);
            if entry.pinned == 0 {
                self.unpinned_capacity_used -= 1;
            }
            entry.pinned += 1;
        } else {
            debug!('k', "ADDING AND PINNING {directory:?} {uri} to the cache");
            self.map.insert(
                uri.clone(),
                DirCacheEntry {
                    directory: directory.clone(),
                    pinned: 1,
                },
            );
            self.order.push(uri);
        }
    }

    pub fn unpin(&mut self, directory: &Directory) {
        let uri = directory.uri();
        if let Some(entry) = self.map.get_mut(&uri) {
            assert!(entry.pinned > 0);
            debug!('k', "UNPINNING {:?} {uri} from the cache", entry.directory);
            entry.pinned -= 1;
            if entry.pinned == 0 {
                self.unpinned_capacity_used += 1;
                self.order.retain(|key| key != &uri);
                self.order.push(uri.to_owned());

                if self.unpinned_capacity_used > self.unpinned_capacity {
                    self.evict_oldest();
                }
            }
        }
    }

    pub fn reduce_unpinned_capacity(&mut self, unpinned_capacity: usize) {
        while self.unpinned_capacity_used > unpinned_capacity && self.evict_oldest() {}
        self.unpinned_capacity = unpinned_capacity.max(self.unpinned_capacity_used);
        self.map.shrink_to(self.unpinned_capacity);
        self.order.shrink_to(self.unpinned_capacity);
    }

    pub fn rename(&mut self, old_uri: &str, uri: &str) {
        for key in self.order.iter_mut() {
            if key == old_uri {
                assert!(!self.map.contains_key(uri));
                *key = uri.to_string();
                if let Some(entry) = self.map.remove(old_uri) {
                    debug!(
                        'k',
                        "RENAMING {:?} in cache from {old_uri} to {uri}", entry.directory
                    );
                    self.map.insert(uri.to_owned(), entry);
                }

                // Remove any children, these are no longer valid.
                self.remove_with_children(old_uri);
                return;
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::connection::{Connection, ConnectionExt};
    use glib::prelude::*;
    use std::{cell::RefCell, rc::Rc};

    #[test]
    fn test_get_insert_evict() {
        let conn: Connection = glib::Object::builder().build();
        conn.dir_cache_mut().reduce_unpinned_capacity(2);

        assert_eq!(conn.dir_cache().get("file:///dir1"), None);

        let directory1 = Directory::new(&conn, "file:///dir1");
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));

        let directory2 = Directory::new(&conn, "file:///dir2");
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));

        // dir1 gets evicted because it is the oldest entry.
        let directory3 = Directory::new(&conn, "file:///dir3");
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));

        // dir2 is pinned, so we aren’t over capacity yet.
        directory2.pin_to_cache();
        let directory1 = Directory::new(&conn, "file:///dir1");
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));

        // dir2 is oldest but it is pinned, so dir3 gets evicted instead.
        let directory4 = Directory::new(&conn, "file:///dir4");
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir3"), None);
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));

        // No changes if dir2 is pinned/unpinned a second time.
        directory2.pin_to_cache();
        directory2.unpin_from_cache();
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));

        // Unpinning dir2 evicts oldest entry which is dir1 now.
        directory2.unpin_from_cache();
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));

        // Explicit remove allows for exactly one entry to be added.
        conn.dir_cache_mut().remove("file:///dir2");
        assert_eq!(conn.dir_cache().get("file:///dir2"), None);
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));

        let directory1 = Directory::new(&conn, "file:///dir1");
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));

        let directory3 = Directory::new(&conn, "file:///dir3");
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));
        assert_eq!(conn.dir_cache().get("file:///dir4"), None);

        // Pinning previously evicted directory adds it to cache.
        directory4.pin_to_cache();
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));

        // Pinned entry can also be removed but doesn’t affect capacity
        conn.dir_cache_mut().remove("file:///dir4");
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));
        assert_eq!(conn.dir_cache().get("file:///dir4"), None);

        let directory2 = Directory::new(&conn, "file:///dir2");
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));
    }

    #[test]
    fn test_rename_unpinned() {
        let conn: Connection = glib::Object::builder().build();
        conn.dir_cache_mut().reduce_unpinned_capacity(4);

        let directory1 = Directory::new(&conn, "file:///dir1");
        let directory2 = Directory::new(&conn, "file:///dir2");
        Directory::new(&conn, "file:///dir1/subdir1");
        Directory::new(&conn, "file:///dir1/something/subdir2");

        conn.dir_cache_mut().rename("file:///dir1", "file:///new1");
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);
        assert_eq!(conn.dir_cache().get("file:///new1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir1/subdir1"), None);
        assert_eq!(conn.dir_cache().get("file:///dir1/something/subdir2"), None);

        // Still considered oldest entry, so evicted after addition
        let directory3 = Directory::new(&conn, "file:///dir3");
        let directory4 = Directory::new(&conn, "file:///dir4");
        let directory5 = Directory::new(&conn, "file:///dir5");
        assert_eq!(conn.dir_cache().get("file:///new1"), None);
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));
        assert_eq!(conn.dir_cache().get("file:///dir5"), Some(&directory5));
    }

    #[test]
    fn test_rename_pinned() {
        let conn: Connection = glib::Object::builder().build();
        conn.dir_cache_mut().reduce_unpinned_capacity(3);

        let directory1 = Directory::new(&conn, "file:///dir1");
        directory1.pin_to_cache();
        let directory2 = Directory::new(&conn, "file:///dir2");
        Directory::new(&conn, "file:///dir1/subdir1");
        Directory::new(&conn, "file:///dir1/something/subdir2");

        conn.dir_cache_mut().rename("file:///dir1", "file:///new1");
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);
        assert_eq!(conn.dir_cache().get("file:///new1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert_eq!(conn.dir_cache().get("file:///dir1/subdir1"), None);
        assert_eq!(conn.dir_cache().get("file:///dir1/something/subdir2"), None);

        // Still pinned, so not evicted after addition
        let directory3 = Directory::new(&conn, "file:///dir3");
        let directory4 = Directory::new(&conn, "file:///dir4");
        let directory5 = Directory::new(&conn, "file:///dir5");
        assert_eq!(conn.dir_cache().get("file:///new1"), Some(&directory1));
        assert_eq!(conn.dir_cache().get("file:///dir2"), None);
        assert_eq!(conn.dir_cache().get("file:///dir3"), Some(&directory3));
        assert_eq!(conn.dir_cache().get("file:///dir4"), Some(&directory4));
        assert_eq!(conn.dir_cache().get("file:///dir5"), Some(&directory5));
    }

    #[test]
    fn test_signalling() {
        fn connect_listener(directory: &Directory, result: &Rc<RefCell<bool>>) {
            let result = result.clone();
            directory.connect_closure(
                "dir-deleted",
                false,
                glib::RustClosure::new_local(move |_| {
                    result.replace(true);
                    None
                }),
            );
        }

        let conn: Connection = glib::Object::builder().build();
        conn.dir_cache_mut().reduce_unpinned_capacity(1);
        let result = Rc::new(RefCell::new(false));

        // No notification when unpinned directories are removed.
        let directory1 = Directory::new(&conn, "file:///dir1");
        connect_listener(&directory1, &result);
        conn.dir_cache_mut().remove("file:///dir1");
        assert!(!result.take());

        // Removing pinned directories results in notification.
        let directory1 = Directory::new(&conn, "file:///dir1");
        directory1.pin_to_cache();
        connect_listener(&directory1, &result);
        conn.dir_cache_mut().remove("file:///dir1");
        assert!(result.take());

        // Regular eviction produces no notification.
        let directory1 = Directory::new(&conn, "file:///dir1");
        connect_listener(&directory1, &result);
        let directory2 = Directory::new(&conn, "file:///dir2");
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);
        assert_eq!(conn.dir_cache().get("file:///dir2"), Some(&directory2));
        assert!(!result.take());

        // Removal of pinned children produces a notification
        directory2.pin_to_cache();
        let subdir = Directory::new(&conn, "file:///dir2/subdir");
        connect_listener(&subdir, &result);
        subdir.pin_to_cache();
        conn.dir_cache_mut().remove_with_children("file:///dir2");
        assert!(result.take());
    }

    #[test]
    fn test_zero_capacity() {
        let conn: Connection = glib::Object::builder().build();
        conn.dir_cache_mut().reduce_unpinned_capacity(0);

        let directory1 = Directory::new(&conn, "file:///dir1");
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);

        directory1.pin_to_cache();
        assert_eq!(conn.dir_cache().get("file:///dir1"), Some(&directory1));

        directory1.unpin_from_cache();
        assert_eq!(conn.dir_cache().get("file:///dir1"), None);
    }
}
