/*
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

use super::user_action::{ShortcutAction, action_description};
use crate::shortcuts::Shortcuts;
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::error::Error;

mod imp {
    use super::*;
    use crate::{
        dialogs::shortcuts::shortcut_dialog::ShortcutDialog,
        utils::{SenderExt, dialog_button_box, display_help, key_sorter},
    };

    pub struct ShortcutsDialog {
        pub store: gio::ListStore,
        pub selection: gtk::SingleSelection,
        pub view: gtk::ColumnView,
        sender: async_channel::Sender<bool>,
        pub receiver: async_channel::Receiver<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ShortcutsDialog {
        const NAME: &'static str = "GnomeCmdShortcutsDialog";
        type Type = super::ShortcutsDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            let (sender, receiver) = async_channel::bounded(1);
            Self {
                store: gio::ListStore::new::<glib::BoxedAnyObject>(),
                selection: gtk::SingleSelection::builder().build(),
                view: gtk::ColumnView::builder().build(),
                sender,
                receiver,
            }
        }
    }

    impl ObjectImpl for ShortcutsDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dialog = self.obj();

            dialog.set_title(Some(&gettext("Keyboard Shortcuts")));
            dialog.set_destroy_with_parent(true);
            dialog.set_size_request(800, 600);
            dialog.set_resizable(true);

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .row_spacing(6)
                .column_spacing(6)
                .build();
            dialog.set_child(Some(&grid));

            let sort_model = gtk::SortListModel::new(Some(self.store.clone()), self.view.sorter());

            self.selection.set_model(Some(&sort_model));
            self.view.set_model(Some(&self.selection));

            let shortcut_column = gtk::ColumnViewColumn::builder()
                .title(gettext("Shortcut Key"))
                .resizable(true)
                .sorter(&key_sorter(|obj| {
                    obj.downcast_ref::<glib::BoxedAnyObject>()
                        .map(|o| o.borrow::<ShortcutAction>())
                        .map(|u| u.shortcut)
                }))
                .factory(&create_accel_factory())
                .build();

            let action_column = gtk::ColumnViewColumn::builder()
                .title(gettext("Action"))
                .resizable(true)
                .sorter(&key_sorter(|obj| {
                    obj.downcast_ref::<glib::BoxedAnyObject>()
                        .map(|o| o.borrow::<ShortcutAction>())
                        .map(|u| u.call.action_name.clone())
                }))
                .factory(&create_action_factory())
                .expand(true)
                .build();

            let data_column = gtk::ColumnViewColumn::builder()
                .title(gettext("Options"))
                .resizable(true)
                .sorter(&key_sorter(|obj| {
                    obj.downcast_ref::<glib::BoxedAnyObject>()
                        .map(|o| o.borrow::<ShortcutAction>())
                        .map(|u| u.call.action_data.clone())
                }))
                .factory(&create_option_factory())
                .expand(true)
                .build();

            self.view.append_column(&shortcut_column);
            self.view.append_column(&action_column);
            self.view.append_column(&data_column);
            self.view
                .sort_by_column(Some(&action_column), gtk::SortType::Ascending);

            let scrolled_window = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Never)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .has_frame(true)
                .child(&self.view)
                .hexpand(true)
                .vexpand(true)
                .build();
            grid.attach(&scrolled_window, 0, 0, 1, 1);

            let actions_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(6)
                .build();
            grid.attach(&actions_box, 1, 0, 1, 1);

            let add_button = gtk::Button::builder()
                .label(&gettext("_Add"))
                .use_underline(true)
                .build();
            actions_box.append(&add_button);
            add_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.add().await });
                }
            ));

            let edit_button = gtk::Button::builder()
                .label(&gettext("_Edit"))
                .use_underline(true)
                .build();
            actions_box.append(&edit_button);
            edit_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.edit_selected().await });
                }
            ));

            let remove_button = gtk::Button::builder()
                .label(&gettext("_Remove"))
                .use_underline(true)
                .build();
            actions_box.append(&remove_button);
            remove_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.remove_selected()
            ));

            let help_button = gtk::Button::builder()
                .label(&gettext("_Help"))
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
                .label(&gettext("_Cancel"))
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
                .label(&gettext("_OK"))
                .use_underline(true)
                .build();
            ok_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(true)
            ));

            grid.attach(
                &dialog_button_box(&[&help_button], &[&cancel_button, &ok_button]),
                0,
                1,
                2,
                1,
            );

            dialog.set_default_widget(Some(&ok_button));
        }
    }

    impl WidgetImpl for ShortcutsDialog {}
    impl WindowImpl for ShortcutsDialog {}

    impl ShortcutsDialog {
        async fn add(&self) {
            if let Some(shortcut_action) =
                ShortcutDialog::run(self.obj().upcast_ref(), None, self.store.upcast_ref()).await
            {
                self.store
                    .append(&glib::BoxedAnyObject::new(shortcut_action));
            }
        }

        async fn edit_selected(&self) {
            if let Some(selected) = self.selection.selected_item() {
                if let Some(position) = self.store.find(&selected) {
                    let action = selected
                        .downcast::<glib::BoxedAnyObject>()
                        .map(|o| o.borrow::<ShortcutAction>().clone())
                        .ok();

                    if let Some(shortcut_action) = ShortcutDialog::run(
                        self.obj().upcast_ref(),
                        action,
                        self.store.upcast_ref(),
                    )
                    .await
                    {
                        self.store.splice(
                            position,
                            1,
                            &[glib::BoxedAnyObject::new(shortcut_action)],
                        );
                    }
                }
            }
        }

        fn remove_selected(&self) {
            if let Some(selected) = self.selection.selected_item() {
                if let Some(position) = self.store.find(&selected) {
                    self.store.remove(position);
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
    pub async fn run(parent: &gtk::Window, shortcuts: &Shortcuts) {
        let dialog = Self::default();
        dialog.set_transient_for(Some(parent));
        fill_model(&dialog.imp().store, shortcuts);

        dialog.present();
        dialog.imp().view.grab_focus();

        let response = dialog.imp().receiver.recv().await;
        if response == Ok(true) {
            if let Err(error) = shortcuts_from_model(&dialog.imp().store, shortcuts) {
                eprintln!("Failed to save shortcuts: {}", error);
            }
        }
        dialog.close();
    }
}

fn create_accel_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, list_item| {
        let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let label = gtk::ShortcutLabel::builder().build();
        list_item.set_child(Some(&label));
    });
    factory.connect_bind(|_, list_item| {
        let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(obj) = list_item.item().and_downcast::<glib::BoxedAnyObject>() else {
            return;
        };
        let user_action = obj.borrow::<ShortcutAction>();
        let Some(label) = list_item.child().and_downcast::<gtk::ShortcutLabel>() else {
            return;
        };
        label.set_accelerator(&user_action.shortcut.name());
    });
    factory.upcast()
}

fn create_action_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, list_item| {
        let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let label = gtk::Label::builder().xalign(0.0).build();
        list_item.set_child(Some(&label));
    });
    factory.connect_bind(|_, list_item| {
        let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(obj) = list_item.item().and_downcast::<glib::BoxedAnyObject>() else {
            return;
        };
        let user_action = obj.borrow::<ShortcutAction>();
        let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_label(&action_description(&user_action.call.action_name));
    });
    factory.upcast()
}

fn create_option_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, list_item| {
        let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let label = gtk::Label::builder().xalign(0.0).build();
        list_item.set_child(Some(&label));
    });
    factory.connect_bind(|_, list_item| {
        let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(obj) = list_item.item().and_downcast::<glib::BoxedAnyObject>() else {
            return;
        };
        let user_action = obj.borrow::<ShortcutAction>();
        let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_label(user_action.call.action_data.as_deref().unwrap_or_default());
    });
    factory.upcast()
}

fn fill_model(model: &gio::ListStore, shotcuts: &Shortcuts) {
    for (shortcut, call) in shotcuts.all() {
        // ignore lowercase keys as they duplicate uppercase ones
        if shortcut.key.is_upper() {
            model.append(&glib::BoxedAnyObject::new(ShortcutAction {
                shortcut,
                call,
            }));
        }
    }
}

fn shortcuts_from_model(
    model: &gio::ListStore,
    shortcuts: &Shortcuts,
) -> Result<(), Box<dyn Error>> {
    shortcuts.clear();

    for item in model.iter() {
        let obj: glib::BoxedAnyObject = item?;
        let user_action = obj.borrow::<ShortcutAction>();

        let shortcut = user_action.shortcut;
        let action_name = &user_action.call.action_name;
        let action_data = &user_action.call.action_data;

        if !action_name.is_empty() {
            shortcuts.register_full(shortcut, &action_name, action_data.as_deref());
        }
    }

    shortcuts.set_mandatory();
    Ok(())
}
