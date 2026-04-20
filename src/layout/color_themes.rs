// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    options::{ColorOptions, types::WriteResult},
    utils::u32_enum,
};
use gtk::{gdk, glib};
use std::{borrow::Cow, cell::RefCell, rc::Rc};

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

impl ColorTheme {
    fn create_css(&self) -> String {
        format!(
            r#"
                .gnome-cmd-file-list {{
                    --color-norm-bg: {norm_bg};
                    --color-norm-fg: {norm_fg};
                    --color-alt-bg: {alt_bg};
                    --color-alt-fg: {alt_fg};
                    --color-sel-bg: {sel_bg};
                    --color-sel-fg: {sel_fg};
                    --color-curs-bg: {curs_bg};
                    --color-curs-fg: {curs_fg};
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

u32_enum! {
    pub enum ColorThemeId {
        None,
        Modern,
        #[default]
        Fusion,
        Classic,
        DeepBlue,
        Cafezinho,
        GreenTiger,
        Winter,
        Custom,
    }
}

type Callback = Box<dyn Fn(&ColorThemes)>;

pub struct ColorThemes {
    callback: RefCell<Option<Callback>>,
    settings: ColorOptions,
    css_provider: Option<gtk::CssProvider>,
}

impl ColorThemes {
    pub fn new() -> Rc<Self> {
        let css_provider = gdk::Display::default().map(|display| {
            // CSS provider for static CSS data
            let css_provider = gtk::CssProvider::new();
            css_provider.load_from_string(include_str!("styles.css"));
            gtk::style_context_add_provider_for_display(
                &display,
                &css_provider,
                gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
            );

            // CSS provider for dynamic CSS data
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

        let weak_ref = Rc::downgrade(&this);
        this.settings.theme.connect_changed(move |_| {
            if let Some(this) = weak_ref.upgrade() {
                this.update_theme();
            }
        });
        for option in [
            &this.settings.custom_norm_fg,
            &this.settings.custom_norm_bg,
            &this.settings.custom_alt_fg,
            &this.settings.custom_alt_bg,
            &this.settings.custom_sel_fg,
            &this.settings.custom_sel_bg,
            &this.settings.custom_curs_fg,
            &this.settings.custom_curs_bg,
        ] {
            let weak_ref = Rc::downgrade(&this);
            option.connect_changed(move |_| {
                if let Some(this) = weak_ref.upgrade() {
                    this.update_theme();
                }
            });
        }
        this.update_theme();

        this
    }

    pub fn set_update_callback(&self, callback: impl Fn(&Self) + 'static) {
        self.callback.replace(Some(Box::new(callback)));
    }

    fn update_theme(&self) {
        let Some(css_provider) = &self.css_provider else {
            eprintln!("No display");
            return;
        };

        css_provider.load_from_string(
            &self
                .theme()
                .map(|theme| theme.create_css())
                .unwrap_or_default(),
        );

        if let Some(callback) = self.callback.borrow().as_ref() {
            (callback)(self);
        }
    }

    fn theme_id(&self) -> ColorThemeId {
        self.settings.theme.get()
    }

    pub fn has_theme(&self) -> bool {
        self.theme_id() != ColorThemeId::None
    }

    pub fn theme(&self) -> Option<Cow<'static, ColorTheme>> {
        theme_by_id(&self.settings, self.theme_id())
    }
}

fn theme_by_id(
    settings: &ColorOptions,
    theme_id: ColorThemeId,
) -> Option<Cow<'static, ColorTheme>> {
    Some(match theme_id {
        ColorThemeId::None => return None,
        ColorThemeId::Modern => Cow::Borrowed(&COLOR_THEME_MODERN),
        ColorThemeId::Fusion => Cow::Borrowed(&COLOR_THEME_FUSION),
        ColorThemeId::Classic => Cow::Borrowed(&COLOR_THEME_CLASSIC),
        ColorThemeId::DeepBlue => Cow::Borrowed(&COLOR_THEME_DEEP_BLUE),
        ColorThemeId::Cafezinho => Cow::Borrowed(&COLOR_THEME_CAFEZINHO),
        ColorThemeId::GreenTiger => Cow::Borrowed(&COLOR_THEME_GREEN_TIGER),
        ColorThemeId::Winter => Cow::Borrowed(&COLOR_THEME_WINTER),
        ColorThemeId::Custom => Cow::Owned(load_custom_theme(settings)),
    })
}

pub fn load_custom_theme(settings: &ColorOptions) -> ColorTheme {
    ColorTheme {
        norm_fg: settings.custom_norm_fg.get(),
        norm_bg: settings.custom_norm_bg.get(),
        alt_fg: settings.custom_alt_fg.get(),
        alt_bg: settings.custom_alt_bg.get(),
        sel_fg: settings.custom_sel_fg.get(),
        sel_bg: settings.custom_sel_bg.get(),
        curs_fg: settings.custom_curs_fg.get(),
        curs_bg: settings.custom_curs_bg.get(),
    }
}

pub fn save_custom_theme(theme: &ColorTheme, settings: &ColorOptions) -> WriteResult {
    settings.custom_norm_fg.set(theme.norm_fg)?;
    settings.custom_norm_bg.set(theme.norm_bg)?;
    settings.custom_alt_fg.set(theme.alt_fg)?;
    settings.custom_alt_bg.set(theme.alt_bg)?;
    settings.custom_sel_fg.set(theme.sel_fg)?;
    settings.custom_sel_bg.set(theme.sel_bg)?;
    settings.custom_curs_fg.set(theme.curs_fg)?;
    settings.custom_curs_bg.set(theme.curs_bg)?;
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
