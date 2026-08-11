// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::ls_colors::{LsPalletteColor, LsPallettePlane};
use crate::options::{ColorOptions, types::RGBAOption};
use gtk::gdk;
use std::{marker::PhantomData, rc::Rc};

type PlaneColors = [gdk::RGBA; LsPalletteColor::count()];

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
    colors: [PlaneColors; LsPallettePlane::count()],
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

    pub fn create_css(&self) -> String {
        let mut css = String::new();
        for plane in LsPallettePlane::all() {
            for palette_color in LsPalletteColor::all() {
                css.push_str(&format!(
                    "@define-color ls-color-{}-{} {};\n",
                    plane.name(),
                    palette_color.name(),
                    self.color(plane, palette_color).to_str()
                ));
            }
        }
        css
    }
}

fn options(
    settings: &ColorOptions,
) -> impl Iterator<Item = ((LsPallettePlane, LsPalletteColor), &RGBAOption)> {
    LsPalletteColor::all()
        .flat_map(|color| LsPallettePlane::all().map(move |plane| (plane, color)))
        .zip([
            &settings.lscm_black_fg,
            &settings.lscm_black_bg,
            &settings.lscm_red_fg,
            &settings.lscm_red_bg,
            &settings.lscm_green_fg,
            &settings.lscm_green_bg,
            &settings.lscm_yellow_fg,
            &settings.lscm_yellow_bg,
            &settings.lscm_blue_fg,
            &settings.lscm_blue_bg,
            &settings.lscm_magenta_fg,
            &settings.lscm_magenta_bg,
            &settings.lscm_cyan_fg,
            &settings.lscm_cyan_bg,
            &settings.lscm_white_fg,
            &settings.lscm_white_bg,
        ])
}

pub fn load_palette() -> LsColorsPalette {
    let mut palette: LsColorsPalette = Default::default();
    for ((plane, palette_color), option) in options(&ColorOptions::instance()) {
        palette.set_color(plane, palette_color, option.get());
    }
    palette
}

pub fn save_palette(
    palette: &LsColorsPalette,
    settings: &ColorOptions,
) -> Result<(), glib::error::BoolError> {
    for ((plane, palette_color), option) in options(settings) {
        option.set(*palette.color(plane, palette_color))?;
    }
    Ok(())
}

pub struct LsColorsPaletteListener<C: ?Sized> {
    settings: Rc<ColorOptions>,
    handlers: Vec<glib::SignalHandlerId>,
    callback: PhantomData<C>,
}

impl<C: Fn() + 'static> LsColorsPaletteListener<C> {
    /// Produces a listener object. As long as that object is alive, the callback will be called on
    /// color palette changes.
    pub fn new(callback: C) -> Self {
        let mut handlers = Vec::new();
        let callback = Rc::new(callback);

        let settings = ColorOptions::instance();
        for (_, option) in options(&settings) {
            let callback_cloned = callback.clone();
            handlers.push(option.connect_changed(move |_| {
                callback_cloned();
            }));
        }

        Self {
            settings,
            handlers,
            callback: Default::default(),
        }
    }
}

impl<C: ?Sized> Drop for LsColorsPaletteListener<C> {
    fn drop(&mut self) {
        for handler in self.handlers.drain(..) {
            self.settings.theme.disconnect(handler);
        }
    }
}
