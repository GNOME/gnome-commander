// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    main_win::MainWindow,
    shortcuts::{Call, Shortcut, Shortcuts},
    user_actions::UserAction,
    utils::{NO_BUTTONS, NO_MOD, SHIFT, SenderExt, WindowExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{gdk, glib, prelude::*, subclass::prelude::*};

#[derive(Clone, PartialEq, Eq)]
struct ListEntry {
    pub action: UserAction,
    pub shortcuts: Vec<Shortcut>,
    pub row: gtk::ListBoxRow,
}

enum CaptureResult {
    Cancel,
    Remove,
    Success(Shortcut),
}

mod imp {
    use super::*;
    use crate::utils::display_help;
    use std::cell::RefCell;

    pub struct ShortcutsDialog {
        pub(super) entries: RefCell<Vec<ListEntry>>,
        hidden_actions: RefCell<Vec<(Shortcut, Call)>>,
        search_field: gtk::SearchEntry,
        filter_text: RefCell<String>,
        modified_only: RefCell<bool>,
        pub(super) default_shortcuts: Shortcuts,
        pub(super) view: gtk::ListBox,
        sender: async_channel::Sender<bool>,
        pub(super) receiver: async_channel::Receiver<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ShortcutsDialog {
        const NAME: &'static str = "GnomeCmdShortcutsDialog";
        type Type = super::ShortcutsDialog;
        type ParentType = gtk::Window;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action(
                "edit-shortcut",
                Some(&<(String, String)>::static_variant_type()),
                move |obj, _, param| {
                    if let Some(param) = param
                        && let Some((action_name, shortcut)) =
                            <(String, String)>::from_variant(param)
                        && let Some(action) = UserAction::from_name(&action_name)
                        && let Some(shortcut) = Shortcut::parse(&shortcut)
                    {
                        glib::spawn_future_local(obj.clone().edit_shortcut(action, shortcut));
                    }
                },
            );
            klass.install_action(
                "add-shortcut",
                Some(&String::static_variant_type()),
                move |obj, _, param| {
                    if let Some(param) = param
                        && let Some(action_name) = String::from_variant(param)
                        && let Some(action) = UserAction::from_name(&action_name)
                    {
                        glib::spawn_future_local(obj.clone().add_shortcut(action));
                    }
                },
            );
            klass.install_action(
                "reset-action",
                Some(&String::static_variant_type()),
                move |obj, _, param| {
                    if let Some(param) = param
                        && let Some(action_name) = String::from_variant(param)
                        && let Some(action) = UserAction::from_name(&action_name)
                    {
                        glib::spawn_future_local(obj.clone().reset_action(action));
                    }
                },
            );
        }

        fn new() -> Self {
            let default_shortcuts = Shortcuts::new();
            default_shortcuts.set_default();

            let (sender, receiver) = async_channel::bounded(1);
            Self {
                entries: Default::default(),
                hidden_actions: Default::default(),
                search_field: gtk::SearchEntry::builder()
                    .placeholder_text(gettext("Search shortcuts…"))
                    .activates_default(false)
                    .search_delay(50)
                    .build(),
                filter_text: Default::default(),
                modified_only: Default::default(),
                default_shortcuts,
                view: gtk::ListBox::builder()
                    .selection_mode(gtk::SelectionMode::None)
                    .show_separators(true)
                    .build(),
                sender,
                receiver,
            }
        }
    }

    impl ObjectImpl for ShortcutsDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dialog = self.obj();

            dialog.add_css_class("dialog");
            dialog.set_title(Some(&gettext("Keyboard Shortcuts")));
            dialog.set_destroy_with_parent(true);
            dialog.set_size_request(800, 600);
            dialog.set_resizable(true);

            let vbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .build();
            dialog.set_child(Some(&vbox));

            self.search_field.connect_search_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |search_field| {
                    imp.filter_text.replace(search_field.text().to_lowercase());
                    imp.view.invalidate_filter();
                }
            ));
            vbox.append(&self.search_field);

            let modified_only = gtk::CheckButton::builder()
                .label("Show only _modified shortcuts")
                .use_underline(true)
                .build();
            modified_only.connect_toggled(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |modified_only| {
                    imp.modified_only.replace(modified_only.is_active());
                    imp.view.invalidate_filter();
                }
            ));
            vbox.append(&modified_only);

            self.view.add_css_class("rich-list");
            self.view.set_filter_func(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                true,
                move |row| imp.filter_row(row),
            ));
            self.view.set_header_func(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |row, previous_row| imp.update_row_header(row, previous_row),
            ));
            self.view.connect_row_activated(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, row| {
                    if let Some(entry) = imp.entries.borrow().get(row.index() as usize) {
                        glib::spawn_future_local(imp.obj().clone().add_shortcut(entry.action));
                    }
                },
            ));

            let controller = gtk::EventControllerKey::new();
            controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, modifiers| imp.on_list_key_pressed(key, modifiers)
            ));
            self.view.add_controller(controller);

            let scrolled_window = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Never)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .has_frame(true)
                .child(&self.view)
                .hexpand(true)
                .vexpand(true)
                .build();
            vbox.append(&scrolled_window);

            let help_button = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .build();
            help_button.connect_clicked(glib::clone!(
                #[weak]
                dialog,
                move |_| {
                    glib::spawn_future_local(async move {
                        display_help(dialog.upcast_ref(), Some("gnome-commander-keyboard")).await
                    });
                }
            ));

            let cancel_button = gtk::Button::builder()
                .label(gettext("_Cancel"))
                .use_underline(true)
                .hexpand(true)
                .halign(gtk::Align::End)
                .build();
            cancel_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(false)
            ));

            let ok_button = gtk::Button::builder()
                .label(gettext("_Save Shortcuts"))
                .use_underline(true)
                .build();
            ok_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(true)
            ));

            vbox.append(&dialog_button_box(
                &[&help_button],
                &[&cancel_button, &ok_button],
            ));

            self.search_field.connect_stop_search(glib::clone!(
                #[weak]
                cancel_button,
                #[weak(rename_to = imp)]
                self,
                move |search_field| {
                    if search_field.text().is_empty() {
                        cancel_button.activate();
                    } else {
                        search_field.set_text("");
                        imp.view.grab_focus();
                    }
                }
            ));

            dialog.set_cancel_widget(&cancel_button);
        }
    }

    impl WidgetImpl for ShortcutsDialog {}
    impl WindowImpl for ShortcutsDialog {}

    impl ShortcutsDialog {
        pub(super) fn fill_list(&self, shortcuts: &Shortcuts) {
            let mut shortcuts: Vec<_> = shortcuts.all();
            shortcuts.retain(|(shortcut, _)| !shortcut.is_mandatory());

            let mut entries: Vec<_> = UserAction::all()
                // TODO: Action parameters cannot currently be configured
                .filter(|action| !action.has_parameter())
                .map(|action| {
                    let shortcuts: Vec<_> = shortcuts
                        .extract_if(.., |(_, call)| call.action == action)
                        .map(|(shortcut, _)| shortcut)
                        .collect();
                    ListEntry {
                        row: gtk::ListBoxRow::builder()
                            .child(&self.create_widget(action, &shortcuts))
                            .activatable(true)
                            .build(),
                        action,
                        shortcuts,
                    }
                })
                .collect();
            entries.sort_by_cached_key(|entry| {
                (
                    entry.action.area(),
                    entry.action.description().to_lowercase(),
                )
            });
            for entry in &entries {
                self.view.append(&entry.row);
            }
            self.entries.replace(entries);

            self.hidden_actions.replace(shortcuts);
        }

        pub(super) fn save_shortcuts(&self, shortcuts: &Shortcuts) {
            shortcuts.clear();

            for entry in self.entries.borrow().iter() {
                for shortcut in &entry.shortcuts {
                    shortcuts.register(*shortcut, entry.action);
                }
            }

            for (shortcut, call) in self.hidden_actions.borrow().iter() {
                shortcuts.register_full(*shortcut, call.action, call.action_data.as_deref());
            }

            shortcuts.set_mandatory();
        }

        fn on_list_key_pressed(
            &self,
            key: gdk::Key,
            modifiers: gdk::ModifierType,
        ) -> glib::Propagation {
            if modifiers.difference(SHIFT) == NO_MOD
                && let Some(char) = key.to_unicode()
                && char >= ' '
            {
                let mut position = self.search_field.position();
                self.search_field
                    .insert_text(&char.to_string(), &mut position);
                self.search_field.grab_focus();
                self.search_field.set_position(position);
                glib::Propagation::Stop
            } else {
                glib::Propagation::Proceed
            }
        }

        fn filter_row(&self, row: &gtk::ListBoxRow) -> bool {
            let filter_text = self.filter_text.borrow();
            let modified_only = *self.modified_only.borrow();
            if filter_text.is_empty() && !modified_only {
                return true;
            }

            let entries = self.entries.borrow();
            let Some(entry) = entries.get(row.index() as usize) else {
                return false;
            };

            if modified_only {
                let default_shortcuts = self.default_shortcuts.for_call(&Call {
                    action: entry.action,
                    action_data: None,
                });

                if default_shortcuts.len() == entry.shortcuts.len()
                    && entry
                        .shortcuts
                        .iter()
                        .all(|s| default_shortcuts.contains(s))
                {
                    return false;
                }
            }

            if filter_text.is_empty() {
                return true;
            }

            if entry
                .action
                .description()
                .to_lowercase()
                .contains(&*filter_text)
            {
                return true;
            }

            for shortcut in &entry.shortcuts {
                if shortcut.label().to_lowercase().contains(&*filter_text) {
                    return true;
                }
            }

            false
        }

        fn update_row_header(&self, row: &gtk::ListBoxRow, previous_row: Option<&gtk::ListBoxRow>) {
            let entries = self.entries.borrow();
            let Some(entry) = entries.get(row.index() as usize) else {
                return;
            };
            let previous = previous_row.and_then(|row| entries.get(row.index() as usize));
            let needs_header =
                previous.is_none_or(|previous| previous.action.area() != entry.action.area());
            if needs_header {
                if row.header().is_none() {
                    row.set_header(Some(
                        &gtk::Label::builder()
                            .label(entry.action.area().label())
                            .halign(gtk::Align::Start)
                            .css_classes(["keyboard-shortcuts-section-header"])
                            .build(),
                    ));
                }
            } else {
                row.set_header(gtk::Widget::NONE);
            }
        }

        fn create_widget(&self, action: UserAction, shortcuts: &[Shortcut]) -> gtk::Widget {
            let default_shortcuts = self.default_shortcuts.for_call(&Call {
                action,
                action_data: None,
            });

            let row = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .build();

            let label = gtk::Label::builder()
                .label(action.description())
                .hexpand(true)
                .halign(gtk::Align::Start)
                .build();
            row.append(&label);

            let shortcuts_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .build();

            let shortcut_actions = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .halign(gtk::Align::End)
                .build();
            shortcut_actions.append(
                &gtk::Button::builder()
                    .icon_name("document-new")
                    .tooltip_text(gettext("Add Shortcut"))
                    .action_name("add-shortcut")
                    .action_target(&action.name().to_variant())
                    .css_classes(["flat"])
                    .build(),
            );
            shortcuts_box.append(&shortcut_actions);

            let mut modified = false;
            for shortcut in shortcuts {
                let shortcut_box = gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .halign(gtk::Align::End)
                    .build();
                if !default_shortcuts.contains(shortcut) {
                    shortcut_box.add_css_class("keyboard-shortcuts-modified");
                    modified = true;
                }
                shortcut_box.append(
                    &gtk::Label::builder()
                        .label(shortcut.label())
                        .css_classes(["keyboard-shortcuts-shortcut"])
                        .build(),
                );
                shortcut_box.append(
                    &gtk::Button::builder()
                        .icon_name("document-edit")
                        .tooltip_text(gettext("Edit Shortcut"))
                        .action_name("edit-shortcut")
                        .action_target(&(action.name(), shortcut.as_accelerator()).to_variant())
                        .css_classes(["flat"])
                        .build(),
                );
                shortcuts_box.append(&shortcut_box);
            }

            if modified || shortcuts.len() != default_shortcuts.len() {
                shortcut_actions.prepend(
                    &gtk::Button::builder()
                        .icon_name("edit-undo")
                        .tooltip_text(gettext("Reset to Default"))
                        .action_name("reset-action")
                        .action_target(&action.name().to_variant())
                        .css_classes(["flat"])
                        .build(),
                );
                label.add_css_class("keyboard-shortcuts-modified");
            }

            row.append(&shortcuts_box);

            row.upcast()
        }

        pub(super) async fn resolve_conflicts(
            &self,
            entries: &mut [ListEntry],
            shortcuts: &[Shortcut],
            assigned_to: UserAction,
        ) -> bool {
            let assigned_area = assigned_to.area();

            loop {
                let mut remove = None;
                for (index, (shortcut, call)) in self.hidden_actions.borrow().iter().enumerate() {
                    if shortcuts.contains(shortcut) && call.action.area().intersects(assigned_area)
                    {
                        remove = Some((index, call.action, *shortcut));
                        break;
                    }
                }
                if let Some((index, action, shortcut)) = remove {
                    if !conflict_confirm(self.obj().upcast_ref(), action, shortcut).await {
                        return false;
                    }

                    // Remove the conflict and re-run the check
                    self.hidden_actions.borrow_mut().remove(index);
                } else {
                    break;
                }
            }

            for entry in entries.iter_mut() {
                if entry.action == assigned_to || !entry.action.area().intersects(assigned_area) {
                    continue;
                }

                loop {
                    let mut remove = None;
                    for (index, shortcut) in entry.shortcuts.iter().enumerate() {
                        if shortcuts.contains(shortcut) {
                            remove = Some((index, entry.action, *shortcut));
                            break;
                        }
                    }

                    if let Some((index, action, shortcut)) = remove {
                        if !conflict_confirm(self.obj().upcast_ref(), action, shortcut).await {
                            return false;
                        }

                        // Remove the conflict and re-run the check
                        entry.shortcuts.remove(index);
                        self.update_row(entry, false);
                    } else {
                        break;
                    }
                }
            }

            true
        }

        pub(super) fn update_row(&self, entry: &ListEntry, focus: bool) {
            entry
                .row
                .set_child(Some(&self.create_widget(entry.action, &entry.shortcuts)));
            if focus {
                entry.row.grab_focus();
            }
        }
    }
}

glib::wrapper! {
    pub struct ShortcutsDialog(ObjectSubclass<imp::ShortcutsDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl Default for ShortcutsDialog {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ShortcutsDialog {
    pub async fn run(parent: &MainWindow, shortcuts: &Shortcuts) {
        if let Some(dialog) = parent.get_dialog::<Self>("shortcuts") {
            dialog.present();
        } else {
            let dialog = parent.set_dialog("shortcuts", Self::default());
            dialog.set_transient_for(Some(parent));
            dialog.imp().fill_list(shortcuts);

            dialog.present();
            if let Some(row) = dialog.imp().view.row_at_index(0) {
                dialog.imp().view.select_row(Some(&row));
                row.grab_focus();
            }

            let response = dialog.imp().receiver.recv().await;
            if response == Ok(true) {
                dialog.imp().save_shortcuts(shortcuts);
            }
            dialog.close();
        }
    }

    #[allow(clippy::await_holding_refcell_ref)] // Modal dialog is unproblematic
    async fn add_shortcut(self, action: UserAction) {
        let mut entries = self.imp().entries.borrow_mut();
        let Some(index) = entries.iter_mut().position(|entry| entry.action == action) else {
            return;
        };
        if let CaptureResult::Success(shortcut) = capture_key(self.upcast_ref(), action, None).await
        {
            if entries[index].shortcuts.contains(&shortcut)
                || !self
                    .imp()
                    .resolve_conflicts(&mut entries, &[shortcut], action)
                    .await
            {
                return;
            } else {
                entries[index].shortcuts.push(shortcut);
            }
            self.imp().update_row(&entries[index], true);
        }
    }

    #[allow(clippy::await_holding_refcell_ref)] // Modal dialog is unproblematic
    async fn edit_shortcut(self, action: UserAction, shortcut: Shortcut) {
        let mut entries = self.imp().entries.borrow_mut();
        let Some(index) = entries.iter_mut().position(|entry| entry.action == action) else {
            return;
        };
        let Some(pos) = entries[index].shortcuts.iter().position(|s| s == &shortcut) else {
            return;
        };

        match capture_key(self.upcast_ref(), action, Some(&shortcut)).await {
            CaptureResult::Success(new_shortcut) => {
                if new_shortcut == shortcut {
                    return;
                }

                if entries[index].shortcuts.contains(&new_shortcut) {
                    entries[index].shortcuts.remove(pos);
                } else if !self
                    .imp()
                    .resolve_conflicts(&mut entries, &[new_shortcut], action)
                    .await
                {
                    return;
                } else {
                    entries[index].shortcuts[pos] = new_shortcut;
                }
                self.imp().update_row(&entries[index], true);
            }
            CaptureResult::Remove => {
                entries[index].shortcuts.remove(pos);
                self.imp().update_row(&entries[index], true);
            }
            CaptureResult::Cancel => (),
        }
    }

    #[allow(clippy::await_holding_refcell_ref)] // Modal dialog is unproblematic
    async fn reset_action(self, action: UserAction) {
        let mut entries = self.imp().entries.borrow_mut();
        let Some(index) = entries.iter_mut().position(|entry| entry.action == action) else {
            return;
        };
        let default_shortcuts = self.imp().default_shortcuts.for_call(&Call {
            action,
            action_data: None,
        });

        if self
            .imp()
            .resolve_conflicts(&mut entries, &default_shortcuts, action)
            .await
        {
            entries[index].shortcuts = default_shortcuts;
            self.imp().update_row(&entries[index], true);
        }
    }
}

async fn mandatory_notify(parent: &gtk::Window, accel: Shortcut) {
    let _ = gtk::AlertDialog::builder()
        .modal(true)
        .message(
            gettext("Shortcut “{shortcut}” cannot be reconfigured.")
                .replace("{shortcut}", &accel.label()),
        )
        .detail(gettext(
            "This shortcut is used by an action that is not configurable.",
        ))
        .buttons([gettext("_Dismiss")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent))
        .await;
}

async fn conflict_confirm(parent: &gtk::Window, action: UserAction, accel: Shortcut) -> bool {
    let description = action.description();
    gtk::AlertDialog::builder()
        .modal(true)
        .message(
            gettext("Shortcut “{shortcut}” is already taken by “{action}”.")
                .replace("{shortcut}", &accel.label())
                .replace("{action}", &description),
        )
        .detail(
            gettext("Reassigning the shortcut will cause it to be removed from “{action}”.")
                .replace("{action}", &description),
        )
        .buttons([gettext("_Cancel"), gettext("_Reassign shortcut")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent))
        .await
        == Ok(1)
}

async fn capture_key(
    parent: &gtk::Window,
    action: UserAction,
    existing_key: Option<&Shortcut>,
) -> CaptureResult {
    let dialog = gtk::Window::builder()
        .transient_for(parent)
        .modal(true)
        .title(if existing_key.is_some() {
            gettext("Edit Shortcut")
        } else {
            gettext("Add Shortcut")
        })
        .width_request(500)
        .resizable(false)
        .build();
    dialog.add_css_class("dialog");

    let vbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .build();
    dialog.set_child(Some(&vbox));

    let label = gtk::Label::builder()
        .label(if let Some(existing_key) = existing_key {
            gettext("Enter a shortcut to replace “{shortcut}” for action “{action}.”")
                .replace("{action}", &action.description())
                .replace("{shortcut}", &existing_key.label())
        } else {
            gettext("Enter a new shortcut for action “{action}.”")
                .replace("{action}", &action.description())
        })
        .wrap(true)
        .wrap_mode(gtk::pango::WrapMode::Word)
        .max_width_chars(1)
        .justify(gtk::Justification::Center)
        .build();
    vbox.append(&label);

    let mut instructions_text = gettext("Press the desired key combination on your keyboard.");
    instructions_text += " ";
    instructions_text += &gettext("Press Esc twice to cancel.");
    if existing_key.is_some() {
        instructions_text += " ";
        instructions_text += &gettext("Press Backspace twice to remove the shortcut.");
    }
    let instructions = gtk::Label::builder()
        .label(instructions_text)
        .wrap(true)
        .wrap_mode(gtk::pango::WrapMode::Word)
        .max_width_chars(1)
        .justify(gtk::Justification::Left)
        .build();
    instructions.add_css_class("keyboard-shortcuts-key-capture-instructions");
    vbox.append(&instructions);

    let (sender, receiver) = async_channel::bounded(1);

    let cancel_button = gtk::Button::builder()
        .label(gettext("Cancel"))
        .focusable(false)
        .build();
    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss((gdk::Key::Escape, gdk::ModifierType::all()))
    ));
    dialog.set_cancel_widget(&cancel_button);

    if existing_key.is_some() {
        let remove_button = gtk::Button::builder()
            .label(gettext("Remove Shortcut"))
            .focusable(false)
            .build();
        remove_button.connect_clicked(glib::clone!(
            #[strong]
            sender,
            move |_| sender.toss((gdk::Key::BackSpace, gdk::ModifierType::all()))
        ));

        vbox.append(&dialog_button_box(
            NO_BUTTONS,
            &[&cancel_button, &remove_button],
        ));
    } else {
        vbox.append(&dialog_button_box(NO_BUTTONS, &[&cancel_button]));
    }

    let controller = gtk::EventControllerKey::new();
    controller.set_propagation_phase(gtk::PropagationPhase::Capture);
    controller.connect_key_pressed(move |_, key, _, modifiers| {
        if gtk::accelerator_valid(key, modifiers)
            || matches!(
                key,
                gdk::Key::Tab
                    | gdk::Key::ISO_Left_Tab
                    | gdk::Key::KP_Tab
                    | gdk::Key::Up
                    | gdk::Key::KP_Up
                    | gdk::Key::Down
                    | gdk::Key::KP_Down
                    | gdk::Key::Left
                    | gdk::Key::KP_Left
                    | gdk::Key::Right
                    | gdk::Key::KP_Right
            )
        {
            sender.toss((key, modifiers));
            glib::Propagation::Stop
        } else {
            glib::Propagation::Proceed
        }
    });
    dialog.add_controller(controller);

    dialog.present();

    let mut state = 0;
    let result = loop {
        let response = receiver.recv().await;
        if let Ok((key, modifiers)) = response {
            if key == gdk::Key::Escape && modifiers == gdk::ModifierType::all() {
                // Not a keyboard input, cancel button clicked
                break CaptureResult::Cancel;
            } else if key == gdk::Key::BackSpace && modifiers == gdk::ModifierType::all() {
                // Not a keyboard input, remove button clicked
                break CaptureResult::Remove;
            } else if key == gdk::Key::Escape && modifiers == gdk::ModifierType::NO_MODIFIER_MASK {
                if state == 1 {
                    break CaptureResult::Cancel;
                }

                state = 1;
                instructions.set_label(&gettext(
                    "Press Esc again to cancel or Enter to use Esc as shortcut key.",
                ));
            } else if key == gdk::Key::BackSpace
                && modifiers == gdk::ModifierType::NO_MODIFIER_MASK
                && existing_key.is_some()
            {
                if state == 2 {
                    break CaptureResult::Remove;
                }

                state = 2;
                instructions.set_label(&gettext("Press Backspace again to remove the shortcut or Enter to use Backspace as shortcut key."));
            } else if (key == gdk::Key::Return || key == gdk::Key::KP_Enter)
                && modifiers == gdk::ModifierType::NO_MODIFIER_MASK
                && state > 0
            {
                break CaptureResult::Success(Shortcut::new(
                    if state == 1 {
                        gdk::Key::Escape
                    } else {
                        gdk::Key::BackSpace
                    },
                    gdk::ModifierType::NO_MODIFIER_MASK,
                ));
            } else {
                let shortcut = Shortcut::new(key, modifiers);
                if shortcut.is_mandatory() {
                    mandatory_notify(dialog.upcast_ref(), shortcut).await;
                } else {
                    break CaptureResult::Success(shortcut);
                }
            }
        } else {
            break CaptureResult::Cancel;
        }
    };

    dialog.close();

    result
}
