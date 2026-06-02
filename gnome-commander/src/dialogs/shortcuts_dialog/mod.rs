// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod action_entry;
mod capture;
mod shortcut_entry;

use crate::{
    main_win::MainWindow,
    shortcuts::{Area, Call, Shortcut, Shortcuts},
    user_actions::UserAction,
    utils::{NO_MOD, SHIFT, WindowExt, display_help},
};
use action_entry::ActionEntry;
use capture::{Capture, CaptureOutput};
use component_framework::{
    Component, ComponentController, ComponentSender, forward_input, forward_output, with,
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};

#[derive(Debug, Default)]
pub struct ShortcutsDialogView {
    dialog: gtk::Window,
    search_field: gtk::SearchEntry,
    modified_only: gtk::CheckButton,
    list: gtk::ListBox,
    cancel_button: gtk::Button,
}

#[derive(Debug)]
pub enum ShortcutsDialogInput {
    UpdateFilter,
    ResetSearch,
    ResetAction(UserAction),
    AddActionShortcut(UserAction),
    EditActionShortcut(UserAction, Shortcut),
    RowActivate(i32),
    TypeToSearch(char),
    DisplayHelp,
}

#[derive(Debug)]
pub enum ShortcutsDialogOutput {
    Cancelled,
    Accepted,
}

pub struct ShortcutsDialog {
    entries: Vec<ComponentController<ActionEntry>>,
    hidden_actions: Vec<(Shortcut, Call)>,
    default_shortcuts: Shortcuts,
}

impl Component for ShortcutsDialog {
    type View = ShortcutsDialogView;
    type Input = ShortcutsDialogInput;
    type Output = ShortcutsDialogOutput;
    type Root = gtk::Window;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = Self::View::default();

        with!(&view.dialog => {
            add_css_class("dialog");
            set_title(Some(&gettext("Keyboard Shortcuts")));
            set_destroy_with_parent(true);
            set_size_request(800, 600);
            set_resizable(true);
            set_cancel_widget(&view.cancel_button);

            gtk::Box => {
                set_orientation(gtk::Orientation::Vertical);

                &view.search_field => {
                    set_placeholder_text(Some(&gettext("Search shortcuts…")));
                    set_activates_default(false);
                    set_search_delay(50);

                    connect_search_changed(forward_input!(sender, Self::Input::UpdateFilter));

                    connect_stop_search(glib::clone!(
                        #[strong]
                        sender,
                        move |search_field| {
                            if search_field.text().is_empty() {
                                sender.output(Self::Output::Cancelled);
                            } else {
                                sender.input(Self::Input::ResetSearch);
                            }
                        }
                    ));
                }

                &view.modified_only => {
                    set_label(Some(&gettext("Show only _modified shortcuts")));
                    set_use_underline(true);
                    connect_toggled(forward_input!(sender, Self::Input::UpdateFilter));
                }

                gtk::ScrolledWindow => {
                    set_hscrollbar_policy(gtk::PolicyType::Never);
                    set_vscrollbar_policy(gtk::PolicyType::Automatic);
                    set_has_frame(true);
                    set_hexpand(true);
                    set_vexpand(true);

                    &view.list => {
                        set_selection_mode(gtk::SelectionMode::None);
                        set_show_separators(true);
                        add_css_class("rich-list");
                        set_header_func(update_row_header);

                        connect_row_activated(glib::clone!(
                            #[strong]
                            sender,
                            move |_, row| {
                                sender.input(Self::Input::RowActivate(row.index()));
                            },
                        ));

                        add_controller(gtk::EventControllerKey => {
                            connect_key_pressed(glib::clone!(
                                #[strong]
                                sender,
                                move |_, key, _, modifiers| {
                                    if modifiers.difference(SHIFT) == NO_MOD
                                        && let Some(chr) = key.to_unicode()
                                        && chr >= ' '
                                    {
                                        sender.input(Self::Input::TypeToSearch(chr));
                                        glib::Propagation::Stop
                                    } else {
                                        glib::Propagation::Proceed
                                    }
                                }
                            ));
                        });

                        for_!(entry in &self.entries => {
                            entry.root() => {}
                        });
                    }
                }

                gtk::Box => {
                    add_css_class("spacing");
                    set_orientation(gtk::Orientation::Horizontal);

                    gtk::Button => {
                        set_label(&gettext("_Help"));
                        set_use_underline(true);

                        connect_clicked(forward_input!(sender, Self::Input::DisplayHelp));
                    }

                    &view.cancel_button => {
                        set_label(&gettext("_Cancel"));
                        set_use_underline(true);
                        set_hexpand(true);
                        set_halign(gtk::Align::End);

                        connect_clicked(forward_output!(sender, Self::Output::Cancelled));
                    }

                    gtk::Button => {
                        set_label(&gettext("_Save Shortcuts"));
                        set_use_underline(true);
                        connect_clicked(forward_output!(sender, Self::Output::Accepted));
                    }
                }
            }
        });

        view.dialog.present();
        if let Some(row) = view.list.row_at_index(0) {
            view.list.select_row(Some(&row));
            row.grab_focus();
        }

        view
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        &view.dialog
    }

    async fn update(
        &mut self,
        msg: Self::Input,
        _sender: &ComponentSender<Self>,
        view: &mut Self::View,
    ) {
        match msg {
            Self::Input::UpdateFilter => {
                let filter_text = view.search_field.text().to_lowercase();
                let modified_only = view.modified_only.is_active();
                view.list
                    .set_filter_func(move |row| filter_row(row, &filter_text, modified_only));
            }
            Self::Input::ResetSearch => {
                view.search_field.set_text("");
                view.list.grab_focus();
            }
            Self::Input::ResetAction(action) => {
                self.reset_action(&view.dialog, action).await;
            }
            Self::Input::AddActionShortcut(action) => {
                self.add_shortcut(&view.dialog, action).await;
            }
            Self::Input::EditActionShortcut(action, shortcut) => {
                self.edit_shortcut(&view.dialog, action, shortcut).await;
            }
            Self::Input::RowActivate(row) => {
                if let Some(action) = usize::try_from(row)
                    .ok()
                    .and_then(|row| self.entries.get(row))
                    .map(|entry| entry.action)
                {
                    self.add_shortcut(&view.dialog, action).await;
                }
            }
            Self::Input::TypeToSearch(chr) => {
                let mut position = view.search_field.position();
                view.search_field
                    .insert_text(&chr.to_string(), &mut position);
                view.search_field.grab_focus();
                view.search_field.set_position(position);
            }
            Self::Input::DisplayHelp => {
                let dialog = view.dialog.clone();
                glib::spawn_future_local(async move {
                    display_help(dialog.upcast_ref(), Some("gnome-commander-keyboard")).await
                });
            }
        }
    }

    async fn forward_messages(&mut self, sender: &ComponentSender<Self>) {
        Self::forward_messages_ident(sender, &mut self.entries).await;
    }
}

impl ShortcutsDialog {
    fn new(shortcuts: &Shortcuts) -> Self {
        let mut shortcuts: Vec<_> = shortcuts.all();
        shortcuts.retain(|(shortcut, _)| !shortcut.is_mandatory());

        let default_shortcuts = Shortcuts::new();
        default_shortcuts.set_default();

        let mut entries: Vec<_> = UserAction::all()
            // TODO: Action parameters cannot currently be configured
            .filter(|action| !action.has_parameter())
            .map(|action| {
                let shortcuts: Vec<_> = shortcuts
                    .extract_if(.., |(_, call)| call.action == action)
                    .map(|(shortcut, _)| shortcut)
                    .collect();
                ActionEntry::new(action, shortcuts, &default_shortcuts)
            })
            .collect();
        entries.sort_by_cached_key(|entry| {
            (
                entry.action.area(),
                entry.action.description().to_lowercase(),
            )
        });

        Self {
            entries: entries.into_iter().map(|action| action.build()).collect(),
            hidden_actions: shortcuts,
            default_shortcuts,
        }
    }

    pub async fn run(parent: &MainWindow, shortcuts: &Shortcuts) {
        if let Some(dialog) = parent.get_dialog::<gtk::Window>("shortcuts") {
            dialog.present();
        } else {
            let mut controller = ShortcutsDialog::new(shortcuts).build();
            controller.root().set_transient_for(Some(parent));
            parent.set_dialog("shortcuts", controller.root().clone());

            let result = controller.receive().await;
            controller.root().close();

            if matches!(result, Ok(ShortcutsDialogOutput::Accepted)) {
                controller.into_model().save_shortcuts(shortcuts);
            }
        }
    }

    fn index_of(&self, action: UserAction) -> Option<usize> {
        self.entries.iter().position(|entry| entry.action == action)
    }

    fn save_shortcuts(self, shortcuts: &Shortcuts) {
        shortcuts.clear();

        for entry in self.entries.into_iter() {
            let entry = entry.into_model();
            for shortcut in entry.shortcuts.into_iter() {
                let shortcut = shortcut.into_model();
                shortcuts.register(shortcut.shortcut, entry.action);
            }
        }

        for (shortcut, call) in self.hidden_actions.into_iter() {
            shortcuts.register_full(shortcut, call.action, call.action_data.as_deref());
        }

        shortcuts.set_mandatory();
    }

    async fn reset_action(&mut self, parent: &gtk::Window, action: UserAction) {
        let default_shortcuts = self.default_shortcuts.for_call(&Call {
            action,
            action_data: None,
        });

        if self
            .resolve_conflicts(parent, &default_shortcuts, action)
            .await
            && let Some(entry) = self.entries.iter_mut().find(|entry| entry.action == action)
        {
            entry.model_mut().reset_to_defaults(&default_shortcuts);
        }
    }

    async fn add_shortcut(&mut self, parent: &gtk::Window, action: UserAction) {
        if let CaptureOutput::Set(shortcut) = Capture::get_shortcut(parent, action, None).await
            && let Some(index) = self.index_of(action)
            && self
                .entries
                .get(index)
                .is_some_and(|entry| !entry.has_shortcut(shortcut))
            && self.resolve_conflicts(parent, &[shortcut], action).await
            && let Some(entry) = self.entries.get_mut(index)
        {
            entry
                .model_mut()
                .add_shortcut(shortcut, &self.default_shortcuts);
        }
    }

    async fn edit_shortcut(
        &mut self,
        parent: &gtk::Window,
        action: UserAction,
        shortcut: Shortcut,
    ) {
        let Some(index) = self.index_of(action) else {
            return;
        };
        match Capture::get_shortcut(parent, action, Some(&shortcut)).await {
            CaptureOutput::Set(new_shortcut) => {
                if new_shortcut == shortcut {
                    return;
                }

                if let Some(entry) = self.entries.get_mut(index)
                    && entry.has_shortcut(new_shortcut)
                {
                    entry
                        .model_mut()
                        .remove_shortcut(shortcut, &self.default_shortcuts);
                } else if self
                    .resolve_conflicts(parent, &[new_shortcut], action)
                    .await
                    && let Some(entry) = self.entries.get_mut(index)
                {
                    entry.model_mut().replace_shortcut(
                        shortcut,
                        new_shortcut,
                        &self.default_shortcuts,
                    );
                }
            }
            CaptureOutput::Removed => {
                if let Some(entry) = self.entries.get_mut(index) {
                    entry
                        .model_mut()
                        .remove_shortcut(shortcut, &self.default_shortcuts);
                }
            }
            CaptureOutput::Cancelled => (),
        }
    }

    async fn resolve_conflicts(
        &mut self,
        parent: &gtk::Window,
        shortcuts: &[Shortcut],
        assigned_to: UserAction,
    ) -> bool {
        let assigned_area = assigned_to.area();

        loop {
            let mut remove = None;
            for (index, (shortcut, call)) in self.hidden_actions.iter().enumerate() {
                if shortcuts.contains(shortcut) && call.action.area().intersects(assigned_area) {
                    remove = Some((index, call.action, *shortcut));
                    break;
                }
            }
            if let Some((index, action, shortcut)) = remove {
                if !conflict_confirm(parent, action, shortcut).await {
                    return false;
                }

                // Remove the conflict and re-run the check
                self.hidden_actions.remove(index);
            } else {
                break;
            }
        }

        for entry in self.entries.iter_mut() {
            let action = entry.action;
            if action == assigned_to || !action.area().intersects(assigned_area) {
                continue;
            }

            loop {
                let mut remove = None;
                for shortcut in entry.shortcuts() {
                    if shortcuts.contains(&shortcut) {
                        remove = Some(shortcut);
                        break;
                    }
                }

                if let Some(shortcut) = remove {
                    if !conflict_confirm(parent, action, shortcut).await {
                        return false;
                    }

                    // Remove the conflict and re-run the check
                    entry
                        .model_mut()
                        .remove_shortcut(shortcut, &self.default_shortcuts);
                } else {
                    break;
                }
            }
        }

        true
    }
}

fn find_text(widget: &gtk::Widget, filter_text: &str) -> bool {
    if let Some(label) = widget.downcast_ref::<gtk::Label>() {
        label.text().to_lowercase().contains(filter_text)
    } else if let Some(container) = widget.downcast_ref::<gtk::Box>() {
        let mut child = container.first_child();
        while let Some(unwrapped) = child {
            if find_text(&unwrapped, filter_text) {
                return true;
            }
            child = unwrapped.next_sibling();
        }
        false
    } else {
        false
    }
}

fn filter_row(row: &gtk::ListBoxRow, filter_text: &str, modified_only: bool) -> bool {
    if filter_text.is_empty() && !modified_only {
        return true;
    }

    let Some(widget) = row.child() else {
        return false;
    };

    if modified_only && !widget.has_css_class("keyboard-shortcuts-modified") {
        return false;
    }

    filter_text.is_empty() || find_text(&widget, filter_text)
}

fn update_row_header(row: &gtk::ListBoxRow, previous_row: Option<&gtk::ListBoxRow>) {
    fn get_area(row: &gtk::ListBoxRow) -> Option<Area> {
        row.child().and_then(|child| {
            for css_class in child.css_classes() {
                if let Some(area_name) = css_class.strip_prefix("area-") {
                    return Some(Area::from_str(area_name));
                }
            }
            None
        })
    }

    let Some(area) = get_area(row) else {
        return;
    };

    if previous_row
        .and_then(get_area)
        .is_none_or(|previous_area| previous_area != area)
    {
        if row.header().is_none() {
            row.set_header(Some(
                &gtk::Label::builder()
                    .label(area.label())
                    .halign(gtk::Align::Start)
                    .css_classes(["keyboard-shortcuts-section-header"])
                    .build(),
            ));
        }
    } else {
        row.set_header(gtk::Widget::NONE);
    }
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
