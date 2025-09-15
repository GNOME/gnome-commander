/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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

use crate::history_entry::HistoryEntry;
use gtk::{gdk, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use std::sync::OnceLock;

    pub struct CommandLine {
        pub cwd: gtk::Label,
        pub entry: HistoryEntry,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for CommandLine {
        const NAME: &'static str = "GnomeCmdCommandLine";
        type Type = super::CommandLine;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            Self {
                cwd: gtk::Label::builder().selectable(true).build(),
                entry: HistoryEntry::default(),
            }
        }
    }

    impl ObjectImpl for CommandLine {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(
                gtk::BoxLayout::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .spacing(4)
                    .build(),
            ));
            obj.set_margin_top(1);
            obj.set_margin_bottom(1);
            obj.set_margin_start(2);
            obj.set_margin_end(2);

            self.cwd.set_parent(&*obj);

            gtk::Label::new(Some("#")).set_parent(&*obj);

            self.entry.set_hexpand(true);
            self.entry.set_position(gtk::PositionType::Top);
            self.entry.set_parent(&*obj);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.set_propagation_phase(gtk::PropagationPhase::Capture);
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.on_key_pressed(key, state)
            ));
            self.entry.add_controller(key_controller);
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("change-directory")
                        .param_types([String::static_type()])
                        .build(),
                    glib::subclass::Signal::builder("execute")
                        .param_types([String::static_type(), bool::static_type()])
                        .build(),
                    glib::subclass::Signal::builder("lose-focus").build(),
                ]
            })
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for CommandLine {
        fn grab_focus(&self) -> bool {
            let result = self.entry.grab_focus();
            self.entry.entry().set_position(-1);
            result
        }
    }

    impl CommandLine {
        fn on_key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            const NO_MODIFIER_MASK: gdk::ModifierType = gdk::ModifierType::empty();
            match (state, key) {
                (gdk::ModifierType::SHIFT_MASK, gdk::Key::Return) => {
                    if self.obj().exec(true) {
                        glib::Propagation::Stop
                    } else {
                        glib::Propagation::Proceed
                    }
                }
                (NO_MODIFIER_MASK, gdk::Key::Return) => {
                    if self.obj().exec(false) {
                        glib::Propagation::Stop
                    } else {
                        glib::Propagation::Proceed
                    }
                }
                (NO_MODIFIER_MASK, gdk::Key::Escape) => {
                    self.entry.set_text("");
                    self.obj().emit_lose_focus();
                    glib::Propagation::Stop
                }
                (NO_MODIFIER_MASK, gdk::Key::Up | gdk::Key::Down) => {
                    self.obj().emit_lose_focus();
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }
    }
}

glib::wrapper! {
    pub struct CommandLine(ObjectSubclass<imp::CommandLine>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl CommandLine {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn set_directory(&self, directory: &str) {
        self.imp().cwd.set_text(directory);
    }

    pub fn append_text(&self, text: &str) {
        fn append(entry: &gtk::Entry, text: &str) {
            let mut position = entry.text_length() as i32;
            entry.insert_text(text, &mut position);
        }

        let entry = &self.imp().entry.entry();
        let current_text = entry.text();
        if !current_text.ends_with(' ') && !current_text.is_empty() {
            append(&entry, " ");
        }
        append(&entry, text);
    }

    pub fn set_text(&self, text: &str) {
        self.imp().entry.set_text(text);
    }

    pub fn is_empty(&self) -> bool {
        self.imp().entry.text().is_empty()
    }

    fn emit_lose_focus(&self) {
        self.emit_by_name::<()>("lose-focus", &[]);
    }

    fn emit_change_directory(&self, directory: &str) {
        self.emit_by_name::<()>("change-directory", &[&directory]);
    }

    fn emit_execute(&self, command: &str, in_terminal: bool) {
        self.emit_by_name::<()>("execute", &[&command, &in_terminal]);
    }

    pub fn connect_lose_focus<F: Fn(&Self) + 'static>(&self, f: F) -> glib::SignalHandlerId {
        self.connect_closure(
            "lose-focus",
            false,
            glib::closure_local!(move |self_: &Self| (f)(self_)),
        )
    }

    pub fn connect_change_directory<F: Fn(&Self, &str) + 'static>(
        &self,
        f: F,
    ) -> glib::SignalHandlerId {
        self.connect_closure(
            "change-directory",
            false,
            glib::closure_local!(move |self_: &Self, dir: &str| (f)(self_, dir)),
        )
    }

    pub fn connect_execute<F: Fn(&Self, &str, bool) + 'static>(
        &self,
        f: F,
    ) -> glib::SignalHandlerId {
        self.connect_closure(
            "execute",
            false,
            glib::closure_local!(move |self_: &Self, command: &str, in_terminal: bool| (f)(
                self_,
                command,
                in_terminal
            )),
        )
    }

    pub fn exec(&self, in_terminal: bool) -> bool {
        let command = self.imp().entry.text().trim().to_owned();

        if command.is_empty() {
            return false;
        }

        if let Some(dest_dir) = command.strip_prefix("cd ").map(|d| d.trim_start()) {
            self.emit_change_directory(dest_dir);
        } else if command == "cd" {
            self.emit_change_directory("~");
        } else {
            self.emit_execute(&command, in_terminal);
        }

        self.imp().entry.add_to_history(&command);
        self.set_text("");

        true
    }

    pub fn history(&self) -> Vec<String> {
        self.imp().entry.history()
    }

    pub fn set_history(&self, items: &[String]) {
        self.imp().entry.set_history(items);
    }

    pub fn set_max_history_size(&self, max_history_size: usize) {
        self.imp().entry.set_max_history_size(max_history_size);
    }

    pub fn show_history(&self) {
        self.imp().entry.show_history();
    }

    pub fn update_style(&self) {}
}
