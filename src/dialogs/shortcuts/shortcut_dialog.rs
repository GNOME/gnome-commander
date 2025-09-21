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
use crate::{
    shortcuts::{Call, Shortcut},
    user_actions::USER_ACTIONS,
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, pango, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        hintbox::hintbox,
        utils::{NO_BUTTONS, SenderExt, dialog_button_box},
    };
    use std::cell::Cell;

    fn actions_model() -> gio::ListStore {
        let actions_model = gio::ListStore::new::<glib::BoxedAnyObject>();
        for user_action in &*USER_ACTIONS {
            if user_action.parameter_type.is_none() {
                // skip parametrized actions
                actions_model.append(&glib::BoxedAnyObject::new(Call {
                    action_name: user_action.action_name.to_owned(),
                    action_data: None,
                }));
            }
        }
        actions_model
    }

    fn action_factory(ellipsize: bool) -> gtk::ListItemFactory {
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(move |_, list_item| {
            let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            if ellipsize {
                list_item.set_child(Some(
                    &gtk::Label::builder()
                        .xalign(0.0)
                        .max_width_chars(1)
                        .width_request(200)
                        .ellipsize(pango::EllipsizeMode::End)
                        .build(),
                ));
            } else {
                list_item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
            }
        });
        factory.connect_bind(|_, list_item| {
            let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(action) = list_item.item().and_downcast::<glib::BoxedAnyObject>() else {
                return;
            };
            let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
                return;
            };
            label.set_label(&action_description(&action.borrow::<Call>().action_name));
        });
        factory.upcast()
    }

    pub struct ShortcutDialog {
        action_widget: gtk::DropDown,
        shortcut_widget: gtk::ShortcutLabel,
        shortcut_button: gtk::Button,
        capturing: Cell<bool>,
        shortcut: Cell<Option<Shortcut>>,
        sender: async_channel::Sender<Option<(Shortcut, Call)>>,
        pub receiver: async_channel::Receiver<Option<(Shortcut, Call)>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ShortcutDialog {
        const NAME: &'static str = "GnomeCmdShortcutDialog";
        type Type = super::ShortcutDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            let (sender, receiver) = async_channel::bounded(1);
            Self {
                action_widget: gtk::DropDown::builder()
                    .model(&actions_model())
                    .factory(&action_factory(true))
                    .list_factory(&action_factory(false))
                    .width_request(200)
                    .hexpand(true)
                    .build(),
                shortcut_widget: gtk::ShortcutLabel::builder().hexpand(true).build(),
                shortcut_button: gtk::Button::builder().build(),
                capturing: Default::default(),
                shortcut: Default::default(),
                sender,
                receiver,
            }
        }
    }

    impl ObjectImpl for ShortcutDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dialog = self.obj();

            dialog.set_title(Some(&gettext("Keyboard Shortcut")));
            dialog.set_resizable(false);
            dialog.set_modal(true);

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .row_spacing(6)
                .column_spacing(6)
                .build();
            dialog.set_child(Some(&grid));

            let action_label = gtk::Label::builder()
                .label(gettext("_Action"))
                .halign(gtk::Align::Start)
                .use_underline(true)
                .mnemonic_widget(&self.action_widget)
                .build();

            grid.attach(&action_label, 0, 0, 1, 1);
            grid.attach(&self.action_widget, 1, 0, 2, 1);

            let shortcut_label = gtk::Label::builder()
                .label(gettext("_Shortcut"))
                .halign(gtk::Align::Start)
                .use_underline(true)
                .mnemonic_widget(&self.shortcut_button)
                .build();

            grid.attach(&shortcut_label, 0, 1, 1, 1);
            grid.attach(&self.shortcut_widget, 1, 1, 1, 1);
            grid.attach(&self.shortcut_button, 2, 1, 1, 1);

            self.end_capture();
            self.shortcut_button.connect_clicked(glib::clone!(
                #[weak(rename_to=imp)]
                self,
                move |_| imp.start_capture()
            ));

            grid.attach(
                &hintbox(&gettext("To edit a shortcut key, click on the \"Assign\" button and type a new accelerator, or press escape to cancel.")),
                0,
                2,
                3,
                1
            );

            let cancel_button = gtk::Button::builder()
                .label(&gettext("_Cancel"))
                .use_underline(true)
                .hexpand(true)
                .halign(gtk::Align::End)
                .build();
            cancel_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(None)
            ));

            let ok_button = gtk::Button::builder()
                .label(&gettext("_OK"))
                .use_underline(true)
                .build();
            ok_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.ok_clicked().await });
                }
            ));

            grid.attach(
                &dialog_button_box(NO_BUTTONS, &[&cancel_button, &ok_button]),
                0,
                3,
                3,
                1,
            );

            let key_controller = gtk::EventControllerKey::new();
            key_controller.set_propagation_phase(gtk::PropagationPhase::Capture);
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to=imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, modifier| imp.capture(key, modifier)
            ));
            dialog.add_controller(key_controller);

            dialog.set_default_widget(Some(&ok_button));

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_, key, _, modifier| {
                    if key == gdk::Key::Escape && modifier.is_empty() {
                        sender.toss(None);
                        glib::Propagation::Stop
                    } else {
                        glib::Propagation::Proceed
                    }
                }
            ));
            dialog.add_controller(key_controller);
        }
    }

    impl WidgetImpl for ShortcutDialog {}
    impl WindowImpl for ShortcutDialog {}

    impl ShortcutDialog {
        pub fn set_action(&self, call: &Call) {
            let position = self
                .action_widget
                .model()
                .and_then(|model| {
                    model.into_iter().position(|i| {
                        i.ok()
                            .and_then(|obj| obj.downcast::<glib::BoxedAnyObject>().ok())
                            .map(|any| *any.borrow::<Call>() == *call)
                            .unwrap_or_default()
                    })
                })
                .and_then(|pos| pos.try_into().ok());

            if let Some(position) = position {
                self.action_widget.set_selected(position);
            } else {
                eprintln!("Unknown action {}", call.action_name);
            }
            self.action_widget.set_sensitive(false);
        }

        pub fn set_shortcut(&self, shortcut: Shortcut) {
            self.shortcut.set(Some(shortcut));
            self.shortcut_widget.set_accelerator(&shortcut.name());
        }

        fn start_capture(&self) {
            self.capturing.set(true);
            self.shortcut_button.set_label(&gettext("New shortcut..."));
        }

        fn end_capture(&self) {
            self.capturing.set(false);
            self.shortcut_button.set_label(&gettext("Assign"));
        }

        fn capture(&self, key: gdk::Key, modifier: gdk::ModifierType) -> glib::Propagation {
            if !self.capturing.get() {
                return glib::Propagation::Proceed;
            }

            if !gtk::accelerator_valid(key, modifier) {
                return glib::Propagation::Proceed;
            }

            if key == gdk::Key::Cancel && modifier.is_empty() {
                // Cancel capturing
            } else {
                self.set_shortcut(Shortcut {
                    key,
                    state: modifier,
                });
            }

            self.end_capture();
            glib::Propagation::Stop
        }

        async fn ok_clicked(&self) {
            let Some(action) = self
                .action_widget
                .selected_item()
                .and_downcast::<glib::BoxedAnyObject>()
                .map(|any| any.borrow::<Call>().clone())
            else {
                ErrorMessage::brief(gettext("No action is selected."))
                    .show(self.obj().upcast_ref())
                    .await;
                return;
            };

            let Some(shortcut) = self.shortcut.get() else {
                ErrorMessage::brief(gettext("No shortcut is defined."))
                    .show(self.obj().upcast_ref())
                    .await;
                return;
            };

            let call = Call {
                action_name: action.action_name,
                action_data: None,
            };
            self.sender.toss(Some((shortcut, call)));
        }
    }
}

glib::wrapper! {
    pub struct ShortcutDialog(ObjectSubclass<imp::ShortcutDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl Default for ShortcutDialog {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ShortcutDialog {
    pub async fn run(
        parent: &gtk::Window,
        current_shortcut: Option<ShortcutAction>,
        existing_actions: &gio::ListModel,
    ) -> Option<ShortcutAction> {
        let dialog = Self::default();
        dialog.set_transient_for(Some(parent));
        if let Some(ShortcutAction {
            ref shortcut,
            ref call,
        }) = current_shortcut
        {
            dialog.imp().set_shortcut(*shortcut);
            dialog.imp().set_action(call);
        }
        dialog.present();

        let result = loop {
            let response = dialog.imp().receiver.recv().await;
            let Ok(Some((shortcut, call))) = response else {
                break None;
            };

            if Some(shortcut) != current_shortcut.as_ref().map(|s| s.shortcut) {
                if let Some(existing_action) = find_action(shortcut, existing_actions) {
                    if !conflict_confirm(dialog.upcast_ref(), &existing_action, shortcut).await {
                        continue;
                    }
                }
            }

            break Some(ShortcutAction { shortcut, call });
        };

        dialog.close();
        result
    }
}

fn find_action(shortcut: Shortcut, existing: &gio::ListModel) -> Option<String> {
    existing.into_iter().find_map(|i| {
        i.ok()
            .and_downcast::<glib::BoxedAnyObject>()
            .map(|o| o.borrow::<ShortcutAction>().clone())
            .filter(|ua| ua.shortcut == shortcut)
            .map(|ua| action_description(&ua.call.action_name))
    })
}

async fn conflict_confirm(parent: &gtk::Window, action: &str, accel: Shortcut) -> bool {
    gtk::AlertDialog::builder()
        .modal(true)
        .message(
            gettext("Shortcut “{shortcut}” is already taken by “{action}”.")
                .replace("{shortcut}", &accel.label())
                .replace("{action}", action),
        )
        .detail(
            gettext("Reassigning the shortcut will cause it to be removed from “{action}”.")
                .replace("{action}", action),
        )
        .buttons([gettext("_Cancel"), gettext("_Reassign shortcut")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent))
        .await
        == Ok(1)
}
