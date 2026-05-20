// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ApiCall, ApiInfo, ApiRequest, ApiResponse};
use crate::utils::u32_enum;

u32_enum! {
    pub enum Apis {
        #[default]
        ExtractMetadata,
    }
}

impl Apis {
    pub fn name(&self) -> &'static str {
        match self {
            Apis::ExtractMetadata => "extract_metadata",
        }
    }

    pub fn version(&self) -> &'static str {
        match self {
            Apis::ExtractMetadata => "1.0",
        }
    }

    pub fn info(&self) -> ApiInfo {
        ApiInfo {
            name: self.name().to_owned(),
            version: self.version().to_owned(),
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
