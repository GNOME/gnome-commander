// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use futures::{
    FutureExt,
    future::{pending, select_all},
};
use gtk::prelude::*;

/// A helper trait used by the [with! macro](with), provides a standard API to add a child to some
/// Gtk widgets. This trait is used when no explicit method is specified in `with!`.
pub trait ContainerExt<T> {
    fn container_set_child(&self, child: &T);
}

macro_rules! container_ext_impl {
    ($container_type:ty, $method:ident, $child_type:ty) => {
        impl<T: IsA<$child_type>> ContainerExt<T> for $container_type {
            fn container_set_child(&self, child: &T) {
                self.$method(child.as_ref().into());
            }
        }

        impl<T: IsA<$child_type>> ContainerExt<T> for &$container_type {
            fn container_set_child(&self, child: &T) {
                self.$method(child.as_ref().into());
            }
        }
    };
}

container_ext_impl!(gtk::ApplicationWindow, set_child, gtk::Widget);
container_ext_impl!(gtk::Box, append, gtk::Widget);
container_ext_impl!(gtk::Button, set_child, gtk::Widget);
container_ext_impl!(gtk::CheckButton, set_child, gtk::Widget);
container_ext_impl!(gtk::Expander, set_child, gtk::Widget);
container_ext_impl!(gtk::Frame, set_child, gtk::Widget);
container_ext_impl!(gtk::LinkButton, set_child, gtk::Widget);
container_ext_impl!(gtk::ListBox, append, gtk::Widget);
container_ext_impl!(gtk::ListBoxRow, set_child, gtk::Widget);
container_ext_impl!(gtk::MenuButton, set_child, gtk::Widget);
container_ext_impl!(gtk::Popover, set_child, gtk::Widget);
container_ext_impl!(gtk::Revealer, set_child, gtk::Widget);
container_ext_impl!(gtk::ScrolledWindow, set_child, gtk::Widget);
container_ext_impl!(gtk::SearchBar, set_child, gtk::Widget);
container_ext_impl!(gtk::Stack, add_child, gtk::Widget);
container_ext_impl!(gtk::StackSidebar, set_stack, gtk::Stack);
container_ext_impl!(gtk::StackSwitcher, set_stack, gtk::Stack);
container_ext_impl!(gtk::ToggleButton, set_child, gtk::Widget);
container_ext_impl!(gtk::TreeExpander, set_child, gtk::Widget);
container_ext_impl!(gtk::Window, set_child, gtk::Widget);
container_ext_impl!(gtk::WindowHandle, set_child, gtk::Widget);

/// A helper macro to manipulate a bunch
macro_rules! with {
    ($type:ty => {
        $($ops:tt)*
    }) => {
        with!{{<$type>::default()} => { $($ops)* }}
    };

    ($expr:expr => {
        $($ops:tt)*
    }) => {{
        let __obj = $expr;
        with!{block, {$($ops)*}, __obj}
        __obj
    }};

    (block, {}, $obj:ident) => {};

    (block, {
        $method:ident($($params:tt)*);
        $($rest:tt)*
    }, $obj:ident) => {
        with!{call, $obj.$method($($params)*)}
        with!{block, {$($rest)*}, $obj}
    };

    (block, {
        $type:ty => {
            $($ops:tt)*
        }
        $($rest:tt)*
    }, $obj:ident) => {
        $crate::components::ContainerExt::container_set_child(&$obj, &with!($type => {$($ops)*}));
        with!{block, {$($rest)*}, $obj}
    };

    (block, {
        $expr:expr => {
            $($ops:tt)*
        }
        $($rest:tt)*
    }, $obj:ident) => {
        $crate::components::ContainerExt::container_set_child(&$obj, with!($expr => {$($ops)*}));
        with!{block, {$($rest)*}, $obj}
    };

    (block, {
        if_!($expr_if:expr => {
            $($ops_if:tt)*
        }
        $(,$expr_elseif:expr => {
            $($ops_elseif:tt)*
        })*
        $(,else => {
            $($ops_else:tt)*
        })?
        $(,)?);
        $($rest:tt)*
    }, $obj:ident) => {
        if $expr_if {
            with!{block, {$($ops_if)*}, $obj}
        }
        $(else if $expr_elseif {
            with!{block, {$($ops_elseif)*}, $obj}
        })*
        $(else {
            with!{block, {$($ops_else)*}, $obj}
        })?
        with!{block, {$($rest)*}, $obj}
    };

    (block, {
        for_!($pat:pat in $expr:expr => {
            $($ops:tt)*
        });
        $($rest:tt)*
    }, $obj:ident) => {
        for $pat in $expr {
            with!{block, {$($ops)*}, $obj}
        }
        with!{block, {$($rest)*}, $obj}
    };

    (call, $obj:ident.$method:ident($($param:expr),*$(,)?)) => {
        $obj.$method($($param,)*);
    };

    (call, $obj:ident.$method:ident($type:ty => {
        $($inner:tt)*
    })) => {
        $obj.$method(with!($type => {$($inner)*}));
    };

    (call, $obj:ident.$method:ident($expr:expr => {
        $($inner:tt)*
    })) => {
        $obj.$method(with!($expr => {$($inner)*}));
    };
}

macro_rules! forward_input {
    ($sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_| {
            __sender.input($expr);
        }
    }};
}

macro_rules! forward_output {
    ($sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_| {
            __sender.output($expr);
        }
    }};
}

pub(crate) use {forward_input, forward_output, with};

pub trait Component {
    type Input;
    type Output;
    type View;
    type Root;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View;

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root;

    async fn update(
        &mut self,
        _msg: Self::Input,
        _sender: &ComponentSender<Self>,
        _view: &mut Self::View,
    ) {
    }

    fn update_view(&self, _sender: &ComponentSender<Self>, _view: &mut Self::View) {}

    async fn forward_messages_ident<T>(
        sender: &ComponentSender<Self>,
        controllers: &mut [ComponentController<T>],
    ) where
        T: Component<Output = Self::Input>,
        Self: Sized,
    {
        if !controllers.is_empty() {
            loop {
                let (result, _, _) = select_all(
                    controllers
                        .iter_mut()
                        .map(|controller| controller.receive().boxed_local()),
                )
                .await;
                match result {
                    Ok(msg) => sender.input(msg),
                    Err(error) => {
                        eprintln!("Something is wrong, failed forwarding messages: {error}");
                        break;
                    }
                }
            }
        } else {
            pending().await
        }
    }

    async fn forward_messages(&mut self, _sender: &ComponentSender<Self>) {
        pending().await
    }

    fn build(mut self) -> ComponentController<Self>
    where
        Self: Sized,
    {
        let (input_sender, input_receiver) = async_channel::bounded(16);
        let (output_sender, output_receiver) = async_channel::bounded(16);

        let sender = ComponentSender {
            input_sender,
            output_sender,
        };
        let view = self.init(&sender);
        ComponentController {
            component: self,
            view,
            sender,
            input_receiver,
            output_receiver,
        }
    }
}

#[derive(Debug)]
pub struct ComponentSender<T: Component + ?Sized> {
    input_sender: async_channel::Sender<T::Input>,
    output_sender: async_channel::Sender<T::Output>,
}

impl<T: Component> Clone for ComponentSender<T> {
    fn clone(&self) -> Self {
        ComponentSender {
            input_sender: self.input_sender.clone(),
            output_sender: self.output_sender.clone(),
        }
    }
}

impl<T: Component> ComponentSender<T> {
    pub fn input(&self, msg: T::Input) {
        self.input_sender
            .try_send(msg)
            .expect("Something is wrong, input channel isn't writable");
    }

    pub fn output(&self, msg: T::Output) {
        self.output_sender
            .try_send(msg)
            .expect("Something is wrong, output channel isn't writable");
    }
}

pub struct Ref<'a, T: Component + Sized> {
    component: &'a mut T,
    view: &'a mut T::View,
    sender: &'a ComponentSender<T>,
}

impl<'a, T: Component + Sized> std::ops::Deref for Ref<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.component
    }
}

impl<'a, T: Component + Sized> std::ops::DerefMut for Ref<'a, T> {
    fn deref_mut(&mut self) -> &mut T {
        self.component
    }
}

impl<'a, T: Component + Sized> std::ops::Drop for Ref<'a, T> {
    fn drop(&mut self) {
        self.component.update_view(self.sender, self.view);
    }
}

#[derive(Debug)]
pub struct ComponentController<T: Component + Sized> {
    component: T,
    view: T::View,
    sender: ComponentSender<T>,
    input_receiver: async_channel::Receiver<T::Input>,
    output_receiver: async_channel::Receiver<T::Output>,
}

impl<T: Component + Sized> std::ops::Deref for ComponentController<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.model()
    }
}

impl<T: Component + Sized> ComponentController<T> {
    pub fn root(&self) -> &T::Root {
        self.component.root(&self.view)
    }

    pub async fn receive(&mut self) -> Result<T::Output, async_channel::RecvError> {
        loop {
            futures::select! {
                input_result = self.input_receiver.recv().fuse() => {
                    let msg = input_result.expect("Something is wrong, input channel got closed");
                    self.component.update(msg, &self.sender, &mut self.view).await;
                }
                output_result = self.output_receiver.recv().fuse() => return output_result,
                _ = self.component.forward_messages(&self.sender).fuse() => {
                    panic!("forward_messages() returned, this should never happen");
                }
            }
        }
    }

    pub fn model(&self) -> &T {
        &self.component
    }

    pub fn model_mut(&mut self) -> Ref<'_, T> {
        Ref {
            component: &mut self.component,
            view: &mut self.view,
            sender: &self.sender,
        }
    }

    pub fn into_model(self) -> T {
        self.component
    }
}
