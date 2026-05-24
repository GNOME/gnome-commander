// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    InactivePluginHostChannel, MessageFromPluginHost, MessageToPluginHost, PluginInstance,
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
    sender: BroadcastSender<MessageFromPluginHost>,
    receiver: Receiver<MessageToPluginHost>,
    pending_api_requests: BTreeMap<u32, usize>,
}

impl PluginHost {
    pub fn new(system_dir: &Path, user_dir: &Path) -> (Self, InactivePluginHostChannel) {
        let (incoming_sender, incoming_receiver) = mpsc::channel(16);
        let (outgoing_sender, _) = broadcast::channel(16);

        let options = PluginsOptions::new();
        let mut host = Self {
            plugins: list_plugins(system_dir, user_dir, &options),
            sender: outgoing_sender.clone(),
            receiver: incoming_receiver,
            pending_api_requests: BTreeMap::new(),
        };
        for instance in host.plugins.values_mut() {
            if instance.is_enabled() {
                instance.start();
            }
        }

        // Remove stale data
        let mut metadata = options.metadata.get();
        metadata.retain(|key, _| host.plugins.contains_key(key));
        let _ = options.metadata.set(metadata);

        let mut settings = options.persistent_settings.get();
        settings.retain(|key, _| host.plugins.contains_key(key));
        let _ = options.persistent_settings.set(settings);

        (
            host,
            InactivePluginHostChannel::new(incoming_sender, outgoing_sender),
        )
    }

    fn process_incoming(&mut self, message: MessageToPluginHost) -> Option<MessageFromPluginHost> {
        match message {
            MessageToPluginHost::GetPlugins => Some(MessageFromPluginHost::Plugins(
                self.plugins
                    .iter()
                    .map(|(name, instance)| (name.to_owned(), instance.data()))
                    .collect(),
            )),
            MessageToPluginHost::StartPlugin(name) => {
                let instance = self.plugins.get_mut(&name)?;
                instance.start();
                Some(updated_message(instance))
            }
            MessageToPluginHost::StopPlugin(name) => {
                let instance = self.plugins.get_mut(&name)?;
                instance.stop();
                Some(updated_message(instance))
            }
            MessageToPluginHost::TogglePlugin(name) => {
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
            MessageToPluginHost::ApiRequest {
                id,
                plugin_name,
                request,
            } => {
                let mut count = 0;
                for instance in self.plugins.values_mut() {
                    if instance.is_enabled()
                        && plugin_name
                            .as_ref()
                            .is_none_or(|plugin_name| plugin_name == &instance.file_name())
                        && instance.handle_api_request(id, &request)
                    {
                        count += 1;
                    }
                }

                if count > 0 {
                    self.pending_api_requests.insert(id, count);
                    None
                } else {
                    Some(MessageFromPluginHost::ApiResponse {
                        id,
                        plugin_name: String::new(),
                        plugin_display_name: String::new(),
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
    ) -> Vec<MessageFromPluginHost> {
        let mut result = Vec::new();

        if matches!(
            message,
            PluginInstanceOutput::Updated | PluginInstanceOutput::UpdatedAndApiResponse { .. }
        ) {
            result.push(updated_message(instance));
        }

        if let PluginInstanceOutput::ApiResponse { id, response }
        | PluginInstanceOutput::UpdatedAndApiResponse { id, response } = message
            && let Some(mut count) = pending_api_requests.remove(&id)
        {
            count -= 1;
            if count > 0 {
                pending_api_requests.insert(id, count);
            }

            if response.is_some() || count == 0 {
                result.push(MessageFromPluginHost::ApiResponse {
                    id,
                    plugin_name: instance.file_name().to_string(),
                    plugin_display_name: instance.name(),
                    response,
                    last: count == 0,
                });
            }
        }

        result
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
                let outgoing =
                    Self::process_instance_message(message, instance, pending_api_requests);
                for message in outgoing.into_iter() {
                    // If a plugin crashed deal with its outstanding API requests
                    if matches!(message, MessageFromPluginHost::PluginUpdated(..))
                        && !instance.is_enabled()
                        && instance.has_pending_api_requests()
                    {
                        for id in instance.drain_pending_api_requests() {
                            let outgoing = Self::process_instance_message(
                                PluginInstanceOutput::ApiResponse { id, response: None },
                                instance,
                                pending_api_requests,
                            );
                            for message in outgoing.into_iter() {
                                let _ = sender.send(message);
                            }
                        }
                    }

                    // An error here is expected when no receivers are active.
                    let _ = sender.send(message);
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

fn list_plugins(
    system_dir: &Path,
    user_dir: &Path,
    options: &PluginsOptions,
) -> BTreeMap<String, PluginInstance> {
    let mut plugins = BTreeMap::new();
    for dir in [system_dir, user_dir] {
        if let Err(error) = list_plugins_in_dir(dir, system_dir, options, &mut plugins) {
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
    system_dir: &Path,
    options: &PluginsOptions,
    plugins: &mut BTreeMap<String, PluginInstance>,
) -> Result<(), Error> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let Ok(metadata) = entry.metadata() else {
            continue;
        };
        if metadata.is_dir() {
            let child = entry.path().join("main");
            if child
                .metadata()
                .is_ok_and(|metadata| metadata.is_file() && is_executable(&metadata))
            {
                let instance = PluginInstance::new(child, entry.file_name(), system_dir, options);
                plugins.insert(instance.file_name().to_string(), instance);
            }
        } else if metadata.is_file() && is_executable(&metadata) {
            let instance =
                PluginInstance::new(entry.path(), entry.file_name(), system_dir, options);
            plugins.insert(instance.file_name().to_string(), instance);
        }
    }
    Ok(())
}

fn updated_message(instance: &PluginInstance) -> MessageFromPluginHost {
    MessageFromPluginHost::PluginUpdated(instance.file_name().to_string(), instance.data())
}
