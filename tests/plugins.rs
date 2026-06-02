// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use futures::{FutureExt, select};
use gnome_commander::{
    file::File,
    plugins::{
        ApiRequestToPlugin, ApiResponseFromPlugin, InactivePluginHostChannel,
        MessageFromPluginHost, MessageToPluginHost, PanelsState, PluginHost, PluginHostChannel,
    },
    tags::{FileMetadataExtractor, plugin::PluginMetadataExtractor},
};
use gtk::{gio, gio::prelude::*};
use std::path::PathBuf;

async fn with_plugin_host<F, T>(f: F)
where
    F: FnOnce(InactivePluginHostChannel) -> T,
    T: Future<Output = ()>,
{
    let (plugin_host, plugin_channel) =
        PluginHost::new(&PathBuf::from("plugins"), &PathBuf::from("plugins"));
    select! {
        _ = plugin_host.fuse() => {}
        _ = f(plugin_channel).fuse() => {}
    }
}

async fn wait_for_initialization(channel: &mut PluginHostChannel, plugin: &str) {
    channel.send(MessageToPluginHost::GetPlugins);
    loop {
        if let MessageFromPluginHost::Plugins(plugins) = channel.receive().await {
            let mut done = false;
            for (name, data) in plugins.into_iter() {
                if &name == plugin {
                    if !data.metadata.enabled {
                        channel.send(MessageToPluginHost::StartPlugin(name));
                    } else if !data.initializing {
                        done = true;
                    }
                } else if data.metadata.enabled {
                    channel.send(MessageToPluginHost::StopPlugin(name));
                }
            }
            if done {
                return;
            } else {
                break;
            }
        }
    }

    loop {
        if let MessageFromPluginHost::PluginUpdated(name, data) = channel.receive().await {
            if &name == plugin && !data.initializing {
                break;
            }
        }
    }
}

fn get_file(path: &str) -> File {
    let file = gio::File::for_path(path);
    let file_info = file
        .query_info(
            File::DEFAULT_ATTRIBUTES,
            gio::FileQueryInfoFlags::NONE,
            gio::Cancellable::NONE,
        )
        .unwrap();
    File::new_from_file(file, file_info)
}

async fn get_response(
    channel: &mut PluginHostChannel,
    request: ApiRequestToPlugin,
) -> Option<ApiResponseFromPlugin> {
    channel.send(MessageToPluginHost::ApiRequest {
        id: 0,
        plugin_name: None,
        request,
    });
    loop {
        if let MessageFromPluginHost::ApiResponse { id, response, .. } = channel.receive().await
            && id == 0
        {
            return response;
        }
    }
}

#[async_std::test]
async fn test_archives_menus() {
    with_plugin_host(async |channel| {
        let mut channel = channel.activate_cloned();
        wait_for_initialization(&mut channel, "archives.py").await;

        if let Some(ApiResponseFromPlugin::MainMenuItems(items)) =
            get_response(&mut channel, ApiRequestToPlugin::MainMenuItems).await
        {
            assert_eq!(items.len(), 2);
            assert!(!items[0].label.is_empty());
            assert_eq!(&items[0].action, "compress");
            assert!(!items[1].label.is_empty());
            assert_eq!(&items[1].action, "uncompress");
        } else {
            assert!(false);
        }

        let state = PanelsState {
            active_directory_path: Some("testfiles".to_owned()),
            active_directory_uri: "file://testfiles".to_owned(),
            active_focused_file: Some("archive".to_owned()),
            active_selected_files: vec!["archive".to_owned()],
            inactive_directory_path: None,
            inactive_directory_uri: "file://testfiles".to_owned(),
            inactive_focused_file: None,
            inactive_selected_files: vec![],
        };
        if let Some(ApiResponseFromPlugin::ContextMenuItems(items)) =
            get_response(&mut channel, ApiRequestToPlugin::ContextMenuItems(state)).await
        {
            assert_eq!(items.len(), 2);
            assert!(!items[0].label.is_empty());
            assert_eq!(&items[0].action, "compress");
            assert!(!items[1].label.is_empty());
            assert_eq!(&items[1].action, "uncompress");
        } else {
            assert!(false);
        }

        let state = PanelsState {
            active_directory_path: Some("testfiles".to_owned()),
            active_directory_uri: "file://testfiles".to_owned(),
            active_focused_file: Some("apev2.tag".to_owned()),
            active_selected_files: vec!["apev2.tag".to_owned()],
            inactive_directory_path: None,
            inactive_directory_uri: "file://testfiles".to_owned(),
            inactive_focused_file: None,
            inactive_selected_files: vec![],
        };
        if let Some(ApiResponseFromPlugin::ContextMenuItems(items)) =
            get_response(&mut channel, ApiRequestToPlugin::ContextMenuItems(state)).await
        {
            assert_eq!(items.len(), 1);
            assert!(!items[0].label.is_empty());
            assert_eq!(&items[0].action, "compress");
        } else {
            assert!(false);
        }
    })
    .await;
}

#[async_std::test]
async fn test_document_tag_extraction() {
    with_plugin_host(async |channel| {
        wait_for_initialization(&mut channel.activate_cloned(), "document-metadata.py").await;

        for path in [
            "testfiles/document.odt",
            "testfiles/document.docx",
            "testfiles/document_xmp.pdf",
            "testfiles/document_legacy.pdf",
        ] {
            let extractor = PluginMetadataExtractor::new(channel.clone());
            let file = get_file(path);
            let metadata = extractor.extract_metadata(&file).await;

            let (tag, value) = metadata
                .iter()
                .find(|(tag, _)| tag.id() == "Doc.Title")
                .unwrap();
            assert!(!tag.description().is_empty());
            assert_eq!(value, "Some document");

            let (tag, value) = metadata
                .iter()
                .find(|(tag, _)| tag.id() == "Doc.Generator")
                .unwrap();
            assert!(!tag.description().is_empty());
            assert!(value.starts_with("LibreOffice"));

            if !path.ends_with(".pdf") {
                let (tag, value) = metadata
                    .iter()
                    .find(|(tag, _)| tag.id() == "Doc.UserDefined.My property")
                    .unwrap();
                assert_eq!(tag.name(), "User-Defined: My property");
                assert!(tag.description().is_empty());
                assert_eq!(value, "Something");
            }
        }
    })
    .await;
}

#[async_std::test]
async fn test_image_tag_extraction() {
    with_plugin_host(async |channel| {
        wait_for_initialization(&mut channel.activate_cloned(), "image-metadata.py").await;

        for path in ["testfiles/image.jpg", "testfiles/image.png"] {
            let extractor = PluginMetadataExtractor::new(channel.clone());
            let file = get_file(path);
            let metadata = extractor.extract_metadata(&file).await;
            assert_eq!(metadata.len(), 1);

            let (tag, value) = &metadata[0];
            assert_eq!(tag.id(), "Exif.Photo.UserComment");
            assert!(!tag.description().is_empty());
            assert_eq!(value, "Great image");
        }
    })
    .await;
}

#[async_std::test]
async fn test_multimedia_tag_extraction() {
    with_plugin_host(async |channel| {
        wait_for_initialization(&mut channel.activate_cloned(), "multimedia-metadata.py").await;

        let extractor = PluginMetadataExtractor::new(channel.clone());
        let file = get_file("testfiles/apev2.tag");
        let metadata = extractor.extract_metadata(&file).await;
        assert_eq!(metadata.len(), 2);

        let (tag, value) = &metadata[0];
        assert_eq!(tag.id(), "APEv2.Title");
        assert!(!tag.description().is_empty());
        assert_eq!(value, "Test Track");

        let (tag, value) = &metadata[1];
        assert_eq!(tag.id(), "APEv2.unknown");
        assert_eq!(tag.name(), "unknown");
        assert!(tag.description().is_empty());
        assert_eq!(value, "something");
    })
    .await;
}
