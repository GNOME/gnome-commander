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

use crate::{
    config::PIXMAPS_DIR, debug::debug, file::File, options::options::GeneralOptions,
    types::GraphicalLayoutMode,
};
use gtk::{gio, glib, prelude::*};
use std::{cell::RefCell, collections::HashMap, path::Path, rc::Rc, sync::LazyLock};

pub struct IconCache {
    file_type_icons: HashMap<gio::FileType, gio::Icon>,
    symlink: gio::Emblem,
    options: GeneralOptions,
    cache: RefCell<HashMap<IconCacheKey, Option<gio::Icon>>>,
}

#[derive(PartialEq, Eq, Hash)]
struct IconCacheKey {
    mime_type: Option<String>,
    file_type: gio::FileType,
    symlink: bool,
}

impl IconCache {
    fn new() -> Rc<Self> {
        let mut file_type_icons = HashMap::new();
        let file_icons_dir = Path::new(PIXMAPS_DIR).join("file-type-icons");
        if let Some(icon) = load_icon(&file_icons_dir.join("file_type_regular.xpm")) {
            file_type_icons.insert(gio::FileType::Unknown, icon.clone());
            file_type_icons.insert(gio::FileType::Regular, icon);
        }
        if let Some(icon) = load_icon(&file_icons_dir.join("file_type_dir.xpm")) {
            file_type_icons.insert(gio::FileType::Directory, icon);
        }
        if let Some(icon) = load_icon(&file_icons_dir.join("file_type_symlink.xpm")) {
            file_type_icons.insert(gio::FileType::SymbolicLink, icon.clone());
            file_type_icons.insert(gio::FileType::Shortcut, icon);
        }
        if let Some(icon) = load_icon(&file_icons_dir.join("file_type_socket.xpm")) {
            file_type_icons.insert(gio::FileType::Special, icon);
        }
        if let Some(icon) = load_icon(&file_icons_dir.join("file_type_block_device.xpm")) {
            file_type_icons.insert(gio::FileType::Mountable, icon);
        }

        let options = GeneralOptions::new();

        let this = Rc::new(Self {
            file_type_icons,
            symlink: gio::Emblem::new(&gio::ThemedIcon::new("overlay_symlink")),
            options,
            cache: Default::default(),
        });

        this.options.mime_icon_dir.connect_changed(glib::clone!(
            #[weak]
            this,
            move |_| {
                this.cache.borrow_mut().clear();
            }
        ));

        this
    }

    pub fn mime_type_icon(
        &self,
        file_type: gio::FileType,
        mime_type: &str,
        symlink: bool,
    ) -> Option<gio::Icon> {
        let cache_key = IconCacheKey {
            mime_type: Some(mime_type.to_owned()),
            file_type,
            symlink,
        };
        self.cache
            .borrow_mut()
            .entry(cache_key)
            .or_insert_with(|| {
                let theme_icon_dir = self.options.mime_icon_dir.get()?;
                let icon = self.mime_icon_in_dir(&theme_icon_dir, file_type, mime_type)?;
                Some(self.maybe_symlink(&icon, symlink))
            })
            .clone()
    }

    pub fn file_type_icon(&self, file_type: gio::FileType, symlink: bool) -> Option<gio::Icon> {
        let cache_key = IconCacheKey {
            mime_type: None,
            file_type,
            symlink,
        };
        self.cache
            .borrow_mut()
            .entry(cache_key)
            .or_insert_with(|| {
                let icon = self.file_type_icons.get(&file_type)?;
                Some(self.maybe_symlink(icon, symlink))
            })
            .clone()
    }

    fn maybe_symlink(&self, icon: &gio::Icon, symlink: bool) -> gio::Icon {
        if symlink {
            gio::EmblemedIcon::new(icon, Some(&self.symlink)).upcast()
        } else {
            icon.clone()
        }
    }

    /// Tries to load an image for the specified mime-type in the specified directory.
    /// If symlink is true a smaller symlink image is painted over the image to indicate this.
    fn mime_icon_in_dir(
        &self,
        icon_dir: &Path,
        file_type: gio::FileType,
        mime_type: &str,
    ) -> Option<gio::Icon> {
        if file_type == gio::FileType::SymbolicLink || file_type == gio::FileType::Shortcut {
            return None;
        }

        debug!('y', "Looking up pixmap for: {mime_type}");

        let icon = try_load_icon(&icon_dir.join(mime_icon_name(mime_type)))
            .or_else(|| {
                category_icon_path(mime_type)
                    .and_then(|file_name| try_load_icon(&icon_dir.join(file_name)))
            })
            .or_else(|| try_load_icon(&icon_dir.join(type_icon_name(file_type))));

        debug!('z', "Icon for {} found: {}", mime_type, icon.is_some());

        icon
    }

    pub fn file_icon(&self, file: &File, mode: GraphicalLayoutMode) -> Option<gio::Icon> {
        let file_type = file.file_info().file_type();
        let is_symlink = !file.is_dotdot() && file.file_info().is_symlink();

        match mode {
            GraphicalLayoutMode::MimeIcons => file
                .content_type()
                .and_then(|mime_type| self.mime_type_icon(file_type, &mime_type, is_symlink))
                .or_else(|| self.file_type_icon(file_type, is_symlink)),
            GraphicalLayoutMode::TypeIcons => self.file_type_icon(file_type, is_symlink),
            GraphicalLayoutMode::Text => None,
        }
    }
}

fn try_load_icon(path: &Path) -> Option<gio::Icon> {
    debug!('z', "Trying {}", path.display());
    if path.exists() {
        Some(gio::FileIcon::new(&gio::File::for_path(path)).upcast())
    } else {
        None
    }
}

fn load_icon(path: &Path) -> Option<gio::Icon> {
    debug!('i', "imageloader: loading pixmap: {}", path.display());
    try_load_icon(path).or_else(|| {
        eprintln!("Cannot load icon {}. File wasn't found.", path.display());
        None
    })
}

/// Takes a mime-type as argument and returns the filename
/// of the image representing it.
fn mime_icon_name(mime_type: &str) -> String {
    format!("gnome-{}.png", mime_type.replace('/', "-"))
}

/// Returns the file name that an image representing the given filetype should have.
fn type_icon_name(file_type: gio::FileType) -> &'static str {
    match file_type {
        gio::FileType::Directory => "i-directory.png",
        gio::FileType::Regular => "i-regular.png",
        gio::FileType::SymbolicLink => "i-symlink.png",
        // TODO: Add filetype names for G_FILE_TYPE_SHORTCUT and G_FILE_TYPE_MOUNTABLE
        _ => "i-regular.png",
    }
}

/// Returns the file name of the image representing the category of the given
/// mime-type. This is a hack to avoid having 20 equal
/// icons for 20 different video formats etc.
fn category_icon_path(mime_type: &str) -> Option<&str> {
    if mime_type.starts_with("text") {
        Some("gnome-text-plain.png")
    } else if mime_type.starts_with("video") {
        Some("gnome-video-plain.png")
    } else if mime_type.starts_with("image") {
        Some("gnome-image-plain.png")
    } else if mime_type.starts_with("audio") {
        Some("gnome-audio-plain.png")
    } else if mime_type.starts_with("pack") {
        Some("gnome-pack-plain.png")
    } else if mime_type.starts_with("font") {
        Some("gnome-font-plain.png")
    } else {
        None
    }
}

pub fn icon_cache() -> Rc<IconCache> {
    static ICON_CACHE: LazyLock<glib::thread_guard::ThreadGuard<Rc<IconCache>>> =
        LazyLock::new(|| glib::thread_guard::ThreadGuard::new(IconCache::new()));
    ICON_CACHE.get_ref().clone()
}
