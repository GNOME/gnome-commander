// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{IncomingPluginMessage, OutgoingPluginMessage, PluginChannel, PluginInstance};
use crate::options::PluginsOptions;
use async_channel::{Receiver, Sender};
use futures::{FutureExt, future::select_all};
use std::{collections::BTreeMap, fs, io::Error, path::Path};

pub struct PluginHost {
    plugins: BTreeMap<String, PluginInstance>,
    sender: Sender<OutgoingPluginMessage>,
    receiver: Receiver<IncomingPluginMessage>,
}

impl PluginHost {
    pub fn new(dirs: &[&Path]) -> (Self, PluginChannel) {
        let (incoming_sender, incoming_receiver) = async_channel::bounded(16);
        let (outgoing_sender, outgoing_receiver) = async_channel::bounded(16);

        let options = PluginsOptions::new();
        let mut host = Self {
            plugins: list_plugins(dirs, &outgoing_sender, &options),
            sender: outgoing_sender,
            receiver: incoming_receiver,
        };
        for plugin in host.enabled_plugins() {
            plugin.start();
        }
        (host, PluginChannel::new(incoming_sender, outgoing_receiver))
    }

    pub async fn run(mut self) {
        let receiver = self.receiver.clone();
        loop {
            futures::select!(
                msg = receiver.recv().fuse() => {
                    match msg {
                        Ok(IncomingPluginMessage::GetPlugins) => {
                            let plugins = self
                                .plugins
                                .iter()
                                .map(|(name, instance)| (name.to_owned(), instance.data()))
                                .collect();
                            let _ = self
                                .sender
                                .try_send(OutgoingPluginMessage::Plugins(plugins));
                        }
                        Ok(IncomingPluginMessage::StartPlugin(name)) => {
                            if let Some(instance) = self.plugins.get_mut(&name) {
                                instance.start();
                            }
                        }
                        Ok(IncomingPluginMessage::StopPlugin(name)) => {
                            if let Some(instance) = self.plugins.get_mut(&name) {
                                instance.stop();
                            }
                        }
                        Err(error) => {
                            eprintln!("Plugin host failed receiving incoming messages: {error}");
                        }
                    }
                }
                // Pump async processing in plugin instances, no result is expected here.
                _ = select_all(self.plugins.values_mut()).fuse() => {}
            );
        }
    }

    fn enabled_plugins(&mut self) -> impl Iterator<Item = &mut PluginInstance> {
        self.plugins
            .values_mut()
            .filter(|plugin| plugin.is_enabled())
    }
}

#[cfg(unix)]
fn is_executable(metadata: &fs::Metadata) -> bool {
    use std::os::unix::fs::PermissionsExt;
    (metadata.permissions().mode() & 0o111) != 0
}

fn list_plugins(
    dirs: &[&Path],
    sender: &Sender<OutgoingPluginMessage>,
    options: &PluginsOptions,
) -> BTreeMap<String, PluginInstance> {
    let mut plugins = BTreeMap::new();
    for dir in dirs {
        if let Err(error) = list_plugins_in_dir(dir, sender, options, &mut plugins) {
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
    sender: &Sender<OutgoingPluginMessage>,
    options: &PluginsOptions,
    plugins: &mut BTreeMap<String, PluginInstance>,
) -> Result<(), Error> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        if !entry
            .metadata()
            .is_ok_and(|metadata| metadata.is_file() && is_executable(&metadata))
        {
            continue;
        }
        let instance = PluginInstance::new(entry.path(), sender, options);
        plugins.insert(instance.file_name().to_string(), instance);
    }
    Ok(())
}
