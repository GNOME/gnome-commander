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

use gtk::{
    gio::{self, ffi::GMenu},
    glib::{
        ffi::{gboolean, GList},
        gobject_ffi::GObject,
        translate::{from_glib_full, from_glib_none, ToGlibPtr},
    },
    prelude::*,
};
use std::{ffi::c_char, os::raw::c_void};

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

#[no_mangle]
pub extern "C" fn gnome_cmd_wrap_plugin_menu(
    plugin_ptr: *const c_char,
    menu_ptr: *mut GMenu,
) -> *mut GMenu {
    if plugin_ptr.is_null() || menu_ptr.is_null() {
        return std::ptr::null_mut();
    }

    let plugin: String = unsafe { from_glib_none(plugin_ptr) };
    let menu: gio::Menu = unsafe { from_glib_full(menu_ptr) };

    let new_menu = wrap_plugin_menu(&plugin, menu.upcast_ref());
    new_menu.to_glib_full()
}

extern "C" {
    fn plugin_manager_get_all() -> *const GList;
}

#[repr(C)]
struct PluginData {
    active: gboolean,
    loaded: gboolean,
    autoload: gboolean,
    fname: *const c_char,
    fpath: *const c_char,
    action_group_name: *const c_char,
    plugin: *const GObject,
    info: *const c_void,   /* PluginInfo */
    module: *const c_void, /* GModule */
}

pub fn plugin_manager_get_active_plugin(action_group_name: &str) -> Option<glib::Object> {
    unsafe {
        let mut list = plugin_manager_get_all();
        while !list.is_null() {
            let plugin_data = (*list).data as *const PluginData;
            if (*plugin_data).active != 0 {
                let name: String = from_glib_none((*plugin_data).action_group_name);
                if name == action_group_name {
                    return Some(from_glib_none((*plugin_data).plugin));
                }
            }
            list = (*list).next;
        }
    }
    None
}
