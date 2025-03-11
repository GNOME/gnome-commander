/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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
