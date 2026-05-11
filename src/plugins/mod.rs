// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod host;
mod instance;
mod metadata;
mod protocol;

pub use host::PluginHost;
pub use instance::PluginInstance;
pub use metadata::PluginMetadata;
use protocol::Message;
