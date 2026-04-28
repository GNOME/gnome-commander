// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    history_entry::HistoryEntry,
    main_win::ExecutionTarget,
    options::GeneralOptions,
    shortcuts::{Area, Shortcuts},
    user_actions::UserAction,
    utils::{MenuBuilderExt, NO_MOD, SenderExt},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, prelude::*, subclass::prelude::*};
use std::path::Path;
use vte::prelude::*;

mod imp {
    use super::*;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
        time::Duration,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::CommandLine)]
    pub struct CommandLine {
        pub action_group: gio::SimpleActionGroup,
        pub terminal: vte::Terminal,
        pub(super) terminal_busy: Cell<bool>,
        pub cwd: gtk::Label,
        pub entry: HistoryEntry,
        pub(super) output_widget: gtk::Widget,
        pub(super) focus_controllers: [gtk::EventControllerFocus; 2],
        #[property(get, set = Self::set_autohide_output)]
        autohide_output: Cell<bool>,
        autohide_timeout: RefCell<Option<glib::SourceId>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for CommandLine {
        const NAME: &'static str = "GnomeCmdCommandLine";
        type Type = super::CommandLine;
        type ParentType = gtk::Frame;

        fn new() -> Self {
            let terminal = vte::Terminal::builder()
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
                .build();
            Self {
                action_group: gio::SimpleActionGroup::new(),
                output_widget: gtk::ScrolledWindow::builder()
                    .child(&terminal)
                    .css_classes(["command-line-output"])
                    .build()
                    .upcast(),
                terminal,
                terminal_busy: Default::default(),
                cwd: gtk::Label::builder().selectable(true).build(),
                entry: HistoryEntry::default(),
                focus_controllers: [
                    gtk::EventControllerFocus::new(),
                    gtk::EventControllerFocus::new(),
                ],
                autohide_output: Cell::new(true),
                autohide_timeout: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for CommandLine {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.add_css_class("flat");

            let hbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .css_classes(["command-line"])
                .build();
            obj.set_child(Some(&hbox));

            // This label is for accessibility only and can be hidden.
            let label = gtk::Label::builder()
                .label(gettext("Command Line"))
                .visible(false)
                .build();
            obj.set_label_widget(Some(&label));

            self.setup_actions();

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
                .feed(gettext("If you run a command its output will appear here.").as_bytes());
            self.terminal.feed("\r\n".as_bytes());

            for (focus_controller, widget) in self
                .focus_controllers
                .iter()
                .zip([obj.upcast_ref::<gtk::Widget>(), &self.output_widget])
            {
                focus_controller.connect_enter(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.stop_autohide_timeout(),
                ));
                focus_controller.connect_leave(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.start_autohide_timeout(),
                ));
                widget.add_controller(focus_controller.clone());
            }
            self.do_autohide(self.autohide_output.get());

            let cwd_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .build();
            cwd_box.append(&self.cwd);
            cwd_box.append(&gtk::Label::new(Some("#")));
            hbox.append(&cwd_box);

            self.entry.add_css_class("command-line-entry-field");
            self.entry.set_hexpand(true);
            self.entry.set_popover_position(gtk::PositionType::Top);
            self.entry
                .update_relation(&[gtk::accessible::Relation::LabelledBy(&[
                    cwd_box.upcast_ref()
                ])]);
            hbox.append(&self.entry);

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
                .label(UserAction::RunEmbedded.menu_description())
                .use_underline(true)
                .action_name(UserAction::RunEmbedded.name())
                .build();
            button_box.append(&run_button);

            let run_menu = gtk::MenuButton::builder()
                .direction(gtk::ArrowType::Up)
                .menu_model(
                    &gio::Menu::new()
                        .action(UserAction::RunExternal)
                        .action(UserAction::RunNoCapture),
                )
                .build();
            button_box.append(&run_menu);

            hbox.append(&button_box);

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

            let options = GeneralOptions::new();
            options
                .command_line_autohide_output
                .bind(&*obj, "autohide-output")
                .build();
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("change-directory")
                        .param_types([String::static_type()])
                        .build(),
                    glib::subclass::Signal::builder("execute")
                        .param_types([String::static_type(), ExecutionTarget::static_type()])
                        .build(),
                    glib::subclass::Signal::builder("lose-focus").build(),
                ]
            })
        }
    }

    impl WidgetImpl for CommandLine {
        fn grab_focus(&self) -> bool {
            let result = self.entry.grab_focus();
            self.entry.set_position(-1);
            result
        }
    }
    #[allow(deprecated)] // FrameImpl is deprecated but we need it anyway
    impl FrameImpl for CommandLine {}

    impl CommandLine {
        fn set_autohide_output(&self, value: bool) {
            self.autohide_output.set(value);
            self.do_autohide(value);
        }

        pub(super) fn start_autohide_timeout(&self) {
            if !self.autohide_output.get()
                || self.autohide_timeout.borrow().is_some()
                || self.terminal_busy.get()
            {
                return;
            }

            const AUTOHIDE_DELAY: u32 = 5;
            self.autohide_timeout
                .replace(Some(glib::timeout_add_seconds_local_once(
                    AUTOHIDE_DELAY,
                    glib::clone!(
                        #[weak(rename_to = imp)]
                        self,
                        move || {
                            imp.autohide_timeout.replace(None);
                            imp.do_autohide(true);
                        },
                    ),
                )));
        }

        fn stop_autohide_timeout(&self) {
            self.do_autohide(false);
        }

        fn do_autohide(&self, autohide: bool) {
            if let Some(timeout) = self.autohide_timeout.replace(None) {
                timeout.remove();
            }

            if autohide {
                if !self.focus_controllers.iter().any(|c| c.contains_focus()) {
                    self.output_widget.set_visible(false);
                }
            } else {
                self.output_widget.set_visible(true);
            }
        }

        fn on_key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match (state, key) {
                (NO_MOD, gdk::Key::Up | gdk::Key::Down) => {
                    self.obj().emit_lose_focus();
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }

        fn setup_actions(&self) {
            let obj = self.obj();

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
        @extends gtk::Widget, gtk::Frame,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl CommandLine {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn add_shortcuts(&self, shortcuts: &Shortcuts) {
        shortcuts.add_controller(&self.imp().entry, Area::CommandLine);
        shortcuts.add_controller(&self.imp().terminal, Area::Terminal);
    }

    pub fn output(&self) -> &gtk::Widget {
        &self.imp().output_widget
    }

    pub fn set_directory(&self, directory: &str) {
        self.imp().cwd.set_text(directory);
    }

    pub fn append_text(&self, text: &str) {
        fn append(entry: &HistoryEntry, text: &str) {
            let mut position = entry.text_length() as i32;
            entry.insert_text(text, &mut position);
        }

        let entry = &self.imp().entry;
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
        self.imp().entry.text().trim().is_empty()
    }

    fn emit_lose_focus(&self) {
        self.emit_by_name::<()>("lose-focus", &[]);
    }

    fn emit_change_directory(&self, directory: &str) {
        self.emit_by_name::<()>("change-directory", &[&directory]);
    }

    fn emit_execute(&self, command: &str, target: ExecutionTarget) {
        self.emit_by_name::<()>("execute", &[&command, &target]);
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

    pub fn connect_execute<F: Fn(&Self, &str, ExecutionTarget) + 'static>(
        &self,
        f: F,
    ) -> glib::SignalHandlerId {
        self.connect_closure(
            "execute",
            false,
            glib::closure_local!(move |self_, command, target| (f)(self_, command, target)),
        )
    }

    pub fn process_command(&self, target: ExecutionTarget) {
        let command = self.imp().entry.text().trim().to_owned();
        if command.is_empty() {
            return;
        }

        if let Some(dest_dir) = command.strip_prefix("cd ").map(|d| d.trim_start()) {
            self.emit_change_directory(dest_dir);
        } else if command == "cd" {
            self.emit_change_directory("~");
        } else {
            self.emit_execute(&command, target);
        }

        self.imp().entry.add_to_history(&command);
        self.set_text("");
    }

    pub async fn exec(
        &self,
        working_directory: Option<&Path>,
        command: &str,
    ) -> Result<(), glib::Error> {
        self.imp().terminal.feed(b"\x1B[1m"); // bold
        if let Some(cwd) = working_directory {
            self.imp().terminal.feed(cwd.as_os_str().as_encoded_bytes());
        }
        self.imp().terminal.feed(b"# \x1B[0m");
        self.imp().terminal.feed(command.as_bytes());
        self.imp().terminal.feed(b"\r\n");

        let shell = std::env::var("SHELL").unwrap_or_else(|_| "/bin/sh".to_owned());

        self.pre_exec();
        let result = match self
            .imp()
            .terminal
            .spawn_future(
                vte::PtyFlags::DEFAULT,
                working_directory.and_then(|p| p.to_str()),
                &[&shell, "-i", "-c", command],
                &[],
                glib::SpawnFlags::DEFAULT,
                || {},
                i32::MAX,
            )
            .await
        {
            Ok(_) => {
                // Child started successfully, wait for it to exit
                let (sender, receiver) = async_channel::bounded(1);
                let handler = self.imp().terminal.connect_child_exited(move |_, _| {
                    sender.toss(());
                });
                let _ = receiver.recv().await;
                self.imp().terminal.disconnect(handler);
                Ok(())
            }
            Err(error) => Err(error),
        };
        self.post_exec();

        result
    }

    fn pre_exec(&self) {
        self.imp().terminal.grab_focus();

        self.imp().terminal_busy.set(true);
        if let Some(root) = self.root() {
            root.action_set_enabled(UserAction::RunEmbedded.name(), false);
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

    fn post_exec(&self) {
        if self.imp().terminal.is_focus() {
            self.grab_focus();
        }

        self.imp().terminal_busy.set(false);
        if let Some(root) = self.root() {
            root.action_set_enabled(UserAction::RunEmbedded.name(), true);
        }

        if let Some(action) = self
            .imp()
            .action_group
            .lookup_action("term-paste")
            .and_downcast::<gio::SimpleAction>()
        {
            action.set_enabled(false);
        }

        if !self
            .imp()
            .focus_controllers
            .iter()
            .any(|c| c.contains_focus())
        {
            self.imp().start_autohide_timeout();
        }
    }

    pub fn terminal(&self) -> &vte::Terminal {
        &self.imp().terminal
    }

    pub fn terminal_available(&self) -> bool {
        self.is_visible() && !self.imp().terminal_busy.get()
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
