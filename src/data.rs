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
    filter::PatternType,
    types::{
        ExtensionDisplayMode, GnomeCmdConfirmOverwriteMode, GraphicalLayoutMode,
        PermissionDisplayMode, SizeDisplayMode,
    },
};
use gtk::{
    gio,
    glib::{ffi::GList, translate::ToGlibPtr},
    prelude::*,
};
use std::ffi::{c_char, c_void};

pub struct GeneralOptions(pub gio::Settings);

pub trait GeneralOptionsRead {
    fn bookmarks(&self) -> glib::Variant;
    fn symlink_format(&self) -> String;
    fn use_trash(&self) -> bool;
    fn keybindings(&self) -> glib::Variant;

    fn date_display_format(&self) -> String;
    fn graphical_layout_mode(&self) -> GraphicalLayoutMode;
    fn extension_display_mode(&self) -> ExtensionDisplayMode;
    fn size_display_mode(&self) -> SizeDisplayMode;
    fn permissions_display_mode(&self) -> PermissionDisplayMode;

    fn select_dirs(&self) -> bool;

    fn case_sensitive(&self) -> bool;

    fn quick_seaech_exact_match_begin(&self) -> bool;
    fn quick_seaech_exact_match_end(&self) -> bool;
}

pub trait GeneralOptionsWrite {
    fn set_bookmarks(&self, bookmarks: &glib::Variant);
    fn reset_bookmarks(&self);

    fn set_symlink_format(&self, symlink_format: &str);

    fn set_use_trash(&self, use_trash: bool);

    fn set_keybindings(&self, keybindings: &glib::Variant);
}

impl GeneralOptions {
    pub fn new() -> Self {
        Self(gio::Settings::new(
            "org.gnome.gnome-commander.preferences.general",
        ))
    }
}

impl GeneralOptionsRead for GeneralOptions {
    fn bookmarks(&self) -> glib::Variant {
        self.0.value("bookmarks")
    }

    fn symlink_format(&self) -> String {
        self.0.string("symlink-string").to_string()
    }

    fn use_trash(&self) -> bool {
        self.0.boolean("delete-to-trash")
    }

    fn keybindings(&self) -> glib::Variant {
        self.0.value("keybindings")
    }

    fn date_display_format(&self) -> String {
        self.0.string("date-disp-format").to_string()
    }

    fn graphical_layout_mode(&self) -> GraphicalLayoutMode {
        self.0
            .enum_("graphical-layout-mode")
            .try_into()
            .ok()
            .and_then(GraphicalLayoutMode::from_repr)
            .unwrap_or_default()
    }

    fn extension_display_mode(&self) -> ExtensionDisplayMode {
        self.0
            .enum_("extension-display-mode")
            .try_into()
            .ok()
            .and_then(ExtensionDisplayMode::from_repr)
            .unwrap_or_default()
    }

    fn size_display_mode(&self) -> SizeDisplayMode {
        self.0
            .enum_("size-display-mode")
            .try_into()
            .ok()
            .and_then(SizeDisplayMode::from_repr)
            .unwrap_or_default()
    }

    fn permissions_display_mode(&self) -> PermissionDisplayMode {
        self.0
            .enum_("perm-display-mode")
            .try_into()
            .ok()
            .and_then(PermissionDisplayMode::from_repr)
            .unwrap_or_default()
    }

    fn select_dirs(&self) -> bool {
        self.0.boolean("select-dirs")
    }

    fn case_sensitive(&self) -> bool {
        self.0.boolean("case-sensitive")
    }

    fn quick_seaech_exact_match_begin(&self) -> bool {
        self.0.boolean("quick-search-exact-match-begin")
    }

    fn quick_seaech_exact_match_end(&self) -> bool {
        self.0.boolean("quick-search-exact-match-end")
    }
}

impl GeneralOptionsWrite for GeneralOptions {
    fn set_bookmarks(&self, bookmarks: &glib::Variant) {
        self.0.set_value("bookmarks", bookmarks);
    }

    fn reset_bookmarks(&self) {
        self.0.reset("bookmarks");
    }

    fn set_symlink_format(&self, symlink_format: &str) {
        self.0.set_string("symlink-string", symlink_format);
    }

    fn set_use_trash(&self, use_trash: bool) {
        self.0.set_boolean("delete-to-trash", use_trash);
    }

    fn set_keybindings(&self, keybindings: &glib::Variant) {
        self.0.set_value("keybindings", keybindings);
    }
}

pub struct ConfirmOptions(pub gio::Settings);

#[derive(Clone, Copy)]
pub enum DeleteDefault {
    Cancel = 3,
    Delete = 1,
}

pub trait ConfirmOptionsRead {
    fn confirm_delete(&self) -> bool;
    fn confirm_delete_default(&self) -> DeleteDefault;
    fn confirm_copy_overwrite(&self) -> GnomeCmdConfirmOverwriteMode;
    fn confirm_move_overwrite(&self) -> GnomeCmdConfirmOverwriteMode;
}

pub trait ConfirmOptionsWrite {
    fn set_confirm_delete(&self, confirm_delete: bool);
    fn confirm_delete_default(&self, delete_default: DeleteDefault);
}

impl ConfirmOptions {
    pub fn new() -> Self {
        Self(gio::Settings::new(
            "org.gnome.gnome-commander.preferences.confirmations",
        ))
    }
}

impl ConfirmOptionsRead for ConfirmOptions {
    fn confirm_delete(&self) -> bool {
        self.0.boolean("delete")
    }

    fn confirm_delete_default(&self) -> DeleteDefault {
        match self.0.enum_("delete-default") {
            1 => DeleteDefault::Delete,
            _ => DeleteDefault::Cancel,
        }
    }

    fn confirm_copy_overwrite(&self) -> GnomeCmdConfirmOverwriteMode {
        self.0
            .enum_("copy-overwrite")
            .try_into()
            .ok()
            .and_then(GnomeCmdConfirmOverwriteMode::from_repr)
            .unwrap_or(GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
    }

    fn confirm_move_overwrite(&self) -> GnomeCmdConfirmOverwriteMode {
        self.0
            .enum_("move-overwrite")
            .try_into()
            .ok()
            .and_then(GnomeCmdConfirmOverwriteMode::from_repr)
            .unwrap_or(GnomeCmdConfirmOverwriteMode::GNOME_CMD_CONFIRM_OVERWRITE_QUERY)
    }
}

impl ConfirmOptionsWrite for ConfirmOptions {
    fn set_confirm_delete(&self, confirm_delete: bool) {
        self.0.set_boolean("delete", confirm_delete);
    }

    fn confirm_delete_default(&self, delete_default: DeleteDefault) {
        self.0.set_enum("delete-default", delete_default as i32);
    }
}

pub struct ProgramsOptions(pub gio::Settings);

pub trait ProgramsOptionsRead {
    fn dont_download(&self) -> bool;
    fn use_internal_viewer(&self) -> bool;
    fn viewer_cmd(&self) -> String;
    fn editor_cmd(&self) -> String;
    fn differ_cmd(&self) -> String;
    fn use_internal_search(&self) -> bool;
    fn search_cmd(&self) -> String;
    fn sendto_cmd(&self) -> String;
    fn terminal_cmd(&self) -> String;
    fn terminal_exec_cmd(&self) -> String;
    fn use_gcmd_block(&self) -> bool;
}

impl ProgramsOptions {
    pub fn new() -> Self {
        Self(gio::Settings::new(
            "org.gnome.gnome-commander.preferences.programs",
        ))
    }
}

impl ProgramsOptionsRead for ProgramsOptions {
    fn dont_download(&self) -> bool {
        self.0.boolean("dont-download")
    }

    fn use_internal_viewer(&self) -> bool {
        self.0.boolean("use-internal-viewer")
    }

    fn viewer_cmd(&self) -> String {
        self.0.string("viewer-cmd").into()
    }

    fn editor_cmd(&self) -> String {
        self.0.string("editor-cmd").into()
    }

    fn differ_cmd(&self) -> String {
        self.0.string("differ-cmd").into()
    }

    fn use_internal_search(&self) -> bool {
        self.0.boolean("use-internal-search")
    }

    fn search_cmd(&self) -> String {
        self.0.string("search-cmd").into()
    }

    fn sendto_cmd(&self) -> String {
        self.0.string("sendto-cmd").into()
    }

    fn terminal_cmd(&self) -> String {
        self.0.string("terminal-cmd").into()
    }

    fn terminal_exec_cmd(&self) -> String {
        self.0.string("terminal-exec-cmd").into()
    }

    fn use_gcmd_block(&self) -> bool {
        self.0.boolean("use-gcmd-block")
    }
}

pub struct SearchConfig(*mut c_void);

extern "C" {
    fn gnome_cmd_search_config_get_name_patterns(ptr: *mut c_void) -> *const GList;
    fn gnome_cmd_search_config_add_name_pattern(ptr: *mut c_void, p: *const c_char);

    fn gnome_cmd_search_config_get_default_profile_syntax(ptr: *mut c_void) -> i32;
    fn gnome_cmd_search_config_set_default_profile_syntax(ptr: *mut c_void, s: i32);
}

impl SearchConfig {
    pub unsafe fn from_ptr(ptr: *mut c_void) -> Self {
        Self(ptr)
    }

    pub unsafe fn as_ptr(&self) -> *mut c_void {
        self.0
    }

    pub fn name_patterns(&self) -> glib::List<glib::GStringPtr> {
        unsafe { glib::List::from_glib_none(gnome_cmd_search_config_get_name_patterns(self.0)) }
    }

    pub fn add_name_pattern(&self, pattern: &str) {
        unsafe { gnome_cmd_search_config_add_name_pattern(self.0, pattern.to_glib_none().0) }
    }

    pub fn default_profile_syntax(&self) -> PatternType {
        let t = unsafe { gnome_cmd_search_config_get_default_profile_syntax(self.0) };
        match t {
            0 => PatternType::Regex,
            _ => PatternType::FnMatch,
        }
    }

    pub fn set_default_profile_syntax(&self, pattern_type: PatternType) {
        unsafe { gnome_cmd_search_config_set_default_profile_syntax(self.0, pattern_type as i32) }
    }
}
