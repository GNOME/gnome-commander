// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

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

#[derive(Debug, serde::Serialize)]
pub struct ApiInfo {
    pub name: String,
    pub version: String,
}

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
    Failed,
    Ready,
}

#[derive(Debug, serde::Serialize)]
#[serde(tag = "type", content = "payload", rename_all = "kebab-case")]
pub enum OutgoingMessage {
    Apis(Vec<ApiInfo>),
}
