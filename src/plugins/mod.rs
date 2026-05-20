// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod apis;
mod channel;
mod host;
mod instance;
mod manager;
mod metadata;
mod protocol;

use apis::Apis;
pub use channel::{
    ApiCall, ApiRequest, ApiResponse, InactivePluginChannel, IncomingPluginMessage,
    OutgoingPluginMessage, PluginChannel, PluginData,
};
pub use host::PluginHost;
use instance::{PluginInstance, PluginInstanceOutput};
pub use manager::show_plugin_manager;
pub use metadata::PluginMetadata;
use protocol::{ApiInfo, IncomingMessage, OutgoingMessage};
