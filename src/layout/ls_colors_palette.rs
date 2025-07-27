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

use super::ls_colors::{LsPalletteColor, LsPallettePlane};
use gtk::{gdk, gio, prelude::*};
use strum::{EnumCount, VariantArray};

type PlaneColors = [gdk::RGBA; LsPalletteColor::COUNT];

const DEFAULT_PLANE_COLORS: PlaneColors = [
    gdk::RGBA::BLACK,
    gdk::RGBA::RED,
    gdk::RGBA::GREEN,
    gdk::RGBA::new(1.0, 1.0, 0.0, 1.0),
    gdk::RGBA::BLUE,
    gdk::RGBA::new(1.0, 0.0, 1.0, 1.0),
    gdk::RGBA::new(0.0, 1.0, 1.0, 1.0),
    gdk::RGBA::WHITE,
];

#[derive(Clone)]
pub struct LsColorsPalette {
    colors: [PlaneColors; LsPallettePlane::COUNT],
}

impl Default for LsColorsPalette {
    fn default() -> Self {
        Self {
            colors: [DEFAULT_PLANE_COLORS, DEFAULT_PLANE_COLORS],
        }
    }
}

impl LsColorsPalette {
    pub fn color(&self, plane: LsPallettePlane, palette_color: LsPalletteColor) -> &gdk::RGBA {
        &self.colors[plane as usize][palette_color as usize]
    }

    pub fn set_color(
        &mut self,
        plane: LsPallettePlane,
        palette_color: LsPalletteColor,
        color: gdk::RGBA,
    ) {
        self.colors[plane as usize][palette_color as usize] = color;
    }
}

pub fn load_palette(settings: &gio::Settings) -> LsColorsPalette {
    let mut palette: LsColorsPalette = Default::default();
    for plane in LsPallettePlane::VARIANTS {
        for palette_color in LsPalletteColor::VARIANTS {
            let key = format!("lscm-{}-{}", palette_color.as_ref(), plane.as_ref());
            if let Ok(color) = gdk::RGBA::parse(settings.string(&key)) {
                palette.set_color(*plane, *palette_color, color);
            }
        }
    }
    palette
}

pub fn save_palette(
    palette: &LsColorsPalette,
    settings: &gio::Settings,
) -> Result<(), glib::error::BoolError> {
    for plane in LsPallettePlane::VARIANTS {
        for palette_color in LsPalletteColor::VARIANTS {
            let key = format!("lscm-{}-{}", palette_color.as_ref(), plane.as_ref());
            settings.set_string(&key, &palette.color(*plane, *palette_color).to_str())?;
        }
    }
    Ok(())
}
