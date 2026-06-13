// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::gio;

/// Type helping to compose submenus via [with! macro](crate::with).
///
/// ```rust
/// # use component_framework::{action_list, with, helpers::{ActionListOutput, Submenu}};
/// # use gtk::gio;
/// action_list!(
///     enum MyWidgetActions {
///         "mywidget.quit" as Quit,
///         "mywidget.copy" as Copy,
///         "mywidget.paste" as Paste,
///     }
/// );
///
/// let menu = with!(gio::Menu {
///     MyWidgetActions::Output::Quit.menuitem("_Quit");
///     Submenu::new("_Edit") {
///         MyWidgetActions::Output::Copy.menuitem("_Copy");
///         MyWidgetActions::Output::Paste.menuitem("_Paste");
///     }
/// });
/// ```
#[derive(Default)]
pub struct Submenu {
    label: String,
    menu: gio::Menu,
}

impl Submenu {
    pub fn new(label: impl Into<String>) -> Self {
        Self {
            label: label.into(),
            menu: gio::Menu::new(),
        }
    }

    pub fn label(&self) -> &str {
        &self.label
    }
}

impl std::ops::Deref for Submenu {
    type Target = gio::Menu;

    fn deref(&self) -> &Self::Target {
        &self.menu
    }
}

impl AsRef<gio::Menu> for Submenu {
    fn as_ref(&self) -> &gio::Menu {
        &self.menu
    }
}

/// Type helping to compose menu sections via [with! macro](crate::with).
///
/// ```rust
/// # use component_framework::{action_list, with, helpers::{ActionListOutput, MenuSection}};
/// # use gtk::gio;
/// action_list!(
///     enum MyWidgetActions {
///         "mywidget.copy" as Copy,
///         "mywidget.paste" as Paste,
///         "mywidget.find-next" as FindNext,
///         "mywidget.find-prev" as FindPrevious,
///     }
/// );
///
/// let menu = with!(gio::Menu {
///     MenuSection {
///         MyWidgetActions::Output::Copy.menuitem("_Copy");
///         MyWidgetActions::Output::Paste.menuitem("_Paste");
///     }
///     MenuSection {
///         MyWidgetActions::Output::FindNext.menuitem("Find _Next");
///         MyWidgetActions::Output::FindPrevious.menuitem("Find P_revious");
///     }
/// });
/// ```
#[derive(Debug, Default)]
pub struct MenuSection {
    menu: gio::Menu,
}

impl std::ops::Deref for MenuSection {
    type Target = gio::Menu;

    fn deref(&self) -> &Self::Target {
        &self.menu
    }
}

impl AsRef<gio::Menu> for MenuSection {
    fn as_ref(&self) -> &gio::Menu {
        &self.menu
    }
}
