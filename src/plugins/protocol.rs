// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::HashMap;

#[derive(Debug, serde::Deserialize)]
#[serde(
    tag = "type",
    content = "payload",
    rename_all = "kebab-case",
    deny_unknown_fields
)]
pub enum MessageFromPlugin {
    Info(Info),
    Error(String),
    Register(ApiInfo),
    Failed,
    Ready,
    ApiRequest(u32, ApiRequestToHost),
    ApiResponse(u32, ApiResponseFromPlugin),
}

#[derive(Debug, serde::Serialize)]
#[serde(tag = "type", content = "payload", rename_all = "kebab-case")]
pub enum MessageToPlugin {
    Apis(Vec<ApiInfo>),
    ApiRequest(u32, ApiRequestToPlugin),
    ApiResponse(u32, ApiResponseFromHost),
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

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum ApiCall {
    ExtractMetadata,
    ListSupportedTags,
    MainMenuItems,
    ContextMenuItems,
    MenuActivated,
}

impl ApiCall {
    pub fn expect_response(&self) -> bool {
        match self {
            Self::ExtractMetadata
            | Self::ListSupportedTags
            | Self::MainMenuItems
            | Self::ContextMenuItems => true,
            Self::MenuActivated => false,
        }
    }
}

#[derive(Debug, Clone, serde::Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum ApiRequestToPlugin {
    ListSupportedTags,
    ExtractMetadata {
        path: String,
        uri: String,
    },
    MainMenuItems,
    ContextMenuItems(PanelsState),
    MenuActivated {
        action: String,
        state: PanelsState,
        parameter: String,
    },
}

impl ApiRequestToPlugin {
    pub fn call(&self) -> ApiCall {
        match self {
            Self::ListSupportedTags => ApiCall::ListSupportedTags,
            Self::ExtractMetadata { .. } => ApiCall::ExtractMetadata,
            Self::MainMenuItems => ApiCall::MainMenuItems,
            Self::ContextMenuItems(..) => ApiCall::ContextMenuItems,
            Self::MenuActivated { .. } => ApiCall::MenuActivated,
        }
    }
}

#[derive(Debug, Clone, serde::Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum ApiResponseFromPlugin {
    ListSupportedTags(Vec<ListSupportedTagsResponse>),
    ExtractMetadata(Vec<ExtractMetadataResponse>),
    MainMenuItems(Vec<MenuItem>),
    ContextMenuItems(Vec<MenuItem>),
}

impl ApiResponseFromPlugin {
    pub fn call(&self) -> ApiCall {
        match self {
            Self::ListSupportedTags(..) => ApiCall::ListSupportedTags,
            Self::ExtractMetadata(..) => ApiCall::ExtractMetadata,
            Self::MainMenuItems(..) => ApiCall::MainMenuItems,
            Self::ContextMenuItems(..) => ApiCall::ContextMenuItems,
        }
    }
}

#[derive(Debug, Clone, serde::Deserialize)]
pub struct ListSupportedTagsResponse {
    pub class: String,
    pub tags: Vec<(String, String)>,
}

#[derive(Debug, Clone, serde::Deserialize)]
pub struct ExtractMetadataResponse {
    pub class: String,
    pub tags: Vec<(String, String, String, String)>,
}

#[derive(Debug, Clone, serde::Serialize)]
pub struct PanelsState {
    pub active_directory_path: Option<String>,
    pub active_directory_uri: String,
    pub active_focused_file: Option<String>,
    pub active_selected_files: Vec<String>,
    pub inactive_directory_path: Option<String>,
    pub inactive_directory_uri: String,
    pub inactive_focused_file: Option<String>,
    pub inactive_selected_files: Vec<String>,
}

#[derive(Debug, Clone, serde::Deserialize)]
pub struct MenuItem {
    pub label: String,
    pub action: String,
    #[serde(default)]
    pub parameter: String,
}

#[derive(Debug, serde::Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum ApiRequestToHost {
    GetSetting(String),
    SetSetting(String, serde_json::Value),
    ShowDialog(DialogSpec),
}

#[derive(Debug, Clone, serde::Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum ApiResponseFromHost {
    GetSetting(serde_json::Value),
    ShowDialog(String, HashMap<String, DialogWidgetValue>),
}

fn default_dialog_width() -> i32 {
    600
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct DialogSpec {
    pub title: String,
    #[serde(default)]
    pub modal: bool,
    #[serde(default = "default_dialog_width")]
    pub width: i32,
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
#[serde(untagged)]
pub enum DialogWidgetValue {
    String(String),
    Bool(bool),
}
