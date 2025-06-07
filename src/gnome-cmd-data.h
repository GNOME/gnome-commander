/**
 * @file gnome-cmd-data.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <vector>
#include <map>
#include <string>
#include <glib-object.h>
#include <glib.h>
#include <config.h>
#include <stdio.h>
#include <gio/gio.h>


#include "gnome-cmd-types.h"
#include "gnome-cmd-file-list.h"
#include "filter.h"
#include "history.h"

#include <tuple>

struct GnomeCmdMainWin;

#define GCMD_TYPE_SETTINGS (gcmd_settings_get_type ())

G_DECLARE_FINAL_TYPE (GcmdSettings, gcmd_settings, GCMD, SETTINGS, GObject)

GcmdSettings *gcmd_settings_new ();

GSettings *gcmd_settings_get_general (GcmdSettings *);

/* *************
 * KEY CONSTANTS
 * *************
 * Attention: When changing the key names here,
 * be sure to change them also in gnome-cmd-data.c!
 */
#define GCMD_PREF_GENERAL                             "org.gnome.gnome-commander.preferences.general"
#define GCMD_SETTINGS_PANEL_FONT                      "panel-font"
#define GCMD_SETTINGS_SYSTEM_FONT                     "monospace-font-name"
#define GCMD_SETTINGS_SIZE_DISP_MODE                  "size-display-mode"
#define GCMD_SETTINGS_PERM_DISP_MODE                  "perm-display-mode"
#define GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE           "graphical-layout-mode"
#define GCMD_SETTINGS_LIST_ROW_HEIGHT                 "list-row-height"
#define GCMD_SETTINGS_DATE_DISP_FORMAT                "date-disp-format"
#define GCMD_SETTINGS_LIST_FONT                       "list-font"
#define GCMD_SETTINGS_EXT_DISP_MODE                   "extension-display-mode"
#define GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM             "clicks-to-open-item"
#define GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS     "left-mouse-btn-unselects"
#define GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE         "right-mouse-btn-mode"
#define GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE        "middle-mouse-btn-mode"
#define GCMD_SETTINGS_USE_TRASH                       "delete-to-trash"
#define GCMD_SETTINGS_ICON_SIZE                       "icon-size"
#define GCMD_SETTINGS_ICON_SCALE_QUALITY              "icon-scale-quality"
#define GCMD_SETTINGS_MIME_ICON_DIR                   "mime-icon-dir"
#define GCMD_SETTINGS_CMDLINE_HISTORY                 "cmdline-history"
#define GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH          "cmdline-history-length"
#define GCMD_SETTINGS_HORIZONTAL_ORIENTATION          "horizontal-orientation"
#define GCMD_SETTINGS_SHOW_DEVBUTTONS                 "show-devbuttons"
#define GCMD_SETTINGS_SHOW_DEVLIST                    "show-devlist"
#define GCMD_SETTINGS_SHOW_CMDLINE                    "show-cmdline"
#define GCMD_SETTINGS_SHOW_TOOLBAR                    "show-toolbar"
#define GCMD_SETTINGS_SHOW_BUTTONBAR                  "show-buttonbar"
#define GCMD_SETTINGS_GUI_UPDATE_RATE                 "gui-update-rate"
#define GCMD_SETTINGS_SYMLINK_PREFIX                  "symlink-string"
#define GCMD_SETTINGS_SAVE_DIRS_ON_EXIT               "save-dirs-on-exit"
#define GCMD_SETTINGS_SAVE_TABS_ON_EXIT               "save-tabs-on-exit"
#define GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT        "save-dir-history-on-exit"
#define GCMD_SETTINGS_SAVE_CMDLINE_HISTORY_ON_EXIT    "save-cmdline-history-on-exit"
#define GCMD_SETTINGS_SAVE_SEARCH_HISTORY_ON_EXIT     "save-search-history-on-exit"
#define GCMD_SETTINGS_ALWAYS_SHOW_TABS                "always-show-tabs"
#define GCMD_SETTINGS_TAB_LOCK_INDICATOR              "tab-lock-indicator"
#define GCMD_SETTINGS_MAIN_WIN_STATE                  "main-win-state"
#define GCMD_SETTINGS_SELECT_DIRS                     "select-dirs"
#define GCMD_SETTINGS_CASE_SENSITIVE                  "case-sensitive"
#define GCMD_SETTINGS_SYMBOLIC_LINKS_AS_REG_FILES     "symbolic-links-as-regular-files"
#define GCMD_SETTINGS_MULTIPLE_INSTANCES              "allow-multiple-instances"
#define GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN  "quick-search-exact-match-begin"
#define GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END    "quick-search-exact-match-end"
#define GCMD_SETTINGS_DEV_ONLY_ICON                   "dev-only-icon"
#define GCMD_SETTINGS_SHOW_SAMBA_WORKGROUP_BUTTON     "show-samba-workgroup-button"
#define GCMD_SETTINGS_MAINMENU_VISIBILITY             "mainmenu-visibility"
#define GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT           "quick-search"
#define GCMD_SETTINGS_MAIN_WIN_WIDTH                  "main-win-width"
#define GCMD_SETTINGS_MAIN_WIN_HEIGHT                 "main-win-height"
#define GCMD_SETTINGS_OPTS_DIALOG_WIDTH               "opts-dialog-width"
#define GCMD_SETTINGS_OPTS_DIALOG_HEIGHT              "opts-dialog-height"
#define GCMD_SETTINGS_COLUMN_WIDTH_ICON               "column-width-icon"
#define GCMD_SETTINGS_COLUMN_WIDTH_NAME               "column-width-name"
#define GCMD_SETTINGS_COLUMN_WIDTH_EXT                "column-width-ext"
#define GCMD_SETTINGS_COLUMN_WIDTH_DIR                "column-width-dir"
#define GCMD_SETTINGS_COLUMN_WIDTH_SIZE               "column-width-size"
#define GCMD_SETTINGS_COLUMN_WIDTH_DATE               "column-width-date"
#define GCMD_SETTINGS_COLUMN_WIDTH_PERM               "column-width-perm"
#define GCMD_SETTINGS_COLUMN_WIDTH_OWNER              "column-width-owner"
#define GCMD_SETTINGS_COLUMN_WIDTH_GROUP              "column-width-group"
#define GCMD_SETTINGS_ADVRENAME_TOOL_WIDTH            "advrename-win-width"
#define GCMD_SETTINGS_ADVRENAME_TOOL_HEIGHT           "advrename-win-height"
#define GCMD_SETTINGS_ADVRENAME_TOOL_TEMPLATE_HISTORY "advrename-template-history"
#define GCMD_SETTINGS_ADVRENAME_PROFILES              "advrename-profiles"
#define GCMD_SETTINGS_ADVRENAME_PROFILE_FORMAT_STRING "(ssuiuuuasasab)"
#define GCMD_SETTINGS_ADVRENAME_PROFILES_FORMAT_STRING "a(ssuiuuuasasab)"
#define GCMD_SETTINGS_FILE_LIST_TABS                  "file-list-tabs"
#define GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING     "(syybb)"
// deprecated after v1.18.0
#define GCMD_SETTINGS_DEVICES                         "devices"
// deprecated after v1.18.0
#define GCMD_SETTINGS_DEVICES_FORMAT_STRING           "(ssss)"
#define GCMD_SETTINGS_DEVICE_LIST                     "device-list"
#define GCMD_SETTINGS_DEVICE_LIST_FORMAT_STRING       "(sssv)"
#define GCMD_SETTINGS_FAV_APPS                        "favorite-apps"
#define GCMD_SETTINGS_FAV_APPS_FORMAT_STRING          "(ssssubbb)"
#define GCMD_SETTINGS_DIRECTORY_HISTORY               "directory-history"
#define GCMD_SETTINGS_SEARCH_WIN_WIDTH                "search-win-width"
#define GCMD_SETTINGS_SEARCH_WIN_HEIGHT               "search-win-height"
#define GCMD_SETTINGS_SEARCH_WIN_IS_TRANSIENT         "search-win-is-transient"
#define GCMD_SETTINGS_SEARCH_PATTERN_HISTORY          "search-pattern-history"
#define GCMD_SETTINGS_SEARCH_TEXT_HISTORY             "search-text-history"
#define GCMD_SETTINGS_SEARCH_PROFILES                 "search-profiles"
#define GCMD_SETTINGS_SEARCH_PROFILE_FORMAT_STRING    "(siisbbs)"
#define GCMD_SETTINGS_SEARCH_PROFILES_FORMAT_STRING   "a(siisbbs)"
#define GCMD_SETTINGS_BOOKMARKS                       "bookmarks"
#define GCMD_SETTINGS_BOOKMARK_FORMAT_STRING          "(bsss)"
#define GCMD_SETTINGS_BOOKMARKS_FORMAT_STRING         "a(bsss)"
#define GCMD_SETTINGS_CONNECTIONS                     "connections"
#define GCMD_SETTINGS_CONNECTION_FORMAT_STRING        "(ss)"
#define GCMD_SETTINGS_KEYBINDINGS                     "keybindings"
#define GCMD_SETTINGS_KEYBINDING_FORMAT_STRING        "(sssbbbbbb)"

#define GCMD_PREF_FILTER                              "org.gnome.gnome-commander.preferences.filter"
#define GCMD_SETTINGS_FILTER_HIDE_UNKNOWN             "hide-unknown"
#define GCMD_SETTINGS_FILTER_HIDE_REGULAR             "hide-regular"
#define GCMD_SETTINGS_FILTER_HIDE_DIRECTORY           "hide-directory"
#define GCMD_SETTINGS_FILTER_HIDE_SYMLINK             "hide-symlink"
#define GCMD_SETTINGS_FILTER_HIDE_SPECIAL             "hide-special"
#define GCMD_SETTINGS_FILTER_HIDE_SHORTCUT            "hide-shortcut"
#define GCMD_SETTINGS_FILTER_HIDE_MOUNTABLE           "hide-mountable"
#define GCMD_SETTINGS_FILTER_HIDE_VIRTUAL             "hide-virtual"
#define GCMD_SETTINGS_FILTER_HIDE_VOLATILE            "hide-volatile"
#define GCMD_SETTINGS_FILTER_HIDE_HIDDEN              "hide-dotfile"
#define GCMD_SETTINGS_FILTER_HIDE_BACKUPS             "hide-backupfiles"
#define GCMD_SETTINGS_FILTER_BACKUP_PATTERN           "backup-pattern"

#define GCMD_PREF_CONFIRM                             "org.gnome.gnome-commander.preferences.confirmations"
#define GCMD_SETTINGS_CONFIRM_DELETE                  "delete"
#define GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT          "delete-default"
#define GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE          "copy-overwrite"
#define GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE          "move-overwrite"
#define GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP     "mouse-drag-and-drop"

#define GCMD_PREF_COLORS                              "org.gnome.gnome-commander.preferences.colors"
#define GCMD_SETTINGS_COLORS_THEME                    "theme"
#define GCMD_SETTINGS_COLORS_USE_LS_COLORS            "use-ls-colors"

#define GCMD_PREF_PROGRAMS                            "org.gnome.gnome-commander.preferences.programs"
#define GCMD_SETTINGS_DONT_DOWNLOAD                   "dont-download"
#define GCMD_SETTINGS_USE_INTERNAL_VIEWER             "use-internal-viewer"
#define GCMD_SETTINGS_VIEWER_CMD                      "viewer-cmd"
#define GCMD_SETTINGS_EDITOR_CMD                      "editor-cmd"
#define GCMD_SETTINGS_DIFFER_CMD                      "differ-cmd"
#define GCMD_SETTINGS_USE_INTERNAL_SEARCH             "use-internal-search"
#define GCMD_SETTINGS_SEARCH_CMD                      "search-cmd"
#define GCMD_SETTINGS_SENDTO_CMD                      "sendto-cmd"
#define GCMD_SETTINGS_TERMINAL_CMD                    "terminal-cmd"
#define GCMD_SETTINGS_TERMINAL_EXEC_CMD               "terminal-exec-cmd"
#define GCMD_SETTINGS_USE_GCMD_BLOCK                  "use-gcmd-block"

#define GCMD_PREF_NETWORK                             "org.gnome.gnome-commander.preferences.network"
#define GCMD_SETTINGS_QUICK_CONNECT_URI               "quick-connect-uri"

//gKeyFile constants
#define DEVICES_FILENAME                              "devices"
#define DEVICES_DEVICE                                "device"
#define DEVICES_MOUNT_POINT                           "mount_point"
#define DEVICES_ICON_PATH                             "icon_path"
#define FAV_APPS_FILENAME                             "fav-apps"
#define FAV_APPS_CMD                                  "cmd"
#define FAV_APPS_ICON                                 "icon"
#define FAV_APPS_PATTERN                              "pattern"
#define FAV_APPS_TARGET                               "target"
#define FAV_APPS_HANDLES_URIS                         "handles_uris"
#define FAV_APPS_HANDLES_MULTIPLE                     "handles_multiple"
#define FAV_APPS_REQUIRES_TERMINAL                    "requires_terminal"
#define TABS_LAYOUT_FILENAME                          "file-list-tabs"
#define TAB_PATH                                      "path"
#define TAB_FILESLECTORID                             "fileSelectorId"
#define TAB_SORT                                      "sort"
#define TAB_ASC                                       "asc"
#define TAB_LOCK                                      "lock"


extern "C" GType gnome_cmd_advanced_rename_profile_get_type ();
struct AdvancedRenameProfile;
extern "C" void gnome_cmd_advanced_rename_profile_copy_from (AdvancedRenameProfile *dst, AdvancedRenameProfile *src);
extern "C" void gnome_cmd_advanced_rename_profile_reset (AdvancedRenameProfile *profile);


struct GnomeCmdConRemote;

extern "C" GType gnome_cmd_search_profile_get_type();
struct SearchProfile;
extern "C" void gnome_cmd_search_profile_copy_from(SearchProfile *dst, SearchProfile *src);

struct GnomeCmdData
{
    typedef enum
    {
        LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK,
        LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK
    }LeftMouseButtonMode;

    typedef enum
    {
        MIDDLE_BUTTON_GOES_UP_DIR,
        MIDDLE_BUTTON_OPENS_NEW_TAB
    }MiddleMouseButtonMode;

    typedef enum
    {
        RIGHT_BUTTON_POPUPS_MENU,
        RIGHT_BUTTON_SELECTS
    }RightMouseButtonMode;

    typedef enum
    {
        TAB_LOCK_ICON,
        TAB_LOCK_ASTERISK,
        TAB_LOCK_STYLED_TEXT
    }TabLockIndicator;

    enum {SEARCH_HISTORY_SIZE=10, ADVRENAME_HISTORY_SIZE=10};

    typedef enum
    {
        G_FILE_IS_UNKNOWN,
        G_FILE_IS_REGULAR,
        G_FILE_IS_DIR,
        G_FILE_IS_SYMLINK,
        G_FILE_IS_SPECIAL,
        G_FILE_IS_SHORTCUT,
        G_FILE_IS_MOUNTABLE,
        G_FILE_IS_HIDDEN,
        G_FILE_IS_BACKUP,
        G_FILE_IS_VIRTUAL,
        G_FILE_IS_VOLATILE
    }GcmdFileType;

    struct FilterSettings
    {
        gboolean file_types[11];

        FilterSettings()
        {
            memset(file_types, 0, sizeof(file_types));
        }
    };

    struct Options
    {
      public:
        GcmdSettings                 *gcmd_settings;
        //  General
        LeftMouseButtonMode          left_mouse_button_mode;
        gboolean                     left_mouse_button_unselects;
        MiddleMouseButtonMode        middle_mouse_button_mode;
        RightMouseButtonMode         right_mouse_button_mode;
        gboolean                     select_dirs;
        gboolean                     case_sens_sort;
        GnomeCmdQuickSearchShortcut  quick_search;
        gboolean                     quick_search_exact_match_begin;
        gboolean                     quick_search_exact_match_end;
        gboolean                     allow_multiple_instances;
        gboolean                     save_dirs_on_exit;
        gboolean                     save_tabs_on_exit;
        gboolean                     save_dir_history_on_exit;
        gboolean                     save_cmdline_history_on_exit;
        gboolean                     save_search_history_on_exit;
        gboolean                     search_window_is_transient {true};
        gboolean                     deleteToTrash;
        // Format
        GnomeCmdSizeDispMode         size_disp_mode;
        GnomeCmdPermDispMode         perm_disp_mode;
        GnomeCmdDateFormat           date_format;           // NOTE: internally stored as locale (which not always defaults to UTF8), needs converting from/to UTF8 for editing and cfg load/save
        //  Layout
        gchar                       *list_font;
        gint                         list_row_height;
        GnomeCmdExtDispMode          ext_disp_mode;
        GnomeCmdLayout               layout;
        gboolean                     use_ls_colors;
        guint                        icon_size;
        GdkInterpType                icon_scale_quality;
        gchar                       *theme_icon_dir;
        //  Confirmation
        gboolean                     confirm_delete;
        GtkButtonsType               confirm_delete_default;
        GnomeCmdConfirmOverwriteMode confirm_copy_overwrite;
        GnomeCmdConfirmOverwriteMode confirm_move_overwrite;
        GnomeCmdDefaultDndMode       mouse_dnd_default;
        //  Filters
        gboolean                     symbolic_links_as_regular_files;
        //  Programs
        gboolean                     honor_expect_uris;
        gchar                       *viewer;
        gboolean                     use_internal_viewer;
        gchar                       *editor;
        gchar                       *differ;
        gboolean                     use_internal_search;
        gchar                       *search;
        gchar                       *sendto;
        gchar                       *termopen;
        gchar                       *termexec;
        //  Devices
        gboolean                     device_only_icon;
        gboolean                     show_samba_workgroups_button;

        Options(): gcmd_settings(nullptr),
                   left_mouse_button_mode(LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK),
                   left_mouse_button_unselects(TRUE),
                   middle_mouse_button_mode(MIDDLE_BUTTON_GOES_UP_DIR),
                   right_mouse_button_mode(RIGHT_BUTTON_POPUPS_MENU),
                   select_dirs(TRUE),
                   case_sens_sort(TRUE),
                   quick_search(GNOME_CMD_QUICK_SEARCH_CTRL_ALT),
                   quick_search_exact_match_begin(TRUE),
                   quick_search_exact_match_end(FALSE),
                   allow_multiple_instances(FALSE),
                   save_dirs_on_exit(FALSE),
                   save_tabs_on_exit(TRUE),
                   save_dir_history_on_exit(TRUE),
                   save_cmdline_history_on_exit(TRUE),
                   save_search_history_on_exit(TRUE),
                   deleteToTrash(TRUE),
                   size_disp_mode(GNOME_CMD_SIZE_DISP_MODE_POWERED),
                   perm_disp_mode(GNOME_CMD_PERM_DISP_MODE_TEXT),
                   date_format(nullptr),
                   list_font(nullptr),
                   list_row_height(16),
                   ext_disp_mode(GNOME_CMD_EXT_DISP_BOTH),
                   layout(GNOME_CMD_LAYOUT_MIME_ICONS),
                   use_ls_colors(FALSE),
                   icon_size(16),
                   icon_scale_quality(GDK_INTERP_HYPER),
                   theme_icon_dir(nullptr),
                   confirm_delete(TRUE),
                   confirm_delete_default(GTK_BUTTONS_OK),
                   confirm_copy_overwrite(GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                   confirm_move_overwrite(GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                   mouse_dnd_default(GNOME_CMD_DEFAULT_DND_QUERY),
                   honor_expect_uris(FALSE),
                   viewer(nullptr),
                   use_internal_viewer(TRUE),
                   editor(nullptr),
                   differ(nullptr),
                   use_internal_search(TRUE),
                   search(nullptr),
                   sendto(nullptr),
                   termopen(nullptr),
                   termexec(nullptr),
                   device_only_icon(FALSE),
                   show_samba_workgroups_button(FALSE)
        {
        }

        Options(const Options &cfg);

        ~Options()
        {
            g_free (date_format);
            g_free (list_font);
            g_free (theme_icon_dir);
            g_free (viewer);
            g_free (editor);
            g_free (differ);
            g_free (search);
            g_free (sendto);
            g_free (termopen);
            g_free (termexec);
        }

        Options &operator = (const Options &cfg);

        GnomeCmdColorMode color_mode();
        void set_color_mode(GnomeCmdColorMode color_mode);

        void set_date_format(const GnomeCmdDateFormat format)
        {
            g_free (date_format);
            date_format = g_strdup (format);
        }

        void set_list_font(const gchar *font)
        {
            g_free (list_font);
            list_font = g_strdup (font);
        }

        void set_theme_icon_dir(const gchar *dir)
        {
            g_free (theme_icon_dir);
            theme_icon_dir = g_strdup (dir);
        }

        void set_viewer(const gchar *command)
        {
            g_free (viewer);
            viewer = g_strdup (command);
        }

        void set_editor(const gchar *command)
        {
            g_free (editor);
            editor = g_strdup (command);
        }

        void set_differ(const gchar *command)
        {
            g_free (differ);
            differ = g_strdup (command);
        }

        void set_search(const gchar *command)
        {
            g_free (search);
            search = g_strdup (command);
        }

        void set_sendto(const gchar *command)
        {
            g_free (sendto);
            sendto = g_strdup (command);
        }

        void set_termexec(const gchar *command)
        {
            g_free (termexec);
            termexec = g_strdup (command);
        }

        void set_termopen(const gchar *command)
        {
            g_free (termopen);
            termopen = g_strdup (command);
        }

        void on_size_display_mode_changed();
    };

    struct SearchConfig
    {
        SearchProfile *default_profile;

        History name_patterns;
        History content_patterns;

        GListStore *profiles; // SearchProfile's

        explicit SearchConfig():
            name_patterns(SEARCH_HISTORY_SIZE),
            content_patterns(SEARCH_HISTORY_SIZE)
        {
            default_profile = (SearchProfile *) g_object_new (gnome_cmd_search_profile_get_type (), "name", "Default", nullptr);
            profiles = g_list_store_new (gnome_cmd_search_profile_get_type ());
        }

        ~SearchConfig() {
            g_object_unref(default_profile);
            g_object_unref(profiles);
        };
    };

    static GSettingsSchemaSource* GetGlobalSchemaSource();

    struct Private;

    Private *priv {nullptr};

  private:

    void load_auto_load_plugins();
    void load_cmdline_history();
    void load_directory_history();
    gboolean save_auto_load_plugins();
    void load_connections();
    void save_connections();
    void save_cmdline_history(GnomeCmdMainWin *main_win);
    void save_directory_history();
    void save_search_history();
    void save_devices();
    gboolean add_bookmark_to_gvariant_builder(GVariantBuilder *builder, std::string bookmarkName, GnomeCmdCon *con);

  public:

    Options                      options;
    GcmdSettings                 *settings {nullptr};

    SearchConfig                 search_defaults;

    guint                        gui_update_rate;

    GStrv                        cmdline_history;
    gint                         cmdline_history_length;
    GList                       *get_list_from_gsettings_string_array (GSettings *settings, const gchar *key);
    gboolean                     set_gsettings_string_array_from_glist (GSettings *settings, const gchar *key, GList *strings);

    gboolean                     use_gcmd_block;

    guint                        opts_dialog_width;
    guint                        opts_dialog_height;

    GnomeCmdData();

    ~GnomeCmdData();

    void load();
    void load_devices();
    void gsettings_init();
    void connect_signals(GnomeCmdMainWin *main_win);
    void migrate_all_data_to_gsettings();
    void load_more();
    void save_bookmarks();
    void load_bookmarks();
    void save(GnomeCmdMainWin *main_win);
    gboolean set_gsettings_when_changed (GSettings *settings, const char *key, gpointer value);
    gboolean set_gsettings_enum_when_changed (GSettings *settings, const char *key, gint value);
};

gpointer gnome_cmd_data_get_con_list ();

GList *gnome_cmd_data_get_auto_load_plugins ();
void gnome_cmd_data_set_auto_load_plugins (GList *plugins);

extern GnomeCmdData gnome_cmd_data;
