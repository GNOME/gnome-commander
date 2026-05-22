// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    InactivePluginChannel, IncomingPluginMessage, OutgoingPluginMessage, PluginInstance,
    PluginInstanceOutput,
};
use crate::options::PluginsOptions;
use std::{
    collections::BTreeMap,
    fs,
    io::Error,
    path::Path,
    pin::Pin,
    task::{Context, Poll},
};
use tokio::sync::{broadcast, broadcast::Sender as BroadcastSender, mpsc, mpsc::Receiver};

pub struct PluginHost {
    plugins: BTreeMap<String, PluginInstance>,
    sender: BroadcastSender<OutgoingPluginMessage>,
    receiver: Receiver<IncomingPluginMessage>,
    pending_api_requests: BTreeMap<u32, usize>,
}

impl PluginHost {
    pub fn new(dirs: &[&Path]) -> (Self, InactivePluginChannel) {
        let (incoming_sender, incoming_receiver) = mpsc::channel(16);
        let (outgoing_sender, _) = broadcast::channel(16);

        let options = PluginsOptions::new();
        let mut host = Self {
            plugins: list_plugins(dirs, &options),
            sender: outgoing_sender.clone(),
            receiver: incoming_receiver,
            pending_api_requests: BTreeMap::new(),
        };
        for instance in host.plugins.values_mut() {
            if instance.is_enabled() {
                instance.start();
            }
        }
        (
            host,
            InactivePluginChannel::new(incoming_sender, outgoing_sender),
        )
    }

    fn process_incoming(
        &mut self,
        message: IncomingPluginMessage,
    ) -> Option<OutgoingPluginMessage> {
        match message {
            IncomingPluginMessage::GetPlugins => Some(OutgoingPluginMessage::Plugins(
                self.plugins
                    .iter()
                    .map(|(name, instance)| (name.to_owned(), instance.data()))
                    .collect(),
            )),
            IncomingPluginMessage::StartPlugin(name) => {
                let instance = self.plugins.get_mut(&name)?;
                instance.start();
                Some(updated_message(instance))
            }
            IncomingPluginMessage::StopPlugin(name) => {
                let instance = self.plugins.get_mut(&name)?;
                instance.stop();
                Some(updated_message(instance))
            }
            IncomingPluginMessage::TogglePlugin(name) => {
                let instance = self.plugins.get_mut(&name)?;
                if instance.is_enabled() {
                    if instance.is_initialized() {
                        instance.stop();
                        Some(updated_message(instance))
                    } else {
                        None
                    }
                } else {
                    instance.start();
                    Some(updated_message(instance))
                }
            }
            IncomingPluginMessage::ApiRequest { id, request } => {
                let mut count = 0;
                for instance in self.plugins.values_mut() {
                    if instance.is_enabled() && instance.handle_api_request(id, &request) {
                        count += 1;
                    }
                }

                if count > 0 {
                    self.pending_api_requests.insert(id, count);
                    None
                } else {
                    Some(OutgoingPluginMessage::ApiResponse {
                        id,
                        response: None,
                        last: true,
                    })
                }
            }
        }
    }

    fn process_instance_message(
        message: PluginInstanceOutput,
        instance: &PluginInstance,
        pending_api_requests: &mut BTreeMap<u32, usize>,
    ) -> Option<OutgoingPluginMessage> {
        match message {
            PluginInstanceOutput::Updated => Some(updated_message(instance)),
            PluginInstanceOutput::ApiResponse { id, response } => {
                let mut count = pending_api_requests.remove(&id)?;
                count -= 1;
                if count > 0 {
                    pending_api_requests.insert(id, count);
                }

                if response.is_some() || count == 0 {
                    Some(OutgoingPluginMessage::ApiResponse {
                        id,
                        response,
                        last: count == 0,
                    })
                } else {
                    None
                }
            }
        }
    }
}

impl Future for PluginHost {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        while let Poll::Ready(Some(message)) = self.receiver.poll_recv(cx) {
            if let Some(outgoing) = self.process_incoming(message) {
                // An error here is expected when no receivers are active.
                let _ = self.sender.send(outgoing);
            }
        }

        let Self {
            ref mut plugins,
            ref mut pending_api_requests,
            ref sender,
            ..
        } = *self;
        for instance in plugins.values_mut() {
            while let Poll::Ready(message) = instance.poll(cx) {
                if let Some(outgoing) =
                    Self::process_instance_message(message, instance, pending_api_requests)
                {
                    // If a plugin crashed deal with its outstanding API requests
                    if matches!(outgoing, OutgoingPluginMessage::PluginUpdated(..))
                        && !instance.is_enabled()
                        && instance.has_pending_api_requests()
                    {
                        for id in instance.drain_pending_api_requests() {
                            if let Some(outgoing) = Self::process_instance_message(
                                PluginInstanceOutput::ApiResponse { id, response: None },
                                instance,
                                pending_api_requests,
                            ) {
                                let _ = sender.send(outgoing);
                            }
                        }
                    }

                    // An error here is expected when no receivers are active.
                    let _ = sender.send(outgoing);
                }
            }
        }

        Poll::Pending
    }
}

#[cfg(unix)]
fn is_executable(metadata: &fs::Metadata) -> bool {
    use std::os::unix::fs::PermissionsExt;
    (metadata.permissions().mode() & 0o111) != 0
}

fn list_plugins(dirs: &[&Path], options: &PluginsOptions) -> BTreeMap<String, PluginInstance> {
    let mut plugins = BTreeMap::new();
    for dir in dirs {
        if let Err(error) = list_plugins_in_dir(dir, options, &mut plugins) {
            eprintln!(
                "Failed searching directory '{}' for plugins: {error}",
                dir.display()
            );
            continue;
        }
    }
    plugins
}

fn list_plugins_in_dir(
    dir: &Path,
    options: &PluginsOptions,
    plugins: &mut BTreeMap<String, PluginInstance>,
) -> Result<(), Error> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        if !entry
            .metadata()
            .is_ok_and(|metadata| metadata.is_file() && is_executable(&metadata))
            || entry
                .path()
                .extension()
                .is_some_and(|extension| extension == "so")
        {
            continue;
        }
        let instance = PluginInstance::new(entry.path(), options);
        plugins.insert(instance.file_name().to_string(), instance);
    }
    Ok(())
}

fn updated_message(instance: &PluginInstance) -> OutgoingPluginMessage {
    OutgoingPluginMessage::PluginUpdated(instance.file_name().to_string(), instance.data())
}
