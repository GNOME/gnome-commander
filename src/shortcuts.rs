// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::user_actions::UserAction;
use gtk::{gdk, prelude::*};
use std::{
    cell::RefCell,
    collections::{BTreeMap, BTreeSet},
    rc::Rc,
};

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub enum Area {
    Panel,
    CommandLine,
    Terminal,
    Any,
}

impl Area {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Panel => "panel",
            Self::CommandLine => "command-line",
            Self::Terminal => "terminal",
            Self::Any => "any",
        }
    }

    pub fn from_str(value: &str) -> Self {
        match value {
            "panel" => Self::Panel,
            "command-line" => Self::CommandLine,
            "terminal" => Self::Terminal,
            _ => Self::Any,
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct Shortcut {
    pub key: gdk::Key,
    pub state: gdk::ModifierType,
}

impl Shortcut {
    pub fn new(key: gdk::Key, state: gdk::ModifierType) -> Self {
        // Normalize letter keys: upper-case for Shift modifier, lower-case otherwise.
        let key = if state.contains(gdk::ModifierType::SHIFT_MASK) {
            key.to_upper()
        } else {
            key.to_lower()
        };

        Self { key, state }
    }

    pub fn parse(accelerator: &str) -> Option<Self> {
        let (key, state) = gtk::accelerator_parse(accelerator)?;
        Some(Self::new(key, state))
    }

    pub fn as_accelerator(&self) -> String {
        gtk::accelerator_name(self.key, self.state).to_string()
    }

    pub fn key(key: gdk::Key) -> Self {
        Self::new(key, gdk::ModifierType::NO_MODIFIER_MASK)
    }

    pub fn ctrl(key: gdk::Key) -> Self {
        Self::new(key, gdk::ModifierType::CONTROL_MASK)
    }

    pub fn shift(key: gdk::Key) -> Self {
        Self::new(key, gdk::ModifierType::SHIFT_MASK)
    }

    pub fn alt(key: gdk::Key) -> Self {
        Self::new(key, gdk::ModifierType::ALT_MASK)
    }

    pub fn sup(key: gdk::Key) -> Self {
        Self::new(key, gdk::ModifierType::SUPER_MASK)
    }

    pub fn ctrl_shift(key: gdk::Key) -> Self {
        Self::new(
            key,
            gdk::ModifierType::CONTROL_MASK | gdk::ModifierType::SHIFT_MASK,
        )
    }

    pub fn name(self) -> glib::GString {
        gtk::accelerator_name(self.key, self.state)
    }

    pub fn label(self) -> glib::GString {
        gtk::accelerator_get_label(self.key, self.state)
    }

    pub fn is_mandatory(self) -> bool {
        self.state.is_empty()
            && matches!(
                self.key,
                gdk::Key::F3
                    | gdk::Key::F4
                    | gdk::Key::F5
                    | gdk::Key::F6
                    | gdk::Key::F7
                    | gdk::Key::F8
                    | gdk::Key::F9
            )
    }

    pub fn as_gtk_shortcut(&self, call: &Call) -> Option<gtk::Shortcut> {
        let mut builder = gtk::Shortcut::builder()
            .trigger(&gtk::KeyvalTrigger::new(self.key, self.state))
            .action(&gtk::NamedAction::new(call.action.name()));
        if let Some(parameter_type) = call.action.parameter_type() {
            match parameter_type {
                // TODO: This should work for types other than strings
                value if value == String::static_variant_type() => {
                    builder = builder.arguments(&call.action_data.to_variant());
                }
                _ => {
                    eprintln!(
                        "<KeyBindings> invalid key: cannot process parameter for action {}, non-string type required.",
                        call.action.name()
                    );
                    return None;
                }
            }
        }
        Some(builder.build())
    }
}

impl PartialOrd for Shortcut {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Shortcut {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        (self.state.bits(), self.key).cmp(&(other.state.bits(), other.key))
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Call {
    pub action: UserAction,
    pub action_data: Option<String>,
}

#[derive(Clone)]
pub struct Shortcuts {
    inner: Rc<RefCell<ShortcutsInner>>,
    controllers: RefCell<Vec<(Area, glib::WeakRef<gtk::ShortcutController>)>>,
    top_level_controller: gtk::ShortcutController,
}

#[derive(Default)]
struct ShortcutsInner {
    action: BTreeMap<(Area, Shortcut), Call>,
}

impl Shortcuts {
    pub fn new() -> Self {
        // This is a hack: this controller doesn't do anything. But we add all shortcuts to it, so
        // that these will be displayed in the menu. Otherwise the menu will only show shortcuts
        // registered for the entire window, not the ones handled by specific widgets.
        let top_level_controller = gtk::ShortcutController::new();
        top_level_controller.set_propagation_phase(gtk::PropagationPhase::None);

        Self {
            inner: Default::default(),
            controllers: Default::default(),
            top_level_controller,
        }
    }

    pub fn set_mandatory(&self) {
        self.register(Shortcut::key(gdk::Key::F3), UserAction::FileView);
        self.register(Shortcut::key(gdk::Key::F4), UserAction::FileEdit);
        self.register(Shortcut::key(gdk::Key::F5), UserAction::FileCopy);
        self.register(Shortcut::key(gdk::Key::F6), UserAction::FileMove);
        self.register(Shortcut::key(gdk::Key::F7), UserAction::FileMkdir);
        self.register(Shortcut::key(gdk::Key::F8), UserAction::FileDelete);
        self.register(Shortcut::key(gdk::Key::F9), UserAction::FileSearch);
    }

    pub fn set_default(&self) {
        use gdk::Key;

        self.register(Shortcut::key(Key::F12), UserAction::ViewMainMenu);
        self.register(Shortcut::ctrl(Key::D), UserAction::BookmarksEdit);
        self.register(Shortcut::ctrl(Key::N), UserAction::ConnectionsNew);
        self.register(Shortcut::ctrl(Key::F), UserAction::ConnectionsOpen);
        self.register(
            Shortcut::ctrl_shift(Key::F),
            UserAction::ConnectionsCloseCurrent,
        );
        self.register(Shortcut::alt(Key::_1), UserAction::ConnectionsChangeLeft);
        self.register(Shortcut::sup(Key::_1), UserAction::ConnectionsChangeLeft);
        self.register(Shortcut::alt(Key::_2), UserAction::ConnectionsChangeRight);
        self.register(Shortcut::sup(Key::_2), UserAction::ConnectionsChangeRight);
        self.register(Shortcut::ctrl(Key::X), UserAction::EditCapCut);
        self.register(Shortcut::ctrl(Key::C), UserAction::EditCapCopy);
        self.register(Shortcut::ctrl(Key::V), UserAction::EditCapPaste);
        self.register(Shortcut::ctrl_shift(Key::C), UserAction::EditCopyNames);
        self.register(Shortcut::key(gdk::Key::KP_Delete), UserAction::FileDelete);
        self.register(Shortcut::key(gdk::Key::Delete), UserAction::FileDelete);
        self.register(Shortcut::ctrl(Key::F12), UserAction::EditFilter);
        self.register(Shortcut::sup(Key::F), UserAction::FileSearch);
        self.register(Shortcut::ctrl(Key::M), UserAction::FileAdvrename);
        self.register(Shortcut::shift(Key::F5), UserAction::FileCopyAs);
        self.register(Shortcut::ctrl_shift(Key::F5), UserAction::FileCreateSymlink);
        self.register(Shortcut::shift(Key::F4), UserAction::FileEditNewDoc);
        self.register(Shortcut::ctrl(Key::Q), UserAction::FileExit);
        self.register(Shortcut::alt(Key::F3), UserAction::FileExternalView);
        self.register(Shortcut::shift(Key::F3), UserAction::FileInternalView);
        self.register(Shortcut::alt(Key::KP_Enter), UserAction::FileProperties);
        self.register(Shortcut::alt(Key::Return), UserAction::FileProperties);
        self.register(Shortcut::shift(Key::F2), UserAction::MarkCompareDirectories);
        self.register(Shortcut::ctrl(Key::equal), UserAction::MarkSelectAll);
        self.register(Shortcut::ctrl(Key::KP_Add), UserAction::MarkSelectAll);
        self.register(Shortcut::ctrl(Key::A), UserAction::MarkSelectAll);
        self.register(Shortcut::ctrl(Key::minus), UserAction::MarkUnselectAll);
        self.register(
            Shortcut::ctrl(Key::KP_Subtract),
            UserAction::MarkUnselectAll,
        );
        self.register(Shortcut::ctrl_shift(Key::A), UserAction::MarkUnselectAll);
        self.register(
            Shortcut::alt(Key::KP_Add),
            UserAction::MarkSelectWithPattern,
        );
        self.register(
            Shortcut::alt(Key::KP_Subtract),
            UserAction::MarkUnselectWithPattern,
        );
        self.register(
            Shortcut::key(Key::KP_Multiply),
            UserAction::MarkInvertSelection,
        );
        self.register(Shortcut::ctrl(Key::O), UserAction::OptionsEdit);
        self.register(Shortcut::alt(Key::Down), UserAction::ViewDirHistory);
        self.register(Shortcut::alt(Key::KP_Down), UserAction::ViewDirHistory);
        self.register(Shortcut::alt(Key::KP_Left), UserAction::ViewBack);
        self.register(Shortcut::alt(Key::Left), UserAction::ViewBack);
        self.register(Shortcut::alt(Key::KP_Right), UserAction::ViewForward);
        self.register(Shortcut::alt(Key::Right), UserAction::ViewForward);
        self.register(Shortcut::key(Key::BackSpace), UserAction::ViewUp);
        self.register(Shortcut::ctrl(Key::KP_Page_Up), UserAction::ViewUp);
        self.register(Shortcut::ctrl(Key::Page_Up), UserAction::ViewUp);
        self.register(Shortcut::ctrl_shift(Key::H), UserAction::ViewHiddenFiles);
        self.register(
            Shortcut::ctrl_shift(Key::KP_Equal),
            UserAction::ViewEqualPanes,
        );
        self.register(Shortcut::ctrl_shift(Key::equal), UserAction::ViewEqualPanes);
        self.register(Shortcut::ctrl_shift(Key::plus), UserAction::ViewEqualPanes);
        self.register(Shortcut::ctrl(Key::period), UserAction::ViewInActivePane);
        self.register(
            Shortcut::ctrl_shift(Key::greater),
            UserAction::ViewInInactivePane,
        );
        self.register(Shortcut::ctrl(Key::KP_Left), UserAction::ViewInLeftPane);
        self.register(Shortcut::ctrl(Key::Left), UserAction::ViewInLeftPane);
        self.register(Shortcut::ctrl(Key::KP_Right), UserAction::ViewInRightPane);
        self.register(Shortcut::ctrl(Key::Right), UserAction::ViewInRightPane);
        self.register(Shortcut::ctrl(Key::KP_Up), UserAction::ViewInNewTab);
        self.register(Shortcut::ctrl(Key::Up), UserAction::ViewInNewTab);
        self.register(
            Shortcut::ctrl_shift(Key::KP_Up),
            UserAction::ViewInInactiveTab,
        );
        self.register(Shortcut::ctrl_shift(Key::Up), UserAction::ViewInInactiveTab);
        self.register(Shortcut::ctrl(Key::KP_Page_Down), UserAction::ViewDirectory);
        self.register(Shortcut::ctrl(Key::Page_Down), UserAction::ViewDirectory);
        self.register(Shortcut::ctrl(Key::quoteleft), UserAction::ViewHome);
        self.register(Shortcut::ctrl_shift(Key::asciitilde), UserAction::ViewHome);
        self.register(Shortcut::ctrl(Key::backslash), UserAction::ViewRoot);
        self.register(Shortcut::ctrl(Key::R), UserAction::ViewRefresh);
        self.register(Shortcut::ctrl(Key::T), UserAction::ViewNewTab);
        self.register(Shortcut::ctrl(Key::W), UserAction::ViewCloseTab);
        self.register(Shortcut::ctrl_shift(Key::W), UserAction::ViewCloseAllTabs);
        self.register(
            Shortcut::ctrl_shift(Key::ISO_Left_Tab),
            UserAction::ViewPrevTab,
        );
        self.register(Shortcut::ctrl_shift(Key::Tab), UserAction::ViewPrevTab);
        self.register(Shortcut::ctrl(Key::ISO_Left_Tab), UserAction::ViewNextTab);
        self.register(Shortcut::ctrl(Key::Tab), UserAction::ViewNextTab);
        self.register(Shortcut::key(Key::F1), UserAction::HelpHelp);
   }

    pub fn register(&self, accelerator: Shortcut, action: UserAction) {
        self.register_full(accelerator, action, None)
    }

    pub fn register_full(&self, accelerator: Shortcut, action: UserAction, data: Option<&str>) {
        self.unregister(&accelerator, action.area());
        if action.area() == Area::Any {
            for area in [Area::Panel, Area::CommandLine, Area::Terminal] {
                self.unregister(&accelerator, area);
            }
        } else {
            self.unregister(&accelerator, Area::Any);
        }

        let call = Call {
            action,
            action_data: data.map(|d| d.to_owned()),
        };

        if let Some(shortcut) = accelerator.as_gtk_shortcut(&call) {
            let call_area = call.action.area();
            self.controllers.borrow_mut().retain(|(area, controller)| {
                if area != &call_area {
                    return true;
                }
                if let Some(controller) = controller.upgrade() {
                    controller.add_shortcut(shortcut.clone());
                    true
                } else {
                    false
                }
            });
            self.top_level_controller.add_shortcut(shortcut)
        }

        self.inner
            .borrow_mut()
            .action
            .insert((action.area(), accelerator), call);
    }

    pub fn unregister(&self, accelerator: &Shortcut, call_area: Area) {
        fn remove_from_controller(
            controller: &gtk::ShortcutController,
            accelerator: &Shortcut,
            action: UserAction,
        ) {
            // In order to remove from a controller we need to find the exact shortcut instance.
            for shortcut in controller.iter::<glib::Object>().flatten() {
                if let Some(shortcut) = shortcut.downcast_ref::<gtk::Shortcut>()
                    && let Some(trigger) =
                        shortcut.trigger().and_downcast_ref::<gtk::KeyvalTrigger>()
                    && trigger.keyval() == accelerator.key
                    && trigger.modifiers() == accelerator.state
                    && let Some(named_action) =
                        shortcut.action().and_downcast_ref::<gtk::NamedAction>()
                    && named_action.action_name() == action.name()
                {
                    controller.remove_shortcut(shortcut);
                    break;
                }
            }
        }

        let key = (call_area, *accelerator);
        let action = &mut self.inner.borrow_mut().action;
        let Some(call) = action.get(&key) else {
            return;
        };
        self.controllers.borrow_mut().retain(|(area, controller)| {
            if area != &call_area {
                return true;
            }
            if let Some(controller) = controller.upgrade() {
                remove_from_controller(&controller, accelerator, call.action);
                true
            } else {
                false
            }
        });
        remove_from_controller(&self.top_level_controller, accelerator, call.action);

        action.remove(&key);
    }

    pub fn clear(&self) {
        self.controllers.borrow_mut().retain(|(_, controller)| {
            if let Some(controller) = controller.upgrade() {
                while let Some(shortcut) = controller.item(0).and_downcast_ref::<gtk::Shortcut>() {
                    controller.remove_shortcut(shortcut);
                }
                true
            } else {
                false
            }
        });
        while let Some(shortcut) = self
            .top_level_controller
            .item(0)
            .and_downcast_ref::<gtk::Shortcut>()
        {
            self.top_level_controller.remove_shortcut(shortcut);
        }

        self.inner.borrow_mut().action.clear();
    }

    pub fn all(&self) -> Vec<(Shortcut, Call)> {
        self.inner
            .borrow()
            .action
            .iter()
            .map(|((_, s), c)| (*s, c.clone()))
            .collect()
    }

    /// Attaches a dummy controller to a window, making sure all shortcuts show up in menu.
    pub fn attach(&self, window: &impl IsA<gtk::Window>) {
        window
            .as_ref()
            .add_controller(self.top_level_controller.clone());
    }

    /// Attaches a shortcut controller to a widget, making it handle shortcuts for the designated
    /// area.
    pub fn add_controller(&self, widget: &impl IsA<gtk::Widget>, controller_area: Area) {
        let controller = gtk::ShortcutController::new();
        controller.set_propagation_phase(gtk::PropagationPhase::Capture);
        for ((area, shortcut), call) in self.inner.borrow().action.iter() {
            if (area == &controller_area || area == &Area::Any)
                && let Some(shortcut) = shortcut.as_gtk_shortcut(call)
            {
                controller.add_shortcut(shortcut);
            }
        }
        self.controllers
            .borrow_mut()
            .push((controller_area, controller.downgrade()));
        widget.as_ref().add_controller(controller);
    }

    pub fn load(&self, bindings: &[ShortcutVariant], legacy: glib::Variant) {
        self.clear();
        self.set_default();

        if !bindings.is_empty() {
            for binding in bindings {
                let Some(shortcut) = Shortcut::parse(&binding.accelerator) else {
                    eprintln!(
                        "<KeyBindings> invalid key: '{}' - ignored",
                        binding.accelerator
                    );
                    continue;
                };

                if binding.action_name.is_empty() {
                    self.unregister(&shortcut, Area::from_str(&binding.action_data));
                } else {
                    let Some(user_action) = UserAction::from_name(&binding.action_name) else {
                        eprintln!(
                            "<KeyBindings> unknown user action: '{}' - ignored",
                            binding.action_name
                        );
                        continue;
                    };
                    self.register_full(
                        shortcut,
                        user_action,
                        Some(binding.action_data.as_str()).filter(|d| !d.is_empty()),
                    );
                }
            }
        } else {
            self.load_legacy(legacy);
        }

        self.set_mandatory();
    }

    fn load_legacy(&self, variant: glib::Variant) {
        use gdk::Key;

        if variant.n_children() == 0 {
            return;
        }

        // These are the default shortcuts from Gnome Commander 1.18.5. We compare the setting to
        // these to identify the differences, only differences need to be migrated.
        let mut defaults = [
            (Shortcut::key(Key::F12), UserAction::ViewMainMenu),
            (Shortcut::ctrl(Key::D), UserAction::BookmarksEdit),
            (Shortcut::ctrl(Key::N), UserAction::ConnectionsNew),
            (Shortcut::ctrl(Key::F), UserAction::ConnectionsOpen),
            (
                Shortcut::ctrl_shift(Key::f),
                UserAction::ConnectionsCloseCurrent,
            ),
            (Shortcut::alt(Key::_1), UserAction::ConnectionsChangeLeft),
            (Shortcut::sup(Key::_1), UserAction::ConnectionsChangeLeft),
            (Shortcut::alt(Key::_2), UserAction::ConnectionsChangeRight),
            (Shortcut::sup(Key::_2), UserAction::ConnectionsChangeRight),
            (Shortcut::ctrl_shift(Key::C), UserAction::EditCopyNames),
            (Shortcut::ctrl(Key::F12), UserAction::EditFilter),
            (Shortcut::ctrl(Key::M), UserAction::FileAdvrename),
            (Shortcut::shift(Key::F5), UserAction::FileCopyAs),
            (Shortcut::ctrl_shift(Key::F5), UserAction::FileCreateSymlink),
            (Shortcut::shift(Key::F4), UserAction::FileEditNewDoc),
            (Shortcut::ctrl(Key::Q), UserAction::FileExit),
            (Shortcut::alt(Key::F3), UserAction::FileExternalView),
            (Shortcut::shift(Key::F3), UserAction::FileInternalView),
            (Shortcut::shift(Key::F2), UserAction::MarkCompareDirectories),
            (Shortcut::ctrl(Key::A), UserAction::MarkSelectAll),
            (Shortcut::ctrl(Key::equal), UserAction::MarkSelectAll),
            (Shortcut::ctrl(Key::KP_Add), UserAction::MarkSelectAll),
            (Shortcut::ctrl_shift(Key::A), UserAction::MarkUnselectAll),
            (Shortcut::ctrl(Key::minus), UserAction::MarkUnselectAll),
            (
                Shortcut::ctrl(Key::KP_Subtract),
                UserAction::MarkUnselectAll,
            ),
            (Shortcut::ctrl(Key::O), UserAction::OptionsEdit),
            (Shortcut::alt(Key::Down), UserAction::ViewDirHistory),
            (Shortcut::alt(Key::KP_Down), UserAction::ViewDirHistory),
            (Shortcut::ctrl(Key::Page_Up), UserAction::ViewUp),
            (Shortcut::ctrl(Key::KP_Page_Up), UserAction::ViewUp),
            (Shortcut::ctrl_shift(Key::plus), UserAction::ViewEqualPanes),
            (
                Shortcut::ctrl_shift(Key::greater),
                UserAction::ViewInInactivePane,
            ),
            (Shortcut::ctrl(Key::Left), UserAction::ViewInLeftPane),
            (Shortcut::ctrl(Key::KP_Left), UserAction::ViewInLeftPane),
            (Shortcut::ctrl(Key::Right), UserAction::ViewInRightPane),
            (Shortcut::ctrl(Key::KP_Right), UserAction::ViewInRightPane),
            (Shortcut::ctrl(Key::Up), UserAction::ViewInNewTab),
            (Shortcut::ctrl(Key::KP_Up), UserAction::ViewInNewTab),
            (Shortcut::ctrl_shift(Key::Up), UserAction::ViewInInactiveTab),
            (
                Shortcut::ctrl_shift(Key::KP_Up),
                UserAction::ViewInInactiveTab,
            ),
            (Shortcut::ctrl(Key::Page_Down), UserAction::ViewDirectory),
            (Shortcut::ctrl(Key::KP_Page_Down), UserAction::ViewDirectory),
            (Shortcut::ctrl(Key::quoteleft), UserAction::ViewHome),
            (Shortcut::ctrl_shift(Key::asciitilde), UserAction::ViewHome),
            (Shortcut::ctrl(Key::backslash), UserAction::ViewRoot),
            (Shortcut::ctrl(Key::R), UserAction::ViewRefresh),
            (Shortcut::ctrl(Key::T), UserAction::ViewNewTab),
            (Shortcut::ctrl(Key::W), UserAction::ViewCloseTab),
            (Shortcut::ctrl_shift(Key::W), UserAction::ViewCloseAllTabs),
        ]
        .into_iter()
        .collect::<BTreeMap<_, _>>();

        for keybinding in variant.iter() {
            let Some(sv) = LegacyShortcutVariant::from_variant(&keybinding) else {
                eprintln!(
                    "<LegacyKeyBindings> wrong variant type of shortcut: {}. Skipping",
                    keybinding.type_()
                );
                continue;
            };

            let Some(user_action) = UserAction::from_name(&sv.action_name) else {
                eprintln!(
                    "<LegacyKeyBindings> unknown user action: '{}' - ignored",
                    sv.action_name
                );
                continue;
            };

            let Some(mut shortcut) = sv.shortcut() else {
                eprintln!(
                    "<LegacyKeyBindings> invalid key name: '{}' - ignored",
                    sv.key_name
                );
                continue;
            };

            if shortcut.state == gdk::ModifierType::NO_MODIFIER_MASK
                && matches!(shortcut.key, Key::_1 | Key::_2)
            {
                // Gnome Commander 1.18.5 had a bug that prevented it from saving Super modifier
                shortcut.state = gdk::ModifierType::SUPER_MASK;
            }

            if !sv.action_data.is_empty() || defaults.get(&shortcut) != Some(&user_action) {
                self.register_full(
                    shortcut,
                    user_action,
                    Some(sv.action_data.as_str()).filter(|d| !d.is_empty()),
                );
            }
            defaults.remove(&shortcut);
        }

        // Any default bindings that we haven’t seen yet must have been removed.
        for (shortcut, action) in defaults {
            self.unregister(&shortcut, action.area());
        }
    }

    pub fn save(&self) -> Vec<ShortcutVariant> {
        let defaults = Self::new();
        defaults.set_default();

        let default_shortcuts = &defaults.inner.borrow().action;
        let actual_shortcuts = &self.inner.borrow().action;

        // Save modified key bindings.
        actual_shortcuts
            .iter()
            .filter(|(key, call)| {
                let (_, shortcut) = key;
                !shortcut.is_mandatory() && default_shortcuts.get(key) != Some(call)
            })
            .map(|((_, shortcut), call)| ShortcutVariant {
                accelerator: shortcut.as_accelerator(),
                action_name: call.action.name().to_owned(),
                action_data: call.action_data.as_ref().cloned().unwrap_or_default(),
            })
            // Save removed key bindings.
            // We mark removed binding with an empty action name. But we still need to know from
            // which area the binding was removed, so we misuse the action data field to store it.
            .chain(
                default_shortcuts
                    .iter()
                    .filter(|(key, _call)| actual_shortcuts.get(key).is_none())
                    .map(|((area, shortcut), _)| ShortcutVariant {
                        accelerator: shortcut.as_accelerator(),
                        action_name: String::new(),
                        action_data: area.as_str().to_owned(),
                    }),
            )
            .collect()
    }

    pub fn bookmark_shortcuts(&self, bookmark_name: &str) -> BTreeSet<Shortcut> {
        self.inner
            .borrow()
            .action
            .iter()
            .filter(|((_, shortcut), call)| {
                shortcut.key.is_upper()
                    && call.action == UserAction::BookmarksGoto
                    && call.action_data.as_deref() == Some(bookmark_name)
            })
            .map(|((_, shortcut), _)| *shortcut)
            .collect()
    }
}

#[derive(glib::Variant)]
pub struct ShortcutVariant {
    pub accelerator: String,
    pub action_name: String,
    pub action_data: String,
}

#[derive(glib::Variant)]
pub struct LegacyShortcutVariant {
    pub key_name: String,
    pub action_name: String,
    pub action_data: String,
    pub shift: bool,
    pub control: bool,
    pub alt: bool,
    pub sup: bool,
    pub hyper: bool,
    pub meta: bool,
}

impl LegacyShortcutVariant {
    pub fn mask(&self) -> gdk::ModifierType {
        let mut accel_mask = gdk::ModifierType::NO_MODIFIER_MASK;
        if self.shift {
            accel_mask |= gdk::ModifierType::SHIFT_MASK;
        }
        if self.control {
            accel_mask |= gdk::ModifierType::CONTROL_MASK;
        }
        if self.alt {
            accel_mask |= gdk::ModifierType::ALT_MASK;
        }
        if self.sup {
            accel_mask |= gdk::ModifierType::SUPER_MASK;
        }
        if self.hyper {
            accel_mask |= gdk::ModifierType::HYPER_MASK;
        }
        if self.meta {
            accel_mask |= gdk::ModifierType::META_MASK;
        }
        accel_mask
    }

    pub fn shortcut(&self) -> Option<Shortcut> {
        Some(Shortcut::new(parse_key(&self.key_name)?, self.mask()))
    }
}

fn parse_key(name: &str) -> Option<gdk::Key> {
    gdk::Key::from_name(name).or_else(|| parse_key_from_old_name(name))
}

fn parse_key_from_old_name(name: &str) -> Option<gdk::Key> {
    match name {
        "ampersand" => Some(gdk::Key::ampersand),
        "apostrophe" => Some(gdk::Key::apostrophe),
        "asciicircum" => Some(gdk::Key::asciicircum),
        "asciitilde" => Some(gdk::Key::asciitilde),
        "asterisk" => Some(gdk::Key::asterisk),
        "at" => Some(gdk::Key::at),
        "backslash" => Some(gdk::Key::backslash),
        "bar" => Some(gdk::Key::bar),
        "braceleft" => Some(gdk::Key::braceleft),
        "braceright" => Some(gdk::Key::braceright),
        "bracketleft" => Some(gdk::Key::bracketleft),
        "bracketright" => Some(gdk::Key::bracketright),
        "colon" => Some(gdk::Key::colon),
        "comma" => Some(gdk::Key::comma),
        "dollar" => Some(gdk::Key::dollar),
        "equal" => Some(gdk::Key::equal),
        "exclam" => Some(gdk::Key::exclam),
        "greater" => Some(gdk::Key::greater),
        "grave" => Some(gdk::Key::grave),
        "less" => Some(gdk::Key::less),
        "minus" => Some(gdk::Key::minus),
        "numbersign" => Some(gdk::Key::numbersign),
        "parenleft" => Some(gdk::Key::parenleft),
        "parenright" => Some(gdk::Key::parenright),
        "percent" => Some(gdk::Key::percent),
        "period" => Some(gdk::Key::period),
        "plus" => Some(gdk::Key::plus),
        "question" => Some(gdk::Key::question),
        "quotedbl" => Some(gdk::Key::quotedbl),
        "quoteleft" => Some(gdk::Key::quoteleft),
        "quoteright" => Some(gdk::Key::quoteright),
        "semicolon" => Some(gdk::Key::semicolon),
        "slash" => Some(gdk::Key::slash),
        "space" => Some(gdk::Key::space),
        "underscore" => Some(gdk::Key::underscore),
        "f1" => Some(gdk::Key::F1),
        "f2" => Some(gdk::Key::F2),
        "f3" => Some(gdk::Key::F3),
        "f4" => Some(gdk::Key::F4),
        "f5" => Some(gdk::Key::F5),
        "f6" => Some(gdk::Key::F6),
        "f7" => Some(gdk::Key::F7),
        "f8" => Some(gdk::Key::F8),
        "f9" => Some(gdk::Key::F9),
        "f10" => Some(gdk::Key::F10),
        "f11" => Some(gdk::Key::F11),
        "f12" => Some(gdk::Key::F12),
        "f13" => Some(gdk::Key::F13),
        "f14" => Some(gdk::Key::F14),
        "f15" => Some(gdk::Key::F15),
        "f16" => Some(gdk::Key::F16),
        "f17" => Some(gdk::Key::F17),
        "f18" => Some(gdk::Key::F18),
        "f19" => Some(gdk::Key::F19),
        "f20" => Some(gdk::Key::F20),
        "f21" => Some(gdk::Key::F21),
        "f22" => Some(gdk::Key::F22),
        "f23" => Some(gdk::Key::F23),
        "f24" => Some(gdk::Key::F24),
        "f25" => Some(gdk::Key::F25),
        "f26" => Some(gdk::Key::F26),
        "f27" => Some(gdk::Key::F27),
        "f28" => Some(gdk::Key::F28),
        "f29" => Some(gdk::Key::F29),
        "f30" => Some(gdk::Key::F30),
        "f31" => Some(gdk::Key::F31),
        "f32" => Some(gdk::Key::F32),
        "f33" => Some(gdk::Key::F33),
        "f34" => Some(gdk::Key::F34),
        "f35" => Some(gdk::Key::F35),
        "kp.0" => Some(gdk::Key::KP_0),
        "kp.1" => Some(gdk::Key::KP_1),
        "kp.2" => Some(gdk::Key::KP_2),
        "kp.3" => Some(gdk::Key::KP_3),
        "kp.4" => Some(gdk::Key::KP_4),
        "kp.5" => Some(gdk::Key::KP_5),
        "kp.6" => Some(gdk::Key::KP_6),
        "kp.7" => Some(gdk::Key::KP_7),
        "kp.8" => Some(gdk::Key::KP_8),
        "kp.9" => Some(gdk::Key::KP_9),
        "kp.add" => Some(gdk::Key::KP_Add),
        "kp.begin" => Some(gdk::Key::KP_Begin),
        "kp.decimal" => Some(gdk::Key::KP_Decimal),
        "kp.delete" => Some(gdk::Key::KP_Delete),
        "kp.divide" => Some(gdk::Key::KP_Divide),
        "kp.down" => Some(gdk::Key::KP_Down),
        "kp.end" => Some(gdk::Key::KP_End),
        "kp.enter" => Some(gdk::Key::KP_Enter),
        "kp.equal" => Some(gdk::Key::KP_Equal),
        "kp.f1" => Some(gdk::Key::KP_F1),
        "kp.f2" => Some(gdk::Key::KP_F2),
        "kp.f3" => Some(gdk::Key::KP_F3),
        "kp.f4" => Some(gdk::Key::KP_F4),
        "kp.home" => Some(gdk::Key::KP_Home),
        "kp.insert" => Some(gdk::Key::KP_Insert),
        "kp.left" => Some(gdk::Key::KP_Left),
        "kp.multiply" => Some(gdk::Key::KP_Multiply),
        "kp.next" => Some(gdk::Key::KP_Next),
        "kp.page.down" => Some(gdk::Key::KP_Page_Down),
        "kp.page.up" => Some(gdk::Key::KP_Page_Up),
        "kp.prior" => Some(gdk::Key::KP_Prior),
        "kp.right" => Some(gdk::Key::KP_Right),
        "kp.separator" => Some(gdk::Key::KP_Separator),
        "kp.space" => Some(gdk::Key::KP_Space),
        "kp.subtract" => Some(gdk::Key::KP_Subtract),
        "kp.tab" => Some(gdk::Key::KP_Tab),
        "kp.up" => Some(gdk::Key::KP_Up),
        "caps.lock" => Some(gdk::Key::Caps_Lock),
        "num.lock" => Some(gdk::Key::Num_Lock),
        "scroll.lock" => Some(gdk::Key::Scroll_Lock),
        "shift.lock" => Some(gdk::Key::Shift_Lock),
        "backspace" => Some(gdk::Key::BackSpace),
        "begin" => Some(gdk::Key::Begin),
        "break" => Some(gdk::Key::Break),
        "cancel" => Some(gdk::Key::Cancel),
        "clear" => Some(gdk::Key::Clear),
        "codeinput" => Some(gdk::Key::Codeinput),
        "delete" => Some(gdk::Key::Delete),
        "down" => Some(gdk::Key::Down),
        "eisu.shift" => Some(gdk::Key::Eisu_Shift),
        "eisu.toggle" => Some(gdk::Key::Eisu_toggle),
        "end" => Some(gdk::Key::End),
        "escape" => Some(gdk::Key::Escape),
        "execute" => Some(gdk::Key::Execute),
        "find" => Some(gdk::Key::Find),
        "first.virtual.screen" => Some(gdk::Key::First_Virtual_Screen),
        "help" => Some(gdk::Key::Help),
        "home" => Some(gdk::Key::Home),
        "hyper.l" => Some(gdk::Key::Hyper_L),
        "hyper.r" => Some(gdk::Key::Hyper_R),
        "insert" => Some(gdk::Key::Insert),
        "last.virtual.screen" => Some(gdk::Key::Last_Virtual_Screen),
        "left" => Some(gdk::Key::Left),
        "linefeed" => Some(gdk::Key::Linefeed),
        "menu" => Some(gdk::Key::Menu),
        "meta.l" => Some(gdk::Key::Meta_L),
        "meta.r" => Some(gdk::Key::Meta_R),
        "mode.switch" => Some(gdk::Key::Mode_switch),
        "multiplecandidate" => Some(gdk::Key::MultipleCandidate),
        "multi.key" => Some(gdk::Key::Multi_key),
        "next" => Some(gdk::Key::Next),
        "next.virtual.screen" => Some(gdk::Key::Next_Virtual_Screen),
        "page.down" => Some(gdk::Key::Page_Down),
        "page.up" => Some(gdk::Key::Page_Up),
        "pause" => Some(gdk::Key::Pause),
        "previouscandidate" => Some(gdk::Key::PreviousCandidate),
        "prev.virtual.screen" => Some(gdk::Key::Prev_Virtual_Screen),
        "print" => Some(gdk::Key::Print),
        "prior" => Some(gdk::Key::Prior),
        "redo" => Some(gdk::Key::Redo),
        "return" => Some(gdk::Key::Return),
        "right" => Some(gdk::Key::Right),
        "script.switch" => Some(gdk::Key::script_switch),
        "select" => Some(gdk::Key::Select),
        "singlecandidate" => Some(gdk::Key::SingleCandidate),
        "super.l" => Some(gdk::Key::Super_L),
        "super.r" => Some(gdk::Key::Super_R),
        "sys.req" => Some(gdk::Key::Sys_Req),
        "tab" => Some(gdk::Key::Tab),
        "terminate.server" => Some(gdk::Key::Terminate_Server),
        "undo" => Some(gdk::Key::Undo),
        "up" => Some(gdk::Key::Up),
        _ => None,
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_type() {
        assert_eq!(&*ShortcutVariant::static_variant_type(), "(sss)");
        assert_eq!(
            &*LegacyShortcutVariant::static_variant_type(),
            "(sssbbbbbb)"
        );
    }
}
