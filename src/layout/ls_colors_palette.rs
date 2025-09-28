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

use super::{
    PREF_COLORS,
    ls_colors::{LsPalletteColor, LsPallettePlane},
};
use gtk::{gdk, gio, prelude::*, subclass::prelude::*};
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

    fn create_css(&self) -> String {
        let mut css = String::from(":root {");
        for plane in LsPallettePlane::VARIANTS {
            for palette_color in LsPalletteColor::VARIANTS {
                css.push_str(&format!(
                    "--ls-color-{}-{}: {};\n",
                    plane.as_ref(),
                    palette_color.as_ref(),
                    self.color(*plane, *palette_color).to_str()
                ));
            }
        }
        css.push_str("}");
        css
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

mod imp {
    use super::*;
    use glib::subclass::Signal;
    use std::{
        cell::{OnceCell, RefCell},
        sync::OnceLock,
    };

    #[derive(glib::Properties, Default)]
    #[properties(wrapper_type = super::LsColorPalettes)]
    pub struct LsColorPalettes {
        #[property(get, construct_only)]
        settings: OnceCell<gio::Settings>,
        #[property(get, construct_only)]
        display: OnceCell<Option<gdk::Display>>,
        #[property(get, set, nullable)]
        css_provider: RefCell<Option<gtk::CssProvider>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for LsColorPalettes {
        const NAME: &'static str = "GnomeCmdLsColorPalettes";
        type Type = super::LsColorPalettes;
    }

    #[glib::derived_properties]
    impl ObjectImpl for LsColorPalettes {
        fn constructed(&self) {
            self.parent_constructed();

            self.obj().settings().connect_changed(
                None,
                glib::clone!(
                    #[weak(rename_to = this)]
                    self.obj(),
                    move |_, _| this.update_palette()
                ),
            );
            self.obj().update_palette();
        }

        fn signals() -> &'static [Signal] {
            static SIGNALS: OnceLock<Vec<Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| vec![Signal::builder("palette-changed").build()])
        }
    }
}

glib::wrapper! {
    pub struct LsColorPalettes(ObjectSubclass<imp::LsColorPalettes>);
}

impl LsColorPalettes {
    pub fn new() -> Self {
        let settings = gio::Settings::new(PREF_COLORS);
        glib::Object::builder()
            .property("settings", settings)
            .property("display", gdk::Display::default())
            .build()
    }

    fn update_palette(&self) {
        let Some(display) = self.display() else {
            eprintln!("No display");
            return;
        };

        if let Some(css_provider) = self.css_provider() {
            gtk::style_context_remove_provider_for_display(&display, &css_provider);
        }

        let palette = load_palette(&self.settings());

        let css_provider = gtk::CssProvider::new();
        css_provider.load_from_string(&palette.create_css());

        gtk::style_context_add_provider_for_display(
            &display,
            &css_provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );

        self.set_css_provider(Some(css_provider));

        self.emit_by_name::<()>("palette-changed", &[]);
    }
}
