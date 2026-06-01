/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 * SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

use std::{fs, path::PathBuf};

pub const PACKAGE: &str = env!("CARGO_PKG_NAME");
pub const ICONS_PREFIX: &str = "/org/gnome/gnome-commander/icons";

pub const PACKAGE_NAME: &str = env!("CARGO_PKG_NAME");
pub const PACKAGE_VERSION: &str = env!("CARGO_PKG_VERSION");
pub const PACKAGE_URL: &str = "https://gnome.pages.gitlab.gnome.org/gnome-commander/";
pub const PACKAGE_BUGREPORT: &str = "https://gitlab.gnome.org/GNOME/gnome-commander/issues";

// In debug builds we want the repository-local directory take precedence if it is present.
const GLOBAL_LOCALE_DIR: &str = env!("GLOBAL_LOCALE_DIR");
#[cfg(debug_assertions)]
const LOCAL_LOCALE_DIR: Option<&str> = option_env!("LOCAL_LOCALE_DIR");
#[cfg(not(debug_assertions))]
const LOCAL_LOCALE_DIR: Option<&str> = None;

const GLOBAL_PLUGIN_DIR: &str = env!("GLOBAL_PLUGIN_DIR");
#[cfg(debug_assertions)]
const LOCAL_PLUGIN_DIR: Option<&str> = option_env!("LOCAL_PLUGIN_DIR");
#[cfg(not(debug_assertions))]
const LOCAL_PLUGIN_DIR: Option<&str> = None;

#[cfg(debug_assertions)]
const LOCAL_SCHEMA_DIR: Option<&str> = option_env!("LOCAL_SCHEMA_DIR");
#[cfg(not(debug_assertions))]
const LOCAL_SCHEMA_DIR: Option<&str> = None;

pub fn locale_dir() -> PathBuf {
    LOCAL_LOCALE_DIR
        .filter(|dir| fs::exists(dir).is_ok_and(|result| result))
        .unwrap_or(GLOBAL_LOCALE_DIR)
        .into()
}

pub fn plugin_dir() -> PathBuf {
    LOCAL_PLUGIN_DIR
        .filter(|dir| fs::exists(dir).is_ok_and(|result| result))
        .unwrap_or(GLOBAL_PLUGIN_DIR)
        .into()
}

pub fn schema_dir() -> Option<PathBuf> {
    LOCAL_SCHEMA_DIR.map(PathBuf::from)
}
