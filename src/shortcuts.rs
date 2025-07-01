/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */

use crate::{debug::debug, main_win::MainWindow, user_actions::USER_ACTIONS};
use gtk::{gdk, glib::translate::FromGlib, prelude::*};
use std::{
    cell::RefCell,
    collections::{BTreeMap, BTreeSet},
    rc::Rc,
};

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct Shortcut {
    pub key: gdk::Key,
    pub state: gdk::ModifierType,
}

impl Shortcut {
    pub fn key(key: gdk::Key) -> Self {
        Self {
            key,
            state: gdk::ModifierType::empty(),
        }
    }

    pub fn ctrl(key: gdk::Key) -> Self {
        Self {
            key,
            state: gdk::ModifierType::CONTROL_MASK,
        }
    }

    pub fn shift(key: gdk::Key) -> Self {
        Self {
            key,
            state: gdk::ModifierType::SHIFT_MASK,
        }
    }

    pub fn alt(key: gdk::Key) -> Self {
        Self {
            key,
            state: gdk::ModifierType::ALT_MASK,
        }
    }

    pub fn sup(key: gdk::Key) -> Self {
        Self {
            key,
            state: gdk::ModifierType::SUPER_MASK,
        }
    }

    pub fn ctrl_shift(key: gdk::Key) -> Self {
        Self {
            key,
            state: gdk::ModifierType::CONTROL_MASK | gdk::ModifierType::SHIFT_MASK,
        }
    }

    pub fn name(self) -> glib::GString {
        gtk::accelerator_name(self.key, self.state)
    }

    pub fn label(self) -> glib::GString {
        gtk::accelerator_get_label(self.key, self.state)
    }

    pub fn is_mandatory(self) -> bool {
        self.state.is_empty()
            && match self.key {
                gdk::Key::F3
                | gdk::Key::F4
                | gdk::Key::F5
                | gdk::Key::F6
                | gdk::Key::F7
                | gdk::Key::F8
                | gdk::Key::F9 => true,
                _ => false,
            }
    }
}

impl PartialOrd for Shortcut {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        (self.state.bits(), self.key).partial_cmp(&(other.state.bits(), other.key))
    }
}

impl Ord for Shortcut {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        (self.state.bits(), self.key).cmp(&(other.state.bits(), other.key))
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Call {
    pub action_name: String,
    pub action_data: Option<String>,
}

#[derive(Clone)]
pub struct Shortcuts {
    inner: Rc<RefCell<ShortcutsInner>>,
}

#[derive(Default)]
struct ShortcutsInner {
    action: BTreeMap<Shortcut, Call>,
}

impl Shortcuts {
    pub fn new() -> Self {
        Self {
            inner: Default::default(),
        }
    }

    pub fn set_mandatory(&self) {
        self.register(Shortcut::key(gdk::Key::F3), "file-view");
        self.register(Shortcut::key(gdk::Key::F4), "file-edit");
        self.register(Shortcut::key(gdk::Key::F5), "file-copy");
        self.register(Shortcut::key(gdk::Key::F6), "file-rename");
        self.register(Shortcut::key(gdk::Key::F7), "file-mkdir");
        self.register(Shortcut::key(gdk::Key::F8), "file-delete");
        self.register(Shortcut::key(gdk::Key::F9), "file-search");
    }

    fn set_default(&self) {
        use gdk::Key;

        self.register(Shortcut::key(Key::F12), "view-main-menu");
        self.register(Shortcut::ctrl(Key::D), "bookmarks-edit");
        self.register(Shortcut::ctrl(Key::N), "connections-new");
        self.register(Shortcut::ctrl(Key::F), "connections-open");
        self.register(Shortcut::ctrl_shift(Key::F), "connections-close");
        self.register(Shortcut::alt(Key::_1), "connections-change-left");
        self.register(Shortcut::sup(Key::_1), "connections-change-left");
        self.register(Shortcut::alt(Key::_2), "connections-change-right");
        self.register(Shortcut::sup(Key::_2), "connections-change-right");
        self.register(Shortcut::ctrl_shift(Key::C), "edit-copy-fnames");
        self.register(Shortcut::ctrl(Key::F12), "edit-filter");
        self.register(Shortcut::alt(Key::F7), "file-search");
        self.register(Shortcut::sup(Key::F), "file-search");
        self.register(Shortcut::ctrl(Key::M), "file-advrename");
        self.register(Shortcut::shift(Key::F5), "file-copy-as");
        self.register(Shortcut::ctrl_shift(Key::F5), "file-create-symlink");
        self.register(Shortcut::shift(Key::F4), "file-edit-new-doc");
        self.register(Shortcut::ctrl(Key::Q), "file-exit");
        self.register(Shortcut::alt(Key::F3), "file-external-view");
        self.register(Shortcut::shift(Key::F3), "file-internal-view");
        self.register(Shortcut::shift(Key::F2), "mark-compare-directories");
        self.register(Shortcut::ctrl(Key::A), "mark-select-all");
        self.register(Shortcut::ctrl(Key::equal), "mark-select-all");
        self.register(Shortcut::ctrl(Key::KP_Add), "mark-select-all");
        self.register(Shortcut::ctrl_shift(Key::A), "mark-unselect-all");
        self.register(Shortcut::ctrl(Key::minus), "mark-unselect-all");
        self.register(Shortcut::ctrl(Key::KP_Subtract), "mark-unselect-all");
        self.register(Shortcut::ctrl(Key::O), "options-edit");
        self.register(Shortcut::alt(Key::Down), "view-dir-history");
        self.register(Shortcut::alt(Key::KP_Down), "view-dir-history");
        self.register(Shortcut::ctrl(Key::Page_Up), "view-up");
        self.register(Shortcut::ctrl(Key::KP_Page_Up), "view-up");
        self.register(Shortcut::ctrl_shift(Key::plus), "view-equal-panes");
        self.register(Shortcut::ctrl(Key::period), "view-in-active-pane");
        self.register(Shortcut::ctrl_shift(Key::greater), "view-in-inactive-pane");
        self.register(Shortcut::ctrl(Key::Left), "view-in-left-pane");
        self.register(Shortcut::ctrl(Key::KP_Left), "view-in-left-pane");
        self.register(Shortcut::ctrl(Key::Right), "view-in-right-pane");
        self.register(Shortcut::ctrl(Key::KP_Right), "view-in-right-pane");
        self.register(Shortcut::ctrl(Key::Up), "view-in-new-tab");
        self.register(Shortcut::ctrl(Key::KP_Up), "view-in-new-tab");
        self.register(Shortcut::ctrl_shift(Key::Up), "view-in-inactive-tab");
        self.register(Shortcut::ctrl_shift(Key::KP_Up), "view-in-inactive-tab");
        self.register(Shortcut::ctrl(Key::Page_Down), "view-directory");
        self.register(Shortcut::ctrl(Key::KP_Page_Down), "view-directory");
        self.register(Shortcut::ctrl(Key::quoteleft), "view-home");
        self.register(Shortcut::ctrl_shift(Key::asciitilde), "view-home");
        self.register(Shortcut::ctrl(Key::backslash), "view-root");
        self.register(Shortcut::ctrl(Key::R), "view-refresh");
        self.register(Shortcut::ctrl(Key::T), "view-new-tab");
        self.register(Shortcut::ctrl(Key::W), "view-close-tab");
        self.register(Shortcut::ctrl_shift(Key::W), "view-close-all-tabs");
    }

    pub fn register(&self, key: Shortcut, action_name: &str) -> bool {
        self.register_full(key, action_name, None)
    }

    pub fn register_full(&self, key: Shortcut, action_name: &str, data: Option<&str>) -> bool {
        let user_actions = &USER_ACTIONS;
        if !user_actions.iter().any(|a| a.action_name == action_name) {
            eprintln!("Unknown user action: '{}' - ignored", action_name);
            return false;
        }

        // event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);
        if self.inner.borrow().action.contains_key(&key) {
            return false;
        }

        if key.key.is_lower() && key.key.is_upper() {
            self.inner.borrow_mut().action.insert(
                Shortcut {
                    key: key.key.to_lower(),
                    state: key.state,
                },
                Call {
                    action_name: action_name.to_owned(),
                    action_data: data.map(|d| d.to_owned()),
                },
            );
            self.inner.borrow_mut().action.insert(
                Shortcut {
                    key: key.key.to_upper(),
                    state: key.state,
                },
                Call {
                    action_name: action_name.to_owned(),
                    action_data: data.map(|d| d.to_owned()),
                },
            );
        } else {
            self.inner.borrow_mut().action.insert(
                key,
                Call {
                    action_name: action_name.to_owned(),
                    action_data: data.map(|d| d.to_owned()),
                },
            );
        }
        true
    }

    pub fn clear(&self) {
        self.inner.borrow_mut().action.clear();
    }

    pub fn all(&self) -> Vec<(Shortcut, Call)> {
        self.inner
            .borrow()
            .action
            .iter()
            .map(|(s, c)| (*s, c.clone()))
            .collect()
    }

    pub fn handle_key_event(&self, mw: &MainWindow, event: Shortcut) -> bool {
        let inner = self.inner.borrow();
        let Some(call) = inner.action.get(&event) else {
            return false;
        };

        debug!(
            'u',
            "Key event: {:?}. Handling key event by {:?}", event, call
        );

        // This is a bit controversial. Majority of actions to not accept arguments
        // and those which accept expect a specific variant and not an arbitrary
        // string.
        if let Err(err) = gtk::prelude::WidgetExt::activate_action(
            mw.upcast_ref::<gtk::Widget>(),
            &format!("win.{}", call.action_name),
            call.action_data
                .as_ref()
                .filter(|d| !d.is_empty())
                .map(|d| d.to_variant())
                .as_ref(),
        ) {
            eprintln!("Failed to activate an action {}: {}", call.action_name, err);
        }
        true
    }

    pub fn load(&self, variant: glib::Variant) {
        self.clear();
        if variant.n_children() == 0 {
            self.set_default();
        } else {
            let user_actions = &*USER_ACTIONS;
            for keybinding in variant.iter() {
                let Some(sv) = ShortcutVariant::from_variant(&keybinding) else {
                    eprintln!(
                        "Wrong variant type of a shortcut: {}. Skipping",
                        keybinding.type_()
                    );
                    continue;
                };

                let Some(user_action) = user_actions
                    .iter()
                    .find(|a| a.name == sv.action_name || a.action_name == sv.action_name)
                else {
                    eprintln!(
                        "<KeyBindings> unknown user action: '{}' - ignored",
                        sv.action_name
                    );
                    continue;
                };

                let Some(shortcut) = sv.shortcut() else {
                    eprintln!(
                        "<KeyBindings> invalid key name: '{}' - ignored",
                        sv.key_name
                    );
                    continue;
                };

                self.register_full(
                    shortcut,
                    &user_action.action_name,
                    Some(sv.action_data.as_str()).filter(|d| !d.is_empty()),
                );
            }
        }
        self.set_mandatory();
    }

    pub fn save(&self) -> glib::Variant {
        glib::Variant::array_from_iter::<ShortcutVariant>(
            self.inner
                .borrow()
                .action
                .iter()
                .filter(|(shortcut, _call)| shortcut.key.is_upper())
                .filter(|(shortcut, _call)| !shortcut.is_mandatory())
                .filter_map(|(shortcut, call)| {
                    ShortcutVariant::from_shortcut(
                        shortcut,
                        &call.action_name,
                        call.action_data.as_deref().unwrap_or_default(),
                    )
                    .map(|sv| sv.to_variant())
                    .or_else(|| {
                        eprintln!("Shortcut {:?} cannot be saved. Skipping", shortcut);
                        None
                    })
                }),
        )
    }

    pub fn bookmark_shortcuts(&self, bookmark_name: &str) -> BTreeSet<Shortcut> {
        self.inner
            .borrow()
            .action
            .iter()
            .filter(|(shortcut, call)| {
                shortcut.key.is_upper()
                    && call.action_name == "bookmarks-goto"
                    && call.action_data.as_deref() == Some(bookmark_name)
            })
            .map(|kv| *kv.0)
            .collect()
    }
}

// const KEYBINDING_FORMAT_STRING: &str = "(sssbbbbbb)";

#[derive(glib::Variant)]
struct ShortcutVariant {
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

impl ShortcutVariant {
    pub fn from_shortcut(
        shortcut: &Shortcut,
        action_name: &str,
        action_data: &str,
    ) -> Option<Self> {
        Some(Self {
            key_name: shortcut.key.name()?.to_string(),
            action_name: action_name.to_owned(),
            action_data: action_data.to_owned(),
            shift: shortcut.state.contains(gdk::ModifierType::SHIFT_MASK),
            control: shortcut.state.contains(gdk::ModifierType::CONTROL_MASK),
            alt: shortcut.state.contains(gdk::ModifierType::ALT_MASK),
            sup: shortcut.state.contains(gdk::ModifierType::SUPER_MASK),
            hyper: shortcut.state.contains(gdk::ModifierType::HYPER_MASK),
            meta: shortcut.state.contains(gdk::ModifierType::META_MASK),
        })
    }

    pub fn mask(&self) -> gdk::ModifierType {
        let mut accel_mask = gdk::ModifierType::empty();
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
        Some(Shortcut {
            key: parse_key(&self.key_name)?,
            state: self.mask(),
        })
    }
}

fn parse_key(name: &str) -> Option<gdk::Key> {
    // GDK name
    if let Some(key) = gdk::Key::from_name(name) {
        return Some(key);
    }
    // Old name
    if let Some(key) = parse_key_from_old_name(name) {
        return Some(key);
    }
    // Single ASCII character
    if let Some(key) = Some(name)
        .map(|s| s.as_bytes())
        .filter(|b| b.len() == 1)
        .and_then(|b| b.first())
        .filter(|b| b.is_ascii_alphanumeric())
        .map(|b| unsafe { gdk::Key::from_glib(*b as u32) })
    {
        return Some(key);
    }
    None
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
