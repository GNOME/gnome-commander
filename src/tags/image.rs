/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */

use crate::libgcmd::file_metadata_extractor::FileMetadataExtractor;
use gettextrs::gettext;
use gtk::{gdk_pixbuf, glib::subclass::prelude::*, prelude::*};

use super::tags::GnomeCmdTag;

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
        tags::tags::GnomeCmdTagClass,
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
            if let Some((_format, width, height)) = fd
                .file()
                .path()
                .and_then(|p| gdk_pixbuf::Pixbuf::file_info(p))
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
