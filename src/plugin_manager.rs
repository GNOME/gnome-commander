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

use crate::{
    config::{PACKAGE, PLUGIN_DIR},
    dialogs::about_plugin::about_plugin_dialog,
    gmodule::{self, GModule, GModuleFlags},
    libgcmd::{
        GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION, PluginInfo,
        configurable::{Configurable, ConfigurableExt},
    },
    utils::{NO_BUTTONS, dialog_button_box, handle_escape_key},
};
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{
        ffi::gpointer,
        gobject_ffi::GObject,
        translate::{from_glib_full, from_glib_none},
    },
    prelude::*,
    subclass::prelude::*,
};
use std::{
    fs, io,
    path::{Path, PathBuf},
};

const GCMD_PREF_PLUGINS: &str = "org.gnome.gnome-commander.plugins.general";

mod imp {
    use super::*;
    use glib::subclass::Signal;
    use std::{cell::RefCell, sync::OnceLock};

    #[derive(Default)]
    pub struct PluginManager {
        pub plugins: RefCell<Vec<PluginData>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for PluginManager {
        const NAME: &'static str = "GnomeCmdPluginManager";
        type Type = super::PluginManager;
    }

    impl ObjectImpl for PluginManager {
        fn constructed(&self) {
            self.parent_constructed();
            self.obj().init();
        }

        fn dispose(&self) {
            self.obj().save();
        }

        fn signals() -> &'static [Signal] {
            static SIGNALS: OnceLock<Vec<Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| vec![Signal::builder("plugins-changed").build()])
        }
    }
}

glib::wrapper! {
    pub struct PluginManager(ObjectSubclass<imp::PluginManager>);
}

impl PluginManager {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    fn init(&self) {
        let user_plugin_dir = glib::user_config_dir().join(PACKAGE).join("plugins");
        if let Err(error) = self.scan_plugins_in_dir(&user_plugin_dir) {
            eprintln!(
                "ERROR: Failed to load plugins from {}: {}",
                user_plugin_dir.display(),
                error
            );
        }
        if let Err(error) = self.scan_plugins_in_dir(&Path::new(PLUGIN_DIR)) {
            eprintln!(
                "ERROR: Failed to load plugins from {}: {}",
                PLUGIN_DIR, error
            );
        }
        for name in gio::Settings::new(GCMD_PREF_PLUGINS).strv("autoload") {
            for data in self.imp().plugins.borrow_mut().iter_mut() {
                if data.file_name == name {
                    data.active = true;
                }
            }
        }
    }

    pub fn save(&self) {
        let plugins = self.imp().plugins.borrow();
        let autoload: Vec<&str> = plugins
            .iter()
            .filter(|d| d.active)
            .map(|d| d.file_name.as_str())
            .collect();
        if let Err(error) = gio::Settings::new(GCMD_PREF_PLUGINS).set_strv("autoload", autoload) {
            eprintln!(
                "ERROR: Failed to save autoload plugin list: {}: {}",
                PLUGIN_DIR, error.message
            );
        }
    }

    fn scan_plugins_in_dir(&self, path: &Path) -> io::Result<()> {
        for entry in fs::read_dir(path)? {
            let entry = entry?;
            let path = entry.path();
            let Some(ext) = path.extension() else {
                continue;
            };
            if ext != "so" && ext != "dylib" && ext != "dll" {
                continue;
            }
            let metadata = entry.metadata()?;
            if !metadata.is_file() {
                continue;
            }
            match load_plugin(path) {
                Ok(mut data) => {
                    data.action_group_name = format!("plugin{}", self.imp().plugins.borrow().len());
                    self.imp().plugins.borrow_mut().push(data);
                }
                Err(error) => {
                    eprintln!("ERROR: {}", error);
                }
            }
        }
        Ok(())
    }

    pub fn set_active(&self, plugin: &glib::Object, active: bool) {
        let mut changed = false;
        for plugin_data in self.imp().plugins.borrow_mut().iter_mut() {
            if plugin_data.plugin == *plugin {
                plugin_data.active = active;
                changed = true;
            }
        }
        if changed {
            self.emit_by_name::<()>("plugins-changed", &[]);
        }
    }

    pub fn active_plugins(&self) -> Vec<(String, glib::Object)> {
        self.imp()
            .plugins
            .borrow()
            .iter()
            .filter(|d| d.active)
            .map(|d| (d.action_group_name.clone(), d.plugin.clone()))
            .collect()
    }

    pub fn connect_plugins_changed<F: Fn(&Self) + 'static>(&self, f: F) -> glib::SignalHandlerId {
        self.connect_closure(
            "plugins-changed",
            false,
            glib::closure_local!(|this| f(this)),
        )
    }
}

pub struct PluginData {
    pub active: bool,
    pub file_name: String,
    pub path: PathBuf,
    pub action_group_name: String,
    pub plugin: glib::Object,
    pub info: PluginInfoOwned,
    pub module: gmodule::GModule,
}

#[derive(Clone)]
pub struct PluginInfoOwned {
    pub plugin_system_version: u32,
    pub name: Option<String>,
    pub version: Option<String>,
    pub copyright: Option<String>,
    pub comments: Option<String>,
    pub authors: Vec<String>,
    pub documenters: Vec<String>,
    pub translator: Option<String>,
    pub webpage: Option<String>,
}

impl From<&PluginInfo> for PluginInfoOwned {
    fn from(value: &PluginInfo) -> Self {
        unsafe {
            Self {
                plugin_system_version: value.plugin_system_version,
                name: from_glib_none(value.name),
                version: from_glib_none(value.version),
                copyright: from_glib_none(value.copyright),
                comments: from_glib_none(value.comments),
                authors: glib::StrV::from_glib_none(value.authors)
                    .iter()
                    .map(|s| s.to_string())
                    .collect(),
                documenters: glib::StrV::from_glib_none(value.documenters)
                    .iter()
                    .map(|s| s.to_string())
                    .collect(),
                translator: from_glib_none(value.translator),
                webpage: from_glib_none(value.webpage),
            }
        }
    }
}

const MODULE_INIT_FUNC: &str = "create_plugin";
const MODULE_INFO_FUNC: &str = "get_plugin_info";

pub type PluginInfoFunc = unsafe extern "C" fn() -> *const PluginInfo;
pub type PluginConstructorFunc = unsafe extern "C" fn() -> *mut GObject;

fn load_plugin(path: PathBuf) -> Result<PluginData, String> {
    let module = GModule::open(
        &path,
        GModuleFlags {
            lazy: true,
            local: false,
        },
    )
    .map_err(|error| {
        format!(
            "ERROR: Failed to load the plugin '{}': {}",
            path.display(),
            error.message()
        )
    })?;

    let info_func = module
        .symbol(MODULE_INFO_FUNC)
        .ok_or_else(|| {
            format!(
                "The plugin file '{}' has no function named '{}'.",
                path.display(),
                MODULE_INFO_FUNC
            )
        })
        .and_then(|ptr| {
            if ptr.is_null() {
                Err(format!(
                    "The plugin ile '{}' has blank function named '{}'.",
                    path.display(),
                    MODULE_INFO_FUNC
                ))
            } else {
                Ok(unsafe { std::mem::transmute::<gpointer, PluginInfoFunc>(ptr) })
            }
        })?;

    // Try to get the plugin info
    let info = unsafe { info_func() };
    if info.is_null() {
        return Err(format!(
            "The plugin-file '{}' did not return valid plugin info. The function '{}' returned NULL.",
            path.display(),
            MODULE_INFO_FUNC
        ));
    }

    // Check that the plugin is compatible
    let plugin_system_version = unsafe { (*info).plugin_system_version };
    if plugin_system_version != GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION {
        return Err(format!(
            "The plugin '{}' is not compatible with this version of {}: Plugin system supported by the plugin is {}, it should be {}.",
            path.display(),
            PACKAGE,
            plugin_system_version,
            GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION
        ));
    }

    let info = PluginInfoOwned::from(unsafe { &*info });

    let init_func = module
        .symbol(MODULE_INIT_FUNC)
        .ok_or_else(|| {
            format!(
                "The plugin '{}' has no function named '{}'.",
                path.display(),
                MODULE_INIT_FUNC
            )
        })
        .and_then(|ptr| {
            if ptr.is_null() {
                Err(format!(
                    "The plugin '{}' has blank function named '{}'.",
                    path.display(),
                    MODULE_INIT_FUNC
                ))
            } else {
                Ok(unsafe { std::mem::transmute::<gpointer, PluginConstructorFunc>(ptr) })
            }
        })?;

    // Try to initialize the plugin
    let plugin = unsafe { init_func() };
    if plugin.is_null() {
        return Err(format!(
            "The plugin '{}' could not be initialized: The '{}' function returned NULL",
            path.display(),
            MODULE_INIT_FUNC
        ));
    }

    Ok(PluginData {
        file_name: path
            .file_name()
            .and_then(|s| s.to_str())
            .unwrap_or_default()
            .to_owned(),
        path,
        active: false,
        action_group_name: String::new(),
        module,
        info,
        plugin: unsafe { from_glib_full(plugin) },
    })
}

#[derive(Debug, glib::Variant)]
pub struct PluginActionVariant {
    pub plugin: String,
    pub action: String,
    pub parameter: glib::Variant,
}

pub fn wrap_plugin_menu(plugin: &str, menu: &gio::MenuModel) -> gio::Menu {
    let new_menu = gio::Menu::new();
    for item_index in 0..menu.n_items() {
        let item = gio::MenuItem::new(None, None);

        let mut action = None;
        let mut target = glib::Variant::from_none(&i32::static_variant_type());
        let iter = menu.iterate_item_attributes(item_index);
        while let Some((attribute, value)) = iter.next() {
            if attribute == gio::MENU_ATTRIBUTE_ACTION {
                action = value.str().map(ToString::to_string);
            } else if attribute == gio::MENU_ATTRIBUTE_TARGET {
                target = value;
            } else {
                item.set_attribute_value(&attribute, Some(&value));
            }
        }

        if let Some(action) = action {
            let plugin_action_target = PluginActionVariant {
                plugin: plugin.to_owned(),
                action: action.to_owned(),
                parameter: target,
            };
            item.set_action_and_target_value(
                Some("win.plugin-action"),
                Some(&plugin_action_target.to_variant()),
            );
        } else {
            item.set_action_and_target_value(None, None);
        }

        let iter = menu.iterate_item_links(item_index);
        while let Some((link_name, model)) = iter.next() {
            let model = wrap_plugin_menu(plugin, &model);
            item.set_link(&link_name, Some(&model));
        }

        new_menu.append_item(&item);
    }
    new_menu
}

pub fn show_plugin_manager(plugin_manager: &PluginManager, parent_window: &gtk::Window) {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .title(gettext("Available plugins"))
        .modal(true)
        .width_request(500)
        .height_request(300)
        .resizable(true)
        .build();

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
    dialog.set_child(Some(&grid));

    let view = gtk::ListBox::builder()
        .selection_mode(gtk::SelectionMode::None)
        .build();
    view.add_css_class("rich-list");

    let sw = gtk::ScrolledWindow::builder()
        .child(&view)
        .hexpand(true)
        .vexpand(true)
        .has_frame(true)
        .build();
    grid.attach(&sw, 0, 0, 1, 1);

    let close_button = gtk::Button::builder()
        .label(gettext("_Close"))
        .use_underline(true)
        .build();
    close_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| dialog.close()
    ));
    grid.attach(&dialog_button_box(NO_BUTTONS, &[close_button]), 0, 1, 1, 1);

    handle_escape_key(
        &dialog,
        &gtk::CallbackAction::new(glib::clone!(
            #[weak]
            dialog,
            #[upgrade_or]
            glib::Propagation::Proceed,
            move |_, _| {
                dialog.close();
                glib::Propagation::Proceed
            }
        )),
    );

    for plugin_data in plugin_manager.imp().plugins.borrow().iter() {
        view.append(&create_plugin_widget(plugin_manager, plugin_data));
    }

    dialog.present();
}

fn create_plugin_widget(plugin_manager: &PluginManager, plugin_data: &PluginData) -> gtk::Widget {
    let info = plugin_data.info.clone();
    let plugin = plugin_data.plugin.clone();

    let grid = gtk::Grid::builder()
        .hexpand(true)
        .column_spacing(12)
        .build();

    let name = gtk::Label::builder()
        .label(info.name.as_deref().unwrap_or(&plugin_data.file_name))
        .halign(gtk::Align::Start)
        .hexpand(true)
        .build();
    name.add_css_class("title");
    grid.attach(&name, 0, 0, 1, 1);

    let version = gtk::Label::builder()
        .label(gettext("Version: {}").replace("{}", info.version.as_deref().unwrap_or("-")))
        .halign(gtk::Align::Start)
        .build();
    version.add_css_class("dim-label");
    grid.attach(&version, 0, 1, 1, 1);

    let file = gtk::Label::builder()
        .label(gettext("File: {}").replace("{}", &plugin_data.file_name))
        .halign(gtk::Align::Start)
        .build();
    file.add_css_class("dim-label");
    grid.attach(&file, 0, 2, 1, 1);

    let about = gtk::Button::builder()
        .child(&gtk::Image::from_icon_name(
            "gnome-commander-info-outline-symbolic",
        ))
        .tooltip_text(gettext("About"))
        .vexpand(true)
        .valign(gtk::Align::Center)
        .build();
    about.add_css_class("flat");
    grid.attach(&about, 1, 0, 1, 3);

    let configure = gtk::Button::builder()
        .child(&gtk::Image::from_icon_name(
            "gnome-commander-settings-symbolic",
        ))
        .tooltip_text(gettext("Configure"))
        .vexpand(true)
        .valign(gtk::Align::Center)
        .sensitive(plugin.is::<Configurable>())
        .build();
    configure.add_css_class("flat");
    grid.attach(&configure, 2, 0, 1, 3);

    let switch = gtk::Switch::builder()
        .active(plugin_data.active)
        .vexpand(true)
        .valign(gtk::Align::Center)
        .build();
    grid.attach(&switch, 3, 0, 1, 3);

    about.connect_clicked(move |btn| {
        if let Some(window) = btn.root().and_downcast::<gtk::Window>() {
            about_plugin_dialog(&window, &info).present();
        }
    });

    configure.connect_clicked(glib::clone!(
        #[strong]
        plugin,
        move |btn| {
            if let Some(window) = btn.root().and_downcast::<gtk::Window>() {
                if let Some(cfg) = plugin.downcast_ref::<Configurable>() {
                    cfg.configure(&window);
                }
            }
        }
    ));

    switch.connect_active_notify(glib::clone!(
        #[strong]
        plugin_manager,
        #[strong]
        plugin,
        move |switch| plugin_manager.set_active(&plugin, switch.is_active())
    ));

    grid.upcast()
}
