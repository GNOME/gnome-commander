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
