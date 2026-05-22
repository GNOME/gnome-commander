// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ApiCall, ApiInfo, ApiRequest, ApiResponse, GenericDialog, PluginInstance};
use crate::{options::PluginsOptions, utils::u32_enum};
use std::io::Error;

pub enum IncomingResult {
    Unhandled,
    Handled,
    HandledWithResponse(ApiResponse),
    HandledWithDialog(GenericDialog),
    Error(Error),
}

u32_enum! {
    pub enum Apis {
        #[default]
        PersistentSettings,
        ExtractMetadata,
        Dialogs,
    }
}

impl Apis {
    pub fn name(&self) -> &'static str {
        match self {
            Apis::PersistentSettings => "persistent_settings",
            Apis::Dialogs => "dialogs",
            Apis::ExtractMetadata => "extract_metadata",
        }
    }

    pub fn version(&self) -> &'static str {
        match self {
            Apis::PersistentSettings => "1.0",
            Apis::Dialogs => "1.0",
            Apis::ExtractMetadata => "1.0",
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
        request: &ApiRequest,
        instance: &PluginInstance,
    ) -> IncomingResult {
        match request {
            ApiRequest::GetSetting(key) if self == &Apis::PersistentSettings => {
                let options = PluginsOptions::new();
                let settings = options.persistent_settings.get();
                let value = settings
                    .get(instance.file_name().as_ref())
                    .and_then(|map| map.get(key))
                    .and_then(|value| serde_json::from_str(value).ok())
                    .unwrap_or_default();

                IncomingResult::HandledWithResponse(ApiResponse::GetSetting(value))
            }
            ApiRequest::SetSetting(key, value) if self == &Apis::PersistentSettings => {
                let options = PluginsOptions::new();
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
            ApiRequest::ShowDialog(spec) if self == &Apis::Dialogs => {
                match GenericDialog::new(spec.clone()) {
                    Ok(dialog) => IncomingResult::HandledWithDialog(dialog),
                    Err(error) => IncomingResult::Error(error),
                }
            }
            _ => IncomingResult::Unhandled,
        }
    }

    pub fn accept_request(&self, request: &ApiRequest) -> bool {
        match request.call() {
            ApiCall::ExtractMetadata if self == &Apis::ExtractMetadata => true,
            ApiCall::ListSupportedTags if self == &Apis::ExtractMetadata => true,
            _ => false,
        }
    }

    pub fn accept_response(&self, response: &ApiResponse) -> bool {
        match response.call() {
            ApiCall::ExtractMetadata if self == &Apis::ExtractMetadata => true,
            ApiCall::ListSupportedTags if self == &Apis::ExtractMetadata => true,
            _ => false,
        }
    }
}
