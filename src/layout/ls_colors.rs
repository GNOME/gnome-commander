/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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

use gtk::gio;
use std::{collections::HashMap, sync::OnceLock};

const DEFAULT_COLORS: &str = "no=00:fi=00:di=01;34:ln=01;36:pi=40;33:so=01;35:do=01;35:bd=40;33;01:cd=40;33;01:or=40;31;01:ex=01;32:*.tar=01;31:*.tgz=01;31:*.arj=01;31:*.taz=01;31:*.lzh=01;31:*.zip=01;31:*.z=01;31:*.Z=01;31:*.gz=01;31:*.bz2=01;31:*.deb=01;31:*.rpm=01;31:*.jar=01;31:*.jpg=01;35:*.jpeg=01;35:*.png=01;35:*.gif=01;35:*.bmp=01;35:*.pbm=01;35:*.pgm=01;35:*.ppm=01;35:*.tga=01;35:*.xbm=01;35:*.xpm=01;35:*.tif=01;35:*.tiff=01;35:*.mpg=01;35:*.mpeg=01;35:*.avi=01;35:*.fli=01;35:*.gl=01;35:*.dl=01;35:*.xcf=01;35:*.xwd=01;35:*.ogg=01;35:*.mp3=01;35:";

#[repr(C)]
#[derive(
    Clone,
    Copy,
    PartialEq,
    Eq,
    Hash,
    strum::EnumCount,
    strum::VariantArray,
    strum::AsRefStr,
    strum::FromRepr,
)]
#[strum(serialize_all = "kebab-case")]
pub enum LsPallettePlane {
    #[strum(serialize = "fg")]
    Foreground = 0,
    #[strum(serialize = "bg")]
    Background,
}

#[repr(C)]
#[derive(
    Clone,
    Copy,
    PartialEq,
    Eq,
    Hash,
    strum::EnumCount,
    strum::VariantArray,
    strum::AsRefStr,
    strum::FromRepr,
)]
#[strum(serialize_all = "kebab-case")]
pub enum LsPalletteColor {
    Black = 0,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
}

#[derive(Clone)]
pub struct Colors {
    pub fg: Option<LsPalletteColor>,
    pub bg: Option<LsPalletteColor>,
}

struct LsColors {
    ext_map: HashMap<String, Colors>,
    type_map: HashMap<gio::FileType, Colors>,
}

impl LsColors {
    fn new(ls_colors: &str) -> Self {
        let mut ext_map = HashMap::new();
        let mut type_map = HashMap::new();
        for entry in ls_colors.split(':') {
            let Some((key, val)) = entry.split_once('=') else {
                continue;
            };
            let Some(colors) = code_to_color(val) else {
                continue;
            };

            if let Some(ext) = key.strip_prefix("*.") {
                ext_map.insert(ext.to_string(), colors);
            } else if let Some(file_type) = code_to_type(key) {
                type_map.insert(file_type, colors);
            }
        }
        Self { ext_map, type_map }
    }

    fn from_env() -> Self {
        let env = std::env::var("LS_COLORS");
        let ls_colors = env.as_deref().unwrap_or(DEFAULT_COLORS);
        Self::new(ls_colors)
    }

    fn get(&self, file_info: &gio::FileInfo) -> Option<Colors> {
        let file_type = file_info.file_type();
        if file_type == gio::FileType::SymbolicLink || file_type == gio::FileType::Directory {
            return self.type_map.get(&file_type).cloned();
        }

        if let Some(colors) = file_info
            .name()
            .extension()
            .and_then(|e| e.to_str())
            .and_then(|e| self.ext_map.get(e))
        {
            return Some(colors.clone());
        }

        self.type_map.get(&file_type).cloned()
    }
}

/// Attribute codes:
/// 00=none 01=bold 04=underscore 05=blink 07=reverse 08=concealed
///
/// Text color codes:
/// 30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white
///
/// Background color codes:
/// 40=black 41=red 42=green 43=yellow 44=blue 45=magenta 46=cyan 47=white
fn code_to_color(codes: &str) -> Option<Colors> {
    let mut fg = None;
    let mut bg = None;
    for code in codes.split(';') {
        let code: u8 = code.parse().ok()?;
        match code {
            30 => fg = Some(LsPalletteColor::Black),
            31 => fg = Some(LsPalletteColor::Red),
            32 => fg = Some(LsPalletteColor::Green),
            33 => fg = Some(LsPalletteColor::Yellow),
            34 => fg = Some(LsPalletteColor::Blue),
            35 => fg = Some(LsPalletteColor::Magenta),
            36 => fg = Some(LsPalletteColor::Cyan),
            37 => fg = Some(LsPalletteColor::White),
            40 => bg = Some(LsPalletteColor::Black),
            41 => bg = Some(LsPalletteColor::Red),
            42 => bg = Some(LsPalletteColor::Green),
            43 => bg = Some(LsPalletteColor::Yellow),
            44 => bg = Some(LsPalletteColor::Blue),
            45 => bg = Some(LsPalletteColor::Magenta),
            46 => bg = Some(LsPalletteColor::Cyan),
            47 => bg = Some(LsPalletteColor::White),
            _ => {}
        }
    }
    if fg.is_some() || bg.is_some() {
        Some(Colors { fg, bg })
    } else {
        None
    }
}

fn code_to_type(key: &str) -> Option<gio::FileType> {
    match key {
        "fi" => Some(gio::FileType::Regular),
        "di" => Some(gio::FileType::Directory),
        "ln" => Some(gio::FileType::SymbolicLink),
        "pi" => Some(gio::FileType::Special),
        "sc" => Some(gio::FileType::Shortcut),
        "mn" => Some(gio::FileType::Mountable),
        _ => None,
    }
}

pub fn ls_colors_get(file_info: &gio::FileInfo) -> Option<Colors> {
    static LS_COLORS: OnceLock<LsColors> = OnceLock::new();
    let ls_colors = LS_COLORS.get_or_init(|| LsColors::from_env());
    ls_colors.get(file_info)
}
