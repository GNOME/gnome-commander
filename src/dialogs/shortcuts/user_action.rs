// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::shortcuts::{Call, Shortcut};

#[derive(Clone, PartialEq, Eq)]
pub struct ShortcutAction {
    pub shortcut: Shortcut,
    pub call: Call,
}
