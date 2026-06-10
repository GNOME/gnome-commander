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

/// Turns a method call into a closure, while cloning the object so that it can be captured by the
/// closure. Typically this is used to produce a signal handler that will always send a particular
/// message to the sender channel. This macro is meant to be used within the
/// [with! macro](crate::with):
///
/// ```rust
/// # use gcmd_component_framework::prelude::*;
/// # use gtk::prelude::*;
/// # gtk::init();
/// # enum MyInput {
/// #   Increase,
/// # }
/// # #[derive(Clone)]
/// # struct Sender;
/// # impl Sender {
/// #   pub fn input(&self, _input: MyInput) {}
/// # }
/// # let _sender = Sender;
/// # let sender = &_sender;
/// with!(gtk::Button {
///     .set_label("Increase counter");
///     .connect_clicked(forward!(sender.input(MyInput::Increase)));
/// });
/// ```
///
/// By default the generated closure receives a single parameter and ignores it. If the closure
/// needs to take a different number of parameters or the parameters needs to be used in the call
/// these have to be listed explicitly:
///
/// ```rust
/// # use gcmd_component_framework::prelude::*;
/// # use gtk::prelude::*;
/// # gtk::init();
/// # enum MyInput {
/// #   MoveFocus(gtk::DirectionType),
/// # }
/// # #[derive(Clone)]
/// # struct Sender;
/// # impl Sender {
/// #   pub fn input(&self, _input: MyInput) {}
/// # }
/// # let _sender = Sender;
/// # let sender = &_sender;
/// with!(gtk::Button {
///     .set_label("Increase counter");
///     .connect_move_focus(
///         forward!(|_, direction| sender.input(MyInput::MoveFocus(direction)))
///     );
/// });
/// ```
#[macro_export]
macro_rules! forward {
    (|$($params:pat_param),*| $obj:ident.$method:ident($($call_params:expr),* $(,)?)) => {{
        let __obj = $obj.clone();
        move |$($params,)*| {
            __obj.$method($($call_params),*);
        }
    }};
    ($obj:ident.$method:ident($($call_params:expr),* $(,)?)) => {
        $crate::forward!(|_| $obj.$method($($call_params),*))
    }
}
