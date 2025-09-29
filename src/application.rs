/*
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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

use crate::options::options::GeneralOptions;
use gettextrs::gettext;
use gtk::{gdk, gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        config::{ICONS_DIR, PACKAGE},
        connection::list::ConnectionList,
        debug::set_debug_flags,
        main_win::MainWindow,
        options::options::SearchConfig,
    };
    use std::{cell::RefCell, ops::ControlFlow, path::PathBuf};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::Application)]
    pub struct Application {
        debug_flags: RefCell<Option<String>>,
        #[property(get, set, nullable)]
        start_left_dir: RefCell<Option<PathBuf>>,
        #[property(get, set, nullable)]
        start_right_dir: RefCell<Option<PathBuf>>,
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
                &gettext("Specify the start directory for the left pane"),
                None,
            );
            app.add_main_option(
                "start-right-dir",
                b'r'.into(),
                glib::OptionFlags::NONE,
                glib::OptionArg::String,
                &gettext("Specify the start directory for the right pane"),
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

            let options = GeneralOptions::new();
            ConnectionList::create(options.show_samba_workgroups_button.get());
            SearchConfig::get().load(&options);
            ConnectionList::get().load(&options);
            ConnectionList::get().set_volume_monitor();
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
            provider.load_from_string(&include_str!("application.css"));
            gtk::style_context_add_provider_for_display(
                &display,
                &provider,
                gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
            );
        }
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
