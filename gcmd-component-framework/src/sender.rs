// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::Component;

/// This type combines the senders for the component’s input and output channels.
#[derive(Debug)]
pub struct ComponentSender<T: Component + ?Sized> {
    pub(crate) input_sender: async_channel::Sender<T::Input>,
    pub(crate) output_sender: async_channel::Sender<T::Output>,
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
    /// Sends a message to the component’s input channel.
    pub fn input(&self, msg: T::Input) {
        self.input_sender
            .try_send(msg)
            .expect("Something is wrong, input channel isn't writable");
    }

    /// Sends a message to the component’s output channel.
    pub fn output(&self, msg: T::Output) {
        self.output_sender
            .try_send(msg)
            .expect("Something is wrong, output channel isn't writable");
    }
}
