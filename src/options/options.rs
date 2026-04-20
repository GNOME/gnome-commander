// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    advanced_rename::profile::AdvancedRenameProfileVariant,
    app::FavoriteAppVariant,
    connection::list::{BookmarkVariant, ConnectionVariant, CustomDeviceVariant},
    file_list::quick_search::QuickSearchMode,
    file_selector::{LegacyTabVariant, TabVariant},
    filter::PatternType,
    history::History,
    intviewer::search_bar::Mode,
    layout::{
        PREF_COLORS,
        color_themes::{ColorTheme, ColorThemeId, load_custom_theme, save_custom_theme},
        ls_colors_palette::{LsColorsPalette, load_palette, save_palette},
    },
    options::types::{
        AppOption, BoolOption, DurationOption, EnumOption, I32Option, OptionalPathOption,
        RGBAOption, StringOption, StrvOption, U32Option, VariantOption, WriteResult,
    },
    search::profile::{LegacySearchProfileVariant, SearchProfile, SearchProfileVariant},
    shortcuts::ShortcutVariant,
    tab_label::TabLockIndicator,
    types::{
        ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GraphicalLayoutMode, IconScaleQuality,
        LeftMouseButtonMode, MiddleMouseButtonMode, PermissionDisplayMode, QuickSearchShortcut,
        RightMouseButtonMode, SizeDisplayMode,
    },
    utils::u32_enum,
};
use gettextrs::gettext;
use gtk::{gio, prelude::*};
use std::{rc::Rc, sync::LazyLock};

pub struct GeneralOptions {
    pub allow_multiple_instances: BoolOption,
    pub device_list: VariantOption<Vec<CustomDeviceVariant>>,
    pub directory_history: StrvOption,
    pub connections: VariantOption<Vec<ConnectionVariant>>,
    pub bookmarks: VariantOption<Vec<BookmarkVariant>>,
    pub keybindings: VariantOption<Vec<ShortcutVariant>>,
    pub legacy_keybindings: AppOption<glib::Variant>,

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
    pub legacy_file_list_tabs: VariantOption<Vec<LegacyTabVariant>>,

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
    pub mime_icon_dir: OptionalPathOption,

    pub select_dirs: BoolOption,

    pub case_sensitive: BoolOption,
    pub symbolic_links_as_regular_files: BoolOption,

    pub left_mouse_button_mode: EnumOption<LeftMouseButtonMode>,
    pub middle_mouse_button_mode: EnumOption<MiddleMouseButtonMode>,
    pub right_mouse_button_mode: EnumOption<RightMouseButtonMode>,
    pub left_mouse_button_unselects: BoolOption,

    pub quick_search_shortcut: EnumOption<QuickSearchShortcut>,
    pub quick_search_default_mode: EnumOption<QuickSearchMode>,
    pub quick_search_case_sensitive: BoolOption,

    pub show_samba_workgroups_button: BoolOption,
    pub device_only_icon: BoolOption,

    pub command_line_history: StrvOption,
    pub command_line_history_length: U32Option,
    pub command_line_split: I32Option,
    pub command_line_autohide_output: BoolOption,

    pub save_tabs_on_exit: BoolOption,
    pub save_dirs_on_exit: BoolOption,
    pub save_directory_history_on_exit: BoolOption,
    pub save_command_line_history_on_exit: BoolOption,
    pub save_search_history: BoolOption,

    pub favorite_apps: VariantOption<Vec<FavoriteAppVariant>>,

    pub advanced_rename_profiles: VariantOption<Vec<AdvancedRenameProfileVariant>>,
    pub advanced_rename_template_history: StrvOption,

    pub search_window_is_transient: BoolOption,
    pub search_profiles: VariantOption<Vec<SearchProfileVariant>>,
    pub legacy_search_profiles: VariantOption<Vec<LegacySearchProfileVariant>>,
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

    pub gui_update_rate: DurationOption,
}

impl GeneralOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.general");

        Self {
            allow_multiple_instances: BoolOption::new(&settings, "allow-multiple-instances"),
            device_list: VariantOption::new(&settings, "device-list"),
            directory_history: StrvOption::new(&settings, "directory-history"),
            connections: VariantOption::new(&settings, "connections"),
            bookmarks: VariantOption::new(&settings, "bookmarks"),
            keybindings: VariantOption::new(&settings, "keybindings-v2"),
            legacy_keybindings: AppOption::new(&settings, "keybindings"),
            symlink_format: StringOption::new(&settings, "symlink-string"),
            use_trash: BoolOption::new(&settings, "delete-to-trash"),
            menu_visible: BoolOption::new(&settings, "mainmenu-visibility"),
            toolbar_visible: BoolOption::new(&settings, "show-toolbar"),
            horizontal_orientation: BoolOption::new(&settings, "horizontal-orientation"),
            command_line_visible: BoolOption::new(&settings, "show-cmdline"),
            buttonbar_visible: BoolOption::new(&settings, "show-buttonbar"),
            connection_buttons_visible: BoolOption::new(&settings, "show-devbuttons"),
            connection_list_visible: BoolOption::new(&settings, "show-devlist"),
            always_show_tabs: BoolOption::new(&settings, "always-show-tabs"),
            tab_lock_indicator: EnumOption::new(&settings, "tab-lock-indicator"),
            file_list_tabs: VariantOption::new(&settings, "file-list-tabs-v2"),
            legacy_file_list_tabs: VariantOption::new(&settings, "file-list-tabs"),
            list_font: StringOption::new(&settings, "list-font"),
            list_row_height: U32Option::new(&settings, "list-row-height"),
            list_column_width: [
                U32Option::new(&settings, "column-width-icon"),
                U32Option::new(&settings, "column-width-name"),
                U32Option::new(&settings, "column-width-ext"),
                U32Option::new(&settings, "column-width-dir"),
                U32Option::new(&settings, "column-width-size"),
                U32Option::new(&settings, "column-width-date"),
                U32Option::new(&settings, "column-width-perm"),
                U32Option::new(&settings, "column-width-owner"),
                U32Option::new(&settings, "column-width-group"),
            ],
            date_display_format: StringOption::new(&settings, "date-disp-format"),
            graphical_layout_mode: EnumOption::new(&settings, "graphical-layout-mode"),
            extension_display_mode: EnumOption::new(&settings, "extension-display-mode"),
            size_display_mode: EnumOption::new(&settings, "size-display-mode"),
            permissions_display_mode: EnumOption::new(&settings, "perm-display-mode"),
            icon_size: U32Option::new(&settings, "icon-size"),
            icon_scale_quality: EnumOption::new(&settings, "icon-scale-quality"),
            mime_icon_dir: OptionalPathOption::new(&settings, "mime-icon-dir"),
            select_dirs: BoolOption::new(&settings, "select-dirs"),
            case_sensitive: BoolOption::new(&settings, "case-sensitive"),
            symbolic_links_as_regular_files: BoolOption::new(
                &settings,
                "symbolic-links-as-regular-files",
            ),
            left_mouse_button_mode: EnumOption::new(&settings, "clicks-to-open-item"),
            middle_mouse_button_mode: EnumOption::new(&settings, "middle-mouse-btn-mode"),
            right_mouse_button_mode: EnumOption::new(&settings, "right-mouse-btn-mode"),
            left_mouse_button_unselects: BoolOption::new(&settings, "left-mouse-btn-unselects"),
            quick_search_shortcut: EnumOption::new(&settings, "quick-search"),
            quick_search_default_mode: EnumOption::new(&settings, "quick-search-default-mode"),
            quick_search_case_sensitive: BoolOption::new(&settings, "quick-search-case-sensitive"),
            show_samba_workgroups_button: BoolOption::new(&settings, "show-samba-workgroup-button"),
            device_only_icon: BoolOption::new(&settings, "dev-only-icon"),
            command_line_history: StrvOption::new(&settings, "cmdline-history"),
            command_line_history_length: U32Option::new(&settings, "cmdline-history-length"),
            command_line_split: I32Option::new(&settings, "cmdline-split"),
            command_line_autohide_output: BoolOption::new(&settings, "cmdline-autohide-output"),
            save_tabs_on_exit: BoolOption::new(&settings, "save-tabs-on-exit"),
            save_dirs_on_exit: BoolOption::new(&settings, "save-dirs-on-exit"),
            save_directory_history_on_exit: BoolOption::new(&settings, "save-dir-history-on-exit"),
            save_command_line_history_on_exit: BoolOption::new(
                &settings,
                "save-cmdline-history-on-exit",
            ),
            save_search_history: BoolOption::new(&settings, "save-search-history-on-exit"),
            favorite_apps: VariantOption::new(&settings, "favorite-apps"),
            advanced_rename_profiles: VariantOption::new(&settings, "advrename-profiles"),
            advanced_rename_template_history: StrvOption::new(
                &settings,
                "advrename-template-history",
            ),
            search_window_is_transient: BoolOption::new(&settings, "search-win-is-transient"),
            search_profiles: VariantOption::new(&settings, "search-profiles-v2"),
            legacy_search_profiles: VariantOption::new(&settings, "search-profiles"),
            search_pattern_history: StrvOption::new(&settings, "search-pattern-history"),
            search_text_history: StrvOption::new(&settings, "search-text-history"),
            main_window_width: U32Option::new(&settings, "main-win-width"),
            main_window_height: U32Option::new(&settings, "main-win-height"),
            main_window_state: U32Option::new(&settings, "main-win-state"),
            options_window_width: U32Option::new(&settings, "opts-dialog-width"),
            options_window_height: U32Option::new(&settings, "opts-dialog-height"),
            bookmarks_window_width: U32Option::new(&settings, "bookmarks-win-width"),
            bookmarks_window_height: U32Option::new(&settings, "bookmarks-win-height"),
            advanced_rename_window_width: U32Option::new(&settings, "advrename-win-width"),
            advanced_rename_window_height: U32Option::new(&settings, "advrename-win-height"),
            search_window_width: U32Option::new(&settings, "search-win-width"),
            search_window_height: U32Option::new(&settings, "search-win-height"),
            gui_update_rate: DurationOption::new(&settings, "gui-update-rate"),
        }
    }
}

pub struct ColorOptions {
    pub theme: EnumOption<ColorThemeId>,
    pub use_ls_colors: BoolOption,
    pub custom_norm_fg: RGBAOption,
    pub custom_norm_bg: RGBAOption,
    pub custom_alt_fg: RGBAOption,
    pub custom_alt_bg: RGBAOption,
    pub custom_sel_fg: RGBAOption,
    pub custom_sel_bg: RGBAOption,
    pub custom_curs_fg: RGBAOption,
    pub custom_curs_bg: RGBAOption,
    pub lscm_black_fg: RGBAOption,
    pub lscm_black_bg: RGBAOption,
    pub lscm_red_fg: RGBAOption,
    pub lscm_red_bg: RGBAOption,
    pub lscm_green_fg: RGBAOption,
    pub lscm_green_bg: RGBAOption,
    pub lscm_yellow_fg: RGBAOption,
    pub lscm_yellow_bg: RGBAOption,
    pub lscm_blue_fg: RGBAOption,
    pub lscm_blue_bg: RGBAOption,
    pub lscm_magenta_fg: RGBAOption,
    pub lscm_magenta_bg: RGBAOption,
    pub lscm_cyan_fg: RGBAOption,
    pub lscm_cyan_bg: RGBAOption,
    pub lscm_white_fg: RGBAOption,
    pub lscm_white_bg: RGBAOption,
}

impl ColorOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new(PREF_COLORS);
        Self {
            theme: EnumOption::new(&settings, "theme"),
            use_ls_colors: BoolOption::new(&settings, "use-ls-colors"),
            custom_norm_fg: RGBAOption::new(&settings, "custom-norm-fg"),
            custom_norm_bg: RGBAOption::new(&settings, "custom-norm-bg"),
            custom_alt_fg: RGBAOption::new(&settings, "custom-alt-fg"),
            custom_alt_bg: RGBAOption::new(&settings, "custom-alt-bg"),
            custom_sel_fg: RGBAOption::new(&settings, "custom-sel-fg"),
            custom_sel_bg: RGBAOption::new(&settings, "custom-sel-bg"),
            custom_curs_fg: RGBAOption::new(&settings, "custom-curs-fg"),
            custom_curs_bg: RGBAOption::new(&settings, "custom-curs-bg"),
            lscm_black_fg: RGBAOption::new(&settings, "lscm-black-fg"),
            lscm_black_bg: RGBAOption::new(&settings, "lscm-black-bg"),
            lscm_red_fg: RGBAOption::new(&settings, "lscm-red-fg"),
            lscm_red_bg: RGBAOption::new(&settings, "lscm-red-bg"),
            lscm_green_fg: RGBAOption::new(&settings, "lscm-green-fg"),
            lscm_green_bg: RGBAOption::new(&settings, "lscm-green-bg"),
            lscm_yellow_fg: RGBAOption::new(&settings, "lscm-yellow-fg"),
            lscm_yellow_bg: RGBAOption::new(&settings, "lscm-yellow-bg"),
            lscm_blue_fg: RGBAOption::new(&settings, "lscm-blue-fg"),
            lscm_blue_bg: RGBAOption::new(&settings, "lscm-blue-bg"),
            lscm_magenta_fg: RGBAOption::new(&settings, "lscm-magenta-fg"),
            lscm_magenta_bg: RGBAOption::new(&settings, "lscm-magenta-bg"),
            lscm_cyan_fg: RGBAOption::new(&settings, "lscm-cyan-fg"),
            lscm_cyan_bg: RGBAOption::new(&settings, "lscm-cyan-bg"),
            lscm_white_fg: RGBAOption::new(&settings, "lscm-white-fg"),
            lscm_white_bg: RGBAOption::new(&settings, "lscm-white-bg"),
        }
    }

    pub fn custom_theme(&self) -> ColorTheme {
        load_custom_theme(self)
    }

    pub fn set_custom_theme(&self, theme: &ColorTheme) -> WriteResult {
        save_custom_theme(theme, self)
    }

    pub fn ls_color_palette(&self) -> LsColorsPalette {
        load_palette(self)
    }

    pub fn set_ls_color_palette(&self, palette: &LsColorsPalette) -> WriteResult {
        save_palette(palette, self)
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
            confirm_delete: BoolOption::new(&settings, "delete"),
            confirm_delete_default: EnumOption::new(&settings, "delete-default"),
            confirm_copy_overwrite: EnumOption::new(&settings, "copy-overwrite"),
            confirm_move_overwrite: EnumOption::new(&settings, "move-overwrite"),
            dnd_mode: EnumOption::new(&settings, "mouse-drag-and-drop"),
        }
    }
}

u32_enum! {
    pub enum DeleteDefault {
        #[default]
        Cancel,
        Delete,
    }
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
            hide_unknown: BoolOption::new(&settings, "hide-unknown"),
            hide_regular: BoolOption::new(&settings, "hide-regular"),
            hide_directory: BoolOption::new(&settings, "hide-directory"),
            hide_special: BoolOption::new(&settings, "hide-special"),
            hide_shortcut: BoolOption::new(&settings, "hide-shortcut"),
            hide_mountable: BoolOption::new(&settings, "hide-mountable"),
            hide_virtual: BoolOption::new(&settings, "hide-virtual"),
            hide_volatile: BoolOption::new(&settings, "hide-volatile"),
            hide_hidden: BoolOption::new(&settings, "hide-dotfile"),
            hide_backup: BoolOption::new(&settings, "hide-backupfiles"),
            hide_symlink: BoolOption::new(&settings, "hide-symlink"),
            backup_pattern: StringOption::new(&settings, "backup-pattern"),
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
}

impl ProgramsOptions {
    pub fn new() -> Self {
        let settings = gio::Settings::new("org.gnome.gnome-commander.preferences.programs");
        Self {
            dont_download: BoolOption::new(&settings, "dont-download"),
            use_internal_viewer: BoolOption::new(&settings, "use-internal-viewer"),
            viewer_cmd: StringOption::new(&settings, "viewer-cmd"),
            editor_cmd: StringOption::new(&settings, "editor-cmd"),
            differ_cmd: StringOption::new(&settings, "differ-cmd"),
            use_internal_search: BoolOption::new(&settings, "use-internal-search"),
            search_cmd: StringOption::new(&settings, "search-cmd"),
            sendto_cmd: StringOption::new(&settings, "sendto-cmd"),
            terminal_cmd: StringOption::new(&settings, "terminal-cmd"),
            terminal_exec_cmd: StringOption::new(&settings, "terminal-exec-cmd"),
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
        self.default_profile().path_syntax()
    }

    pub fn set_default_profile_syntax(&self, pattern_type: PatternType) {
        self.default_profile().set_path_syntax(pattern_type)
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

        // Reset legacy setting, making sure we won’t keep adding legacy profiles if the user
        // doesn’t have any search profiles.
        let _ = settings
            .legacy_search_profiles
            .set(Vec::<LegacySearchProfileVariant>::new());

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
            for v in iter {
                let p = SearchProfile::default();
                p.load(v);
                self.profiles().append(&p);
            }
        } else {
            // No search profiles, we might have the legacy setting to migrate.
            self.load_legacy(settings);
        }

        for item in settings.search_pattern_history.get().iter().rev() {
            self.name_patterns.add(item.to_string());
        }
        for item in settings.search_text_history.get().iter().rev() {
            self.content_patterns.add(item.to_string());
        }
    }

    fn load_legacy(&self, settings: &GeneralOptions) {
        let mut iter = settings.legacy_search_profiles.get().into_iter();
        if let Some(v) = iter.next() {
            self.default_profile().load_legacy(v);
            for v in iter {
                let p = SearchProfile::default();
                p.load_legacy(v);
                self.profiles().append(&p);
            }
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
            quick_connect_uri: StringOption::new(&settings, "quick-connect-uri"),
        }
    }
}

pub struct ViewerOptions {
    pub window_width: U32Option,
    pub window_height: U32Option,

    pub encoding: StringOption,
    pub monospaced_font: StringOption,
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
            window_width: U32Option::new(&settings, "window-width"),
            window_height: U32Option::new(&settings, "window-height"),
            encoding: StringOption::new(&settings, "charset"),
            monospaced_font: StringOption::new(&settings, "monospaced-font"),
            tab_size: U32Option::new(&settings, "tab-size"),
            wrap_mode: BoolOption::new(&settings, "wrap-mode"),
            binary_bytes_per_line: U32Option::new(&settings, "binary-bytes-per-line"),
            display_hex_offset: BoolOption::new(&settings, "display-hex-offset"),
            metadata_visible: BoolOption::new(&settings, "metadata-visible"),
            search_pattern_text: StrvOption::new(&settings, "search-pattern-text"),
            search_pattern_hex: StrvOption::new(&settings, "search-pattern-hex"),
            case_sensitive_search: BoolOption::new(&settings, "case-sensitive-search"),
            search_mode: EnumOption::new(&settings, "search-mode"),
        }
    }

    pub fn add_to_history(&self, text: &str, mode: Mode) -> WriteResult {
        const INTVIEWER_HISTORY_SIZE: usize = 16;

        let option = match mode {
            Mode::Text => &self.search_pattern_text,
            Mode::Binary => &self.search_pattern_hex,
        };

        let mut history = option.get();
        if history.first().is_some_and(|s| s == text) {
            return Ok(());
        }
        history.insert(0, text.to_owned());
        history.truncate(INTVIEWER_HISTORY_SIZE);
        option.set(history)
    }
}
