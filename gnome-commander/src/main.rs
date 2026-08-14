// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod advanced_rename;
mod app;
mod chmod_component;
mod chown_component;
mod command_line;
mod config;
mod connection;
mod connection_bar;
mod connection_button;
mod debug;
mod dialogs;
mod dir;
mod directory_indicator;
mod dirlist;
mod file;
mod file_edit;
mod file_list;
mod file_metainfo_view;
mod file_selector;
mod file_view;
mod filter;
mod hintbox;
mod history;
mod history_entry;
mod i18n;
mod intviewer;
mod layout;
mod main_win;
mod notebook_ext;
mod open_connection;
mod open_file;
mod options;
mod paned_ext;
mod plugins;
mod pwd;
mod search;
mod select_directory_button;
mod select_icon_button;
mod shortcuts;
mod spawn;
mod tab_label;
mod tags;
mod transfer;
mod types;
mod user_actions;
mod utils;
mod weak_map;
mod weak_ref;
mod weak_set;

use crate::{
    config::{ICONS_PREFIX, PACKAGE, locale_dir},
    connection::list::ConnectionList,
    debug::set_debug_flags,
    layout::{
        color_themes::{ColorThemeListener, ColorThemes},
        ls_colors_palette::{LsColorsPaletteListener, load_palette},
    },
    main_win::MainWindow,
    options::{GeneralOptions, SearchConfig},
    shortcuts::Shortcuts,
};
use gettextrs::gettext;
use gtk::{gdk, gio, pango, prelude::*};
use std::{
    borrow::Cow,
    cell::{OnceCell, RefCell},
    error::Error,
    ops::ControlFlow,
    path::PathBuf,
    process::Termination,
};

thread_local! {
    static START_LEFT_DIR: RefCell<Option<PathBuf>> = Default::default();
    static START_RIGHT_DIR: RefCell<Option<PathBuf>> = Default::default();
    static THEME_LISTENER: OnceCell<ColorThemeListener<Box<dyn Fn()>>> = Default::default();
    static LS_COLORS_PALETTE_LISTENER: OnceCell<LsColorsPaletteListener<Box<dyn Fn()>>> = Default::default();
}

fn load_application_css() {
    if let Some(display) = gdk::Display::default() {
        // CSS provider for static CSS data
        let provider = gtk::CssProvider::new();
        provider.load_from_string(include_str!("application.css"));
        gtk::style_context_add_provider_for_display(
            &display,
            &provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );

        // CSS provider for color theme CSS data
        let css_provider = gtk::CssProvider::new();
        gtk::style_context_add_provider_for_display(
            &display,
            &css_provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
        THEME_LISTENER.with(move |theme_listener| {
            theme_listener.get_or_init(move || {
                update_theme_css(&css_provider);
                ColorThemeListener::new(Box::new(move || update_theme_css(&css_provider)))
            });
        });

        // CSS provider for LS_COLORS palette CSS data
        let css_provider = gtk::CssProvider::new();
        gtk::style_context_add_provider_for_display(
            &display,
            &css_provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
        LS_COLORS_PALETTE_LISTENER.with(move |ls_colors_palette_listener| {
            ls_colors_palette_listener.get_or_init(move || {
                update_theme_css(&css_provider);
                LsColorsPaletteListener::new(Box::new(move || {
                    update_ls_colors_palette_css(&css_provider)
                }))
            });
        });
    }
}

fn update_theme_css(css_provider: &gtk::CssProvider) {
    css_provider.load_from_string(
        &ColorThemes::theme()
            .map(|theme| theme.create_css())
            .unwrap_or_default(),
    );
}

fn update_ls_colors_palette_css(css_provider: &gtk::CssProvider) {
    css_provider.load_from_string(&load_palette().create_css());
}

fn escape_css(value: &str) -> Cow<'_, str> {
    if value.contains(['"', '\\']) {
        Cow::Owned(value.replace('\\', "\\\\").replace('"', "\\\""))
    } else {
        Cow::Borrowed(value)
    }
}

fn font_to_css(font_name: &str) -> String {
    let font_desc = pango::FontDescription::from_string(font_name);
    let mut result = String::from(".gnome-cmd-file-list{font:");
    match font_desc.style() {
        pango::Style::Italic => result.push_str(" italic"),
        pango::Style::Oblique => result.push_str(" oblique"),
        _ => {}
    }
    if font_desc.variant() == pango::Variant::SmallCaps {
        result.push_str(" small-caps");
    }
    match font_desc.weight() {
        pango::Weight::Thin => result.push_str(" 100"),
        pango::Weight::Ultralight => result.push_str(" 200"),
        pango::Weight::Light => result.push_str(" 300"),
        pango::Weight::Semilight => result.push_str(" 350"),
        pango::Weight::Book => result.push_str(" 380"),
        pango::Weight::Medium => result.push_str(" 500"),
        pango::Weight::Semibold => result.push_str(" 600"),
        pango::Weight::Bold => result.push_str(" 700"),
        pango::Weight::Ultrabold => result.push_str(" 800"),
        pango::Weight::Heavy => result.push_str(" 900"),
        pango::Weight::Ultraheavy => result.push_str(" 1000"),
        _ => {}
    }
    {
        let size = font_desc.size();
        if size > 0 && !font_desc.is_size_absolute() {
            result.push_str(&format!(
                " {:.1}pt",
                f64::from(size) / f64::from(pango::SCALE)
            ));
        } else {
            result.push_str(" 10pt")
        }
    }
    if let Some(family) = font_desc.family() {
        result.push_str(&format!(" \"{}\",monospace", escape_css(&family)));
    } else {
        result.push_str(" monospace");
    }
    result.push_str(";}");
    result
}

fn setup_list_font(options: &GeneralOptions) {
    let Some(display) = gdk::Display::default() else {
        return;
    };

    let css_provider = gtk::CssProvider::new();
    css_provider.load_from_string(&font_to_css(&options.list_font.get()));
    gtk::style_context_add_provider_for_display(
        &display,
        &css_provider,
        gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
    );

    options.list_font.connect_changed(glib::clone!(
        #[strong]
        css_provider,
        move |font_name| {
            css_provider.load_from_string(&font_to_css(&font_name));
        }
    ));
}

fn create_config_directory() {
    let conf_dir = glib::user_config_dir().join(PACKAGE);
    if let Err(error) = std::fs::create_dir_all(&conf_dir) {
        eprintln!(
            "{}",
            gettext("Failed to create the directory {directory}: {error}")
                .replace("{directory}", &conf_dir.display().to_string())
                .replace("{error}", &error.to_string())
        );
    }
}

fn app_startup(_app: &gtk::Application) {
    load_application_css();

    // disable beeping for the application
    if let Some(settings) = gtk::Settings::default() {
        settings.set_gtk_error_bell(false);
    }

    if let Some(display) = gdk::Display::default() {
        gio::resources_register_include!("icons.gresource").unwrap();
        gtk::IconTheme::for_display(&display).add_search_path(ICONS_PREFIX);
    }

    create_config_directory();

    let options = GeneralOptions::instance();
    ConnectionList::create(options.show_samba_workgroups_button.get());
    SearchConfig::get().load(&options);
    ConnectionList::get().load(&options);
    ConnectionList::get().set_volume_monitor();
    setup_list_font(&options);

    Shortcuts::global().load(&options.keybindings.get(), options.legacy_keybindings.get());
}

fn app_activate(app: &gtk::Application) {
    if let Some(window) = app.windows().first() {
        window.present();
        return;
    }

    let main_win = MainWindow::new();
    main_win.set_application(Some(app));
    START_LEFT_DIR.with_borrow(|start_left_dir| {
        START_RIGHT_DIR.with_borrow(|start_right_dir| {
            main_win.load_tabs(start_left_dir.as_deref(), start_right_dir.as_deref());
        });
    });
    main_win.load_command_line_history(&GeneralOptions::instance());
    main_win.present();
}

fn app_shutdown(_app: &gtk::Application) {
    let options = GeneralOptions::instance();
    if let Err(error) = SearchConfig::get().save(&options, options.save_search_history.get()) {
        eprintln!("Failed to save search profiles: {}", error.message);
    }
    if let Err(error) = ConnectionList::get().save(&options) {
        eprintln!("Failed to save connection data: {}", error.message);
    }

    gio::Settings::sync();
}

fn app_handle_local_options(
    _app: &gtk::Application,
    options: &glib::VariantDict,
) -> ControlFlow<glib::ExitCode> {
    fn get_string_option(options: &glib::VariantDict, key: &str) -> Option<String> {
        options
            .lookup_value(key, Some(&String::static_variant_type()))
            .and_then(|v| v.get())
    }

    let debug_flags = get_string_option(options, "debug").map(|flags| {
        if flags.contains('a') {
            "giklmnpstuvwyzx".to_owned()
        } else {
            flags
        }
    });
    if let Some(ref debug_flags) = debug_flags {
        set_debug_flags(debug_flags);
    }

    START_LEFT_DIR.with_borrow_mut(|start_left_dir| {
        *start_left_dir = get_string_option(options, "start-left-dir").map(PathBuf::from);
    });
    START_RIGHT_DIR.with_borrow_mut(|start_right_dir| {
        *start_right_dir = get_string_option(options, "start-right-dir").map(PathBuf::from);
    });

    ControlFlow::Continue(())
}

fn main() -> Result<impl Termination, Box<dyn Error>> {
    if let Some(mismatch) = glib::check_version(2, 80, 0) {
        eprintln!("GLib version mismatch: {mismatch}");
        return Ok(glib::ExitCode::FAILURE);
    }
    if let Some(mismatch) = gtk::check_version(4, 14, 0) {
        eprintln!("GTK version mismatch: {mismatch}");
        return Ok(glib::ExitCode::FAILURE);
    }

    gettextrs::setlocale(gettextrs::LocaleCategory::LcAll, "");
    gettextrs::bindtextdomain(PACKAGE, locale_dir())?;
    gettextrs::bind_textdomain_codeset(PACKAGE, "UTF-8")?;
    gettextrs::textdomain(PACKAGE)?;

    gtk::init()?;

    let flags = if GeneralOptions::instance().allow_multiple_instances.get() {
        gio::ApplicationFlags::NON_UNIQUE
    } else {
        gio::ApplicationFlags::default()
    };
    let app = gtk::Application::new(Some("org.gnome.gnome-commander"), flags);
    #[cfg(debug_assertions)]
    app.add_main_option(
        "debug",
        b'd'.into(),
        glib::OptionFlags::NONE,
        glib::OptionArg::String,
        &gettext("Specify debug flags to use"),
        None,
    );
    app.add_main_option(
        "start-left-dir",
        b'l'.into(),
        glib::OptionFlags::NONE,
        glib::OptionArg::String,
        &gettext("Specify the start directory for the left panel"),
        None,
    );
    app.add_main_option(
        "start-right-dir",
        b'r'.into(),
        glib::OptionFlags::NONE,
        glib::OptionArg::String,
        &gettext("Specify the start directory for the right panel"),
        None,
    );

    app.connect_startup(app_startup);
    app.connect_activate(app_activate);
    app.connect_shutdown(app_shutdown);
    app.connect_handle_local_options(app_handle_local_options);

    Ok(app.run())
}
