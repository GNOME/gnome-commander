// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod component;
pub mod container;
mod controller;
pub mod helpers;
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
///
/// By default the generated closure expects a single parameter which is ignored. If the signal has
/// additional parameters, these need to be listed and can be added to your message:
///
/// ```rust,ignore
/// # use gcmd_component_framework::{forward_input, with};
/// # use gtk::prelude::*;
/// # gtk::init();
/// with!(gtk::Button => {
///     set_label("Increase counter");
///     connect_move_focus(forward_input!(|direction| sender, Self::Input::MoveFocus(direction)));
/// });
/// ```
#[macro_export]
macro_rules! forward_input {
    ($(|$($param:ident),*|)? $sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_$($(, $param)*)?| {
            __sender.input($expr);
        }
    }};
}

/// Produces a simple signal handler that will always send a particular message to the sender’s
/// output channel. This macro is meant to be used within the [with! macro](crate::with):
///
/// ```rust,ignore
/// # use gcmd_component_framework::{forward_output, with};
/// # use gtk::prelude::*;
/// # gtk::init();
/// with!(gtk::Button => {
///     set_label("Cancel");
///     connect_clicked(forward_output!(sender, Self::Output::Cancelled));
/// });
/// ```
#[macro_export]
macro_rules! forward_output {
    ($(|$($param:ident),*|)? $sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_$($(, $param)*)?| {
            __sender.output($expr);
        }
    }};
}
