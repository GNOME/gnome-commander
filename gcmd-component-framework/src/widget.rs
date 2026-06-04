// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::{gdk, glib, prelude::*};

pub trait GcmdWidgetExt {
    /// Sets this widget as default widget for its window. It will be triggered if Enter is pressed.
    fn set_as_default(&self);

    /// Sets this widget as cancel widget for its window. It will be triggered if Escape is pressed
    /// or the window frame’s close button is clicked.
    fn set_as_cancel(&self);
}

impl<T> GcmdWidgetExt for T
where
    T: AsRef<gtk::Widget>,
{
    fn set_as_default(&self) {
        with_window(self.as_ref(), set_as_default);
    }

    fn set_as_cancel(&self) {
        with_window(self.as_ref(), set_as_cancel);
    }
}

fn with_window(widget: &gtk::Widget, handler: impl Fn(&gtk::Window, &gtk::Widget) + 'static) {
    if let Some(root) = widget.root() {
        if let Some(window) = root.downcast_ref() {
            handler(window, widget);
        }
    } else {
        widget.connect_realize(move |widget| {
            if let Some(window) = widget.root().and_downcast() {
                handler(&window, widget);
            }
        });
    }
}

fn set_as_default(window: &gtk::Window, widget: &gtk::Widget) {
    window.set_default_widget(Some(widget));
}

fn set_as_cancel(window: &gtk::Window, widget: &gtk::Widget) {
    window.connect_close_request(glib::clone!(
        #[weak]
        widget,
        #[upgrade_or]
        glib::Propagation::Proceed,
        move |_| {
            // Activating won’t trigger clicked signal at this stage, so emit it explicitly.
            if let Some(button) = widget.downcast_ref::<gtk::Button>() {
                button.emit_clicked();
            };
            glib::Propagation::Proceed
        }
    ));

    let widget = widget.clone();
    let controller = gtk::ShortcutController::new();
    controller.add_shortcut(
        gtk::Shortcut::builder()
            .trigger(&gtk::KeyvalTrigger::new(
                gdk::Key::Escape,
                gdk::ModifierType::empty(),
            ))
            .action(&gtk::CallbackAction::new(glib::clone!(
                #[weak]
                widget,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, _| {
                    widget.activate();
                    glib::Propagation::Stop
                }
            )))
            .build(),
    );
    window.add_controller(controller);
}
