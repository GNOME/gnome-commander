// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ShortcutsDialogInput, shortcut_entry::ShortcutEntry};
use crate::{
    shortcuts::{Call, Shortcut, Shortcuts},
    user_actions::UserAction,
};
use component_framework::prelude::*;
use gettextrs::gettext;
use gtk::prelude::*;
use std::borrow::Cow;

#[derive(Debug)]
pub struct ActionEntry {
    pub(super) index: usize,
    pub(super) modified: bool,
    pub(super) action: UserAction,
    pub(super) action_data: Option<String>,
    pub(super) description: Option<String>,
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
                sender.output(Self::Output::EditActionShortcut(self.index, shortcut))
            }
        }
    }

    fn update_view(&self, sender: &ComponentSender<Self>, view: &mut Self::View) {
        let was_focused = view
            .root()
            .and_then(|root| root.focus())
            .is_some_and(|focus| focus.is_ancestor(view));

        let index = self.index;
        with!(&*view {
            gtk::Box {
                .set_orientation(gtk::Orientation::Horizontal);
                if self.modified {
                    .add_css_class("keyboard-shortcuts-modified");
                }
                .add_css_class(&format!("area-{}", self.action.area().as_name()));
                if UserAction::BookmarksGoto == self.action {
                    .add_css_class("is-bookmark");
                } else if UserAction::PluginAction == self.action {
                    .add_css_class("is-plugin");
                }

                gtk::Label {
                    .set_label(&self.description());
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
                                    forward!(sender.output(Self::Output::ResetAction(index)))
                                );
                            }
                        }

                        gtk::Button {
                            .set_icon_name("document-new");
                            .set_tooltip_text(Some(&gettext("Add Shortcut")));
                            .add_css_class("flat");
                            .connect_clicked(
                                forward!(sender.output(Self::Output::AddActionShortcut(index)))
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
        action_data: Option<String>,
        shortcuts: Vec<Shortcut>,
        default_shortcuts: &Shortcuts,
    ) -> Self {
        let default_shortcuts = default_shortcuts.for_call(&Call {
            action,
            action_data: action_data.clone(),
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
            index: 0,
            modified: shortcuts.len() != default_shortcuts.len()
                || shortcuts.iter().any(|s| s.modified),
            action,
            action_data,
            description: None,
            shortcuts,
        }
    }

    pub fn override_description(&mut self, description: String) {
        self.description = Some(description);
    }

    pub fn description(&self) -> Cow<'_, str> {
        if let Some(description) = &self.description {
            Cow::Borrowed(description)
        } else {
            Cow::Owned(self.action.description())
        }
    }

    fn defaults(&self, default_shortcuts: &Shortcuts) -> Vec<Shortcut> {
        default_shortcuts.for_call(&Call {
            action: self.action,
            action_data: self.action_data.clone(),
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
