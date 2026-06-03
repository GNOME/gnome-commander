// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod edit_profile_dialog;
pub mod manage_profiles_dialog;

#[path = "profiles.rs"]
mod profiles_inner;
pub use profiles_inner::*;
