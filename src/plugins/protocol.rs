// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

#[derive(Debug, Default, serde::Serialize, serde::Deserialize)]
pub struct ReadyMessage {
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
#[serde(
    tag = "type",
    content = "payload",
    rename_all = "kebab-case",
    deny_unknown_fields
)]
pub enum Message {
    Ready(ReadyMessage),
}
