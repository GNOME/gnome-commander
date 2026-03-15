// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! ## The already reserved debug flags:
//!
//! | Flag | Description                   |
//! |------|-------------------------------|
//! | a    | set all debug flags           |
//! | g    | run command debugging         |
//! | i    | imageloader                   |
//! | k    | directory pool                |
//! | l    | directory listings            |
//! | m    | connection debugging          |
//! | n    | directory monitoring          |
//! | s    | smb network browser           |
//! | t    | metadata tags                 |
//! | u    | user actions debugging        |
//! | v    | internal viewer               |
//! | y    | brief mime-based imageload    |
//! | z    | detailed mime-based imageload |
//! | x    | file transfer                 |

#[cfg(debug_assertions)]
use std::{
    collections::HashSet,
    sync::{LazyLock, Mutex},
};

#[cfg(debug_assertions)]
static DEBUG_FLAGS: LazyLock<Mutex<HashSet<char>>> = LazyLock::new(Mutex::default);

pub fn set_debug_flags(_flags: &str) {
    #[cfg(debug_assertions)]
    if let Ok(mut guard) = DEBUG_FLAGS.lock() {
        guard.clear();
        guard.extend(_flags.chars());
    }
}

#[cfg(debug_assertions)]
pub fn is_debug_enabled(flag: char) -> bool {
    if let Ok(guard) = DEBUG_FLAGS.lock() {
        guard.contains(&flag)
    } else {
        false
    }
}

#[cfg(debug_assertions)]
macro_rules! debug {
    ($flag:literal, $fmt:expr) => {
        if crate::debug::is_debug_enabled($flag) {
            eprintln!($fmt);
        }
    };
    ($flag:literal, $fmt:expr, $($args:tt)*) => {
        if crate::debug::is_debug_enabled($flag) {
            eprintln!($fmt, $($args)*);
        }
    };
}

#[cfg(not(debug_assertions))]
macro_rules! debug {
    ($($args:tt)*) => {};
}

pub(crate) use debug;

#[cfg(test)]
mod test {
    use super::*;

    static STDERR_OUTPUT: LazyLock<Mutex<Vec<String>>> = LazyLock::new(Default::default);

    // Shadowing `eprintln` to capture debug output
    macro_rules! eprintln {
        ($fmt:expr) => {
            STDERR_OUTPUT.lock().unwrap().push(format!($fmt));
        };
        ($fmt:expr, $($args:tt)*) => {
            STDERR_OUTPUT.lock().unwrap().push(format!($fmt, $($args)*));
        };
    }

    #[test]
    fn test_debug() {
        set_debug_flags("u");

        debug!('u', "message-1");
        debug!('t', "message-{}", 2);
        debug!('u', "message-{}", "three");

        assert_eq!(
            STDERR_OUTPUT.lock().unwrap().as_ref(),
            vec!["message-1".to_string(), "message-three".to_string(),]
        );

        set_debug_flags("");
        STDERR_OUTPUT.lock().unwrap().clear();

        debug!('u', "message-4");
        assert!(STDERR_OUTPUT.lock().unwrap().is_empty());
    }
}
