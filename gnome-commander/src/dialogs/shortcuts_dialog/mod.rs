// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod action_entry;
mod capture;
mod shortcut_entry;

use crate::{
    connection::{ConnectionExt, list::ConnectionList},
    main_win::MainWindow,
    plugins::{
        ApiRequestToPlugin, ApiResponseFromPlugin, MessageFromPluginHost, MessageToPluginHost,
        PluginHostChannel,
    },
    shortcuts::{Area, Call, Shortcut, Shortcuts},
    user_actions::{BookmarkActionVariant, PluginActionVariant, UserAction},
    utils::{NO_MOD, SHIFT, display_help},
};
use action_entry::ActionEntry;
use capture::{Capture, CaptureOutput};
use component_framework::prelude::*;
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::ops::Deref;

#[derive(Debug, Default)]
pub struct ShortcutsDialogView {
    dialog: gtk::Window,
    search_field: gtk::SearchEntry,
    modified_only: gtk::CheckButton,
    list: gtk::ListBox,
}

#[derive(Debug)]
pub enum ShortcutsDialogInput {
    AddPluginEntries(Vec<(String, PluginActionVariant)>),
    UpdateFilter,
    ResetSearch,
    ResetAction(usize),
    AddActionShortcut(usize),
    EditActionShortcut(usize, Shortcut),
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
    remaining_shortcuts: Vec<(Shortcut, Call)>,
    default_shortcuts: Shortcuts,
}

impl Component for ShortcutsDialog {
    type View = ShortcutsDialogView;
    type Input = ShortcutsDialogInput;
    type Output = ShortcutsDialogOutput;
    type Root = gtk::Window;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = Self::View::default();

        with!(&view.dialog {
            .add_css_class("dialog");
            .set_title(Some(&gettext("Keyboard Shortcuts")));
            .set_destroy_with_parent(true);
            .set_size_request(800, 600);
            .set_resizable(true);

            gtk::Box {
                .set_orientation(gtk::Orientation::Vertical);

                &view.search_field {
                    .set_placeholder_text(Some(&gettext("Search shortcuts…")));
                    .set_activates_default(false);
                    .set_search_delay(50);

                    .connect_search_changed(forward!(sender.input(Self::Input::UpdateFilter)));

                    .connect_stop_search(glib::clone!(
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

                &view.modified_only {
                    .set_label(Some(&gettext("Show only _modified shortcuts")));
                    .set_use_underline(true);
                    .connect_toggled(forward!(sender.input(Self::Input::UpdateFilter)));
                }

                gtk::ScrolledWindow {
                    .set_hscrollbar_policy(gtk::PolicyType::Never);
                    .set_vscrollbar_policy(gtk::PolicyType::Automatic);
                    .set_has_frame(true);
                    .set_hexpand(true);
                    .set_vexpand(true);

                    &view.list {
                        .set_selection_mode(gtk::SelectionMode::None);
                        .set_show_separators(true);
                        .add_css_class("rich-list");
                        .set_header_func(update_row_header);

                        .connect_row_activated(glib::clone!(
                            #[strong]
                            sender,
                            move |_, row| {
                                sender.input(Self::Input::RowActivate(row.index()));
                            },
                        ));

                        .add_controller(with!(gtk::EventControllerKey {
                            .connect_key_pressed(glib::clone!(
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
                        }));

                        for entry in &self.entries {
                            entry.attach(sender, |message| message) {}
                        }
                    }
                }

                gtk::Box {
                    .add_css_class("spacing");
                    .set_orientation(gtk::Orientation::Horizontal);

                    gtk::Button {
                        .set_label(&gettext("_Help"));
                        .set_use_underline(true);

                        .connect_clicked(forward!(sender.input(Self::Input::DisplayHelp)));
                    }

                    gtk::Button {
                        .set_label(&gettext("_Cancel"));
                        .set_use_underline(true);
                        .set_as_cancel();
                        .set_hexpand(true);
                        .set_halign(gtk::Align::End);

                        .connect_clicked(forward!(sender.output(Self::Output::Cancelled)));
                    }

                    gtk::Button {
                        .set_label(&gettext("_Save Shortcuts"));
                        .set_use_underline(true);
                        .connect_clicked(forward!(sender.output(Self::Output::Accepted)));
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
        sender: &ComponentSender<Self>,
        view: &mut Self::View,
    ) {
        match msg {
            Self::Input::AddPluginEntries(menu_items) => {
                for (description, action) in menu_items {
                    let action_data = action.to_variant().print(false).to_string();

                    let shortcuts: Vec<_> = self
                        .remaining_shortcuts
                        .extract_if(.., |(_, call)| {
                            call.action == UserAction::PluginAction
                                && call.action_data.as_ref() == Some(&action_data)
                        })
                        .map(|(shortcut, _)| shortcut)
                        .collect();
                    let mut entry = ActionEntry::new(
                        UserAction::PluginAction,
                        Some(action_data),
                        shortcuts,
                        &self.default_shortcuts,
                    );
                    entry.override_description(description);
                    entry.index = self.entries.len();

                    let controller = entry.build();
                    view.list
                        .append(controller.attach(sender, |message| message));
                    self.entries.push(controller);
                }
            }
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
            Self::Input::ResetAction(index) => {
                self.reset_action(&view.dialog, index).await;
            }
            Self::Input::AddActionShortcut(index) => {
                self.add_shortcut(&view.dialog, index).await;
            }
            Self::Input::EditActionShortcut(index, shortcut) => {
                self.edit_shortcut(&view.dialog, index, shortcut).await;
            }
            Self::Input::RowActivate(row) => {
                if let Ok(index) = usize::try_from(row) {
                    self.add_shortcut(&view.dialog, index).await;
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

    async fn handle_subcomponents(&mut self) {
        Self::handle_subcomponent_list(&mut self.entries).await;
    }
}

impl ShortcutsDialog {
    fn new(shortcuts: &Shortcuts) -> Self {
        let mut shortcuts: Vec<_> = shortcuts.all();
        shortcuts.retain(|(shortcut, _)| !shortcut.is_mandatory());

        let default_shortcuts = Shortcuts::new();
        default_shortcuts.set_default();

        let mut entries: Vec<_> = UserAction::all()
            .filter(|action| !action.has_parameter())
            .map(|action| {
                let shortcuts: Vec<_> = shortcuts
                    .extract_if(.., |(_, call)| call.action == action)
                    .map(|(shortcut, _)| shortcut)
                    .collect();
                ActionEntry::new(action, None, shortcuts, &default_shortcuts)
            })
            .collect();
        entries.sort_by_cached_key(|entry| {
            (
                entry.action.area(),
                entry.action.description().to_lowercase(),
            )
        });

        let mut bookmark_entries = Vec::new();
        for connection in ConnectionList::get().iter() {
            for bookmark in &*connection.bookmarks() {
                let action_data = BookmarkActionVariant::new(&connection, bookmark)
                    .to_variant()
                    .print(false)
                    .to_string();

                let shortcuts: Vec<_> = shortcuts
                    .extract_if(.., |(_, call)| {
                        call.action == UserAction::BookmarksGoto
                            && call.action_data.as_ref() == Some(&action_data)
                    })
                    .map(|(shortcut, _)| shortcut)
                    .collect();
                let mut entry = ActionEntry::new(
                    UserAction::BookmarksGoto,
                    Some(action_data),
                    shortcuts,
                    &default_shortcuts,
                );
                entry.override_description(format!(
                    "{} ⮞ {}",
                    connection.display_name(),
                    bookmark.name()
                ));
                bookmark_entries.push(entry);
            }
        }
        bookmark_entries.sort_by_cached_key(|entry| entry.description().to_lowercase());
        entries.extend(bookmark_entries);

        for (index, entry) in entries.iter_mut().enumerate() {
            entry.index = index;
        }

        Self {
            entries: entries.into_iter().map(|action| action.build()).collect(),
            remaining_shortcuts: shortcuts,
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

            retrieve_plugin_items(parent.plugin_channel(), controller.sender().clone());

            let result = controller.receive().await;
            controller.root().close();

            if matches!(result, Ok(ShortcutsDialogOutput::Accepted)) {
                controller.into_model().save_shortcuts(shortcuts);
            }
        }
    }

    fn save_shortcuts(self, shortcuts: &Shortcuts) {
        shortcuts.clear();

        for entry in self.entries.into_iter() {
            let entry = entry.into_model();
            for shortcut in entry.shortcuts.into_iter() {
                let shortcut = shortcut.into_model();
                shortcuts.register_full(
                    shortcut.shortcut,
                    entry.action,
                    entry.action_data.as_deref(),
                );
            }
        }

        shortcuts.set_mandatory();
    }

    async fn reset_action(&mut self, parent: &gtk::Window, index: usize) {
        let Some(ActionEntry { action, .. }) = self.entries.get(index).map(Deref::deref) else {
            return;
        };
        let default_shortcuts = self.default_shortcuts.for_call(&Call {
            action: *action,
            action_data: None,
        });

        if self
            .resolve_conflicts(parent, &default_shortcuts, *action, index)
            .await
            && let Some(entry) = self.entries.iter_mut().find(|entry| entry.index == index)
        {
            entry.model_mut().reset_to_defaults(&default_shortcuts);
        }
    }

    async fn add_shortcut(&mut self, parent: &gtk::Window, index: usize) {
        let Some((action, description)) = self
            .entries
            .get(index)
            .map(|entry| (entry.action, entry.description()))
        else {
            return;
        };
        if let CaptureOutput::Set(shortcut) = Capture::get_shortcut(parent, description, None).await
            && self
                .entries
                .get(index)
                .is_some_and(|entry| !entry.has_shortcut(shortcut))
            && self
                .resolve_conflicts(parent, &[shortcut], action, index)
                .await
            && let Some(entry) = self.entries.get_mut(index)
        {
            entry
                .model_mut()
                .add_shortcut(shortcut, &self.default_shortcuts);
        }
    }

    async fn edit_shortcut(&mut self, parent: &gtk::Window, index: usize, shortcut: Shortcut) {
        let Some((action, description)) = self
            .entries
            .get(index)
            .map(|entry| (entry.action, entry.description()))
        else {
            return;
        };
        match Capture::get_shortcut(parent, description, Some(&shortcut)).await {
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
                    .resolve_conflicts(parent, &[new_shortcut], action, index)
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
        assigned_index: usize,
    ) -> bool {
        let assigned_area = assigned_to.area();

        for entry in self.entries.iter_mut() {
            if entry.index == assigned_index || !entry.action.area().intersects(assigned_area) {
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
                    if !conflict_confirm(parent, &entry.description(), shortcut).await {
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

fn retrieve_plugin_items(mut channel: PluginHostChannel, sender: ComponentSender<ShortcutsDialog>) {
    glib::spawn_future_local(async move {
        let id = channel.new_id();
        channel.send(MessageToPluginHost::ApiRequest {
            id,
            plugin_name: None,
            request: ApiRequestToPlugin::MainMenuItems,
        });

        let mut received = Vec::new();
        loop {
            if let MessageFromPluginHost::ApiResponse {
                id: resp_id,
                plugin_name,
                plugin_display_name,
                response,
                last,
            } = channel.receive().await
                && resp_id == id
            {
                if let Some(ApiResponseFromPlugin::MainMenuItems(items)) = response {
                    received.extend(items.into_iter().map(|item| {
                        (
                            if plugin_display_name.is_empty() {
                                item.label
                            } else {
                                format!("{plugin_display_name} ⮞ {}", item.label)
                            },
                            PluginActionVariant {
                                plugin_name: plugin_name.clone(),
                                action: item.action,
                                parameter: item.parameter,
                            },
                        )
                    }));
                }
                if last {
                    break;
                }
            }
        }

        received.sort_by_cached_key(|(d, PluginActionVariant { plugin_name, .. })| {
            (plugin_name.clone(), d.clone())
        });
        sender.input(ShortcutsDialogInput::AddPluginEntries(received));
    });
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

#[derive(Debug, Eq, PartialEq)]
enum RowType {
    Bookmark,
    Plugin,
    General(Area),
}

impl RowType {
    pub fn section_label(&self) -> String {
        match self {
            Self::Bookmark => gettext("Bookmarks"),
            Self::Plugin => gettext("Plugins"),
            Self::General(area) => area.label(),
        }
    }
}

fn update_row_header(row: &gtk::ListBoxRow, previous_row: Option<&gtk::ListBoxRow>) {
    fn get_type(row: &gtk::ListBoxRow) -> Option<RowType> {
        let child = row.child()?;
        if child.css_classes().iter().any(|cls| cls == "is-bookmark") {
            Some(RowType::Bookmark)
        } else if child.css_classes().iter().any(|cls| cls == "is-plugin") {
            Some(RowType::Plugin)
        } else {
            for css_class in child.css_classes() {
                if let Some(area_name) = css_class.strip_prefix("area-") {
                    return Some(RowType::General(Area::from_name(area_name)));
                }
            }
            None
        }
    }

    let Some(row_type) = get_type(row) else {
        return;
    };

    if previous_row
        .and_then(get_type)
        .is_none_or(|previous_type| previous_type != row_type)
    {
        if row.header().is_none() {
            row.set_header(Some(
                &gtk::Label::builder()
                    .label(row_type.section_label())
                    .halign(gtk::Align::Start)
                    .css_classes(["keyboard-shortcuts-section-header"])
                    .build(),
            ));
        }
    } else {
        row.set_header(gtk::Widget::NONE);
    }
}

async fn conflict_confirm(parent: &gtk::Window, description: &str, accel: Shortcut) -> bool {
    gtk::AlertDialog::builder()
        .modal(true)
        .message(
            gettext("Shortcut “{shortcut}” is already taken by “{action}”.")
                .replace("{shortcut}", &accel.label())
                .replace("{action}", description),
        )
        .detail(
            gettext("Reassigning the shortcut will cause it to be removed from “{action}”.")
                .replace("{action}", description),
        )
        .buttons([gettext("_Cancel"), gettext("_Reassign shortcut")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent))
        .await
        == Ok(1)
}
