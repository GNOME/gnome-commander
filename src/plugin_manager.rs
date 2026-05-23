// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    config::{PACKAGE, PLUGIN_DIR},
    gmodule::{self, GModule, GModuleFlags},
    libgcmd::{GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION, PluginInfo},
};
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
        if let Err(error) = self.scan_plugins_in_dir(Path::new(PLUGIN_DIR)) {
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
