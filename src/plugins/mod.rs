// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod channel;
mod host;
mod instance;
mod manager;
mod metadata;
mod protocol;

pub use channel::{IncomingPluginMessage, OutgoingPluginMessage, PluginChannel, PluginData};
pub use host::PluginHost;
pub use instance::PluginInstance;
pub use manager::show_plugin_manager;
pub use metadata::PluginMetadata;
use protocol::{IncomingMessage, OutgoingMessage};
