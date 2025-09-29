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
    advanced_rename::profile::AdvancedRenameProfileVariant,
    app::FavoriteAppVariant,
    connection::{
        history::History,
        list::{BookmarkVariant, ConnectionVariant, CustomDeviceVariant},
    },
    enum_convert_strum,
    file_selector::TabVariant,
    filter::PatternType,
    intviewer::search_dialog::Mode,
    layout::{
        PREF_COLORS,
        color_themes::{ColorTheme, ColorThemeId, load_custom_theme, save_custom_theme},
        ls_colors_palette::{LsColorsPalette, load_palette, save_palette},
    },
    options::types::{
        AppOption, BoolOption, DURATION_MILLIS_TYPE, EnumOption, EnumRepr, OPTIONAL_PATH_TYPE,
        StringOption, StrvOption, TypeConvertCallback, U32Option, VariantOption, WriteResult,
    },
    search::profile::{SearchProfile, SearchProfileVariant},
    tab_label::TabLockIndicator,
    types::{
        ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GraphicalLayoutMode, IconScaleQuality,
        LeftMouseButtonMode, MiddleMouseButtonMode, PermissionDisplayMode, QuickSearchShortcut,
        RightMouseButtonMode, SizeDisplayMode,
    },
};
use gettextrs::gettext;
use gtk::{gio, prelude::*};
use std::{path::PathBuf, rc::Rc, sync::LazyLock, time::Duration};

pub struct GeneralOptions {
    pub allow_multiple_instances: BoolOption,
    pub device_list: VariantOption<Vec<CustomDeviceVariant>>,
    pub directory_history: StrvOption,
    pub connections: VariantOption<Vec<ConnectionVariant>>,
    pub bookmarks: VariantOption<Vec<BookmarkVariant>>,
    pub keybindings: AppOption<glib::Variant>,

    pub symlink_format: StringOption,
    pub use_trash: BoolOption,

    pub menu_visible: BoolOption,
    pub toolbar_visible: BoolOption,
    pub horizontal_orientation: BoolOption,
    pub command_line_visible: BoolOption,
    pub buttonbar_visible: BoolOption,
    pub connection_buttons_visible: BoolOption,
    pub connection_list_visible: BoolOption,

    pub always_show_tabs: BoolOption,
    pub tab_lock_indicator: EnumOption<TabLockIndicator>,

    pub file_list_tabs: VariantOption<Vec<TabVariant>>,

    pub list_font: StringOption,
    pub list_row_height: U32Option,
    pub list_column_width: [U32Option; 9],

    pub date_display_format: StringOption,
    pub graphical_layout_mode: EnumOption<GraphicalLayoutMode>,
    pub extension_display_mode: EnumOption<ExtensionDisplayMode>,
    pub size_display_mode: EnumOption<SizeDisplayMode>,
    pub permissions_display_mode: EnumOption<PermissionDisplayMode>,

    pub icon_size: U32Option,
    pub icon_scale_quality: EnumOption<IconScaleQuality>,
    pub mime_icon_dir: AppOption<Option<PathBuf>, String>,

    pub select_dirs: BoolOption,

    pub case_sensitive: BoolOption,
    pub symbolic_links_as_regular_files: BoolOption,

    pub left_mouse_button_mode: EnumOption<LeftMouseButtonMode>,
    pub middle_mouse_button_mode: EnumOption<MiddleMouseButtonMode>,
    pub right_mouse_button_mode: EnumOption<RightMouseButtonMode>,
    pub left_mouse_button_unselects: BoolOption,

    pub quick_search_shortcut: EnumOption<QuickSearchShortcut>,
    pub quick_search_exact_match_begin: BoolOption,
    pub quick_search_exact_match_end: BoolOption,

    pub show_samba_workgroups_button: BoolOption,
    pub device_only_icon: BoolOption,

    pub command_line_history: StrvOption,
    pub command_line_history_length: U32Option,

    pub save_tabs_on_exit: BoolOption,
    pub save_dirs_on_exit: BoolOption,
    pub save_directory_history_on_exit: BoolOption,
    pub save_command_line_history_on_exit: BoolOption,
    pub save_search_history: BoolOption,

    pub favorite_apps: VariantOption<Vec<FavoriteAppVariant>>,

    pub advanced_rename_profiles: VariantOption<Vec<AdvancedRenameProfileVariant>>,
    pub advanced_rename_template_history: AppOption<Vec<String>>,

    pub search_window_is_transient: BoolOption,
    pub search_profiles: VariantOption<Vec<SearchProfileVariant>>,
    pub search_pattern_history: StrvOption,
    pub search_text_history: StrvOption,

    pub main_window_width: U32Option,
    pub main_window_height: U32Option,
    pub main_window_state: U32Option,

    pub options_window_width: U32Option,
    pub options_window_height: U32Option,

    pub bookmarks_window_width: U32Option,
    pub bookmarks_window_height: U32Option,

    pub advanced_rename_window_width: U32Option,
    pub advanced_rename_window_height: U32Option,

    pub search_window_width: U32Option,
    pub search_window_height: U32Option,

    pub gui_update_rate: AppOption<Duration, u32>,
}

impl GeneralOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.general");
        Self {
            allow_multiple_instances: BoolOption::simple(&settings, "allow-multiple-instances"),
            device_list: VariantOption::variant(&settings, "device-list"),
            directory_history: StrvOption::simple(&settings, "directory-history"),
            connections: VariantOption::variant(&settings, "connections"),
            bookmarks: VariantOption::variant(&settings, "bookmarks"),
            keybindings: AppOption::simple(&settings, "keybindings"),
            symlink_format: StringOption::simple(&settings, "symlink-string"),
            use_trash: BoolOption::simple(&settings, "delete-to-trash"),
            menu_visible: AppOption::simple(&settings, "mainmenu-visibility"),
            toolbar_visible: AppOption::simple(&settings, "show-toolbar"),
            horizontal_orientation: AppOption::simple(&settings, "horizontal-orientation"),
            command_line_visible: AppOption::simple(&settings, "show-cmdline"),
            buttonbar_visible: AppOption::simple(&settings, "show-buttonbar"),
            connection_buttons_visible: AppOption::simple(&settings, "show-devbuttons"),
            connection_list_visible: AppOption::simple(&settings, "show-devlist"),
            always_show_tabs: BoolOption::simple(&&settings, "always-show-tabs"),
            tab_lock_indicator: AppOption::new(
                &settings,
                "tab-lock-indicator",
                enum_convert_strum!(TabLockIndicator),
            ),
            file_list_tabs: VariantOption::variant(&settings, "file-list-tabs"),
            list_font: StringOption::simple(&settings, "list-font"),
            list_row_height: U32Option::simple(&settings, "list-row-height"),
            list_column_width: [
                U32Option::simple(&settings, "column-width-icon"),
                U32Option::simple(&settings, "column-width-name"),
                U32Option::simple(&settings, "column-width-ext"),
                U32Option::simple(&settings, "column-width-dir"),
                U32Option::simple(&settings, "column-width-size"),
                U32Option::simple(&settings, "column-width-date"),
                U32Option::simple(&settings, "column-width-perm"),
                U32Option::simple(&settings, "column-width-owner"),
                U32Option::simple(&settings, "column-width-group"),
            ],
            date_display_format: StringOption::simple(&settings, "date-disp-format"),
            graphical_layout_mode: AppOption::new(
                &settings,
                "graphical-layout-mode",
                enum_convert_strum!(GraphicalLayoutMode),
            ),
            extension_display_mode: AppOption::new(
                &settings,
                "extension-display-mode",
                enum_convert_strum!(ExtensionDisplayMode),
            ),
            size_display_mode: AppOption::new(
                &settings,
                "size-display-mode",
                enum_convert_strum!(SizeDisplayMode),
            ),
            permissions_display_mode: AppOption::new(
                &settings,
                "perm-display-mode",
                enum_convert_strum!(PermissionDisplayMode),
            ),
            icon_size: U32Option::simple(&settings, "icon-size"),
            icon_scale_quality: AppOption::new(
                &settings,
                "icon-scale-quality",
                enum_convert_strum!(IconScaleQuality),
            ),
            mime_icon_dir: AppOption::new(&settings, "mime-icon-dir", OPTIONAL_PATH_TYPE),
            select_dirs: BoolOption::simple(&settings, "select-dirs"),
            case_sensitive: BoolOption::simple(&settings, "case-sensitive"),
            symbolic_links_as_regular_files: BoolOption::simple(
                &settings,
                "symbolic-links-as-regular-files",
            ),
            left_mouse_button_mode: AppOption::new(
                &settings,
                "clicks-to-open-item",
                enum_convert_strum!(LeftMouseButtonMode),
            ),
            middle_mouse_button_mode: AppOption::new(
                &settings,
                "middle-mouse-btn-mode",
                enum_convert_strum!(MiddleMouseButtonMode),
            ),
            right_mouse_button_mode: AppOption::new(
                &settings,
                "right-mouse-btn-mode",
                enum_convert_strum!(RightMouseButtonMode),
            ),
            left_mouse_button_unselects: BoolOption::simple(&settings, "left-mouse-btn-unselects"),
            quick_search_shortcut: AppOption::new(
                &settings,
                "quick-search",
                enum_convert_strum!(QuickSearchShortcut),
            ),
            quick_search_exact_match_begin: BoolOption::simple(
                &settings,
                "quick-search-exact-match-begin",
            ),
            quick_search_exact_match_end: BoolOption::simple(
                &settings,
                "quick-search-exact-match-end",
            ),
            show_samba_workgroups_button: BoolOption::simple(
                &settings,
                "show-samba-workgroup-button",
            ),
            device_only_icon: BoolOption::simple(&settings, "dev-only-icon"),
            command_line_history: StrvOption::simple(&settings, "cmdline-history"),
            command_line_history_length: U32Option::simple(&settings, "cmdline-history-length"),
            save_tabs_on_exit: BoolOption::simple(&settings, "save-tabs-on-exit"),
            save_dirs_on_exit: BoolOption::simple(&settings, "save-dirs-on-exit"),
            save_directory_history_on_exit: BoolOption::simple(
                &settings,
                "save-dir-history-on-exit",
            ),
            save_command_line_history_on_exit: BoolOption::simple(
                &settings,
                "save-cmdline-history-on-exit",
            ),
            save_search_history: BoolOption::simple(&settings, "save-search-history-on-exit"),
            favorite_apps: VariantOption::variant(&settings, "favorite-apps"),
            advanced_rename_profiles: AppOption::variant(&settings, "advrename-profiles"),
            advanced_rename_template_history: AppOption::simple(
                &settings,
                "advrename-template-history",
            ),
            search_window_is_transient: BoolOption::simple(&settings, "search-win-is-transient"),
            search_profiles: VariantOption::variant(&settings, "search-profiles"),
            search_pattern_history: StrvOption::simple(&settings, "search-pattern-history"),
            search_text_history: StrvOption::simple(&settings, "search-text-history"),
            main_window_width: U32Option::simple(&settings, "main-win-width"),
            main_window_height: U32Option::simple(&settings, "main-win-height"),
            main_window_state: U32Option::simple(&settings, "main-win-state"),
            options_window_width: AppOption::simple(&settings, "opts-dialog-width"),
            options_window_height: AppOption::simple(&settings, "opts-dialog-height"),
            bookmarks_window_width: AppOption::simple(&settings, "bookmarks-win-width"),
            bookmarks_window_height: AppOption::simple(&settings, "bookmarks-win-height"),
            advanced_rename_window_width: AppOption::simple(&settings, "advrename-win-width"),
            advanced_rename_window_height: AppOption::simple(&settings, "advrename-win-height"),
            search_window_width: AppOption::simple(&settings, "search-win-width"),
            search_window_height: AppOption::simple(&settings, "search-win-height"),
            gui_update_rate: AppOption::new(&settings, "gui-update-rate", DURATION_MILLIS_TYPE),
        }
    }
}

pub struct ColorOptions {
    settings: gio::Settings,
    pub theme: AppOption<Option<ColorThemeId>, EnumRepr>,
    pub use_ls_colors: BoolOption,
}

impl ColorOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new(PREF_COLORS);
        Self {
            theme: AppOption::new(
                &settings,
                "theme",
                TypeConvertCallback {
                    from_repr: |e: EnumRepr| e.0.try_into().ok().and_then(ColorThemeId::from_repr),
                    to_repr: |e| EnumRepr(e.map(|t| t as i32).unwrap_or_default()),
                },
            ),
            use_ls_colors: AppOption::simple(&settings, "use-ls-colors"),
            settings,
        }
    }

    pub fn custom_theme(&self) -> ColorTheme {
        load_custom_theme(&self.settings)
    }

    pub fn set_custom_theme(&self, theme: &ColorTheme) -> WriteResult {
        save_custom_theme(&theme, &self.settings)
    }

    pub fn ls_color_palette(&self) -> LsColorsPalette {
        load_palette(&self.settings)
    }

    pub fn set_ls_color_palette(&self, palette: &LsColorsPalette) -> WriteResult {
        save_palette(palette, &self.settings)
    }
}

pub struct ConfirmOptions {
    pub confirm_delete: BoolOption,
    pub confirm_delete_default: EnumOption<DeleteDefault>,
    pub confirm_copy_overwrite: EnumOption<ConfirmOverwriteMode>,
    pub confirm_move_overwrite: EnumOption<ConfirmOverwriteMode>,
    pub dnd_mode: EnumOption<DndMode>,
}

impl ConfirmOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.confirmations");
        Self {
            confirm_delete: BoolOption::simple(&settings, "delete"),
            confirm_delete_default: AppOption::new(
                &settings,
                "delete-default",
                enum_convert_strum!(DeleteDefault, DeleteDefault::Cancel),
            ),
            confirm_copy_overwrite: AppOption::new(
                &settings,
                "copy-overwrite",
                enum_convert_strum!(ConfirmOverwriteMode, ConfirmOverwriteMode::Query),
            ),
            confirm_move_overwrite: AppOption::new(
                &settings,
                "move-overwrite",
                enum_convert_strum!(ConfirmOverwriteMode, ConfirmOverwriteMode::Query),
            ),
            dnd_mode: AppOption::new(
                &settings,
                "mouse-drag-and-drop",
                enum_convert_strum!(DndMode),
            ),
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, strum::FromRepr)]
pub enum DeleteDefault {
    Cancel = 3,
    Delete = 1,
}

pub struct FiltersOptions {
    pub hide_unknown: BoolOption,
    pub hide_regular: BoolOption,
    pub hide_directory: BoolOption,
    pub hide_special: BoolOption,
    pub hide_shortcut: BoolOption,
    pub hide_mountable: BoolOption,
    pub hide_virtual: BoolOption,
    pub hide_volatile: BoolOption,
    pub hide_hidden: BoolOption,
    pub hide_backup: BoolOption,
    pub hide_symlink: BoolOption,
    pub backup_pattern: StringOption,
}

impl FiltersOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.filter");
        Self {
            hide_unknown: BoolOption::simple(&settings, "hide-unknown"),
            hide_regular: BoolOption::simple(&settings, "hide-regular"),
            hide_directory: BoolOption::simple(&settings, "hide-directory"),
            hide_special: BoolOption::simple(&settings, "hide-special"),
            hide_shortcut: BoolOption::simple(&settings, "hide-shortcut"),
            hide_mountable: BoolOption::simple(&settings, "hide-mountable"),
            hide_virtual: BoolOption::simple(&settings, "hide-virtual"),
            hide_volatile: BoolOption::simple(&settings, "hide-volatile"),
            hide_hidden: BoolOption::simple(&settings, "hide-dotfile"),
            hide_backup: BoolOption::simple(&settings, "hide-backupfiles"),
            hide_symlink: BoolOption::simple(&settings, "hide-symlink"),
            backup_pattern: StringOption::simple(&settings, "backup-pattern"),
        }
    }
}

pub struct ProgramsOptions {
    pub dont_download: BoolOption,
    pub use_internal_viewer: BoolOption,
    pub viewer_cmd: StringOption,
    pub editor_cmd: StringOption,
    pub differ_cmd: StringOption,
    pub use_internal_search: BoolOption,
    pub search_cmd: StringOption,
    pub sendto_cmd: StringOption,
    pub terminal_cmd: StringOption,
    pub terminal_exec_cmd: StringOption,
    pub use_gcmd_block: BoolOption,
}

impl ProgramsOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.programs");
        Self {
            dont_download: BoolOption::simple(&settings, "dont-download"),
            use_internal_viewer: BoolOption::simple(&settings, "use-internal-viewer"),
            viewer_cmd: StringOption::simple(&settings, "viewer-cmd"),
            editor_cmd: StringOption::simple(&settings, "editor-cmd"),
            differ_cmd: StringOption::simple(&settings, "differ-cmd"),
            use_internal_search: BoolOption::simple(&settings, "use-internal-search"),
            search_cmd: StringOption::simple(&settings, "search-cmd"),
            sendto_cmd: StringOption::simple(&settings, "sendto-cmd"),
            terminal_cmd: StringOption::simple(&settings, "terminal-cmd"),
            terminal_exec_cmd: StringOption::simple(&settings, "terminal-exec-cmd"),
            use_gcmd_block: BoolOption::simple(&settings, "use-gcmd-block"),
        }
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

    pub fn save(&self, settings: &GeneralOptions, save_search_history: bool) -> WriteResult {
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
        settings.search_profiles.set(vs)?;

        if save_search_history {
            settings.search_pattern_history.set(self.name_patterns())?;
            settings.search_text_history.set(self.content_patterns())?;
        } else {
            settings.search_pattern_history.set(&[])?;
            settings.search_text_history.set(&[])?;
        }

        Ok(())
    }

    pub fn load(&self, settings: &GeneralOptions) {
        self.default_profile().reset();
        self.profiles().remove_all();
        let mut iter = settings.search_profiles.get().into_iter();
        if let Some(v) = iter.next() {
            self.default_profile().load(v);
        }
        for v in iter {
            let p = SearchProfile::default();
            p.load(v);
            self.profiles().append(&p);
        }
        for item in settings.search_pattern_history.get().iter().rev() {
            self.name_patterns.add(item.to_string());
        }
        for item in settings.search_text_history.get().iter().rev() {
            self.content_patterns.add(item.to_string());
        }
    }
}

pub struct NetworkOptions {
    pub quick_connect_uri: StringOption,
}

impl NetworkOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.network");
        Self {
            quick_connect_uri: AppOption::simple(&settings, "quick-connect-uri"),
        }
    }
}

pub struct ViewerOptions {
    pub window_width: U32Option,
    pub window_height: U32Option,

    pub encoding: StringOption,
    pub fixed_font_name: StringOption,
    pub variable_font_name: StringOption,
    pub font_size: U32Option,
    pub tab_size: U32Option,
    pub wrap_mode: BoolOption,
    pub binary_bytes_per_line: U32Option,
    pub display_hex_offset: BoolOption,
    pub metadata_visible: BoolOption,
    pub search_pattern_text: StrvOption,
    pub search_pattern_hex: StrvOption,
    pub case_sensitive_search: BoolOption,
    pub search_mode: EnumOption<Mode>,
}

impl ViewerOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.internal-viewer");
        Self {
            window_width: AppOption::simple(&settings, "window-width"),
            window_height: AppOption::simple(&settings, "window-height"),
            encoding: AppOption::simple(&settings, "charset"),
            fixed_font_name: AppOption::simple(&settings, "fixed-font-name"),
            variable_font_name: AppOption::simple(&settings, "variable-font-name"),
            font_size: AppOption::simple(&settings, "font-size"),
            tab_size: AppOption::simple(&settings, "tab-size"),
            wrap_mode: AppOption::simple(&settings, "wrap-mode"),
            binary_bytes_per_line: AppOption::simple(&settings, "binary-bytes-per-line"),
            display_hex_offset: AppOption::simple(&settings, "display-hex-offset"),
            metadata_visible: AppOption::simple(&settings, "metadata-visible"),
            search_pattern_text: AppOption::simple(&settings, "search-pattern-text"),
            search_pattern_hex: AppOption::simple(&settings, "search-pattern-hex"),
            case_sensitive_search: AppOption::simple(&settings, "case-sensitive-search"),
            search_mode: AppOption::new(&settings, "search-mode", enum_convert_strum!(Mode)),
        }
    }
}
