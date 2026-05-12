// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::PluginMetadata;
use async_channel::{Receiver, Sender};
use std::collections::BTreeMap;

#[derive(Debug)]
pub enum IncomingPluginMessage {
    GetPlugins,
    StartPlugin(String),
    StopPlugin(String),
}

#[derive(Debug)]
pub struct PluginData {
    pub metadata: PluginMetadata,
    pub initializing: bool,
    pub errors: Vec<String>,
}

#[derive(Debug)]
pub enum OutgoingPluginMessage {
    Plugins(BTreeMap<String, PluginData>),
    PluginUpdated(String, PluginData),
}

#[derive(Debug, Clone)]
pub struct PluginChannel {
    sender: Sender<IncomingPluginMessage>,
    receiver: Receiver<OutgoingPluginMessage>,
}

impl PluginChannel {
    pub fn new(
        sender: Sender<IncomingPluginMessage>,
        receiver: Receiver<OutgoingPluginMessage>,
    ) -> Self {
        Self { sender, receiver }
    }

    pub fn send(&self, msg: IncomingPluginMessage) {
        self.sender
            .try_send(msg)
            .expect("Something is wrong, sending message to plugins host failed");
    }

    pub async fn receive(&self) -> OutgoingPluginMessage {
        self.receiver
            .recv()
            .await
            .expect("Something is wrong, receiving a message from plugins host failed")
    }
}
