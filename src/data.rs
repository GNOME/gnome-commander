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
    app::FavoriteAppVariant,
    connection::history::History,
    file_selector::TabVariant,
    filter::PatternType,
    search::profile::{SearchProfile, SearchProfileVariant},
    tab_label::TabLockIndicator,
    types::{
        ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GraphicalLayoutMode,
        PermissionDisplayMode, QuickSearchShortcut, SizeDisplayMode,
    },
};
use gettextrs::gettext;
use gtk::{gio, prelude::*};
use std::{rc::Rc, sync::LazyLock};

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

    fn quick_seaech_shortcut(&self) -> QuickSearchShortcut;
    fn quick_seaech_exact_match_begin(&self) -> bool;
    fn quick_seaech_exact_match_end(&self) -> bool;

    fn show_samba_workgroups_button(&self) -> bool;
    fn device_only_icon(&self) -> bool;

    fn save_tabs_on_exit(&self) -> bool;
    fn save_dirs_on_exit(&self) -> bool;

    fn favorite_apps(&self) -> Vec<FavoriteAppVariant>;

    fn search_window_is_transient(&self) -> bool;
    fn search_profiles(&self) -> Vec<SearchProfileVariant>;
    fn search_pattern_history(&self) -> Vec<String>;
    fn search_text_history(&self) -> Vec<String>;
    fn save_search_history(&self) -> bool;
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

    fn set_show_samba_workgroups_button(&self, value: bool) -> WriteResult;
    fn set_device_only_icon(&self, value: bool) -> WriteResult;

    fn set_favorite_apps(&self, apps: &[FavoriteAppVariant]) -> WriteResult;

    fn set_search_window_is_transient(&self, value: bool) -> WriteResult;
    fn set_search_profiles(&self, profiles: &[SearchProfileVariant]) -> WriteResult;
    fn set_search_pattern_history(&self, values: &[String]) -> WriteResult;
    fn set_search_text_history(&self, values: &[String]) -> WriteResult;
    fn set_save_search_history(&self, value: bool) -> WriteResult;
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

    fn quick_seaech_shortcut(&self) -> QuickSearchShortcut {
        self.0
            .enum_("quick-search")
            .try_into()
            .ok()
            .and_then(QuickSearchShortcut::from_repr)
            .unwrap_or_default()
    }

    fn quick_seaech_exact_match_begin(&self) -> bool {
        self.0.boolean("quick-search-exact-match-begin")
    }

    fn quick_seaech_exact_match_end(&self) -> bool {
        self.0.boolean("quick-search-exact-match-end")
    }

    fn save_tabs_on_exit(&self) -> bool {
        self.0.boolean("save-tabs-on-exit")
    }

    fn save_dirs_on_exit(&self) -> bool {
        self.0.boolean("save-dirs-on-exit")
    }

    fn show_samba_workgroups_button(&self) -> bool {
        self.0.boolean("show-samba-workgroup-button")
    }

    fn device_only_icon(&self) -> bool {
        self.0.boolean("dev-only-icon")
    }

    fn favorite_apps(&self) -> Vec<FavoriteAppVariant> {
        let variant = self.0.value("favorite-apps");
        Vec::<FavoriteAppVariant>::from_variant(&variant).unwrap_or_default()
    }

    fn search_window_is_transient(&self) -> bool {
        self.0.boolean("search-win-is-transient")
    }

    fn search_profiles(&self) -> Vec<SearchProfileVariant> {
        Vec::<SearchProfileVariant>::from_variant(&self.0.value("search-profiles"))
            .unwrap_or_default()
    }

    fn search_pattern_history(&self) -> Vec<String> {
        self.0
            .strv("search-pattern-history")
            .iter()
            .map(|v| v.to_string())
            .collect()
    }

    fn search_text_history(&self) -> Vec<String> {
        self.0
            .strv("search-text-history")
            .iter()
            .map(|v| v.to_string())
            .collect()
    }

    fn save_search_history(&self) -> bool {
        self.0.boolean("save-search-history-on-exit")
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

    fn set_show_samba_workgroups_button(&self, value: bool) -> WriteResult {
        self.0.set_boolean("show-samba-workgroup-button", value)
    }

    fn set_device_only_icon(&self, value: bool) -> WriteResult {
        self.0.set_boolean("dev-only-icon", value)
    }

    fn set_favorite_apps(&self, apps: &[FavoriteAppVariant]) -> WriteResult {
        self.0.set_value("favorite-apps", &apps.to_variant())
    }

    fn set_search_window_is_transient(&self, value: bool) -> WriteResult {
        self.0.set_boolean("search-win-is-transient", value)
    }

    fn set_search_profiles(&self, profiles: &[SearchProfileVariant]) -> WriteResult {
        self.0.set_value("search-profiles", &profiles.to_variant())
    }

    fn set_search_pattern_history(&self, values: &[String]) -> WriteResult {
        self.0.set_strv("search-pattern-history", values)
    }

    fn set_search_text_history(&self, values: &[String]) -> WriteResult {
        self.0.set_strv("search-text-history", values)
    }

    fn set_save_search_history(&self, value: bool) -> WriteResult {
        self.0.set_boolean("save-search-history-on-exit", value)
    }
}

pub struct ConfirmOptions(pub gio::Settings);

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum DeleteDefault {
    Cancel = 3,
    Delete = 1,
}

pub trait ConfirmOptionsRead {
    fn confirm_delete(&self) -> bool;
    fn confirm_delete_default(&self) -> DeleteDefault;
    fn confirm_copy_overwrite(&self) -> ConfirmOverwriteMode;
    fn confirm_move_overwrite(&self) -> ConfirmOverwriteMode;
    fn dnd_mode(&self) -> DndMode;
}

pub trait ConfirmOptionsWrite {
    fn set_confirm_delete(&self, confirm_delete: bool) -> WriteResult;
    fn set_confirm_delete_default(&self, delete_default: DeleteDefault) -> WriteResult;
    fn set_confirm_copy_overwrite(&self, mode: ConfirmOverwriteMode) -> WriteResult;
    fn set_confirm_move_overwrite(&self, mode: ConfirmOverwriteMode) -> WriteResult;
    fn set_dnd_mode(&self, mode: DndMode) -> WriteResult;
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

    fn confirm_copy_overwrite(&self) -> ConfirmOverwriteMode {
        self.0
            .enum_("copy-overwrite")
            .try_into()
            .ok()
            .and_then(ConfirmOverwriteMode::from_repr)
            .unwrap_or(ConfirmOverwriteMode::Query)
    }

    fn confirm_move_overwrite(&self) -> ConfirmOverwriteMode {
        self.0
            .enum_("move-overwrite")
            .try_into()
            .ok()
            .and_then(ConfirmOverwriteMode::from_repr)
            .unwrap_or(ConfirmOverwriteMode::Query)
    }

    fn dnd_mode(&self) -> DndMode {
        self.0
            .enum_("mouse-drag-and-drop")
            .try_into()
            .ok()
            .and_then(DndMode::from_repr)
            .unwrap_or_default()
    }
}

impl ConfirmOptionsWrite for ConfirmOptions {
    fn set_confirm_delete(&self, confirm_delete: bool) -> WriteResult {
        self.0.set_boolean("delete", confirm_delete)
    }

    fn set_confirm_delete_default(&self, delete_default: DeleteDefault) -> WriteResult {
        self.0.set_enum("delete-default", delete_default as i32)
    }

    fn set_confirm_copy_overwrite(&self, mode: ConfirmOverwriteMode) -> WriteResult {
        self.0.set_enum("copy-overwrite", mode as i32)
    }

    fn set_confirm_move_overwrite(&self, mode: ConfirmOverwriteMode) -> WriteResult {
        self.0.set_enum("move-overwrite", mode as i32)
    }

    fn set_dnd_mode(&self, mode: DndMode) -> WriteResult {
        self.0.set_enum("mouse-drag-and-drop", mode as i32)
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

pub trait ProgramsOptionsWrite {
    fn set_dont_download(&self, value: bool) -> WriteResult;
    fn set_use_internal_viewer(&self, value: bool) -> WriteResult;
    fn set_viewer_cmd(&self, value: &str) -> WriteResult;
    fn set_editor_cmd(&self, value: &str) -> WriteResult;
    fn set_differ_cmd(&self, value: &str) -> WriteResult;
    fn set_use_internal_search(&self, value: bool) -> WriteResult;
    fn set_search_cmd(&self, value: &str) -> WriteResult;
    fn set_sendto_cmd(&self, value: &str) -> WriteResult;
    fn set_terminal_cmd(&self, value: &str) -> WriteResult;
    fn set_terminal_exec_cmd(&self, value: &str) -> WriteResult;
    fn set_use_gcmd_block(&self, value: bool) -> WriteResult;
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

impl ProgramsOptionsWrite for ProgramsOptions {
    fn set_dont_download(&self, value: bool) -> WriteResult {
        self.0.set_boolean("dont-download", value)
    }

    fn set_use_internal_viewer(&self, value: bool) -> WriteResult {
        self.0.set_boolean("use-internal-viewer", value)
    }

    fn set_viewer_cmd(&self, value: &str) -> WriteResult {
        self.0.set_string("viewer-cmd", value)
    }

    fn set_editor_cmd(&self, value: &str) -> WriteResult {
        self.0.set_string("editor-cmd", value)
    }

    fn set_differ_cmd(&self, value: &str) -> WriteResult {
        self.0.set_string("differ-cmd", value)
    }

    fn set_use_internal_search(&self, value: bool) -> WriteResult {
        self.0.set_boolean("use-internal-search", value)
    }

    fn set_search_cmd(&self, value: &str) -> WriteResult {
        self.0.set_string("search-cmd", value)
    }

    fn set_sendto_cmd(&self, value: &str) -> WriteResult {
        self.0.set_string("sendto-cmd", value)
    }

    fn set_terminal_cmd(&self, value: &str) -> WriteResult {
        self.0.set_string("terminal-cmd", value)
    }

    fn set_terminal_exec_cmd(&self, value: &str) -> WriteResult {
        self.0.set_string("terminal-exec-cmd", value)
    }

    fn set_use_gcmd_block(&self, value: bool) -> WriteResult {
        self.0.set_boolean("use-gcmd-block", value)
    }
}

pub struct SearchConfig {
    default_profile: SearchProfile,
    name_patterns: History<String>,
    content_patterns: History<String>,
    profiles: gio::ListStore,
}

const SEARCH_HISTORY_SIZE: usize = 10;

impl Default for SearchConfig {
    fn default() -> Self {
        let default_profile = SearchProfile::default();
        default_profile.set_name(gettext("Default"));
        Self {
            default_profile,
            name_patterns: History::new(SEARCH_HISTORY_SIZE),
            content_patterns: History::new(SEARCH_HISTORY_SIZE),
            profiles: gio::ListStore::new::<SearchProfile>(),
        }
    }
}

impl SearchConfig {
    pub fn get() -> Rc<Self> {
        static CONFIG: LazyLock<glib::thread_guard::ThreadGuard<Rc<SearchConfig>>> =
            LazyLock::new(|| {
                glib::thread_guard::ThreadGuard::new(Rc::new(SearchConfig::default()))
            });
        CONFIG.get_ref().clone()
    }

    pub fn name_patterns(&self) -> Vec<String> {
        self.name_patterns.export()
    }

    pub fn add_name_pattern(&self, pattern: &str) {
        self.name_patterns.add(pattern.to_owned());
    }

    pub fn content_patterns(&self) -> Vec<String> {
        self.content_patterns.export()
    }

    pub fn add_content_pattern(&self, pattern: &str) {
        self.content_patterns.add(pattern.to_owned());
    }

    pub fn default_profile(&self) -> &SearchProfile {
        &self.default_profile
    }

    pub fn profiles(&self) -> &gio::ListStore {
        &self.profiles
    }

    pub fn set_profiles(&self, profiles: &gio::ListStore) {
        self.profiles.remove_all();
        for p in profiles.iter::<SearchProfile>().flatten() {
            self.profiles.append(&p);
        }
    }

    pub fn default_profile_syntax(&self) -> PatternType {
        self.default_profile().pattern_type()
    }

    pub fn set_default_profile_syntax(&self, pattern_type: PatternType) {
        self.default_profile().set_pattern_type(pattern_type)
    }

    pub fn save(
        &self,
        settings: &dyn GeneralOptionsWrite,
        save_search_history: bool,
    ) -> WriteResult {
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
        settings.set_search_profiles(&vs)?;

        if save_search_history {
            settings.set_search_pattern_history(&self.name_patterns())?;
            settings.set_search_text_history(&self.content_patterns())?;
        } else {
            settings.set_search_pattern_history(&[])?;
            settings.set_search_text_history(&[])?;
        }

        Ok(())
    }

    pub fn load(&self, settings: &dyn GeneralOptionsRead) {
        self.default_profile().reset();
        self.profiles().remove_all();
        let mut iter = settings.search_profiles().into_iter();
        if let Some(v) = iter.next() {
            self.default_profile().load(v);
        }
        for v in iter {
            let p = SearchProfile::default();
            p.load(v);
            self.profiles().append(&p);
        }
        for item in settings.search_pattern_history().iter().rev() {
            self.name_patterns.add(item.to_string());
        }
        for item in settings.search_text_history().iter().rev() {
            self.content_patterns.add(item.to_string());
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_search_config_load() {
    let search_config = SearchConfig::get();
    let options = GeneralOptions::new();
    search_config.load(&options);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_search_config_save() {
    let search_config = SearchConfig::get();
    let options = GeneralOptions::new();
    if let Err(error) = search_config.save(&options, options.save_search_history()) {
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
