// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ShortcutsDialogInput, shortcut_entry::ShortcutEntry};
use crate::{
    shortcuts::{Call, Shortcut, Shortcuts},
    user_actions::UserAction,
};
use component_framework::{Component, ComponentController, ComponentSender, forward_output, with};
use gettextrs::gettext;
use gtk::prelude::*;

#[derive(Debug)]
pub struct ActionEntry {
    pub(super) modified: bool,
    pub(super) action: UserAction,
    pub(super) shortcuts: Vec<ComponentController<ShortcutEntry>>,
}

#[derive(Debug)]
pub enum ActionEntryInput {
    EditShortcut(Shortcut),
}

impl Component for ActionEntry {
    type View = gtk::ListBoxRow;
    type Input = ActionEntryInput;
    type Output = ShortcutsDialogInput;
    type Root = gtk::ListBoxRow;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let mut view = Self::View::default();
        self.update_view(sender, &mut view);
        view
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        view
    }

    async fn update(
        &mut self,
        msg: Self::Input,
        sender: &ComponentSender<Self>,
        _view: &mut Self::View,
    ) {
        match msg {
            Self::Input::EditShortcut(shortcut) => {
                sender.output(Self::Output::EditActionShortcut(self.action, shortcut))
            }
        }
    }

    fn update_view(&self, sender: &ComponentSender<Self>, view: &mut Self::View) {
        let was_focused = view
            .root()
            .and_then(|root| root.focus())
            .is_some_and(|focus| focus.is_ancestor(view));

        let action = self.action;
        with!(&*view {
            gtk::Box {
                .set_orientation(gtk::Orientation::Horizontal);
                if self.modified {
                    .add_css_class("keyboard-shortcuts-modified");
                }
                .add_css_class(&format!("area-{}", action.area().as_name()));

                gtk::Label {
                    .set_label(&action.description());
                    .set_hexpand(true);
                    .set_halign(gtk::Align::Start);
                }

                gtk::Box {
                    .set_orientation(gtk::Orientation::Vertical);

                    gtk::Box {
                        .set_orientation(gtk::Orientation::Horizontal);
                        .set_halign(gtk::Align::End);

                        if self.modified {
                            gtk::Button {
                                .set_icon_name("edit-undo");
                                .set_tooltip_text(Some(&gettext("Reset to Default")));
                                .add_css_class("flat");
                                .connect_clicked(
                                    forward_output!(sender, Self::Output::ResetAction(action))
                                );
                            }
                        }

                        gtk::Button {
                            .set_icon_name("document-new");
                            .set_tooltip_text(Some(&gettext("Add Shortcut")));
                            .add_css_class("flat");
                            .connect_clicked(
                                forward_output!(sender, Self::Output::AddActionShortcut(action))
                            );
                        }
                    }

                    for shortcut in &self.shortcuts {
                        shortcut.attach(sender, |message| message) {}
                    }
                }
            }
        });

        if was_focused {
            view.grab_focus();
        }
    }

    async fn handle_subcomponents(&mut self) {
        Self::handle_subcomponent_list(&mut self.shortcuts).await;
    }
}

impl ActionEntry {
    pub fn new(
        action: UserAction,
        shortcuts: Vec<Shortcut>,
        default_shortcuts: &Shortcuts,
    ) -> Self {
        let default_shortcuts = default_shortcuts.for_call(&Call {
            action,
            action_data: None,
        });

        let shortcuts = shortcuts
            .into_iter()
            .map(|shortcut| {
                ShortcutEntry {
                    shortcut,
                    modified: !default_shortcuts.contains(&shortcut),
                }
                .build()
            })
            .collect::<Vec<_>>();

        Self {
            modified: shortcuts.len() != default_shortcuts.len()
                || shortcuts.iter().any(|s| s.modified),
            action,
            shortcuts,
        }
    }

    fn defaults(&self, default_shortcuts: &Shortcuts) -> Vec<Shortcut> {
        default_shortcuts.for_call(&Call {
            action: self.action,
            action_data: None,
        })
    }

    fn update_modified(&mut self, default_shortcuts: &[Shortcut]) {
        self.modified = self.shortcuts.len() != default_shortcuts.len()
            || self.shortcuts.iter().any(|shortcut| shortcut.modified);
    }

    pub fn shortcuts(&self) -> impl Iterator<Item = Shortcut> {
        self.shortcuts.iter().map(|entry| entry.shortcut)
    }

    pub fn has_shortcut(&self, shortcut: Shortcut) -> bool {
        self.shortcuts().any(|s| s == shortcut)
    }

    pub fn add_shortcut(&mut self, shortcut: Shortcut, default_shortcuts: &Shortcuts) {
        let default_shortcuts = self.defaults(default_shortcuts);
        self.shortcuts.push(
            ShortcutEntry {
                shortcut,
                modified: !default_shortcuts.contains(&shortcut),
            }
            .build(),
        );
        self.update_modified(&default_shortcuts);
    }

    pub fn remove_shortcut(&mut self, shortcut: Shortcut, default_shortcuts: &Shortcuts) {
        let Some(index) = self.shortcuts().position(|s| s == shortcut) else {
            return;
        };
        let default_shortcuts = self.defaults(default_shortcuts);
        self.shortcuts.remove(index);
        self.update_modified(&default_shortcuts);
    }

    pub fn replace_shortcut(
        &mut self,
        shortcut: Shortcut,
        new_shortcut: Shortcut,
        default_shortcuts: &Shortcuts,
    ) {
        let Some(index) = self.shortcuts().position(|s| s == shortcut) else {
            return;
        };
        let default_shortcuts = self.defaults(default_shortcuts);
        self.shortcuts[index] = ShortcutEntry {
            shortcut: new_shortcut,
            modified: !default_shortcuts.contains(&new_shortcut),
        }
        .build();
        self.update_modified(&default_shortcuts);
    }

    pub fn reset_to_defaults(&mut self, shortcuts: &[Shortcut]) {
        self.shortcuts.clear();
        self.shortcuts.extend(shortcuts.iter().map(|shortcut| {
            ShortcutEntry {
                shortcut: *shortcut,
                modified: false,
            }
            .build()
        }));
        self.modified = false;
    }
}
