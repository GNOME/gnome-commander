// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Provides helpers allowing to use some Gtk objects in a type-safe and structured way.

mod action_group;
mod grid;
mod menu;

pub use action_group::{
    ActionGroup, ActionGroupInput, ActionList, ActionListOutput, ActionListState,
};
pub use grid::{Grid, GridRow};
pub use menu::{MenuSection, Submenu};
