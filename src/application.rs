// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::options::GeneralOptions;
use gettextrs::gettext;
use gtk::{gdk, gio, glib, pango, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        config::{ICONS_DIR, PACKAGE},
        connection::list::ConnectionList,
        debug::set_debug_flags,
        main_win::MainWindow,
        options::SearchConfig,
    };
    use std::{borrow::Cow, cell::RefCell, ops::ControlFlow, path::PathBuf};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::Application)]
    pub struct Application {
        options: GeneralOptions,
        debug_flags: RefCell<Option<String>>,
        #[property(get, set, nullable)]
        start_left_dir: RefCell<Option<PathBuf>>,
        #[property(get, set, nullable)]
        start_right_dir: RefCell<Option<PathBuf>>,
    }

    impl Default for Application {
        fn default() -> Self {
            Self {
                options: GeneralOptions::new(),
                debug_flags: Default::default(),
                start_left_dir: Default::default(),
                start_right_dir: Default::default(),
            }
        }
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Application {
        const NAME: &'static str = "GnomeCmdApplication";
        type Type = super::Application;
        type ParentType = gtk::Application;
    }

    #[glib::derived_properties]
    impl ObjectImpl for Application {
        fn constructed(&self) {
            self.parent_constructed();

            let app = self.obj();
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
        }
    }

    impl ApplicationImpl for Application {
        fn startup(&self) {
            self.parent_startup();

            load_application_css();

            // disable beeping for the application
            if let Some(settings) = gtk::Settings::default() {
                settings.set_gtk_error_bell(false);
            }

            if let Some(display) = gdk::Display::default() {
                gtk::IconTheme::for_display(&display).add_search_path(ICONS_DIR);
            }

            create_config_directory();

            ConnectionList::create(self.options.show_samba_workgroups_button.get());
            SearchConfig::get().load(&self.options);
            ConnectionList::get().load(&self.options);
            ConnectionList::get().set_volume_monitor();
            setup_list_font(&self.options);
        }

        fn activate(&self) {
            self.parent_activate();

            if let Some(window) = self.obj().windows().first() {
                window.present();
                return;
            }

            let main_win = MainWindow::new();
            main_win.set_application(Some(&*self.obj()));
            main_win.load_tabs(
                self.start_left_dir.borrow().as_deref(),
                self.start_right_dir.borrow().as_deref(),
            );
            let options = GeneralOptions::new();
            main_win.load_command_line_history(&options);
            main_win.present();
            main_win.set_current_panel(0);
        }

        fn shutdown(&self) {
            let options = GeneralOptions::new();
            if let Err(error) =
                SearchConfig::get().save(&options, options.save_search_history.get())
            {
                eprintln!("Failed to save search profiles: {}", error.message);
            }
            if let Err(error) = ConnectionList::get().save(&options) {
                eprintln!("Failed to save connection data: {}", error.message);
            }

            gio::Settings::sync();
            self.parent_shutdown();
        }

        fn handle_local_options(&self, options: &glib::VariantDict) -> ControlFlow<glib::ExitCode> {
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
            self.debug_flags.replace(debug_flags);
            self.start_left_dir
                .replace(get_string_option(options, "start-left-dir").map(parse_dir));
            self.start_right_dir
                .replace(get_string_option(options, "start-right-dir").map(parse_dir));

            self.parent_handle_local_options(options)
        }
    }

    impl GtkApplicationImpl for Application {}

    fn load_application_css() {
        if let Some(display) = gdk::Display::default() {
            let provider = gtk::CssProvider::new();
            provider.load_from_string(include_str!("application.css"));
            gtk::style_context_add_provider_for_display(
                &display,
                &provider,
                gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
            );
        }
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

    fn get_string_option(options: &glib::VariantDict, key: &str) -> Option<String> {
        options
            .lookup_value(key, Some(&String::static_variant_type()))
            .and_then(|v| v.get())
    }

    fn parse_dir(dir: String) -> PathBuf {
        if dir == "." {
            glib::current_dir()
        } else {
            PathBuf::from(dir)
        }
    }
}

glib::wrapper! {
    pub struct Application(ObjectSubclass<imp::Application>)
        @extends gtk::Application, gio::Application,
        @implements gio::ActionMap, gio::ActionGroup;
}

impl Application {
    pub fn new(allow_multiple_instances: bool) -> Self {
        let flags = if allow_multiple_instances {
            gio::ApplicationFlags::NON_UNIQUE
        } else {
            gio::ApplicationFlags::default()
        };

        glib::Object::builder()
            .property("application-id", "org.gnome.gnome-commander")
            .property("flags", flags)
            .build()
    }
}

impl Default for Application {
    fn default() -> Self {
        Self::new(GeneralOptions::new().allow_multiple_instances.get())
    }
}
