// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{shortcuts::Shortcut, utils::NO_MOD};
use component_framework::prelude::*;
use gettextrs::gettext;
use gtk::{gdk, glib, prelude::*};
use std::borrow::Cow;

#[derive(Debug, Default)]
pub struct CaptureView {
    dialog: gtk::Window,
    instructions: gtk::Label,
}

#[derive(Debug)]
pub enum CaptureInput {
    Key(gdk::Key, gdk::ModifierType),
}

#[derive(Debug, Default)]
pub enum CaptureOutput {
    #[default]
    Cancelled,
    Removed,
    Set(Shortcut),
}

pub struct Capture {
    action: String,
    existing_key: Option<Shortcut>,
    state: u8,
}

impl Component for Capture {
    type View = CaptureView;
    type Input = CaptureInput;
    type Output = CaptureOutput;
    type Root = gtk::Window;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = Self::View::default();
        with!(&view.dialog {
            .add_css_class("dialog");
            .set_modal(true);
            .set_title(Some(&if self.existing_key.is_some() {
                gettext("Edit Shortcut")
            } else {
                gettext("Add Shortcut")
            }));
            .set_width_request(500);
            .set_resizable(false);

            .add_controller(with!(gtk::EventControllerKey {
                .set_propagation_phase(gtk::PropagationPhase::Capture);
                .connect_key_pressed(glib::clone!(
                    #[strong]
                    sender,
                    move |_, key, _, modifiers| {
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
                            sender.input(Self::Input::Key(key, modifiers));
                            glib::Propagation::Stop
                        } else {
                            glib::Propagation::Proceed
                        }
                    }
                ));
            }));

            gtk::Box {
                .set_orientation(gtk::Orientation::Vertical);

                gtk::Label {
                    .set_label(&if let Some(existing_key) = self.existing_key {
                        gettext("Enter a shortcut to replace “{shortcut}” for action “{action}.”")
                            .replace("{action}", &self.action)
                            .replace("{shortcut}", &existing_key.label())
                    } else {
                        gettext("Enter a new shortcut for action “{action}.”")
                            .replace("{action}", &self.action)
                    });
                    .set_wrap(true);
                    .set_wrap_mode(gtk::pango::WrapMode::Word);
                    .set_max_width_chars(1);
                    .set_justify(gtk::Justification::Center);
                }

                &view.instructions {
                    .add_css_class("keyboard-shortcuts-key-capture-instructions");
                    .set_label(&{
                        let mut instructions_text = gettext(
                            "Press the desired key combination on your keyboard."
                        );
                        instructions_text += " ";
                        instructions_text += &gettext("Press Esc twice to cancel.");
                        if self.existing_key.is_some() {
                            instructions_text += " ";
                            instructions_text += &gettext(
                                "Press Backspace twice to remove the shortcut."
                            );
                        }
                        instructions_text
                    });
                    .set_wrap(true);
                    .set_wrap_mode(gtk::pango::WrapMode::Word);
                    .set_max_width_chars(1);
                    .set_justify(gtk::Justification::Left);
                }

                gtk::Box {
                    .set_orientation(gtk::Orientation::Horizontal);
                    .add_css_class("spacing");

                    gtk::Button {
                        .set_label(&gettext("Cancel"));
                        .set_as_cancel();
                        .set_focusable(false);
                        .set_hexpand(true);
                        .set_halign(gtk::Align::End);
                        .connect_clicked(forward!(sender.output(Self::Output::Cancelled)));
                    }

                    if self.existing_key.is_some() {
                        gtk::Button {
                            .set_label(&gettext("Remove Shortcut"));
                            .set_focusable(false);
                            .connect_clicked(forward!(sender.output(Self::Output::Removed)));
                        }
                    }
                }
            }
        });

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
            Self::Input::Key(key, modifiers) => {
                if key == gdk::Key::Escape && modifiers == NO_MOD {
                    if self.state == 1 {
                        sender.output(Self::Output::Cancelled);
                    } else {
                        self.state = 1;
                        self.update_view(sender, view);
                    }
                } else if key == gdk::Key::BackSpace
                    && modifiers == NO_MOD
                    && self.existing_key.is_some()
                {
                    if self.state == 2 {
                        sender.output(Self::Output::Removed);
                    } else {
                        self.state = 2;
                        self.update_view(sender, view);
                    }
                } else if (key == gdk::Key::Return || key == gdk::Key::KP_Enter)
                    && modifiers == NO_MOD
                    && self.state > 0
                {
                    sender.output(Self::Output::Set(Shortcut::new(
                        if self.state == 1 {
                            gdk::Key::Escape
                        } else {
                            gdk::Key::BackSpace
                        },
                        NO_MOD,
                    )));
                } else {
                    let shortcut = Shortcut::new(key, modifiers);
                    if shortcut.is_mandatory() {
                        mandatory_notify(&view.dialog, shortcut).await;
                    } else {
                        sender.output(Self::Output::Set(shortcut));
                    }
                }
            }
        }
    }

    fn update_view(&self, _sender: &ComponentSender<Self>, view: &mut Self::View) {
        if self.state == 1 {
            view.instructions.set_label(&gettext(
                "Press Esc again to cancel or Enter to use Esc as shortcut key.",
            ));
        } else if self.state == 2 {
            view.instructions.set_label(&gettext(
                "Press Backspace again to remove the shortcut or Enter to use Backspace as shortcut key."
            ));
        }
    }
}

impl Capture {
    pub async fn get_shortcut(
        parent: &gtk::Window,
        action: Cow<'_, str>,
        existing_key: Option<&Shortcut>,
    ) -> CaptureOutput {
        let mut controller = Self {
            action: action.to_string(),
            existing_key: existing_key.copied(),
            state: 0,
        }
        .build();

        controller.root().set_transient_for(Some(parent));
        controller.root().present();

        let result = controller.receive().await.unwrap_or_default();
        controller.root().close();
        result
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
