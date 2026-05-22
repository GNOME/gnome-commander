// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::BTreeMap;

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
    ApiRequest(u32, ApiRequest),
    ApiResponse(u32, ApiResponse),
}

#[derive(Debug, serde::Serialize)]
#[serde(tag = "type", content = "payload", rename_all = "kebab-case")]
pub enum OutgoingMessage {
    Apis(Vec<ApiInfo>),
    ApiRequest(u32, ApiRequest),
    ApiResponse(u32, ApiResponse),
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

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum ApiCall {
    ExtractMetadata,
    ListSupportedTags,
    GetSetting,
    SetSetting,
    ShowDialog,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum ApiRequest {
    ListSupportedTags,
    ExtractMetadata { path: String, uri: String },
    GetSetting(String),
    SetSetting(String, serde_json::Value),
    ShowDialog(DialogSpec),
}

impl ApiRequest {
    pub fn call(&self) -> ApiCall {
        match self {
            Self::ListSupportedTags => ApiCall::ListSupportedTags,
            Self::ExtractMetadata { .. } => ApiCall::ExtractMetadata,
            Self::GetSetting(..) => ApiCall::GetSetting,
            Self::SetSetting(..) => ApiCall::SetSetting,
            Self::ShowDialog(..) => ApiCall::ShowDialog,
        }
    }
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct DialogSpec {
    pub title: String,
    #[serde(default)]
    pub modal: bool,
    pub child: WidgetSpec,
    pub buttons: Vec<ButtonSpec>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
#[serde(tag = "type")]
pub enum WidgetSpec {
    HBox {
        children: Vec<WidgetSpec>,
    },
    VBox {
        children: Vec<WidgetSpec>,
    },
    Text {
        label: String,
    },
    Input {
        id: String,
        label: String,
        #[serde(default)]
        value: String,
        #[serde(default)]
        placeholder: String,
        #[serde(default)]
        vertical: bool,
    },
    Group {
        label: String,
        child: Box<WidgetSpec>,
    },
    Checkbox {
        id: String,
        label: String,
        #[serde(default)]
        checked: bool,
    },
    RadioGroup {
        children: Vec<WidgetSpec>,
        #[serde(default)]
        vertical: bool,
    },
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ButtonSpec {
    pub id: String,
    pub label: String,
    #[serde(default)]
    pub default: bool,
    #[serde(default)]
    pub cancel: bool,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum ApiResponse {
    ListSupportedTags(Vec<ListSupportedTagsResponse>),
    ExtractMetadata(Vec<ExtractMetadataResponse>),
    GetSetting(serde_json::Value),
    ShowDialog(String, BTreeMap<String, DialogWidgetValue>),
}

impl ApiResponse {
    pub fn call(&self) -> ApiCall {
        match self {
            Self::ListSupportedTags(..) => ApiCall::ListSupportedTags,
            Self::ExtractMetadata(..) => ApiCall::ExtractMetadata,
            Self::GetSetting(..) => ApiCall::GetSetting,
            Self::ShowDialog(..) => ApiCall::ShowDialog,
        }
    }
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ListSupportedTagsResponse {
    pub class: String,
    pub tags: Vec<(String, String)>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ExtractMetadataResponse {
    pub class: String,
    pub tags: Vec<(String, String, String, String)>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
#[serde(untagged)]
pub enum DialogWidgetValue {
    String(String),
    Bool(bool),
}

impl std::fmt::Display for ApiInfo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "{} {}", self.name, self.version)
    }
}
