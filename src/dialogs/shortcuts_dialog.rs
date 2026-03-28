// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    main_win::MainWindow,
    shortcuts::{Call, Shortcut, Shortcuts},
    user_actions::UserAction,
    utils::{SenderExt, WindowExt},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, prelude::*, subclass::prelude::*};

#[derive(Clone, PartialEq, Eq)]
struct ListEntry {
    pub action: UserAction,
    pub shortcuts: Vec<Shortcut>,
}

enum CaptureResult {
    Cancel,
    Remove,
    Success(Shortcut),
}

mod imp {
    use super::*;
    use crate::utils::{dialog_button_box, display_help};
    use std::cell::RefCell;

    pub struct ShortcutsDialog {
        pub store: gio::ListStore,
        pub hidden_actions: RefCell<Vec<(Shortcut, Call)>>,
        pub filter_text: RefCell<String>,
        pub modified_only: RefCell<bool>,
        pub default_shortcuts: Shortcuts,
        pub view: gtk::ListBox,
        sender: async_channel::Sender<bool>,
        pub receiver: async_channel::Receiver<bool>,
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
                store: gio::ListStore::new::<glib::BoxedAnyObject>(),
                hidden_actions: Default::default(),
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

            let search_field = gtk::SearchEntry::builder()
                .placeholder_text(gettext("Search shortcuts…"))
                .activates_default(false)
                .search_delay(50)
                .build();
            search_field.connect_search_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |search_field| {
                    imp.filter_text.replace(search_field.text().to_lowercase());
                    imp.view.invalidate_filter();
                }
            ));
            vbox.append(&search_field);

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
            self.view.bind_model(
                Some(&self.store),
                glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    #[upgrade_or]
                    gtk::Box::builder().build().upcast(),
                    move |item| imp.create_widget(item),
                ),
            );
            // Note: This will produce a bogus warning, see
            // https://gitlab.gnome.org/GNOME/gtk/-/issues/4088
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
                    if let Some(obj) = imp
                        .store
                        .item(row.index() as u32)
                        .and_downcast_ref::<glib::BoxedAnyObject>()
                    {
                        glib::spawn_future_local(
                            imp.obj()
                                .clone()
                                .add_shortcut(obj.borrow::<ListEntry>().action),
                        );
                    }
                },
            ));

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

            search_field.connect_stop_search(glib::clone!(
                #[weak]
                cancel_button,
                move |search_field| {
                    if search_field.text().is_empty() {
                        cancel_button.activate();
                    } else {
                        search_field.set_text("");
                    }
                }
            ));

            dialog.set_cancel_widget(&cancel_button);
        }
    }

    impl WidgetImpl for ShortcutsDialog {}
    impl WindowImpl for ShortcutsDialog {}

    impl ShortcutsDialog {
        pub fn fill_model(&self, shortcuts: &Shortcuts) {
            let mut shortcuts: Vec<_> = shortcuts.all();
            shortcuts.retain(|(shortcut, _)| !shortcut.is_mandatory());

            let mut entries: Vec<_> = UserAction::all()
                // TODO: Action parameters cannot currently be configured
                .filter(|action| !action.has_parameter())
                .map(|action| ListEntry {
                    action,
                    shortcuts: shortcuts
                        .extract_if(.., |(_, call)| call.action == action)
                        .map(|(shortcut, _)| shortcut)
                        .collect(),
                })
                .collect();
            entries.sort_by_cached_key(|entry| {
                (
                    entry.action.area(),
                    entry.action.description().to_lowercase(),
                )
            });

            for entry in entries {
                self.store.append(&glib::BoxedAnyObject::new(entry));
            }

            self.hidden_actions.replace(shortcuts);
        }

        pub fn save_shortcuts(&self, shortcuts: &Shortcuts) {
            shortcuts.clear();

            for obj in self.store.iter::<glib::BoxedAnyObject>().flatten() {
                let entry = obj.borrow::<ListEntry>();

                for shortcut in &entry.shortcuts {
                    shortcuts.register(*shortcut, entry.action);
                }
            }

            for (shortcut, call) in self.hidden_actions.borrow().iter() {
                shortcuts.register_full(*shortcut, call.action, call.action_data.as_deref());
            }

            shortcuts.set_mandatory();
        }

        fn filter_row(&self, row: &gtk::ListBoxRow) -> bool {
            let filter_text = self.filter_text.borrow();
            let modified_only = *self.modified_only.borrow();
            if filter_text.is_empty() && !modified_only {
                return true;
            }

            let Some(entry) = self
                .store
                .item(row.index() as u32)
                .and_downcast_ref::<glib::BoxedAnyObject>()
                .map(|o| o.borrow::<ListEntry>().clone())
            else {
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
            let Some(entry) = self
                .store
                .item(row.index() as u32)
                .and_downcast_ref::<glib::BoxedAnyObject>()
                .map(|o| o.borrow::<ListEntry>().clone())
            else {
                return;
            };
            let previous = previous_row
                .and_then(|row| self.store.item(row.index() as u32))
                .and_downcast_ref::<glib::BoxedAnyObject>()
                .map(|o| o.borrow::<ListEntry>().clone());
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

        fn create_widget(&self, item: &glib::Object) -> gtk::Widget {
            if let Some(entry) = item
                .downcast_ref::<glib::BoxedAnyObject>()
                .map(|obj| obj.borrow::<ListEntry>())
            {
                let default_shortcuts = self.default_shortcuts.for_call(&Call {
                    action: entry.action,
                    action_data: None,
                });

                let row = gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .build();

                let label = gtk::Label::builder()
                    .label(entry.action.description())
                    .hexpand(true)
                    .halign(gtk::Align::Start)
                    .build();
                row.append(&label);

                let shortcuts = gtk::Box::builder()
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
                        .action_target(&entry.action.name().to_variant())
                        .css_classes(["flat"])
                        .build(),
                );
                shortcuts.append(&shortcut_actions);

                let mut modified = false;
                for shortcut in &entry.shortcuts {
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
                            .action_target(
                                &(entry.action.name(), shortcut.as_accelerator()).to_variant(),
                            )
                            .css_classes(["flat"])
                            .build(),
                    );
                    shortcuts.append(&shortcut_box);
                }

                if modified || entry.shortcuts.len() != default_shortcuts.len() {
                    shortcut_actions.prepend(
                        &gtk::Button::builder()
                            .icon_name("edit-undo")
                            .tooltip_text(gettext("Reset to Default"))
                            .action_name("reset-action")
                            .action_target(&entry.action.name().to_variant())
                            .css_classes(["flat"])
                            .build(),
                    );
                    label.add_css_class("keyboard-shortcuts-modified");
                }

                row.append(&shortcuts);

                row.upcast()
            } else {
                gtk::Box::builder().build().upcast()
            }
        }

        pub fn lookup_action(&self, action: UserAction) -> Option<glib::BoxedAnyObject> {
            for obj in self.store.iter::<glib::BoxedAnyObject>() {
                if let Ok(obj) = obj
                    && obj.borrow::<ListEntry>().action == action
                {
                    return Some(obj);
                }
            }
            None
        }

        pub async fn resolve_conflicts(
            &self,
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

            for obj in self.store.iter::<glib::BoxedAnyObject>().flatten() {
                {
                    let entry = obj.borrow::<ListEntry>();
                    if entry.action == assigned_to || !entry.action.area().intersects(assigned_area)
                    {
                        continue;
                    }
                }

                let mut needs_update = false;
                loop {
                    let mut remove = None;
                    {
                        let entry = obj.borrow::<ListEntry>();
                        for (index, shortcut) in entry.shortcuts.iter().enumerate() {
                            if shortcuts.contains(shortcut) {
                                remove = Some((index, entry.action, *shortcut));
                                break;
                            }
                        }
                    }
                    if let Some((index, action, shortcut)) = remove {
                        if !conflict_confirm(self.obj().upcast_ref(), action, shortcut).await {
                            return false;
                        }

                        // Remove the conflict and re-run the check
                        obj.borrow_mut::<ListEntry>().shortcuts.remove(index);
                        needs_update = true;
                    } else {
                        break;
                    }
                }

                if needs_update {
                    self.update_row(&obj, false);
                }
            }

            true
        }

        pub fn update_row(&self, obj: &glib::BoxedAnyObject, focus: bool) {
            if let Some(pos) = self
                .store
                .iter::<glib::BoxedAnyObject>()
                .position(|o| o.as_ref() == Ok(obj))
                && let Some(row) = self.view.row_at_index(pos as i32)
            {
                row.set_child(Some(&self.create_widget(obj.upcast_ref())));
                if focus {
                    row.grab_focus();
                }
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
            dialog.imp().fill_model(shortcuts);

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

    async fn add_shortcut(self, action: UserAction) {
        let Some(obj) = self.imp().lookup_action(action) else {
            return;
        };
        if let CaptureResult::Success(shortcut) = capture_key(self.upcast_ref(), action, None).await
        {
            if obj.borrow::<ListEntry>().shortcuts.contains(&shortcut)
                || !self.imp().resolve_conflicts(&[shortcut], action).await
            {
                return;
            } else {
                obj.borrow_mut::<ListEntry>().shortcuts.push(shortcut);
            }
            self.imp().update_row(&obj, true);
        }
    }

    async fn edit_shortcut(self, action: UserAction, shortcut: Shortcut) {
        let Some(obj) = self.imp().lookup_action(action) else {
            return;
        };
        let Some(pos) = obj
            .borrow::<ListEntry>()
            .shortcuts
            .iter()
            .position(|s| s == &shortcut)
        else {
            return;
        };

        match capture_key(self.upcast_ref(), action, Some(&shortcut)).await {
            CaptureResult::Success(new_shortcut) => {
                if new_shortcut == shortcut {
                    return;
                }

                if obj.borrow::<ListEntry>().shortcuts.contains(&new_shortcut) {
                    obj.borrow_mut::<ListEntry>().shortcuts.remove(pos);
                } else if !self.imp().resolve_conflicts(&[new_shortcut], action).await {
                    return;
                } else {
                    obj.borrow_mut::<ListEntry>()
                        .shortcuts
                        .splice(pos..=pos, [new_shortcut]);
                }
                self.imp().update_row(&obj, true);
            }
            CaptureResult::Remove => {
                obj.borrow_mut::<ListEntry>().shortcuts.remove(pos);
                self.imp().update_row(&obj, true);
            }
            CaptureResult::Cancel => (),
        }
    }

    async fn reset_action(self, action: UserAction) {
        let Some(obj) = self.imp().lookup_action(action) else {
            return;
        };
        let default_shortcuts = self.imp().default_shortcuts.for_call(&Call {
            action,
            action_data: None,
        });

        if self
            .imp()
            .resolve_conflicts(&default_shortcuts, action)
            .await
        {
            obj.borrow_mut::<ListEntry>().shortcuts = default_shortcuts;
            self.imp().update_row(&obj, true);
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
    let controller = gtk::EventControllerKey::new();
    controller.set_propagation_phase(gtk::PropagationPhase::Capture);
    controller.connect_key_pressed(move |_, key, _, modifiers| {
        if gtk::accelerator_valid(key, modifiers) {
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
            if key == gdk::Key::Escape && modifiers == gdk::ModifierType::NO_MODIFIER_MASK {
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
