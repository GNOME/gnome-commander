// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod basic;
pub mod file_metadata;
pub mod image;

#[path = "tags.rs"]
mod tags_inner;
pub use tags_inner::*;
