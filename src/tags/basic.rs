// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{FileMetadataExtractor, Tag};
use crate::{
    file::{File, FileOps},
    utils::{permissions_to_text, u32_enum},
};
use gettextrs::gettext;
use std::borrow::Cow;

u32_enum! {
    enum FileTag {
        #[default]
        Name,
        Path,
        Link,
        Size,
        Modified,
        Accessed,
        Permissions,
        Format,
        Publisher,
        Description,
        Keywords,
        Rank,
        Content,
    }
}

impl FileTag {
    fn boxed(self) -> Box<dyn Tag> {
        Box::new(self)
    }
}

impl Tag for FileTag {
    fn id(&self) -> &str {
        match self {
            Self::Name => "File.Name",
            Self::Path => "File.Path",
            Self::Link => "File.Link",
            Self::Size => "File.Size",
            Self::Modified => "File.Modified",
            Self::Accessed => "File.Accessed",
            Self::Permissions => "File.Permissions",
            Self::Format => "File.Format",
            Self::Publisher => "File.Publisher",
            Self::Description => "File.Description",
            Self::Keywords => "File.Keywords",
            Self::Rank => "File.Rank",
            Self::Content => "File.Content",
        }
    }

    fn class(&self) -> Cow<'_, str> {
        Cow::Owned(gettext("File"))
    }

    fn name(&self) -> Cow<'_, str> {
        Cow::Owned(match self {
            Self::Name => gettext("Name"),
            Self::Path => gettext("Path"),
            Self::Link => gettext("Link"),
            Self::Size => gettext("Size"),
            Self::Modified => gettext("Modified"),
            Self::Accessed => gettext("Accessed"),
            Self::Permissions => gettext("Permissions"),
            Self::Format => gettext("Format"),
            Self::Publisher => gettext("Publisher"),
            Self::Description => gettext("Description"),
            Self::Keywords => gettext("Keywords"),
            Self::Rank => gettext("Rank"),
            Self::Content => gettext("Content"),
        })
    }

    fn description(&self) -> Cow<'_, str> {
        Cow::Owned(match self {
            Self::Name => gettext("File name excluding path but including the file extension."),
            Self::Path => gettext("Full file path of file excluding the file name."),
            Self::Link => gettext("URI of link target."),
            Self::Size => {
                gettext("Size of the file in bytes or if a directory number of items it contains.")
            }
            Self::Modified => gettext("Last modified datetime."),
            Self::Accessed => gettext("Last access datetime."),
            Self::Permissions => gettext("Permission string in unix format eg “-rw-r--r--”."),
            Self::Format => {
                gettext("MIME type of the file or if a directory it should contain value “Folder”.")
            }
            Self::Publisher => gettext(
                "Editable DC type for the name of the publisher of the file (EG dc:publisher field in RSS feed).",
            ),
            Self::Description => gettext("Editable free text/notes."),
            Self::Keywords => gettext("Editable array of keywords."),
            Self::Rank => gettext(
                "Editable file rank for grading favourites. Value should be in the range 1..10.",
            ),
            Self::Content => gettext("File’s contents filtered as plain text."),
        })
    }

    fn clone(&self) -> Box<dyn Tag> {
        (*self).boxed()
    }
}

#[derive(Debug, Default)]
pub struct BasicMetadataExtractor {}

impl FileMetadataExtractor for BasicMetadataExtractor {
    async fn supported_tags(&self) -> Vec<(String, Vec<Box<dyn Tag>>)> {
        let class = FileTag::Name.class().into_owned();
        vec![(class, FileTag::all().map(FileTag::boxed).collect())]
    }

    fn summary_tags(&self) -> Vec<Box<dyn Tag>> {
        [FileTag::Description, FileTag::Publisher]
            .into_iter()
            .map(FileTag::boxed)
            .collect()
    }

    async fn extract_metadata(&self, file: &File) -> Vec<(Box<dyn Tag>, String)> {
        let mut tags = Vec::new();

        tags.push((FileTag::Name.boxed(), file.name()));
        if let Some(path) = file.parent_path() {
            tags.push((FileTag::Path.boxed(), path.to_string_lossy().to_string()));
        }
        tags.push((FileTag::Link.boxed(), file.uri()));
        if let Some(size) = file.size() {
            tags.push((FileTag::Size.boxed(), size.to_string()));
        }

        if let Some(dt) = file
            .access_date()
            .and_then(|dt| dt.format("%Y-%m-%d %T").ok())
        {
            tags.push((FileTag::Accessed.boxed(), dt.to_string()));
        }
        if let Some(dt) = file
            .modification_date()
            .and_then(|dt| dt.format("%Y-%m-%d %T").ok())
        {
            tags.push((FileTag::Modified.boxed(), dt.to_string()));
        }

        tags.push((
            FileTag::Permissions.boxed(),
            permissions_to_text(file.permissions()),
        ));

        if file.is_directory() {
            tags.push((FileTag::Format.boxed(), gettext("Folder")));
        } else if let Some(content_type) = file.content_type() {
            tags.push((FileTag::Format.boxed(), content_type.to_string()));
        }

        tags
    }
}
