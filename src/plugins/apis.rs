// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    ApiCall, ApiInfo, ApiRequestToPlugin, ApiResponseFromPlugin, GenericDialog, PluginInstance,
    protocol::{ApiRequestToHost, ApiResponseFromHost},
};
use crate::{main_win::ExecutionTarget, options::PluginsOptions, utils::u32_enum};
use std::io::Error;

pub enum IncomingResult {
    Unhandled,
    Handled,
    HandledWithResponse(ApiResponseFromHost),
    HandledWithDialog(GenericDialog),
    HandledWithCommand(String, ExecutionTarget),
    Error(Error),
}

u32_enum! {
    pub enum Apis {
        #[default]
        PersistentSettings,
        ExtractMetadata,
        Dialogs,
        Menus,
        Commands,
    }
}

impl Apis {
    pub fn name(&self) -> &'static str {
        match self {
            Apis::PersistentSettings => "persistent_settings",
            Apis::ExtractMetadata => "extract_metadata",
            Apis::Dialogs => "dialogs",
            Apis::Menus => "menus",
            Apis::Commands => "commands",
        }
    }

    pub fn version(&self) -> &'static str {
        match self {
            Apis::PersistentSettings => "1.0",
            Apis::ExtractMetadata => "1.0",
            Apis::Dialogs => "1.0",
            Apis::Menus => "1.0",
            Apis::Commands => "1.0",
        }
    }

    pub fn info(&self) -> ApiInfo {
        ApiInfo {
            name: self.name().to_owned(),
            version: self.version().to_owned(),
        }
    }

    pub fn handle_incoming(
        &self,
        request: &ApiRequestToHost,
        instance: &PluginInstance,
    ) -> IncomingResult {
        match request {
            ApiRequestToHost::GetSetting(key) if self == &Apis::PersistentSettings => {
                let settings = PluginsOptions::instance().persistent_settings.get();
                let value = settings
                    .get(instance.file_name().as_ref())
                    .and_then(|map| map.get(key))
                    .and_then(|value| serde_json::from_str(value).ok())
                    .unwrap_or_default();

                IncomingResult::HandledWithResponse(ApiResponseFromHost::GetSetting(value))
            }
            ApiRequestToHost::SetSetting(key, value) if self == &Apis::PersistentSettings => {
                let options = PluginsOptions::instance();
                let mut settings = options.persistent_settings.get();
                settings
                    .entry(instance.file_name().to_string())
                    .or_default()
                    .insert(key.to_owned(), value.to_string());
                if let Err(error) = options.persistent_settings.set(settings) {
                    eprintln!("Failed saving persistent plugin setting: {error}");
                }
                IncomingResult::Handled
            }
            ApiRequestToHost::ShowDialog(spec) if self == &Apis::Dialogs => {
                match GenericDialog::new(spec.clone(), instance) {
                    Ok(dialog) => IncomingResult::HandledWithDialog(dialog),
                    Err(error) => IncomingResult::Error(error),
                }
            }
            ApiRequestToHost::RunCommand(command, target) if self == &Apis::Commands => {
                if command.is_empty() {
                    return IncomingResult::Error(Error::other("Command cannot be empty"));
                }

                let target = match target.as_str() {
                    "background" => ExecutionTarget::Background,
                    "any" => ExecutionTarget::AnyTerminal,
                    "embedded" => ExecutionTarget::EmbeddedTerminal,
                    "external" => ExecutionTarget::ExternalTerminal,
                    _ => {
                        return IncomingResult::Error(Error::other(format!(
                            "Invalid execution target: {target}"
                        )));
                    }
                };

                IncomingResult::HandledWithCommand(command.clone(), target)
            }
            _ => IncomingResult::Unhandled,
        }
    }

    pub fn accept_request(&self, request: &ApiRequestToPlugin) -> bool {
        match request.call() {
            ApiCall::ExtractMetadata if self == &Apis::ExtractMetadata => true,
            ApiCall::ListSupportedTags if self == &Apis::ExtractMetadata => true,
            ApiCall::MainMenuItems if self == &Apis::Menus => true,
            ApiCall::ContextMenuItems if self == &Apis::Menus => true,
            ApiCall::MenuActivated if self == &Apis::Menus => true,
            _ => false,
        }
    }

    pub fn accept_response(&self, response: &ApiResponseFromPlugin) -> bool {
        match response.call() {
            ApiCall::ExtractMetadata if self == &Apis::ExtractMetadata => true,
            ApiCall::ListSupportedTags if self == &Apis::ExtractMetadata => true,
            ApiCall::MainMenuItems if self == &Apis::Menus => true,
            ApiCall::ContextMenuItems if self == &Apis::Menus => true,
            _ => false,
        }
    }
}
