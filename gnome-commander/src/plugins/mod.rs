// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod apis;
mod channel;
mod dialogs;
mod host;
mod instance;
mod manager;
mod metadata;
mod protocol;

use crate::config::{PACKAGE, plugin_dir};
use apis::{Apis, IncomingResult};
pub use channel::{
    InactivePluginHostChannel, MessageFromPluginHost, MessageToPluginHost, PluginData,
    PluginHostChannel,
};
use dialogs::GenericDialog;
pub use host::PluginHost;
use instance::{PluginInstance, PluginInstanceOutput};
pub use manager::show_plugin_manager;
pub use metadata::PluginMetadata;
pub use protocol::{
    ApiCall, ApiInfo, ApiRequestToPlugin, ApiResponseFromPlugin, ModifierState, PanelsState,
};

pub fn plugin_channel() -> PluginHostChannel {
    thread_local! {
        static CHANNEL: InactivePluginHostChannel = {
            let system_plugins_dir = plugin_dir();
            let user_plugins_dir = glib::user_config_dir().join(PACKAGE).join("plugins");
            let (plugin_host, plugin_channel) =
                PluginHost::new(&system_plugins_dir, &user_plugins_dir);
            glib::spawn_future_local(plugin_host);
            plugin_channel
        };
    }

    CHANNEL.with(|channel| channel.activate_cloned())
}
