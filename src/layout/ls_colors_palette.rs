// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::ls_colors::{LsPalletteColor, LsPallettePlane};
use crate::options::{ColorOptions, types::RGBAOption};
use gtk::gdk;
use std::{cell::RefCell, rc::Rc};

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

    fn create_css(&self) -> String {
        let mut css = String::from(":root {");
        for plane in LsPallettePlane::all() {
            for palette_color in LsPalletteColor::all() {
                css.push_str(&format!(
                    "--ls-color-{}-{}: {};\n",
                    plane.name(),
                    palette_color.name(),
                    self.color(plane, palette_color).to_str()
                ));
            }
        }
        css.push('}');
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

pub fn load_palette(settings: &ColorOptions) -> LsColorsPalette {
    let mut palette: LsColorsPalette = Default::default();
    for ((plane, palette_color), option) in options(settings) {
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

type Callback = Box<dyn Fn(&LsColorPalettes)>;

pub struct LsColorPalettes {
    callback: RefCell<Option<Callback>>,
    settings: ColorOptions,
    css_provider: Option<gtk::CssProvider>,
}

impl LsColorPalettes {
    pub fn new() -> Rc<Self> {
        let css_provider = gdk::Display::default().map(|display| {
            let css_provider = gtk::CssProvider::new();
            gtk::style_context_add_provider_for_display(
                &display,
                &css_provider,
                gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
            );
            css_provider
        });

        let this = Rc::new(Self {
            callback: Default::default(),
            settings: ColorOptions::new(),
            css_provider,
        });

        for (_, option) in options(&this.settings) {
            let weak_ref = Rc::downgrade(&this);
            option.connect_changed(move |_| {
                if let Some(this) = weak_ref.upgrade() {
                    this.update_palette();
                }
            });
        }
        this.update_palette();

        this
    }

    pub fn set_update_callback(&self, callback: impl Fn(&Self) + 'static) {
        self.callback.replace(Some(Box::new(callback)));
    }

    fn update_palette(&self) {
        let Some(css_provider) = self.css_provider.as_ref() else {
            eprintln!("No display");
            return;
        };

        let palette = load_palette(&self.settings);
        css_provider.load_from_string(&palette.create_css());

        if let Some(callback) = self.callback.borrow().as_ref() {
            (callback)(self);
        }
    }
}
