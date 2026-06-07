// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{Component, ComponentSender};
use futures::FutureExt;
use gtk::glib;
use std::cell::Cell;

/// A guard type allowing mutable access to the component’s model. When this goes out of scope the
/// model’s [update_view() method](Component#method.update_view) will be run.
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

/// Controller of an active component encapsulating the component’s model, view as well as input and
/// output channels. Transparent read access to model’s properties is possible, for mutable access
/// use [model_mut() method](ComponentController#method.model_mut).
pub struct ComponentController<T: Component + Sized> {
    pub(crate) component: T,
    pub(crate) view: T::View,
    pub(crate) sender: ComponentSender<T>,
    pub(crate) input_receiver: async_channel::Receiver<T::Input>,
    pub(crate) output_receiver: async_channel::Receiver<T::Output>,
    pub(crate) forward_handler: Cell<Option<glib::JoinHandle<()>>>,
}

impl<T> std::fmt::Debug for ComponentController<T>
where
    T: Component + std::fmt::Debug,
    T::Input: std::fmt::Debug,
    T::Output: std::fmt::Debug,
    T::View: std::fmt::Debug,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        f.debug_struct("ComponentController")
            .field("component", &self.component)
            .field("view", &self.view)
            .field("sender", &self.sender)
            .field("input_receiver", &self.input_receiver)
            .field("output_receiver", &self.output_receiver)
            .finish()
    }
}

impl<T: Component + Sized> std::ops::Deref for ComponentController<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.model()
    }
}

impl<T: Component + Sized> ComponentController<T> {
    /// Retrieves the component’s root widget.
    pub fn root(&self) -> &T::Root {
        self.component.root(&self.view)
    }

    /// Processes the component’s messages until a message is received on the output channel.
    pub async fn receive(&mut self) -> Result<T::Output, async_channel::RecvError> {
        loop {
            futures::select! {
                output_result = self.output_receiver.recv().fuse() => return output_result,
                input_result = self.input_receiver.recv().fuse() => {
                    let msg = input_result.expect("Something is wrong, input channel got closed");
                    self.component.update(msg, &self.sender, &mut self.view).await;
                }
                _ = self.component.handle_subcomponents().fuse() => {
                    panic!("handle_subcomponents() returned, this should never happen");
                }
            }
        }
    }

    /// Returns the sender which can be used to communicate with the component.
    pub fn sender(&self) -> &ComponentSender<T> {
        &self.sender
    }

    /// Processes incoming messages for the component and its subcomponents, updating the component
    /// as necessary. This function never returns.
    pub async fn handle_incoming(&mut self) {
        loop {
            futures::select! {
                input_result = self.input_receiver.recv().fuse() => {
                    let msg = input_result.expect("Something is wrong, input channel got closed");
                    self.component.update(msg, &self.sender, &mut self.view).await;
                }
                _ = self.component.handle_subcomponents().fuse() => {
                    panic!("handle_subcomponents() returned, this should never happen");
                }
            }
        }
    }

    /// This method is meant to be called to insert subcomponents via [with!() macro](super::with).
    /// It fuses the outgoing channel of the component with the incoming channel of a parent
    /// component applying the given message conversion function. It also returns the root widget of
    /// the component, allowing it to be directly inserted into the view.
    pub fn attach<Parent, F>(&self, sender: &ComponentSender<Parent>, conversion: F) -> &T::Root
    where
        Parent: Component + 'static,
        F: Fn(T::Output) -> Parent::Input + 'static,
        T::Output: 'static,
    {
        let receiver = self.output_receiver.clone();
        let sender = sender.clone();
        let handler = Some(glib::spawn_future_local(async move {
            // We can exit the loop on Err() because it means the sender is dropped, typically along
            // with the corresponding component.
            while let Ok(message) = receiver.recv().await {
                sender.input(conversion(message));
            }
        }));

        if let Some(old_handler) = self.forward_handler.replace(handler) {
            old_handler.abort();
        }

        self.root()
    }

    /// Provides access to the component’s model.
    pub fn model(&self) -> &T {
        &self.component
    }

    /// Provides mutable access to the component’s model. When the reference goes out of scope the
    /// components [update_view() method](Component#method.update_view) will be run automatically.
    pub fn model_mut(&mut self) -> Ref<'_, T> {
        Ref {
            component: &mut self.component,
            view: &mut self.view,
            sender: &self.sender,
        }
    }

    /// Deactivates the component and retrieves the original model data. Note that the component’s
    /// widgets are *not* removed automatically and may remain visible.
    pub fn into_model(self) -> T {
        self.component
    }
}
