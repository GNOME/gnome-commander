// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod component;
pub mod container;
mod controller;
pub mod prelude;
mod sender;
mod widget;

pub use component::Component;
pub use component_framework_macros::with;
pub use controller::{ComponentController, Ref};
pub use sender::ComponentSender;
pub use widget::GcmdWidgetExt;

/// Produces a simple signal handler that will always send a particular message to the sender’s
/// input channel. This macro is meant to be used within the [with! macro](crate::with):
///
/// ```rust,ignore
/// # use gcmd_component_framework::{forward_input, with};
/// # use gtk::prelude::*;
/// # gtk::init();
/// with!(gtk::Button => {
///     set_label("Increase counter");
///     connect_clicked(forward_input!(sender, Self::Input::Increase));
/// });
/// ```
#[macro_export]
macro_rules! forward_input {
    ($sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_| {
            __sender.input($expr);
        }
    }};
}

/// Produces a simple signal handler that will always send a particular message to the sender’s
/// output channel. This macro is meant to be used within the [with! macro](crate::with):
///
/// ```rust,ignore
/// # use gcmd_component_framework::{forward_input, with};
/// # use gtk::prelude::*;
/// # gtk::init();
/// with!(gtk::Button => {
///     set_label("Cancel");
///     connect_clicked(forward_input!(sender, Self::Output::Cancelled));
/// });
/// ```
#[macro_export]
macro_rules! forward_output {
    ($sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_| {
            __sender.output($expr);
        }
    }};
}
