/*
 * SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
 * SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

use std::{
    env, fs,
    path::{Path, PathBuf},
};

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

const GLOBAL_SCHEMA_DIR: Option<&str> = option_env!("GLOBAL_SCHEMA_DIR");
#[cfg(debug_assertions)]
const LOCAL_SCHEMA_DIR: Option<&str> = option_env!("LOCAL_SCHEMA_DIR");
#[cfg(not(debug_assertions))]
const LOCAL_SCHEMA_DIR: Option<&str> = None;

const SETTINGS_KEYFILE: Option<&str> = option_env!("SETTINGS_KEYFILE");

fn resolve_path(path: &str) -> PathBuf {
    env::current_exe()
        .ok()
        .and_then(|executable| executable.parent().map(Path::to_owned))
        .map(|dir| dir.join(path))
        .unwrap_or_else(|| PathBuf::from(path))
}

pub fn locale_dir() -> PathBuf {
    resolve_path(
        LOCAL_LOCALE_DIR
            .filter(|dir| fs::exists(dir).is_ok_and(|result| result))
            .unwrap_or(GLOBAL_LOCALE_DIR),
    )
}

pub fn plugin_dir() -> PathBuf {
    resolve_path(
        LOCAL_PLUGIN_DIR
            .filter(|dir| fs::exists(dir).is_ok_and(|result| result))
            .unwrap_or(GLOBAL_PLUGIN_DIR),
    )
}

pub fn schema_dir() -> Option<PathBuf> {
    LOCAL_SCHEMA_DIR
        .filter(|dir| fs::exists(dir).is_ok_and(|result| result))
        .or(GLOBAL_SCHEMA_DIR)
        .map(resolve_path)
}

pub fn settings_keyfile() -> Option<PathBuf> {
    SETTINGS_KEYFILE.map(resolve_path)
}
