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

use gtk::{
    gdk,
    glib::{
        self,
        ffi::gboolean,
        translate::{from_glib_borrow, from_glib_none, Borrowed},
    },
    prelude::*,
    subclass::prelude::*,
};
use std::ffi::c_char;

mod imp {
    use super::*;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    pub struct CommandLine {
        pub cwd: gtk::Label,
        pub store: gtk::ListStore,
        pub combo: gtk::ComboBox,
        pub history: RefCell<Vec<String>>,
        pub max_history_size: Cell<usize>,
        select_in_progress: Cell<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for CommandLine {
        const NAME: &'static str = "GnomeCmdCommandLine";
        type Type = super::CommandLine;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            let store = gtk::ListStore::new(&[String::static_type()]);

            let combo = gtk::ComboBox::builder()
                .has_entry(true)
                .hexpand(true)
                .model(&store)
                .id_column(0)
                .entry_text_column(0)
                .build();

            Self {
                cwd: gtk::Label::builder().selectable(true).build(),
                combo,
                store,
                history: Default::default(),
                max_history_size: Cell::new(16),
                select_in_progress: Default::default(),
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

            let entry = self.entry();
            entry.set_editable(true);
            entry.set_can_focus(true);
            entry.set_size_request(60, -1);

            self.combo.connect_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |cb| {
                    if !imp.select_in_progress.get() {
                        if let Some(item) = cb
                            .active_iter()
                            .and_then(|iter| imp.store.get_value(&iter, 0).get::<String>().ok())
                        {
                            let entry = imp.entry();
                            entry.set_text(&item);
                            entry.grab_focus();
                        }
                    }
                }
            ));
            self.combo.connect_popup_shown_notify(|cb| {
                if !cb.is_popup_shown() {
                    let entry = cb.child().unwrap().grab_focus();
                }
            });

            self.combo.set_parent(&*obj);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.set_propagation_phase(gtk::PropagationPhase::Capture);
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.on_key_pressed(key, state)
            ));
            entry.add_controller(key_controller);
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
            let entry = self.entry();
            let result = entry.grab_focus();
            entry.set_position(-1);
            result
        }
    }

    impl CommandLine {
        pub fn entry(&self) -> gtk::Entry {
            self.combo.child().and_downcast::<gtk::Entry>().unwrap()
        }

        fn on_key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            const NO_MODIFIER_MASK: gdk::ModifierType = gdk::ModifierType::empty();
            match (state, key) {
                (gdk::ModifierType::CONTROL_MASK, gdk::Key::Down) => {
                    self.obj().show_history();
                    glib::Propagation::Stop
                }
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
                    self.entry().set_text("");
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

        pub fn add_to_history(&self, value: &str) {
            let mut history = self.history.borrow_mut();
            if let Some(position) = history.iter().position(|i| i == value) {
                // if the same value has been given before move it first in the list
                let item = history.remove(position);
                history.insert(0, item);
            } else {
                // or if its new just add it
                history.insert(0, value.to_owned());
            }
            // don't let the history get too long
            history.truncate(self.max_history_size.get());
        }

        pub fn update_history_combo(&self) {
            self.select_in_progress.set(true);
            self.store.clear();
            for item in self.history.borrow().iter() {
                let iter = self.store.append();
                self.store.set(&iter, &[(0, item)]);
            }
            self.select_in_progress.set(false);
        }
    }
}

glib::wrapper! {
    pub struct CommandLine(ObjectSubclass<imp::CommandLine>)
        @extends gtk::Widget;
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

        let entry = self.imp().entry();
        let current_text = entry.text();
        if !current_text.ends_with(' ') && !current_text.is_empty() {
            append(&entry, " ");
        }
        append(&entry, text);
    }

    pub fn set_text(&self, text: &str) {
        let entry = self.imp().entry();
        entry.set_text(text);
    }

    pub fn is_empty(&self) -> bool {
        let entry = self.imp().entry();
        entry.text().is_empty()
    }

    pub fn focus(&self) {
        let entry = self.imp().entry();
        entry.grab_focus();
        entry.set_position(-1);
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
        let command = self.imp().entry().text().trim().to_owned();

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

        self.imp().add_to_history(&command);
        self.imp().update_history_combo();
        self.set_text("");

        true
    }

    pub fn history(&self) -> Vec<String> {
        self.imp().history.borrow().clone()
    }

    pub fn set_history(&self, mut items: Vec<String>) {
        items.truncate(self.imp().max_history_size.get());
        self.imp().history.replace(items);
        self.imp().update_history_combo();
    }

    pub fn set_max_history_size(&self, max_history_size: usize) {
        self.imp().history.borrow_mut().truncate(max_history_size);
        self.imp().max_history_size.set(max_history_size);
        self.imp().update_history_combo();
    }

    pub fn show_history(&self) {
        self.imp().combo.popup();
    }

    pub fn update_style(&self) {}
}

#[no_mangle]
extern "C" fn gnome_cmd_cmdline_append_text(
    cmdline: *mut <CommandLine as glib::object::ObjectType>::GlibType,
    text: *const c_char,
) {
    let cmdline: Borrowed<CommandLine> = unsafe { from_glib_borrow(cmdline) };
    let text: String = unsafe { from_glib_none(text) };
    cmdline.append_text(&text);
}

#[no_mangle]
extern "C" fn gnome_cmd_cmdline_is_empty(
    cmdline: *mut <CommandLine as glib::object::ObjectType>::GlibType,
) -> gboolean {
    let cmdline: Borrowed<CommandLine> = unsafe { from_glib_borrow(cmdline) };
    cmdline.is_empty() as gboolean
}

#[no_mangle]
extern "C" fn gnome_cmd_cmdline_exec(
    cmdline: *mut <CommandLine as glib::object::ObjectType>::GlibType,
) {
    let cmdline: Borrowed<CommandLine> = unsafe { from_glib_borrow(cmdline) };
    cmdline.exec(false);
}
