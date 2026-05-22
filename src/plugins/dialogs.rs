// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    ApiResponse,
    protocol::{DialogSpec, DialogWidgetValue, WidgetSpec},
};
use crate::utils::{NO_BUTTONS, SenderExt, WindowExt, dialog_button_box};
use gettextrs::gettext;
use gtk::prelude::*;
use std::{
    collections::BTreeMap,
    io::Error,
    pin::Pin,
    task::{Context, Poll},
};
use tokio::sync::{mpsc, mpsc::Receiver};

#[derive(Debug)]
pub struct GenericDialog {
    receiver: Receiver<String>,
    dialog: gtk::Window,
    inputs: Vec<(String, gtk::Widget)>,
}

impl GenericDialog {
    pub fn new(spec: DialogSpec) -> Result<Self, Error> {
        let (sender, receiver) = mpsc::channel(1);

        let dialog = gtk::Window::builder()
            .title(spec.title)
            .modal(spec.modal)
            .build();
        dialog.add_css_class("dialog");

        let mut this = Self {
            receiver,
            dialog: dialog.clone(),
            inputs: Vec::new(),
        };

        let container = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .build();
        container.add_css_class("spacing");
        container.append(&this.create_widget(spec.child));

        let mut buttons = Vec::new();
        let mut seen_cancel_button = false;
        for spec in spec.buttons {
            let button = gtk::Button::builder()
                .label(spec.label)
                .use_underline(true)
                .build();
            if spec.default {
                dialog.set_default_widget(Some(&button));
            }
            if spec.cancel {
                if seen_cancel_button {
                    return Err(Error::other(gettext("Multiple cancel buttons in a dialog")));
                }
                dialog.set_cancel_widget(&button);
                seen_cancel_button = true;
            }

            let sender = sender.clone();
            button.connect_clicked(move |_| {
                sender.toss(spec.id.clone());
            });
            buttons.push(button);
        }
        if !seen_cancel_button {
            return Err(Error::other(gettext("No cancel button in a dialog")));
        }
        container.append(&dialog_button_box(NO_BUTTONS, &buttons));
        dialog.set_child(Some(&container));

        dialog.present();
        Ok(this)
    }

    pub fn cancel(&self) {
        self.dialog.close();
    }

    fn create_widget(&mut self, spec: WidgetSpec) -> gtk::Widget {
        match spec {
            WidgetSpec::HBox { children } => {
                let widget = gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .build();
                widget.add_css_class("spacing");
                for child in children {
                    widget.append(&self.create_widget(child));
                }
                widget.upcast()
            }
            WidgetSpec::VBox { children } => {
                let widget = gtk::Box::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .build();
                widget.add_css_class("spacing");
                for child in children {
                    widget.append(&self.create_widget(child));
                }
                widget.upcast()
            }
            WidgetSpec::Text { label } => gtk::Label::builder()
                .label(label)
                .halign(gtk::Align::Start)
                .build()
                .upcast(),
            WidgetSpec::Input {
                id,
                label,
                value,
                placeholder,
                vertical,
            } => {
                let container = gtk::Box::builder()
                    .orientation(if vertical {
                        gtk::Orientation::Vertical
                    } else {
                        gtk::Orientation::Horizontal
                    })
                    .build();
                container.add_css_class("spacing");
                let text = gtk::Entry::builder()
                    .activates_default(true)
                    .placeholder_text(placeholder)
                    .text(value)
                    .hexpand(true)
                    .vexpand(true)
                    .build();
                let label = gtk::Label::builder()
                    .label(label)
                    .mnemonic_widget(&text)
                    .use_underline(true)
                    .build();
                container.append(&label);
                container.append(&text);
                self.inputs.push((id, text.upcast()));
                container.upcast()
            }
            WidgetSpec::Group { label, child } => gtk::Frame::builder()
                .label(label)
                .child(&self.create_widget(*child))
                .build()
                .upcast(),
            WidgetSpec::Checkbox { id, label, checked } => {
                let checkbox: gtk::Widget = gtk::CheckButton::builder()
                    .label(label)
                    .use_underline(true)
                    .active(checked)
                    .build()
                    .upcast();
                self.inputs.push((id, checkbox.clone()));
                checkbox
            }
            WidgetSpec::RadioGroup { children, vertical } => {
                let container = gtk::Box::builder()
                    .orientation(if vertical {
                        gtk::Orientation::Vertical
                    } else {
                        gtk::Orientation::Horizontal
                    })
                    .build();
                container.add_css_class("spacing");

                let mut first = None;
                for child in children {
                    let widget = self.create_widget(child);
                    container.append(&widget);

                    if let Ok(checkbox) = widget.downcast::<gtk::CheckButton>() {
                        if let Some(group) = first.as_ref() {
                            checkbox.set_group(Some(group));
                        } else {
                            first = Some(checkbox);
                        }
                    }
                }
                container.upcast()
            }
        }
    }
}

impl Future for GenericDialog {
    type Output = ApiResponse;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if let Poll::Ready(Some(button)) = self.receiver.poll_recv(cx) {
            self.dialog.close();

            let mut inputs = BTreeMap::new();
            for (id, widget) in &self.inputs {
                if let Some(input) = widget.downcast_ref::<gtk::Entry>() {
                    inputs.insert(
                        id.to_owned(),
                        DialogWidgetValue::String(input.text().to_string()),
                    );
                } else if let Some(checkbox) = widget.downcast_ref::<gtk::CheckButton>() {
                    inputs.insert(id.to_owned(), DialogWidgetValue::Bool(checkbox.is_active()));
                }
            }
            Poll::Ready(ApiResponse::ShowDialog(button, inputs))
        } else {
            Poll::Pending
        }
    }
}
