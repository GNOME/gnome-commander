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

use std::{
    collections::HashSet,
    sync::{LazyLock, Mutex},
};

static DEBUG_FLAGS: LazyLock<Mutex<HashSet<char>>> = LazyLock::new(|| Mutex::default());

pub fn set_debug_flags(flags: &str) {
    if let Ok(mut guard) = DEBUG_FLAGS.lock() {
        guard.clear();
        guard.extend(flags.chars());
    }
}

pub fn is_debug_enabled(flag: char) -> bool {
    if let Ok(guard) = DEBUG_FLAGS.lock() {
        guard.contains(&flag)
    } else {
        false
    }
}

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
