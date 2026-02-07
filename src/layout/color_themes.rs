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

use super::PREF_COLORS;
use gtk::{
    gdk, gio,
    glib::{self, subclass::prelude::*},
    prelude::*,
};
use std::{borrow::Cow, cell::RefCell};

const fn from_hex(color: &'static str) -> gdk::RGBA {
    const fn strip_prefix(input: &str, prefix: char) -> Option<&str> {
        if let Some((actual_prefix, bytes)) = input.as_bytes().split_first()
            && prefix == *actual_prefix as char
            && let Ok(result) = str::from_utf8(bytes)
        {
            Some(result)
        } else {
            None
        }
    }

    const fn parse_hex(input: &str) -> u32 {
        match u32::from_str_radix(input, 16) {
            Ok(result) => result,
            Err(_) => panic!("Invalid hex value"),
        }
    }

    const fn convert_16bit_color(color: u32) -> u32 {
        // 0000ABCD => 0A0B0C0D
        (((color & 0xF000) << 12) | ((color & 0xF00) << 8) | ((color & 0xF0) << 4) | (color & 0xF))
            // 0A0B0C0D => AABBCCDD
            * 0x11
    }

    let color = strip_prefix(color, '#').expect("Colors should start with #");

    let color = match color.len() {
        3 => convert_16bit_color((parse_hex(color) << 4) | 0xF),
        4 => convert_16bit_color(parse_hex(color)),
        6 => (parse_hex(color) << 8) | 0xFF,
        8 => parse_hex(color),
        _ => panic!("Invalid color length"),
    }
    .to_be_bytes();

    gdk::RGBA::new(
        (color[0] as f32) / 255.0,
        (color[1] as f32) / 255.0,
        (color[2] as f32) / 255.0,
        (color[3] as f32) / 255.0,
    )
}

#[derive(Clone)]
pub struct ColorTheme {
    pub sel_fg: gdk::RGBA,
    pub sel_bg: gdk::RGBA,
    pub norm_fg: gdk::RGBA,
    pub norm_bg: gdk::RGBA,
    pub curs_fg: gdk::RGBA,
    pub curs_bg: gdk::RGBA,
    pub alt_fg: gdk::RGBA,
    pub alt_bg: gdk::RGBA,
}

impl Default for ColorTheme {
    fn default() -> Self {
        COLOR_THEME_DEFAULT.clone()
    }
}

const NO_THEME_CSS: &str = r#"
    columnview.gnome-cmd-file-list listview .sel {
        font-weight: bold;
    }
"#;

impl ColorTheme {
    fn create_css(&self) -> String {
        format!(
            r#"
                columnview.gnome-cmd-file-list,
                columnview.gnome-cmd-file-list listview {{
                    background-color: {norm_bg};
                }}
                columnview.gnome-cmd-file-list listview row {{
                    background-color: unset;
                    color: {norm_fg};
                }}
                columnview.gnome-cmd-file-list listview row:nth-child(2n),
                columnview.gnome-cmd-file-list listview row:nth-child(2n):selected {{
                    background-color: {alt_bg};
                    color: {alt_fg};
                }}
                columnview.gnome-cmd-file-list listview row .sel {{
                    background-color: {sel_bg};
                    color: {sel_fg};
                }}
                columnview.gnome-cmd-file-list listview row:focus:selected .cell {{
                    background-color: {curs_bg};
                    color: {curs_fg};
                }}
                columnview.gnome-cmd-file-list listview row:selected .cell.sel {{
                    color: {sel_fg};
                }}
            "#,
            norm_bg = self.norm_bg,
            norm_fg = self.norm_fg,
            alt_bg = self.alt_bg,
            alt_fg = self.alt_fg,
            sel_bg = self.sel_bg,
            sel_fg = self.sel_fg,
            curs_bg = self.curs_bg,
            curs_fg = self.curs_fg,
        )
    }
}

const COLOR_THEME_DEFAULT: ColorTheme = ColorTheme {
    sel_fg: from_hex("#ff0000"),
    sel_bg: from_hex("#000044"),
    norm_fg: from_hex("#ffffff"),
    norm_bg: from_hex("#000044"),
    curs_fg: from_hex("#000000"),
    curs_bg: from_hex("#aaaaaa"),
    alt_fg: from_hex("#ffffff"),
    alt_bg: from_hex("#000044"),
};

const COLOR_THEME_MODERN: ColorTheme = ColorTheme {
    norm_fg: from_hex("#000000"),
    norm_bg: from_hex("#dddddd"),
    alt_fg: from_hex("#000000"),
    alt_bg: from_hex("#dddddd"),
    sel_fg: from_hex("#ff0000"),
    sel_bg: from_hex("#dddddd"),
    curs_fg: from_hex("#ffffff"),
    curs_bg: from_hex("#000044"),
};

const COLOR_THEME_FUSION: ColorTheme = ColorTheme {
    norm_fg: from_hex("#80ffff"),
    norm_bg: from_hex("#004080"),
    alt_fg: from_hex("#80ffff"),
    alt_bg: from_hex("#004080"),
    sel_fg: from_hex("#ffff00"),
    sel_bg: from_hex("#004080"),
    curs_fg: from_hex("#000080"),
    curs_bg: from_hex("#008080"),
};

const COLOR_THEME_CLASSIC: ColorTheme = ColorTheme {
    norm_fg: from_hex("#ffffff"),
    norm_bg: from_hex("#000044"),
    alt_fg: from_hex("#ffffff"),
    alt_bg: from_hex("#000044"),
    sel_fg: from_hex("#ffff00"),
    sel_bg: from_hex("#000044"),
    curs_fg: from_hex("#000000"),
    curs_bg: from_hex("#aaaaaa"),
};

const COLOR_THEME_DEEP_BLUE: ColorTheme = ColorTheme {
    norm_fg: from_hex("#00ffff"),
    norm_bg: from_hex("#000080"),
    alt_fg: from_hex("#00ffff"),
    alt_bg: from_hex("#000080"),
    sel_fg: from_hex("#ffff00"),
    sel_bg: from_hex("#000080"),
    curs_fg: from_hex("#000000"),
    curs_bg: from_hex("#aaaaaa"),
};

const COLOR_THEME_CAFEZINHO: ColorTheme = ColorTheme {
    norm_fg: from_hex("#e4ded5"),
    norm_bg: from_hex("#191511"),
    alt_fg: from_hex("#e4ded5"),
    alt_bg: from_hex("#191511"),
    sel_fg: from_hex("#ffcf36"),
    sel_bg: from_hex("#191511"),
    curs_fg: from_hex("#e4ded5"),
    curs_bg: from_hex("#4d4d4d"),
};

const COLOR_THEME_GREEN_TIGER: ColorTheme = ColorTheme {
    norm_fg: from_hex("#ffc600"),
    norm_bg: from_hex("#192e00"),
    alt_fg: from_hex("#ffc600"),
    alt_bg: from_hex("#1f3901"),
    sel_fg: from_hex("#ffffff"),
    sel_bg: from_hex("#000044"),
    curs_fg: from_hex("#000000"),
    curs_bg: from_hex("#aaaaaa"),
};

const COLOR_THEME_WINTER: ColorTheme = ColorTheme {
    norm_fg: from_hex("#000000"),
    norm_bg: from_hex("#ffffff"),
    alt_fg: from_hex("#000000"),
    alt_bg: from_hex("#f0f0f0"),
    sel_fg: from_hex("#0000ff"),
    sel_bg: from_hex("#c8c8c8"),
    curs_fg: from_hex("#000000"),
    curs_bg: from_hex("#00ffff"),
};

#[derive(
    Clone,
    Copy,
    Default,
    Debug,
    PartialEq,
    Eq,
    strum::FromRepr,
    strum::VariantArray,
    strum::VariantNames,
)]
pub enum ColorThemeId {
    Modern = 1,
    #[default]
    Fusion,
    Classic,
    DeepBlue,
    Cafezinho,
    GreenTiger,
    Winter,
    Custom,
}

mod imp {
    use super::*;
    use glib::subclass::Signal;
    use std::{cell::OnceCell, sync::OnceLock};

    #[derive(glib::Properties, Default)]
    #[properties(wrapper_type = super::ColorThemes)]
    pub struct ColorThemes {
        #[property(get, construct_only)]
        settings: OnceCell<gio::Settings>,
        #[property(get, construct_only)]
        display: OnceCell<Option<gdk::Display>>,
        #[property(get, set, nullable)]
        css_provider: RefCell<Option<gtk::CssProvider>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ColorThemes {
        const NAME: &'static str = "GnomeCmdColorThemes";
        type Type = super::ColorThemes;
    }

    #[glib::derived_properties]
    impl ObjectImpl for ColorThemes {
        fn constructed(&self) {
            self.parent_constructed();

            if let Some(display) = self.obj().display() {
                let css_provider = gtk::CssProvider::new();
                css_provider.load_from_string(include_str!("styles.css"));
                gtk::style_context_add_provider_for_display(
                    &display,
                    &css_provider,
                    gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
                );
            } else {
                eprintln!("No display");
            }

            self.obj().settings().connect_changed(
                Some("theme"),
                glib::clone!(
                    #[weak(rename_to = this)]
                    self.obj(),
                    move |_, _| this.update_theme()
                ),
            );
            self.obj().update_theme();
        }

        fn signals() -> &'static [Signal] {
            static SIGNALS: OnceLock<Vec<Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| vec![Signal::builder("theme-changed").build()])
        }
    }
}

glib::wrapper! {
    pub struct ColorThemes(ObjectSubclass<imp::ColorThemes>);
}

impl ColorThemes {
    pub fn new() -> Self {
        let settings = gio::Settings::new(PREF_COLORS);
        glib::Object::builder()
            .property("settings", settings)
            .property("display", gdk::Display::default())
            .build()
    }

    fn update_theme(&self) {
        let Some(display) = self.display() else {
            eprintln!("No display");
            return;
        };

        if let Some(css_provider) = self.css_provider() {
            gtk::style_context_remove_provider_for_display(&display, &css_provider);
        }

        let style: Cow<'static, str> = if let Some(theme) = self.theme() {
            theme.create_css().into()
        } else {
            NO_THEME_CSS.into()
        };

        let css_provider = gtk::CssProvider::new();
        css_provider.load_from_string(&style);

        gtk::style_context_add_provider_for_display(
            &display,
            &css_provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );

        self.set_css_provider(Some(css_provider));

        self.emit_by_name::<()>("theme-changed", &[]);
    }

    fn theme_id(&self) -> Option<ColorThemeId> {
        self.settings()
            .enum_("theme")
            .try_into()
            .ok()
            .and_then(ColorThemeId::from_repr)
    }

    pub fn theme(&self) -> Option<Cow<'static, ColorTheme>> {
        let theme_id = self.theme_id()?;
        Some(theme_by_id(&self.settings(), theme_id))
    }
}

fn theme_by_id(settings: &gio::Settings, theme_id: ColorThemeId) -> Cow<'static, ColorTheme> {
    match theme_id {
        ColorThemeId::Modern => Cow::Borrowed(&COLOR_THEME_MODERN),
        ColorThemeId::Fusion => Cow::Borrowed(&COLOR_THEME_FUSION),
        ColorThemeId::Classic => Cow::Borrowed(&COLOR_THEME_CLASSIC),
        ColorThemeId::DeepBlue => Cow::Borrowed(&COLOR_THEME_DEEP_BLUE),
        ColorThemeId::Cafezinho => Cow::Borrowed(&COLOR_THEME_CAFEZINHO),
        ColorThemeId::GreenTiger => Cow::Borrowed(&COLOR_THEME_GREEN_TIGER),
        ColorThemeId::Winter => Cow::Borrowed(&COLOR_THEME_WINTER),
        ColorThemeId::Custom => Cow::Owned(load_custom_theme(settings)),
    }
}

pub fn load_custom_theme(settings: &gio::Settings) -> ColorTheme {
    ColorTheme {
        norm_fg: gdk::RGBA::parse(settings.string("custom-norm-fg")).unwrap_or(gdk::RGBA::BLACK),
        norm_bg: gdk::RGBA::parse(settings.string("custom-norm-bg")).unwrap_or(gdk::RGBA::BLACK),
        alt_fg: gdk::RGBA::parse(settings.string("custom-alt-fg")).unwrap_or(gdk::RGBA::BLACK),
        alt_bg: gdk::RGBA::parse(settings.string("custom-alt-bg")).unwrap_or(gdk::RGBA::BLACK),
        sel_fg: gdk::RGBA::parse(settings.string("custom-sel-fg")).unwrap_or(gdk::RGBA::BLACK),
        sel_bg: gdk::RGBA::parse(settings.string("custom-sel-bg")).unwrap_or(gdk::RGBA::BLACK),
        curs_fg: gdk::RGBA::parse(settings.string("custom-curs-fg")).unwrap_or(gdk::RGBA::BLACK),
        curs_bg: gdk::RGBA::parse(settings.string("custom-curs-bg")).unwrap_or(gdk::RGBA::BLACK),
    }
}

pub fn save_custom_theme(
    theme: &ColorTheme,
    settings: &gio::Settings,
) -> Result<(), glib::error::BoolError> {
    settings.set_string("custom-norm-fg", &theme.norm_fg.to_str())?;
    settings.set_string("custom-norm-bg", &theme.norm_bg.to_str())?;
    settings.set_string("custom-alt-fg", &theme.alt_fg.to_str())?;
    settings.set_string("custom-alt-bg", &theme.alt_bg.to_str())?;
    settings.set_string("custom-sel-fg", &theme.sel_fg.to_str())?;
    settings.set_string("custom-sel-bg", &theme.sel_bg.to_str())?;
    settings.set_string("custom-curs-fg", &theme.curs_fg.to_str())?;
    settings.set_string("custom-curs-bg", &theme.curs_bg.to_str())?;
    Ok(())
}

#[cfg(test)]
mod test {
    use super::from_hex;

    #[test]
    fn test_color_parsing() {
        assert_eq!(&from_hex("#abc").to_str(), "rgb(170,187,204)");
        assert_eq!(&from_hex("#ABCD").to_str(), "rgba(170,187,204,0.866667)");
        assert_eq!(&from_hex("#123def").to_str(), "rgb(18,61,239)");
        assert_eq!(
            &from_hex("#456def01").to_str(),
            "rgba(69,109,239,0.00392157)"
        );
    }
}
