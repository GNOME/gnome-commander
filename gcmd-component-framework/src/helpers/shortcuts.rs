// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ActionList, ActionListOutput};
use crate::prelude::*;
use gtk::{gdk, prelude::*};
use std::collections::HashMap;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
struct Shortcut {
    key: gdk::Key,
    modifiers: gdk::ModifierType,
}

impl Shortcut {
    pub fn new(key: gdk::Key, modifiers: gdk::ModifierType) -> Self {
        // Normalize letter keys: upper-case for Shift modifier, lower-case otherwise.
        let key = if modifiers.contains(gdk::ModifierType::SHIFT_MASK) {
            key.to_upper()
        } else {
            key.to_lower()
        };

        Self { key, modifiers }
    }

    pub fn from_accelerator(accelerator: &str) -> Option<Self> {
        let (key, modifiers) = gtk::accelerator_parse(accelerator)?;
        Some(Self::new(key, modifiers))
    }

    pub fn accelerator(&self) -> String {
        gtk::accelerator_name(self.key, self.modifiers).to_string()
    }
}

#[derive(Debug)]
pub enum ShortcutsView {
    Active(gtk::ShortcutController, gtk::ShortcutController),
    Inactive(gtk::ShortcutController),
}

impl ShortcutsView {
    pub fn controllers(&self) -> Vec<gtk::ShortcutController> {
        match self {
            Self::Active(bubble, capture) => vec![bubble.clone(), capture.clone()],
            Self::Inactive(dummy) => vec![dummy.clone()],
        }
    }
}

/// Helper component wrapping `gtk4::ShortcutController` and managing keyboard shortcuts for an
/// action list.
///
/// This component takes the default keyboard shortcuts from [`ActionList::shortcuts()`].
/// Modifications to the default list can be provided.
#[derive(Debug)]
pub struct Shortcuts<L: ActionList> {
    active: bool,
    current: HashMap<Shortcut, L::Output>,
    defaults: HashMap<Shortcut, L::Output>,
}

impl<L: ActionList> Shortcuts<L> {
    /// Creates a new shortcuts component.
    ///
    /// `modified` lists accelerators (in the format used by `gtk4::parse_accelerator()`) with their
    /// respective action. If the action is `None`, the default mapping for the keyboard shortcut is
    /// unset, otherwise it is overridden by the respective action.
    pub fn new(modified: impl IntoIterator<Item = (String, Option<L::Output>)>) -> Self {
        let defaults = Self::defaults();
        Self {
            active: true,
            current: Self::merge(&defaults, modified),
            defaults,
        }
    }

    /// Creates an inactive shortcuts component.
    ///
    /// An inactive component will not trigger any actions but it will still make shortcuts show up
    /// in menus.
    pub fn inactive(modified: impl IntoIterator<Item = (String, Option<L::Output>)>) -> Self {
        let defaults = Self::defaults();
        Self {
            active: false,
            current: Self::merge(&defaults, modified),
            defaults,
        }
    }

    fn defaults() -> HashMap<Shortcut, L::Output> {
        L::shortcuts()
            .filter_map(|(accelerator, action)| {
                if let Some(shortcut) = Shortcut::from_accelerator(accelerator) {
                    Some((shortcut, action))
                } else {
                    eprintln!("Failed to parse default action accelerator: {accelerator}");
                    None
                }
            })
            .collect()
    }

    fn merge(
        defaults: &HashMap<Shortcut, L::Output>,
        modified: impl IntoIterator<Item = (String, Option<L::Output>)>,
    ) -> HashMap<Shortcut, L::Output> {
        let mut merged = defaults.clone();
        for (accelerator, action) in modified {
            let Some(shortcut) = Shortcut::from_accelerator(&accelerator) else {
                eprintln!("Failed to parse action accelerator: {accelerator}");
                continue;
            };
            if let Some(action) = action {
                merged.insert(shortcut, action);
            } else {
                merged.remove(&shortcut);
            }
        }
        merged
    }

    /// Updates the modifications to the default keyboard shortcuts.
    ///
    /// The `modified` parameters is interpreted in the same way as for [`Shortcuts::new()`].
    /// It is applied to the default mapping, previous modifications are ignored.
    pub fn update_modified(
        &mut self,
        modified: impl IntoIterator<Item = (String, Option<L::Output>)>,
    ) {
        self.current = Self::merge(&self.defaults, modified);
    }

    /// Lists currently modified keyboard shortcuts compared to defaults.
    ///
    /// The result format matches the `modified` parameters of [`Shortcuts::new()`].
    pub fn modified(&self) -> Vec<(String, Option<L::Output>)> {
        let mut modified = Vec::new();
        for (shortcut, action) in &self.current {
            if self.defaults.get(shortcut) != Some(action) {
                modified.push((shortcut.accelerator(), Some(action.clone())));
            }
        }
        for shortcut in self.defaults.keys() {
            if !self.current.contains_key(shortcut) {
                modified.push((shortcut.accelerator(), None));
            }
        }
        modified
    }
}

impl<L: ActionList> Component for Shortcuts<L> {
    type Input = ();
    type Output = ();
    type View = ShortcutsView;
    type Root = ShortcutsView;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let mut view = if self.active {
            let bubble = gtk::ShortcutController::new();
            let capture = gtk::ShortcutController::new();
            capture.set_propagation_phase(gtk::PropagationPhase::Capture);
            Self::View::Active(bubble, capture)
        } else {
            let dummy = gtk::ShortcutController::new();
            dummy.set_propagation_phase(gtk::PropagationPhase::None);
            Self::View::Inactive(dummy)
        };
        self.update_view(sender, &mut view);
        view
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        view
    }

    fn update_view(&self, _sender: &ComponentSender<Self>, view: &mut Self::View) {
        let (bubble, capture) = match view {
            Self::View::Active(bubble, capture) => (bubble, Some(capture)),
            Self::View::Inactive(dummy) => (dummy, None),
        };
        while let Some(shortcut) = bubble.item(0).and_downcast_ref::<gtk::Shortcut>() {
            bubble.remove_shortcut(shortcut);
        }
        if let Some(capture) = &capture {
            while let Some(shortcut) = capture.item(0).and_downcast_ref::<gtk::Shortcut>() {
                capture.remove_shortcut(shortcut);
            }
        }

        for (shortcut, action) in &self.current {
            let (action, param) = action.to_action_params();
            let gtk_shortcut = gtk::Shortcut::new(
                Some(gtk::KeyvalTrigger::new(shortcut.key, shortcut.modifiers)),
                Some(gtk::NamedAction::new(action.name())),
            );
            gtk_shortcut.set_arguments(param.as_ref());
            if !gtk::accelerator_valid(shortcut.key, shortcut.modifiers)
                && let Some(capture) = &capture
            {
                // This is a keyboard shortcut that is used by Gtk itself. We have to capture it
                // because in the bubble phase it will be processed before we get to see it.
                capture.add_shortcut(gtk_shortcut);
            } else {
                bubble.add_shortcut(gtk_shortcut);
            }
        }
    }
}
