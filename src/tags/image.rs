// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::libgcmd::file_metadata_extractor::FileMetadataExtractor;
use gettextrs::gettext;
use gtk::{gdk_pixbuf, glib::subclass::prelude::*, prelude::*};

use super::GnomeCmdTag;

#[derive(Clone, Copy, strum::VariantArray, strum::IntoStaticStr, strum::EnumString)]
enum ImageTag {
    #[strum(serialize = "Image.Width")]
    Width,
    #[strum(serialize = "Image.Height")]
    Height,
    #[strum(serialize = "Image.Size")]
    Size,
}

impl ImageTag {
    fn tag(&self) -> GnomeCmdTag {
        let str: &'static str = self.into();
        GnomeCmdTag(str.into())
    }

    fn name(self) -> String {
        match self {
            Self::Width => gettext("Width"),
            Self::Height => gettext("Height"),
            Self::Size => gettext("Image size"),
        }
    }

    fn description(self) -> String {
        match self {
            Self::Width => gettext("Width in pixels."),
            Self::Height => gettext("Height in pixels."),
            Self::Size => gettext("Width and height in pixels."),
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
        tags::GnomeCmdTagClass,
    };
    use std::str::FromStr;
    use strum::VariantArray;

    #[derive(Default)]
    pub struct ImageMetadataExtractor {}

    #[glib::object_subclass]
    impl ObjectSubclass for ImageMetadataExtractor {
        const NAME: &'static str = "GnomeCmdImageMetadataExtractor";
        type Type = super::ImageMetadataExtractor;
        type Interfaces = (FileMetadataExtractor,);
    }

    impl ObjectImpl for ImageMetadataExtractor {}

    impl FileMetadataExtractorImpl for ImageMetadataExtractor {
        fn supported_tags(&self) -> Vec<GnomeCmdTag> {
            ImageTag::VARIANTS.iter().map(|t| t.tag()).collect()
        }

        fn summary_tags(&self) -> Vec<GnomeCmdTag> {
            [ImageTag::Size].iter().map(|t| t.tag()).collect()
        }

        fn class_name(&self, class: &GnomeCmdTagClass) -> Option<String> {
            if class.0 == "Image" {
                Some(gettext("Image"))
            } else {
                None
            }
        }

        fn tag_name(&self, tag: &GnomeCmdTag) -> Option<String> {
            Some(ImageTag::from_str(&tag.0).ok()?.name())
        }

        fn tag_description(&self, tag: &GnomeCmdTag) -> Option<String> {
            Some(ImageTag::from_str(&tag.0).ok()?.description())
        }

        fn extract_metadata<F: FnMut(GnomeCmdTag, Option<&str>)>(
            &self,
            fd: &impl IsA<FileDescriptor>,
            mut add: F,
        ) {
            if let Some((_format, width, height)) =
                fd.file().path().and_then(gdk_pixbuf::Pixbuf::file_info)
            {
                (add)(ImageTag::Width.tag(), Some(&width.to_string()));
                (add)(ImageTag::Height.tag(), Some(&height.to_string()));
                (add)(ImageTag::Size.tag(), Some(&format!("{width} x {height}")));
            }
        }
    }
}

glib::wrapper! {
    pub struct ImageMetadataExtractor(ObjectSubclass<imp::ImageMetadataExtractor>)
        @implements FileMetadataExtractor;
}

impl Default for ImageMetadataExtractor {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}
