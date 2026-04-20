// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::GnomeCmdTag;
use crate::{libgcmd::file_metadata_extractor::FileMetadataExtractor, utils::u32_enum};
use gettextrs::gettext;
use gtk::{gio, glib::subclass::prelude::*, prelude::*};

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
    fn tag(&self) -> GnomeCmdTag {
        let str = match self {
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
        };
        GnomeCmdTag(str.into())
    }

    fn from_tag(tag: &GnomeCmdTag) -> Option<Self> {
        Self::all().find(|t| &t.tag() == tag)
    }

    fn name(self) -> String {
        match self {
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
        }
    }

    fn description(self) -> String {
        match self {
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
        }
    }
}

mod imp {
    use super::*;
    use crate::{
        libgcmd::{
            file_descriptor::{FileDescriptor, FileDescriptorExt},
            file_metadata_extractor::FileMetadataExtractorImpl,
        },
        tags::{GnomeCmdTag, GnomeCmdTagClass},
        utils::permissions_to_text,
    };

    #[derive(Default)]
    pub struct BasicMetadataExtractor {}

    #[glib::object_subclass]
    impl ObjectSubclass for BasicMetadataExtractor {
        const NAME: &'static str = "GnomeCmdBasicMetadataExtractor";
        type Type = super::BasicMetadataExtractor;
        type Interfaces = (FileMetadataExtractor,);
    }

    impl ObjectImpl for BasicMetadataExtractor {}

    impl FileMetadataExtractorImpl for BasicMetadataExtractor {
        fn supported_tags(&self) -> Vec<GnomeCmdTag> {
            FileTag::all().map(|t| t.tag()).collect()
        }

        fn summary_tags(&self) -> Vec<GnomeCmdTag> {
            [FileTag::Description, FileTag::Publisher]
                .iter()
                .map(|t| t.tag())
                .collect()
        }

        fn class_name(&self, class: &GnomeCmdTagClass) -> Option<String> {
            if class.0 == "File" {
                Some(gettext("File"))
            } else {
                None
            }
        }

        fn tag_name(&self, tag: &GnomeCmdTag) -> Option<String> {
            Some(FileTag::from_tag(tag)?.name())
        }

        fn tag_description(&self, tag: &GnomeCmdTag) -> Option<String> {
            Some(FileTag::from_tag(tag)?.description())
        }

        fn extract_metadata<F: FnMut(GnomeCmdTag, Option<&str>)>(
            &self,
            fd: &impl IsA<FileDescriptor>,
            mut add: F,
        ) {
            let file = fd.file();
            let file_info = fd.file_info();

            (add)(FileTag::Name.tag(), Some(&file_info.display_name()));
            (add)(
                FileTag::Path.tag(),
                file.parent()
                    .and_then(|p| p.path())
                    .as_ref()
                    .and_then(|p| p.to_str()),
            );
            (add)(FileTag::Link.tag(), Some(&file.uri()));
            (add)(
                FileTag::Size.tag(),
                Some(
                    &file_info
                        .attribute_uint64(gio::FILE_ATTRIBUTE_STANDARD_SIZE)
                        .to_string(),
                ),
            );

            if let Ok(info) = file.query_info(
                "time::*",
                gio::FileQueryInfoFlags::NONE,
                gio::Cancellable::NONE,
            ) {
                (add)(
                    FileTag::Accessed.tag(),
                    info.access_date_time()
                        .and_then(|dt| dt.format("%Y-%m-%d %T").ok())
                        .as_deref(),
                );

                (add)(
                    FileTag::Modified.tag(),
                    info.modification_date_time()
                        .and_then(|dt| dt.format("%Y-%m-%d %T").ok())
                        .as_deref(),
                );
            }

            (add)(
                FileTag::Permissions.tag(),
                Some(&permissions_to_text(
                    file_info.attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE),
                )),
            );

            if file_info.file_type() == gio::FileType::Directory {
                (add)(FileTag::Format.tag(), Some(&gettext("Folder")));
            } else {
                (add)(
                    FileTag::Format.tag(),
                    file_info
                        .attribute_string(gio::FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)
                        .as_deref(),
                );
            }
        }
    }
}

glib::wrapper! {
    pub struct BasicMetadataExtractor(ObjectSubclass<imp::BasicMetadataExtractor>)
        @implements FileMetadataExtractor;
}

impl Default for BasicMetadataExtractor {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}
