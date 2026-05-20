// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ApiRequest, ApiResponse};

#[derive(Debug, serde::Deserialize)]
#[serde(
    tag = "type",
    content = "payload",
    rename_all = "kebab-case",
    deny_unknown_fields
)]
pub enum IncomingMessage {
    Info(Info),
    Error(String),
    Register(ApiInfo),
    Failed,
    Ready,
    ApiResponse(u32, ApiResponse),
}

#[derive(Debug, serde::Serialize)]
#[serde(tag = "type", content = "payload", rename_all = "kebab-case")]
pub enum OutgoingMessage {
    Apis(Vec<ApiInfo>),
    ApiRequest(u32, ApiRequest),
}

#[derive(Debug, Default, serde::Deserialize)]
pub struct Info {
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub copyright: Option<String>,
    #[serde(default)]
    pub comments: Option<String>,
    #[serde(default)]
    pub authors: Vec<String>,
    #[serde(default)]
    pub documenters: Vec<String>,
    #[serde(default)]
    pub translators: Vec<String>,
    #[serde(default)]
    pub webpage: Option<String>,
}

#[derive(Debug, serde::Serialize, serde::Deserialize)]
pub struct ApiInfo {
    pub name: String,
    pub version: String,
}

impl std::fmt::Display for ApiInfo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "{} {}", self.name, self.version)
    }
}
