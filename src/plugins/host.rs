// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    InactivePluginChannel, IncomingPluginMessage, OutgoingPluginMessage, PluginInstance,
    PluginInstanceOutput,
};
use crate::options::PluginsOptions;
use async_broadcast::Sender as BroadcastSender;
use async_channel::Receiver;
use futures::Stream;
use std::{
    collections::BTreeMap,
    fs,
    io::Error,
    path::Path,
    pin::Pin,
    task::{Context, Poll},
};

#[pin_project::pin_project]
pub struct PluginHost {
    plugins: BTreeMap<String, PluginInstance>,
    sender: BroadcastSender<OutgoingPluginMessage>,
    #[pin]
    receiver: Receiver<IncomingPluginMessage>,
    pending_api_requests: BTreeMap<u32, usize>,
}

impl PluginHost {
    pub fn new(dirs: &[&Path]) -> (Self, InactivePluginChannel) {
        let (incoming_sender, incoming_receiver) = async_channel::bounded(16);
        let (outgoing_sender, outgoing_receiver) = async_broadcast::broadcast(16);

        let options = PluginsOptions::new();
        let mut host = Self {
            plugins: list_plugins(dirs, &options),
            sender: outgoing_sender,
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
            InactivePluginChannel::new(incoming_sender, outgoing_receiver),
        )
    }

    fn process_incoming(
        message: IncomingPluginMessage,
        plugins: &mut BTreeMap<String, PluginInstance>,
        pending_api_requests: &mut BTreeMap<u32, usize>,
    ) -> Option<OutgoingPluginMessage> {
        match message {
            IncomingPluginMessage::GetPlugins => Some(OutgoingPluginMessage::Plugins(
                plugins
                    .iter()
                    .map(|(name, instance)| (name.to_owned(), instance.data()))
                    .collect(),
            )),
            IncomingPluginMessage::StartPlugin(name) => {
                let instance = plugins.get_mut(&name)?;
                instance.start();
                Some(updated_message(instance))
            }
            IncomingPluginMessage::StopPlugin(name) => {
                let instance = plugins.get_mut(&name)?;
                instance.stop();
                Some(updated_message(instance))
            }
            IncomingPluginMessage::TogglePlugin(name) => {
                let instance = plugins.get_mut(&name)?;
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
                for instance in plugins.values_mut() {
                    if instance.is_enabled() && instance.handle_api_request(id, &request) {
                        count += 1;
                    }
                }

                if count > 0 {
                    pending_api_requests.insert(id, count);
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

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let mut this = self.project();

        while let Poll::Ready(Some(message)) = this.receiver.as_mut().poll_next(cx) {
            if let Some(outgoing) =
                Self::process_incoming(message, this.plugins, this.pending_api_requests)
            {
                // An error here is expected when no receivers are active.
                let _ = this.sender.try_broadcast(outgoing);
            }
        }

        for mut instance in this.plugins.values_mut() {
            while let Poll::Ready(Some(message)) = Pin::new(&mut instance).poll_next(cx) {
                if let Some(outgoing) =
                    Self::process_instance_message(message, instance, this.pending_api_requests)
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
                                this.pending_api_requests,
                            ) {
                                let _ = this.sender.try_broadcast(outgoing);
                            }
                        }
                    }

                    // An error here is expected when no receivers are active.
                    let _ = this.sender.try_broadcast(outgoing);
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
