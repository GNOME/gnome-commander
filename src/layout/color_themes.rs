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
    gdk::{self, ffi::GdkRGBA},
    gio,
    glib::{self, ffi::GType, subclass::prelude::*, translate::IntoGlib},
    prelude::*,
};
use once_cell::sync::Lazy;
use std::{borrow::Cow, cell::RefCell};

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

#[derive(strum::FromRepr)]
#[repr(C)]
enum ThemeItem {
    SelectionForeground = 0,
    SelectionBackground,
    NormalForeground,
    NormalBackground,
    CursorForeground,
    CursorrBackground,
    AlternateForeground,
    AlternateBackground,
}

impl ColorTheme {
    fn create_css(&self) -> String {
        format!(
            r#"
                treeview.view.gnome-cmd-file-list,
                treeview.view.gnome-cmd-file-list.even {{
                    background-color: {norm_bg};
                }}
                treeview.view.gnome-cmd-file-list.odd {{
                    background-color: {alt_bg};
                }}
                treeview.view.gnome-cmd-file-list:selected:focus {{
                    background-color: {curs_bg};
                    color: {curs_fg};
                }}
            "#,
            norm_bg = self.norm_bg,
            alt_bg = self.alt_bg,
            curs_bg = self.curs_bg,
            curs_fg = self.curs_fg,
        )
    }
}

static COLOR_THEME_MODERN: Lazy<ColorTheme> = Lazy::new(|| ColorTheme {
    norm_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    norm_bg: gdk::RGBA::parse("#dddddddddddd").unwrap(),
    alt_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    alt_bg: gdk::RGBA::parse("#dddddddddddd").unwrap(),
    sel_fg: gdk::RGBA::parse("#ffff00000000").unwrap(),
    sel_bg: gdk::RGBA::parse("#dddddddddddd").unwrap(),
    curs_fg: gdk::RGBA::parse("#ffffffffffff").unwrap(),
    curs_bg: gdk::RGBA::parse("#000000004444").unwrap(),
});

static COLOR_THEME_FUSION: Lazy<ColorTheme> = Lazy::new(|| ColorTheme {
    norm_fg: gdk::RGBA::parse("#8080ffffffff").unwrap(),
    norm_bg: gdk::RGBA::parse("#000040408080").unwrap(),
    alt_fg: gdk::RGBA::parse("#8080ffffffff").unwrap(),
    alt_bg: gdk::RGBA::parse("#000040408080").unwrap(),
    sel_fg: gdk::RGBA::parse("#ffffffff0000").unwrap(),
    sel_bg: gdk::RGBA::parse("#000040408080").unwrap(),
    curs_fg: gdk::RGBA::parse("#000000008080").unwrap(),
    curs_bg: gdk::RGBA::parse("#000080808080").unwrap(),
});

static COLOR_THEME_CLASSIC: Lazy<ColorTheme> = Lazy::new(|| ColorTheme {
    norm_fg: gdk::RGBA::parse("#ffffffffffff").unwrap(),
    norm_bg: gdk::RGBA::parse("#000000004444").unwrap(),
    alt_fg: gdk::RGBA::parse("#ffffffffffff").unwrap(),
    alt_bg: gdk::RGBA::parse("#000000004444").unwrap(),
    sel_fg: gdk::RGBA::parse("#ffffffff0000").unwrap(),
    sel_bg: gdk::RGBA::parse("#000000004444").unwrap(),
    curs_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    curs_bg: gdk::RGBA::parse("#aaaaaaaaaaaa").unwrap(),
});

static COLOR_THEME_DEEP_BLUE: Lazy<ColorTheme> = Lazy::new(|| ColorTheme {
    norm_fg: gdk::RGBA::parse("#0000ffffffff").unwrap(),
    norm_bg: gdk::RGBA::parse("#000000008080").unwrap(),
    alt_fg: gdk::RGBA::parse("#0000ffffffff").unwrap(),
    alt_bg: gdk::RGBA::parse("#000000008080").unwrap(),
    sel_fg: gdk::RGBA::parse("#ffffffff0000").unwrap(),
    sel_bg: gdk::RGBA::parse("#000000008080").unwrap(),
    curs_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    curs_bg: gdk::RGBA::parse("#aaaaaaaaaaaa").unwrap(),
});

static COLOR_THEME_CAFEZINHO: Lazy<ColorTheme> = Lazy::new(|| ColorTheme {
    norm_fg: gdk::RGBA::parse("#e4e4deded5d5").unwrap(),
    norm_bg: gdk::RGBA::parse("#199a153011a8").unwrap(),
    alt_fg: gdk::RGBA::parse("#e4e4deded5d5").unwrap(),
    alt_bg: gdk::RGBA::parse("#199a153011a8").unwrap(),
    sel_fg: gdk::RGBA::parse("#ffffcfcf3636").unwrap(),
    sel_bg: gdk::RGBA::parse("#199a153011a8").unwrap(),
    curs_fg: gdk::RGBA::parse("#e4e4deded5d5").unwrap(),
    curs_bg: gdk::RGBA::parse("#4d4d4d4d4d4d").unwrap(),
});

static COLOR_THEME_GREEN_TIGER: Lazy<ColorTheme> = Lazy::new(|| ColorTheme {
    norm_fg: gdk::RGBA::parse("#ffffc6440000").unwrap(),
    norm_bg: gdk::RGBA::parse("#19192e2e0000").unwrap(),
    alt_fg: gdk::RGBA::parse("#ffffc6c60000").unwrap(),
    alt_bg: gdk::RGBA::parse("#1f1f39390101").unwrap(),
    sel_fg: gdk::RGBA::parse("#ffffffffffff").unwrap(),
    sel_bg: gdk::RGBA::parse("#000000004444").unwrap(),
    curs_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    curs_bg: gdk::RGBA::parse("#aaaaaaaaaaaa").unwrap(),
});

static COLOR_THEME_WINTER: Lazy<ColorTheme> = Lazy::new(|| ColorTheme {
    norm_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    norm_bg: gdk::RGBA::parse("#ffffffffffff").unwrap(),
    alt_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    alt_bg: gdk::RGBA::parse("#f0f0f0f0f0f0").unwrap(),
    sel_fg: gdk::RGBA::parse("#00000000ffff").unwrap(),
    sel_bg: gdk::RGBA::parse("#c8c8c8c8c8c8").unwrap(),
    curs_fg: gdk::RGBA::parse("#000000000000").unwrap(),
    curs_bg: gdk::RGBA::parse("#0000ffffffff").unwrap(),
});

#[derive(Clone, Copy, strum::FromRepr, Default, Debug)]
enum ColorThemeId {
    Modern = 1,
    Fusion,
    Classic,
    #[default]
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

        if let Some(theme) = self.theme() {
            let css_provider = gtk::CssProvider::new();
            css_provider.load_from_string(&theme.create_css());

            gtk::style_context_add_provider_for_display(
                &display,
                &css_provider,
                gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
            );

            self.set_css_provider(Some(css_provider));
        } else {
            self.set_css_provider(None::<gtk::CssProvider>);
        }

        self.emit_by_name::<()>("theme-changed", &[]);
    }

    fn theme_id(&self) -> Option<ColorThemeId> {
        self.settings()
            .enum_("theme")
            .try_into()
            .ok()
            .and_then(ColorThemeId::from_repr)
    }

    fn theme(&self) -> Option<Cow<'static, ColorTheme>> {
        let theme_id = self.theme_id()?;
        Some(theme_by_id(&self.settings(), theme_id))
    }
}

fn theme_by_id(settings: &gio::Settings, theme_id: ColorThemeId) -> Cow<'static, ColorTheme> {
    match theme_id {
        ColorThemeId::Modern => Cow::Borrowed(&*COLOR_THEME_MODERN),
        ColorThemeId::Fusion => Cow::Borrowed(&*COLOR_THEME_FUSION),
        ColorThemeId::Classic => Cow::Borrowed(&*COLOR_THEME_CLASSIC),
        ColorThemeId::DeepBlue => Cow::Borrowed(&*COLOR_THEME_DEEP_BLUE),
        ColorThemeId::Cafezinho => Cow::Borrowed(&*COLOR_THEME_CAFEZINHO),
        ColorThemeId::GreenTiger => Cow::Borrowed(&*COLOR_THEME_GREEN_TIGER),
        ColorThemeId::Winter => Cow::Borrowed(&*COLOR_THEME_WINTER),
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

#[no_mangle]
pub extern "C" fn gnome_cmd_color_themes_get_type() -> GType {
    ColorThemes::static_type().into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_get_current_theme() -> *mut Cow<'static, ColorTheme> {
    let Some(theme) = ColorThemes::new().theme() else {
        return std::ptr::null_mut();
    };
    let theme = Box::new(theme);
    Box::into_raw(theme)
}

#[no_mangle]
pub extern "C" fn gnome_cmd_theme_free(theme: *mut Cow<'static, ColorTheme>) {
    if theme.is_null() {
        return;
    }
    let theme: Box<Cow<'static, ColorTheme>> = unsafe { Box::from_raw(theme) };
    drop(theme);
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_color_get_color(
    theme_ptr: *const Cow<'static, ColorTheme>,
    item: i32,
) -> *mut GdkRGBA {
    if theme_ptr.is_null() {
        return std::ptr::null_mut();
    }
    let theme: &Cow<'static, ColorTheme> = unsafe { &*theme_ptr };
    let Some(item) = ThemeItem::from_repr(item as usize) else {
        return std::ptr::null_mut();
    };
    match item {
        ThemeItem::SelectionForeground => theme.sel_fg.as_ptr(),
        ThemeItem::SelectionBackground => theme.sel_bg.as_ptr(),
        ThemeItem::NormalForeground => theme.norm_fg.as_ptr(),
        ThemeItem::NormalBackground => theme.norm_bg.as_ptr(),
        ThemeItem::CursorForeground => theme.curs_fg.as_ptr(),
        ThemeItem::CursorrBackground => theme.curs_bg.as_ptr(),
        ThemeItem::AlternateForeground => theme.alt_fg.as_ptr(),
        ThemeItem::AlternateBackground => theme.alt_bg.as_ptr(),
    }
}
