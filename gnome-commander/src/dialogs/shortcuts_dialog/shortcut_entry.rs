// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::action_entry::ActionEntryInput;
use crate::shortcuts::Shortcut;
use component_framework::{Component, ComponentSender, forward_output, with};
use gettextrs::gettext;
use gtk::prelude::*;

#[derive(Debug)]
pub struct ShortcutEntry {
    pub(super) shortcut: Shortcut,
    pub(super) modified: bool,
}

impl Component for ShortcutEntry {
    type View = gtk::Box;
    type Input = ();
    type Output = ActionEntryInput;
    type Root = gtk::Box;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let root = Self::View::default();
        let shortcut = self.shortcut;
        with!(&root {
            .set_orientation(gtk::Orientation::Horizontal);
            if self.modified {
                .add_css_class("keyboard-shortcuts-modified");
            }
            .set_halign(gtk::Align::End);

            gtk::Label {
                .set_label(&shortcut.label());
            }

            gtk::Button {
                .set_icon_name("document-edit");
                .set_tooltip_text(Some(&gettext("Edit Shortcut")));
                .add_css_class("flat");
                .connect_clicked(forward_output!(sender, Self::Output::EditShortcut(shortcut)));
            }
        });

        root
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        view
    }
}
