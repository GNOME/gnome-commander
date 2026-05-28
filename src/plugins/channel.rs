// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ApiRequestToPlugin, ApiResponseFromPlugin, PluginMetadata};
use async_broadcast::{
    InactiveReceiver as InactiveBroadcastReceiver, Receiver as BroadcastReceiver,
};
use async_channel::Sender;
use std::{
    collections::HashMap,
    path::PathBuf,
    sync::atomic::{AtomicU32, Ordering},
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
    Plugins(HashMap<String, PluginData>),
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
    pub path: PathBuf,
    pub metadata: PluginMetadata,
    pub initializing: bool,
    pub errors: Vec<String>,
}

/// An inactive channel can be used to send messages to the plugin host but will not receive any
/// messages unless activated.
#[derive(Debug, Clone)]
pub struct InactivePluginHostChannel {
    sender: Sender<MessageToPluginHost>,
    receiver: InactiveBroadcastReceiver<MessageFromPluginHost>,
}

impl InactivePluginHostChannel {
    pub fn new(
        sender: Sender<MessageToPluginHost>,
        receiver: BroadcastReceiver<MessageFromPluginHost>,
    ) -> Self {
        Self {
            sender,
            receiver: receiver.deactivate(),
        }
    }

    pub fn activate_cloned(&self) -> PluginHostChannel {
        PluginHostChannel {
            sender: self.sender.clone(),
            receiver: self.receiver.activate_cloned(),
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
#[derive(Debug, Clone)]
pub struct PluginHostChannel {
    sender: Sender<MessageToPluginHost>,
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
            receiver: self.receiver.clone().deactivate(),
        }
    }

    pub fn new_id(&self) -> u32 {
        static MAX_ID: AtomicU32 = AtomicU32::new(0);
        MAX_ID.fetch_add(1, Ordering::Relaxed)
    }
}
