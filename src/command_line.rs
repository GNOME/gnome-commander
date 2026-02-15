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

use crate::{
    history_entry::HistoryEntry,
    utils::{ALT, NO_MOD, SHIFT},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, prelude::*, subclass::prelude::*};
use vte::prelude::*;

mod imp {
    use super::*;
    use std::{sync::OnceLock, time::Duration};

    pub struct CommandLine {
        pub action_group: gio::SimpleActionGroup,
        pub terminal: vte::Terminal,
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
                action_group: gio::SimpleActionGroup::new(),
                terminal: vte::Terminal::builder()
                    .allow_hyperlink(true)
                    .audible_bell(false)
                    .enable_fallback_scrolling(false)
                    .input_enabled(true)
                    .scroll_on_keystroke(true)
                    .scroll_unit_is_pixels(true)
                    .can_focus(true)
                    .can_target(true)
                    .focus_on_click(true)
                    .vexpand(true)
                    .context_menu_model(&{
                        let menu = gio::Menu::new();
                        menu.append(Some(&gettext("_Copy")), Some("cmdline.term-copy"));
                        menu.append(Some(&gettext("_Paste")), Some("cmdline.term-paste"));
                        menu.append(
                            Some(&gettext("_Select all")),
                            Some("cmdline.term-select-all"),
                        );
                        menu
                    })
                    .build(),
                cwd: gtk::Label::builder().selectable(true).build(),
                entry: HistoryEntry::default(),
            }
        }
    }

    impl ObjectImpl for CommandLine {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.add_css_class("command-line");
            obj.set_layout_manager(Some(
                gtk::BoxLayout::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .build(),
            ));

            self.setup_actions();

            let scrolled_window = gtk::ScrolledWindow::builder().child(&self.terminal).build();
            scrolled_window.set_parent(&*obj);

            self.terminal.connect_child_exited(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.exec_done(),
            ));
            self.terminal.connect_selection_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    if let Some(action) = imp
                        .action_group
                        .lookup_action("term-copy")
                        .and_downcast::<gio::SimpleAction>()
                    {
                        action.set_enabled(
                            imp.terminal
                                .text_selected(vte::Format::Text)
                                .is_some_and(|s| !s.is_empty()),
                        );
                    }
                }
            ));
            self.terminal
                .connect_current_directory_uri_notify(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| {
                        if let Some(dest_dir) = imp
                            .terminal
                            .current_directory_uri()
                            .map(|uri| gio::File::for_uri(&uri))
                            .and_then(|file| file.path())
                            .as_deref()
                            .and_then(|path| path.to_str())
                        {
                            imp.obj().emit_change_directory(dest_dir);
                        }
                    }
                ));
            self.terminal
                .feed(gettext("If you run a command its output will appear here.\r\n").as_bytes());

            let entry_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .build();
            entry_box.add_css_class("command-line-entry");
            entry_box.set_parent(&*obj);

            entry_box.append(&self.cwd);
            entry_box.append(&gtk::Label::new(Some("#")));

            self.entry.add_css_class("command-line-entry-field");
            self.entry.set_hexpand(true);
            self.entry.set_position(gtk::PositionType::Top);
            entry_box.append(&self.entry);

            self.terminal.connect_bell(glib::clone!(
                #[weak(rename_to = entry)]
                self.entry,
                move |_| {
                    entry.add_css_class("bell");
                    glib::timeout_add_local_once(Duration::from_millis(500), move || {
                        entry.remove_css_class("bell")
                    });
                }
            ));

            let button_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .build();

            let run_button = gtk::Button::builder()
                .label(&gettext("_Run"))
                .use_underline(true)
                .action_name("cmdline.run")
                .build();
            button_box.append(&run_button);

            let run_menu = gtk::MenuButton::builder()
                .direction(gtk::ArrowType::Up)
                .menu_model(&{
                    let menu = gio::Menu::new();
                    menu.append(
                        Some(&gettext("Run in _terminal")),
                        Some("cmdline.run-external-terminal"),
                    );
                    menu.append(
                        Some(&gettext("Run _ignoring output")),
                        Some("cmdline.run-nocapture"),
                    );
                    menu
                })
                .build();
            button_box.append(&run_menu);

            entry_box.append(&button_box);

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
            match (state, key) {
                (NO_MOD | SHIFT | ALT, gdk::Key::Return) => {
                    self.action_group.activate_action(
                        if state == SHIFT {
                            "run-external-terminal"
                        } else if state == ALT {
                            "run-nocapture"
                        } else {
                            "run"
                        },
                        None,
                    );
                    glib::Propagation::Stop
                }
                (NO_MOD, gdk::Key::Escape) => {
                    self.entry.set_text("");
                    self.obj().emit_lose_focus();
                    glib::Propagation::Stop
                }
                (NO_MOD, gdk::Key::Up | gdk::Key::Down) => {
                    self.obj().emit_lose_focus();
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }

        fn setup_actions(&self) {
            let obj = self.obj();

            let action = gio::SimpleAction::new("run", None);
            action.connect_activate(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.exec(false, false),
            ));
            self.action_group.add_action(&action);

            let action = gio::SimpleAction::new("run-nocapture", None);
            action.connect_activate(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.exec(true, false),
            ));
            self.action_group.add_action(&action);

            let action = gio::SimpleAction::new("run-external-terminal", None);
            action.connect_activate(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.exec(true, true),
            ));
            self.action_group.add_action(&action);

            let action = gio::SimpleAction::new("term-copy", None);
            action.set_enabled(false);
            action.connect_activate(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.imp().terminal.copy_clipboard_format(vte::Format::Text),
            ));
            self.action_group.add_action(&action);

            let action = gio::SimpleAction::new("term-paste", None);
            action.set_enabled(false);
            action.connect_activate(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.imp().terminal.paste_clipboard(),
            ));
            self.action_group.add_action(&action);

            let action = gio::SimpleAction::new("term-select-all", None);
            action.connect_activate(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.imp().terminal.select_all(),
            ));
            self.action_group.add_action(&action);

            obj.connect_realize(|obj| {
                if let Some(application) = obj
                    .root()
                    .and_downcast::<gtk::ApplicationWindow>()
                    .and_then(|w| w.application())
                {
                    application.set_accels_for_action("cmdline.run-nocapture", &["<Alt>Return"]);
                    application
                        .set_accels_for_action("cmdline.run-external-terminal", &["<Shift>Return"]);
                }
            });

            obj.insert_action_group("cmdline", Some(&self.action_group));

            let shortcuts = gtk::ShortcutController::new();
            shortcuts.add_shortcut(gtk::Shortcut::new(
                gtk::ShortcutTrigger::parse_string("<Ctrl><Shift>c"),
                Some(gtk::NamedAction::new("cmdline.term-copy")),
            ));
            shortcuts.add_shortcut(gtk::Shortcut::new(
                gtk::ShortcutTrigger::parse_string("<Ctrl><Shift>v"),
                Some(gtk::NamedAction::new("cmdline.term-paste")),
            ));
            obj.imp().terminal.add_controller(shortcuts);
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
            append(entry, " ");
        }
        append(entry, text);
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

    pub fn exec(&self, externally: bool, in_terminal: bool) {
        let command = self.imp().entry.text().trim().to_owned();

        if command.is_empty() {
            return;
        }

        if let Some(dest_dir) = command.strip_prefix("cd ").map(|d| d.trim_start()) {
            self.emit_change_directory(dest_dir);
        } else if command == "cd" {
            self.emit_change_directory("~");
        } else if externally {
            self.emit_execute(&command, in_terminal);
        } else {
            let cwd = self.imp().cwd.text();
            self.imp().terminal.feed(b"\x1B[1m"); // bold
            self.imp().terminal.feed(cwd.as_bytes());
            self.imp().terminal.feed(b"# \x1B[0m");
            self.imp().terminal.feed(command.as_bytes());
            self.imp().terminal.feed(b"\r\n");

            let shell = std::env::var("SHELL").unwrap_or_else(|_| "/bin/sh".to_owned());
            self.imp().terminal.spawn_async(
                vte::PtyFlags::DEFAULT,
                Some(&cwd),
                &[&shell, "-i", "-c", &command],
                &[],
                glib::SpawnFlags::DEFAULT,
                || {},
                i32::MAX,
                gio::Cancellable::NONE,
                glib::clone!(
                    #[weak(rename_to = this)]
                    self,
                    move |result| {
                        if result.is_err() {
                            this.exec_done();
                        }
                    }
                ),
            );

            self.imp().terminal.grab_focus();

            if let Some(action) = self
                .imp()
                .action_group
                .lookup_action("run")
                .and_downcast::<gio::SimpleAction>()
            {
                action.set_enabled(false);
            }

            if let Some(action) = self
                .imp()
                .action_group
                .lookup_action("term-paste")
                .and_downcast::<gio::SimpleAction>()
            {
                action.set_enabled(true);
            }
        }

        self.imp().entry.add_to_history(&command);
        self.set_text("");
    }

    pub fn exec_done(&self) {
        if self.imp().terminal.is_focus() {
            self.grab_focus();
        }

        if let Some(action) = self
            .imp()
            .action_group
            .lookup_action("run")
            .and_downcast::<gio::SimpleAction>()
        {
            action.set_enabled(true);
        }

        if let Some(action) = self
            .imp()
            .action_group
            .lookup_action("term-paste")
            .and_downcast::<gio::SimpleAction>()
        {
            action.set_enabled(false);
        }
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
