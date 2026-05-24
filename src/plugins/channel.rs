// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ApiRequestToPlugin, ApiResponseFromPlugin, PluginMetadata};
use std::{
    collections::BTreeMap,
    sync::atomic::{AtomicU32, Ordering},
};
use tokio::sync::{
    broadcast::{Receiver as BroadcastReceiver, Sender as BroadcastSender},
    mpsc::Sender,
};

#[derive(Debug)]
#[allow(clippy::large_enum_variant)]
pub enum MessageToPluginHost {
    GetPlugins,
    StartPlugin(String),
    StopPlugin(String),
    TogglePlugin(String),
    ApiRequest {
        id: u32,
        plugin_name: Option<String>,
        request: ApiRequestToPlugin,
    },
}

#[derive(Debug, Clone)]
pub enum MessageFromPluginHost {
    Plugins(BTreeMap<String, PluginData>),
    PluginUpdated(String, PluginData),
    ApiResponse {
        id: u32,
        plugin_name: String,
        plugin_display_name: String,
        response: Option<ApiResponseFromPlugin>,
        last: bool,
    },
}

#[derive(Debug, Clone)]
pub struct PluginData {
    pub metadata: PluginMetadata,
    pub initializing: bool,
    pub errors: Vec<String>,
}

/// An inactive channel can be used to send messages to the plugin host but will not receive any
/// messages unless activated.
#[derive(Debug, Clone)]
pub struct InactivePluginHostChannel {
    sender: Sender<MessageToPluginHost>,
    broadcast_sender: BroadcastSender<MessageFromPluginHost>,
}

impl InactivePluginHostChannel {
    pub fn new(
        sender: Sender<MessageToPluginHost>,
        broadcast_sender: BroadcastSender<MessageFromPluginHost>,
    ) -> Self {
        Self {
            sender,
            broadcast_sender,
        }
    }

    pub fn activate_cloned(&self) -> PluginHostChannel {
        PluginHostChannel {
            sender: self.sender.clone(),
            broadcast_sender: self.broadcast_sender.clone(),
            receiver: self.broadcast_sender.subscribe(),
        }
    }

    pub fn send(&self, msg: MessageToPluginHost) {
        self.sender
            .try_send(msg)
            .expect("Something is wrong, sending message to plugins host failed");
    }
}

/// An active channel can be used to both send messages to the plugin host and receive messages
/// from it. It MUST receive messages or the queue will run full, resulting in plugin host messages
/// no longer being delivered to any receivers.
#[derive(Debug)]
pub struct PluginHostChannel {
    sender: Sender<MessageToPluginHost>,
    broadcast_sender: BroadcastSender<MessageFromPluginHost>,
    receiver: BroadcastReceiver<MessageFromPluginHost>,
}

impl PluginHostChannel {
    pub fn send(&self, msg: MessageToPluginHost) {
        self.sender
            .try_send(msg)
            .expect("Something is wrong, sending message to plugins host failed");
    }

    pub async fn receive(&mut self) -> MessageFromPluginHost {
        self.receiver
            .recv()
            .await
            .expect("Something is wrong, receiving a message from plugins host failed")
    }

    pub fn deactivate_cloned(&self) -> InactivePluginHostChannel {
        InactivePluginHostChannel {
            sender: self.sender.clone(),
            broadcast_sender: self.broadcast_sender.clone(),
        }
    }

    pub fn new_id(&self) -> u32 {
        static MAX_ID: AtomicU32 = AtomicU32::new(0);
        MAX_ID.fetch_add(1, Ordering::Relaxed)
    }
}

impl Clone for PluginHostChannel {
    fn clone(&self) -> Self {
        Self {
            sender: self.sender.clone(),
            broadcast_sender: self.broadcast_sender.clone(),
            receiver: self.receiver.resubscribe(),
        }
    }
}
