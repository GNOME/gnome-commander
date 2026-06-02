// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{FileMetadataExtractor, Tag};
use crate::{
    file::{File, FileOps},
    utils::u32_enum,
};
use gettextrs::gettext;
use gtk::gdk_pixbuf::Pixbuf;
use std::borrow::Cow;

u32_enum! {
    enum ImageTag {
        #[default]
        Width,
        Height,
        Size,
    }
}

impl ImageTag {
    fn boxed(self) -> Box<dyn Tag> {
        Box::new(self)
    }
}

impl Tag for ImageTag {
    fn id(&self) -> &str {
        match self {
            Self::Width => "Image.Width",
            Self::Height => "Image.Height",
            Self::Size => "Image.Size",
        }
    }

    fn class(&self) -> Cow<'_, str> {
        Cow::Owned(gettext("Image"))
    }

    fn name(&self) -> Cow<'_, str> {
        Cow::Owned(match self {
            Self::Width => gettext("Width"),
            Self::Height => gettext("Height"),
            Self::Size => gettext("Image size"),
        })
    }

    fn description(&self) -> Cow<'_, str> {
        Cow::Owned(match self {
            Self::Width => gettext("Width in pixels."),
            Self::Height => gettext("Height in pixels."),
            Self::Size => gettext("Width and height in pixels."),
        })
    }

    fn clone(&self) -> Box<dyn Tag> {
        (*self).boxed()
    }
}

#[derive(Debug, Default)]
pub struct ImageMetadataExtractor {}

impl FileMetadataExtractor for ImageMetadataExtractor {
    async fn supported_tags(&self) -> Vec<(String, Vec<Box<dyn Tag>>)> {
        let class = ImageTag::Size.class().into_owned();
        vec![(class, ImageTag::all().map(ImageTag::boxed).collect())]
    }

    fn summary_tags(&self) -> Vec<Box<dyn Tag>> {
        [ImageTag::Size].into_iter().map(ImageTag::boxed).collect()
    }

    async fn extract_metadata(&self, file: &File) -> Vec<(Box<dyn Tag>, String)> {
        if let Some(path) = file.local_path()
            && let Ok(Some((_format, width, height))) = Pixbuf::file_info_future(path).await
        {
            vec![
                (ImageTag::Width.boxed(), width.to_string()),
                (ImageTag::Height.boxed(), height.to_string()),
                (ImageTag::Size.boxed(), format!("{width} x {height}")),
            ]
        } else {
            vec![]
        }
    }
}
