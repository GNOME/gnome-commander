// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::PluginInstance;
use crate::{
    config::{PACKAGE, PLUGIN_DIR},
    options::PluginsOptions,
};
use std::{
    collections::BTreeMap,
    fs,
    future::Future,
    io::Error,
    path::{Path, PathBuf},
    pin::Pin,
    task::{Context, Poll},
};

pub struct PluginHost {
    plugins: BTreeMap<String, PluginInstance>,
}

impl PluginHost {
    pub fn new() -> Self {
        let options = PluginsOptions::new();
        let mut host = Self {
            plugins: list_plugins(&options),
        };
        for plugin in host.enabled_plugins() {
            plugin.start();
        }
        host
    }

    fn enabled_plugins(&mut self) -> impl Iterator<Item = &mut PluginInstance> {
        self.plugins
            .values_mut()
            .filter(|plugin| plugin.is_enabled())
    }
}

impl Future for PluginHost {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        for plugin in self.enabled_plugins() {
            if let Poll::Ready(_) = Pin::new(plugin).poll(cx) {
                return Poll::Ready(());
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

fn list_plugins(options: &PluginsOptions) -> BTreeMap<String, PluginInstance> {
    let mut plugins = BTreeMap::new();
    for dir in [
        PathBuf::from(PLUGIN_DIR),
        glib::user_config_dir().join(PACKAGE).join("plugins"),
    ] {
        if let Err(error) = list_plugins_in_dir(&dir, options, &mut plugins) {
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
        {
            continue;
        }
        let instance = PluginInstance::new(entry.path(), options);
        plugins.insert(instance.file_name().to_string(), instance);
    }
    Ok(())
}
