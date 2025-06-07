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
    file_selector::TabVariant,
    filter::PatternType,
    search::profile::{SearchProfile, SearchProfilePtr, SearchProfileVariant},
    tab_label::TabLockIndicator,
    types::{
        ExtensionDisplayMode, GnomeCmdConfirmOverwriteMode, GraphicalLayoutMode,
        PermissionDisplayMode, SizeDisplayMode,
    },
};
use gtk::{
    gio::{self, ffi::GListStore},
    glib::{
        ffi::{gboolean, GList},
        translate::{from_glib_none, ToGlibPtr},
    },
    prelude::*,
};
use std::ffi::{c_char, c_void};

pub type WriteResult = Result<(), glib::BoolError>;

pub struct GeneralOptions(pub gio::Settings);

pub trait GeneralOptionsRead {
    fn allow_multiple_instances(&self) -> bool;

    fn bookmarks(&self) -> glib::Variant;
    fn symlink_format(&self) -> String;
    fn use_trash(&self) -> bool;
    fn keybindings(&self) -> glib::Variant;

    fn always_show_tabs(&self) -> bool;
    fn tab_lock_indicator(&self) -> TabLockIndicator;

    fn file_list_tabs(&self) -> Vec<TabVariant>;

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

    fn set_always_show_tabs(&self, always_show_tabs: bool) -> WriteResult;
    fn set_tab_lock_indicator(&self, tab_lock_indicator: TabLockIndicator) -> WriteResult;

    fn set_file_list_tabs(&self, tabs: &[TabVariant]);
}

impl GeneralOptions {
    pub fn new() -> Self {
        Self(gio::Settings::new(
            "org.gnome.gnome-commander.preferences.general",
        ))
    }
}

impl GeneralOptionsRead for GeneralOptions {
    fn allow_multiple_instances(&self) -> bool {
        self.0.boolean("allow-multiple-instances")
    }

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

    fn always_show_tabs(&self) -> bool {
        self.0.boolean("always-show-tabs")
    }

    fn tab_lock_indicator(&self) -> TabLockIndicator {
        self.0
            .enum_("tab-lock-indicator")
            .try_into()
            .ok()
            .and_then(TabLockIndicator::from_repr)
            .unwrap_or_default()
    }

    fn file_list_tabs(&self) -> Vec<TabVariant> {
        self.0.get("file-list-tabs")
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

    fn set_always_show_tabs(&self, always_show_tabs: bool) -> WriteResult {
        self.0.set_boolean("always-show-tabs", always_show_tabs)
    }

    fn set_tab_lock_indicator(&self, tab_lock_indicator: TabLockIndicator) -> WriteResult {
        self.0
            .set_enum("tab-lock-indicator", tab_lock_indicator as i32)
    }

    fn set_file_list_tabs(&self, tabs: &[TabVariant]) {
        self.0.set("file-list-tabs", tabs);
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

pub struct FiltersOptions(pub gio::Settings);

pub trait FiltersOptionsRead {
    fn hide_unknown(&self) -> bool;
    fn hide_regular(&self) -> bool;
    fn hide_directory(&self) -> bool;
    fn hide_special(&self) -> bool;
    fn hide_shortcut(&self) -> bool;
    fn hide_mountable(&self) -> bool;
    fn hide_virtual(&self) -> bool;
    fn hide_volatile(&self) -> bool;
    fn hide_hidden(&self) -> bool;
    fn hide_backup(&self) -> bool;
    fn hide_symlink(&self) -> bool;
    fn backup_pattern(&self) -> String;
}

pub trait FiltersOptionsWrite {
    fn set_hide_unknown(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_regular(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_directory(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_special(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_shortcut(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_mountable(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_virtual(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_volatile(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_hidden(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_backup(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_hide_symlink(&self, value: bool) -> Result<(), glib::error::BoolError>;
    fn set_backup_pattern(&self, value: &str) -> Result<(), glib::error::BoolError>;
}

pub const PREFERENCES_FILTER: &str = "org.gnome.gnome-commander.preferences.filter";

impl FiltersOptions {
    pub fn new() -> Self {
        Self(gio::Settings::new(PREFERENCES_FILTER))
    }
}

impl FiltersOptionsRead for FiltersOptions {
    fn hide_unknown(&self) -> bool {
        self.0.boolean("hide-unknown")
    }
    fn hide_regular(&self) -> bool {
        self.0.boolean("hide-regular")
    }
    fn hide_directory(&self) -> bool {
        self.0.boolean("hide-directory")
    }
    fn hide_special(&self) -> bool {
        self.0.boolean("hide-special")
    }
    fn hide_shortcut(&self) -> bool {
        self.0.boolean("hide-shortcut")
    }
    fn hide_mountable(&self) -> bool {
        self.0.boolean("hide-mountable")
    }
    fn hide_virtual(&self) -> bool {
        self.0.boolean("hide-virtual")
    }
    fn hide_volatile(&self) -> bool {
        self.0.boolean("hide-volatile")
    }
    fn hide_hidden(&self) -> bool {
        self.0.boolean("hide-dotfile")
    }
    fn hide_backup(&self) -> bool {
        self.0.boolean("hide-backupfiles")
    }
    fn hide_symlink(&self) -> bool {
        self.0.boolean("hide-symlink")
    }
    fn backup_pattern(&self) -> String {
        self.0.string("backup-pattern").to_string()
    }
}

impl FiltersOptionsWrite for FiltersOptions {
    fn set_hide_unknown(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-unknown", value)
    }
    fn set_hide_regular(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-regular", value)
    }
    fn set_hide_directory(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-directory", value)
    }
    fn set_hide_special(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-special", value)
    }
    fn set_hide_shortcut(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-shortcut", value)
    }
    fn set_hide_mountable(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-mountable", value)
    }
    fn set_hide_virtual(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-virtual", value)
    }
    fn set_hide_volatile(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-volatile", value)
    }
    fn set_hide_hidden(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-dotfile", value)
    }
    fn set_hide_backup(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-backupfiles", value)
    }
    fn set_hide_symlink(&self, value: bool) -> Result<(), glib::error::BoolError> {
        self.0.set_boolean("hide-symlink", value)
    }
    fn set_backup_pattern(&self, value: &str) -> Result<(), glib::error::BoolError> {
        self.0.set_string("backup-pattern", value)
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

#[derive(Clone, Copy)]
pub struct SearchConfig(*mut c_void);

extern "C" {
    fn gnome_cmd_search_config_get_name_patterns(ptr: *mut c_void) -> *const GList;
    fn gnome_cmd_search_config_add_name_pattern(ptr: *mut c_void, p: *const c_char);

    fn gnome_cmd_search_config_get_content_patterns(ptr: *mut c_void) -> *const GList;

    fn gnome_cmd_search_config_get_default_profile(ptr: *mut c_void) -> *mut SearchProfilePtr;
    fn gnome_cmd_search_config_get_profiles(ptr: *mut c_void) -> *mut GListStore;
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

    pub fn content_patterns(&self) -> glib::List<glib::GStringPtr> {
        unsafe { glib::List::from_glib_none(gnome_cmd_search_config_get_content_patterns(self.0)) }
    }

    pub fn default_profile(&self) -> SearchProfile {
        unsafe { from_glib_none(gnome_cmd_search_config_get_default_profile(self.0)) }
    }

    pub fn profiles(&self) -> gio::ListStore {
        unsafe { from_glib_none(gnome_cmd_search_config_get_profiles(self.0)) }
    }

    pub fn set_profiles(&self, profiles: &gio::ListStore) {
        let store = self.profiles();
        store.remove_all();
        for p in profiles.iter::<SearchProfile>().flatten() {
            store.append(&p);
        }
    }

    pub fn default_profile_syntax(&self) -> PatternType {
        match self.default_profile().syntax() {
            0 => PatternType::Regex,
            _ => PatternType::FnMatch,
        }
    }

    pub fn set_default_profile_syntax(&self, pattern_type: PatternType) {
        self.default_profile().set_syntax(match pattern_type {
            PatternType::Regex => 0,
            PatternType::FnMatch => 1,
        });
    }

    pub fn save(&self, save_search_history: bool) -> glib::Variant {
        let mut vs = Vec::<SearchProfileVariant>::new();
        vs.push(if save_search_history {
            self.default_profile().save()
        } else {
            SearchProfile::default().save()
        });
        vs.extend(
            self.profiles()
                .iter::<SearchProfile>()
                .flatten()
                .map(|p| p.save()),
        );
        vs.to_variant()
    }

    pub fn load(&self, variant: &glib::Variant) {
        self.default_profile().reset();
        self.profiles().remove_all();
        let Some(vs) = Vec::<SearchProfileVariant>::from_variant(variant) else {
            return;
        };
        let mut iter = vs.into_iter();
        if let Some(v) = iter.next() {
            self.default_profile().load(v);
        }
        for v in iter {
            let p = SearchProfile::default();
            p.load(v);
            self.profiles().append(&p);
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_search_config_load(ptr: *mut c_void) {
    let search_config = unsafe { SearchConfig::from_ptr(ptr) };
    let options = GeneralOptions::new();
    let variant = options.0.value("search-profiles");
    search_config.load(&variant);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_search_config_save(ptr: *mut c_void, save_search_history: gboolean) {
    let search_config = unsafe { SearchConfig::from_ptr(ptr) };
    let variant = search_config.save(save_search_history != 0);
    let options = GeneralOptions::new();
    if let Err(error) = options.0.set_value("search-profiles", &variant) {
        eprintln!("Failed to save search profiles: {}", error.message);
    }
}

pub struct NetworkOptions(pub gio::Settings);

pub trait NetworkOptionsRead {
    fn quick_connect_uri(&self) -> String;
}

pub trait NetworkOptionsWrite {
    fn set_quick_connect_uri(&self, uri: &str);
}

impl NetworkOptions {
    pub fn new() -> Self {
        Self(gio::Settings::new(
            "org.gnome.gnome-commander.preferences.network",
        ))
    }
}

impl NetworkOptionsRead for NetworkOptions {
    fn quick_connect_uri(&self) -> String {
        self.0.string("quick-connect-uri").to_string()
    }
}

impl NetworkOptionsWrite for NetworkOptions {
    fn set_quick_connect_uri(&self, uri: &str) {
        self.0.set_string("quick-connect-uri", uri);
    }
}
