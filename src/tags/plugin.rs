// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{FileMetadataExtractor, Tag};
use crate::{
    file::{File, FileOps},
    plugins::{
        ApiRequestToPlugin, ApiResponseFromPlugin, InactivePluginHostChannel,
        MessageFromPluginHost, MessageToPluginHost,
    },
};
use std::borrow::Cow;

#[derive(Debug)]
struct PluginTag {
    id: String,
    class: String,
    name: String,
    description: String,
}

impl PluginTag {
    fn boxed(self) -> Box<dyn Tag> {
        Box::new(self)
    }
}

impl Tag for PluginTag {
    fn id(&self) -> &str {
        &self.id
    }

    fn class(&self) -> Cow<'_, str> {
        Cow::Borrowed(&self.class)
    }

    fn name(&self) -> Cow<'_, str> {
        Cow::Borrowed(&self.name)
    }

    fn description(&self) -> Cow<'_, str> {
        Cow::Borrowed(&self.description)
    }

    fn clone(&self) -> Box<dyn Tag> {
        Self {
            id: self.id.clone(),
            class: self.class.clone(),
            name: self.name.clone(),
            description: self.description.clone(),
        }
        .boxed()
    }
}

#[derive(Debug)]
pub struct PluginMetadataExtractor {
    plugin_channel: InactivePluginHostChannel,
}

impl PluginMetadataExtractor {
    pub fn new(plugin_channel: InactivePluginHostChannel) -> Self {
        Self { plugin_channel }
    }
}

impl FileMetadataExtractor for PluginMetadataExtractor {
    async fn supported_tags(&self) -> Vec<(String, Vec<Box<dyn Tag>>)> {
        let mut channel = self.plugin_channel.activate_cloned();
        let id = channel.new_id();
        channel.send(MessageToPluginHost::ApiRequest {
            id,
            request: ApiRequestToPlugin::ListSupportedTags,
        });

        let mut result = Vec::new();
        loop {
            if let MessageFromPluginHost::ApiResponse {
                id: resp_id,
                response,
                last,
            } = channel.receive().await
                && resp_id == id
            {
                if let Some(ApiResponseFromPlugin::ListSupportedTags(response)) = response {
                    for entry in response {
                        let tags = entry
                            .tags
                            .into_iter()
                            .map(|(id, name)| {
                                PluginTag {
                                    id,
                                    class: entry.class.clone(),
                                    name,
                                    description: String::new(),
                                }
                                .boxed()
                            })
                            .collect();
                        result.push((entry.class, tags));
                    }
                }
                if last {
                    break;
                }
            }
        }
        result
    }

    fn summary_tags(&self) -> Vec<Box<dyn Tag>> {
        Vec::new()
    }

    async fn extract_metadata(&self, file: &File) -> Vec<(Box<dyn Tag>, String)> {
        let mut channel = self.plugin_channel.activate_cloned();
        let id = channel.new_id();
        channel.send(MessageToPluginHost::ApiRequest {
            id,
            request: ApiRequestToPlugin::ExtractMetadata {
                path: file
                    .local_path()
                    .map(|path| path.as_os_str().to_string_lossy().to_string())
                    .unwrap_or_default(),
                uri: file.uri(),
            },
        });

        let mut result = Vec::new();
        loop {
            if let MessageFromPluginHost::ApiResponse {
                id: resp_id,
                response,
                last,
            } = channel.receive().await
                && resp_id == id
            {
                if let Some(ApiResponseFromPlugin::ExtractMetadata(response)) = response {
                    for entry in response {
                        result.extend(entry.tags.into_iter().map(
                            |(id, name, description, value)| {
                                (
                                    PluginTag {
                                        id,
                                        class: entry.class.clone(),
                                        name,
                                        description,
                                    }
                                    .boxed(),
                                    value,
                                )
                            },
                        ));
                    }
                }
                if last {
                    break;
                }
            }
        }
        result
    }
}
