// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod configurable;
pub mod file_actions;
pub mod file_descriptor;
pub mod file_metadata_extractor;
pub mod state;

use std::ffi::c_char;

pub const GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION: u32 = 1;

#[repr(C)]
pub struct PluginInfo {
    pub plugin_system_version: u32,
    pub name: *const c_char,
    pub version: *const c_char,
    pub copyright: *const c_char,
    pub comments: *const c_char,
    pub authors: *const *const c_char,
    pub documenters: *const *const c_char,
    pub translator: *const c_char,
    pub webpage: *const c_char,
}
