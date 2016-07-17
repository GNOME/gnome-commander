/**
 * @file gnome-cmd-data.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#ifndef __GNOME_CMD_DATA_H__
#define __GNOME_CMD_DATA_H__

#include <vector>
#include <string>
#include <glib-object.h>
#include <glib.h>
#include <config.h>
#include <stdio.h>
#include <gio/gio.h>


#include "gnome-cmd-app.h"
#include "gnome-cmd-types.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-regex.h"
#include "gnome-cmd-xml-config.h"
#include "filter.h"
#include "history.h"
#include "dict.h"
#include "tuple.h"

#define GCMD_TYPE_SETTINGS (gcmd_settings_get_type ())

G_DECLARE_FINAL_TYPE (GcmdSettings, gcmd_settings, GCMD, SETTINGS, GObject)

GcmdSettings *gcmd_settings_new (void);

/* *************
 * KEY CONSTANTS
 * *************
 * Attention: When changing the key names here,
 * be sure to change them also in gnome-cmd-data.c!
 */
#define GCMD_PREF_GENERAL                             "org.gnome.gnome-commander.preferences.general"
#define GCMD_SETTINGS_USE_DEFAULT_FONT                "use-default-font"
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
#define GCMD_SETTINGS_ICON_SIZE                       "icon-size"
#define GCMD_SETTINGS_DEV_ICON_SIZE                   "dev-icon-size"
#define GCMD_SETTINGS_ICON_SCALE_QUALITY              "icon-scale-quality"
#define GCMD_SETTINGS_MIME_ICON_DIR                   "mime-icon-dir"
#define GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH          "cmdline-history-length"
#define GCMD_SETTINGS_HORIZONTAL_ORIENTATION          "horizontal-orientation"
#define GCMD_SETTINGS_SHOW_DEVBUTTONS                 "show-devbuttons"
#define GCMD_SETTINGS_SHOW_DEVLIST                    "show-devlist"
#define GCMD_SETTINGS_SHOW_CMDLINE                    "show-cmdline"
#define GCMD_SETTINGS_SHOW_TOOLBAR                    "show-toolbar"
#define GCMD_SETTINGS_SHOW_BUTTONBAR                  "show-buttonbar"
#define GCMD_SETTINGS_GUI_UPDATE_RATE                 "gui-update-rate"
#define GCMD_SETTINGS_SYMLINK_PREFIX                  "symlink-string"
#define GCMD_SETTINGS_MAIN_WIN_POS_X                  "main-win-pos-x"
#define GCMD_SETTINGS_MAIN_WIN_POS_Y                  "main-win-pos-y"
#define GCMD_SETTINGS_SAVE_DIRS_ON_EXIT               "save-dirs-on-exit"
#define GCMD_SETTINGS_SAVE_TABS_ON_EXIT               "save-tabs-on-exit"
#define GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT        "save-dir-history-on-exit"
#define GCMD_SETTINGS_ALWAYS_SHOW_TABS                "always-show-tabs"
#define GCMD_SETTINGS_TAB_LOCK_INDICATOR              "tab-lock-indicator"
#define GCMD_SETTINGS_MAIN_WIN_STATE                  "main-win-state"
#define GCMD_SETTINGS_SELECT_DIRS                     "select-dirs"
#define GCMD_SETTINGS_CASE_SENSITIVE                  "case-sensitive"
#define GCMD_SETTINGS_MULTIPLE_INSTANCES              "allow-multiple-instances"
#define GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN  "quick-search-exact-match-begin"
#define GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END    "quick-search-exact-match-end"
#define GCMD_SETTINGS_DEV_SKIP_MOUNTING               "dev-skip-mounting"
#define GCMD_SETTINGS_DEV_ONLY_ICON                   "dev-only-icon"
#define GCMD_SETTINGS_MAINMENU_VISIBILITY             "mainmenu-visibility"

#define GCMD_PREF_FILTER                              "org.gnome.gnome-commander.preferences.filter"
#define GCMD_SETTINGS_FILTER_HIDE_UNKNOWN             "hide-unknown"
#define GCMD_SETTINGS_FILTER_HIDE_REGULAR             "hide-regular"
#define GCMD_SETTINGS_FILTER_HIDE_DIRECTORY           "hide-directory"
#define GCMD_SETTINGS_FILTER_HIDE_FIFO                "hide-fifo"
#define GCMD_SETTINGS_FILTER_HIDE_SOCKET              "hide-socket"
#define GCMD_SETTINGS_FILTER_HIDE_CHARACTER_DEVICE    "hide-char-device"
#define GCMD_SETTINGS_FILTER_HIDE_BLOCK_DEVICE        "hide-block-device"
#define GCMD_SETTINGS_FILTER_HIDE_SYMBOLIC_LINK       "hide-symbolic-link"
#define GCMD_SETTINGS_FILTER_DOTFILE                  "hide-dotfile"
#define GCMD_SETTINGS_FILTER_BACKUP                   "hide-backup-files"
#define GCMD_SETTINGS_FILTER_BACKUP_PATTERN           "backup-pattern"

#define GCMD_PREF_CONFIRM                             "org.gnome.gnome-commander.preferences.confirmations"
#define GCMD_SETTINGS_CONFIRM_DELETE                  "delete"
#define GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT          "delete-default"
#define GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE          "copy-overwrite"
#define GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE          "move-overwrite"
#define GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP     "mouse-drag-and-drop"

#define GCMD_PREF_COLORS                              "org.gnome.gnome-commander.preferences.colors"
#define GCMD_SETTINGS_COLORS_THEME                    "theme"
#define GCMD_SETTINGS_COLORS_NORM_FG                  "custom-norm-fg"
#define GCMD_SETTINGS_COLORS_NORM_BG                  "custom-norm-bg"
#define GCMD_SETTINGS_COLORS_ALT_FG                   "custom-alt-fg"
#define GCMD_SETTINGS_COLORS_ALT_BG                   "custom-alt-bg"
#define GCMD_SETTINGS_COLORS_SEL_FG                   "custom-sel-fg"
#define GCMD_SETTINGS_COLORS_SEL_BG                   "custom-sel-bg"
#define GCMD_SETTINGS_COLORS_CURS_FG                  "custom-curs-fg"
#define GCMD_SETTINGS_COLORS_CURS_BG                  "custom-curs-bg"
#define GCMD_SETTINGS_COLORS_USE_LS_COLORS            "use-ls-colors"
#define GCMD_SETTINGS_LS_COLORS_BLACK_FG              "lscm-black-fg"
#define GCMD_SETTINGS_LS_COLORS_BLACK_BG              "lscm-black-bg"
#define GCMD_SETTINGS_LS_COLORS_RED_FG                "lscm-red-fg"
#define GCMD_SETTINGS_LS_COLORS_RED_BG                "lscm-red-bg"
#define GCMD_SETTINGS_LS_COLORS_GREEN_FG              "lscm-green-fg"
#define GCMD_SETTINGS_LS_COLORS_GREEN_BG              "lscm-green-bg"
#define GCMD_SETTINGS_LS_COLORS_YELLOW_FG             "lscm-yellow-fg"
#define GCMD_SETTINGS_LS_COLORS_YELLOW_BG             "lscm-yellow-bg"
#define GCMD_SETTINGS_LS_COLORS_BLUE_FG               "lscm-blue-fg"
#define GCMD_SETTINGS_LS_COLORS_BLUE_BG               "lscm-blue-bg"
#define GCMD_SETTINGS_LS_COLORS_MAGENTA_FG            "lscm-magenta-fg"
#define GCMD_SETTINGS_LS_COLORS_MAGENTA_BG            "lscm-magenta-bg"
#define GCMD_SETTINGS_LS_COLORS_CYAN_FG               "lscm-cyan-fg"
#define GCMD_SETTINGS_LS_COLORS_CYAN_BG               "lscm-cyan-bg"
#define GCMD_SETTINGS_LS_COLORS_WHITE_FG              "lscm-white-fg"
#define GCMD_SETTINGS_LS_COLORS_WHITE_BG              "lscm-white-bg"

#define GCMD_PREF_PROGRAMS                            "org.gnome.gnome-commander.preferences.programs"
#define GCMD_SETTINGS_DONT_DOWNLOAD                   "dont-download"
#define GCMD_SETTINGS_USE_INTERNAL_VIEWER             "use-internal-viewer"
#define GCMD_SETTINGS_VIEWER_CMD                      "viewer-cmd"
#define GCMD_SETTINGS_EDITOR_CMD                      "editor-cmd"
#define GCMD_SETTINGS_DIFFER_CMD                      "differ-cmd"
#define GCMD_SETTINGS_SENDTO_CMD                      "sendto-cmd"
#define GCMD_SETTINGS_TERMINAL_CMD                    "terminal-cmd"
#define GCMD_SETTINGS_TERMINAL_EXEC_CMD               "terminal-exec-cmd"
#define GCMD_SETTINGS_USE_GCMD_BLOCK                  "use-gcmd-block"

#define GCMD_PREF_KEYBINDINGS                         "org.gnome.gnome-commander.preferences.keybindings"
#define GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT           "quick-search"

#define GCMD_PREF_NETWORK                             "org.gnome.gnome-commander.preferences.network"
#define GCMD_SETTINGS_QUICK_CONNECT_URI               "quick-connect-uri"
#define GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD          "ftp-anonymous-password"

#define GCMD_PREF_INTERNAL_VIEWER                     "org.gnome.gnome-commander.preferences.internal-viewer"
#define GCMD_SETTINGS_IV_CASE_SENSITIVE               "case-sensitive-search"
#define GCMD_SETTINGS_IV_SEARCH_MODE                  "search-mode"

struct GnomeCmdConRemote;

struct GnomeCmdData
{
    enum LeftMouseButtonMode
    {
        LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK,
        LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK
    };

    enum MiddleMouseButtonMode
    {
        MIDDLE_BUTTON_GOES_UP_DIR,
        MIDDLE_BUTTON_OPENS_NEW_TAB
    };

    enum RightMouseButtonMode
    {
        RIGHT_BUTTON_POPUPS_MENU,
        RIGHT_BUTTON_SELECTS
    };

    enum TabLockIndicator
    {
        TAB_LOCK_ICON,
        TAB_LOCK_ASTERISK,
        TAB_LOCK_STYLED_TEXT
    };

    enum {SEARCH_HISTORY_SIZE=10, ADVRENAME_HISTORY_SIZE=10, INTVIEWER_HISTORY_SIZE=16};

    struct FilterSettings
    {
        gboolean file_types[8];

        gboolean hidden;
        gboolean backup;

        FilterSettings()
        {
            memset(file_types, 0, sizeof(file_types));
            hidden = TRUE;
            backup = TRUE;
        }
    };

    struct Options
    {
      // private:
        GnomeCmdColorTheme           color_themes[GNOME_CMD_NUM_COLOR_MODES];

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
        gchar                       *symlink_prefix;
        gint                         main_win_pos[2];
        // Format
        GnomeCmdSizeDispMode         size_disp_mode;
        GnomeCmdPermDispMode         perm_disp_mode;
        GnomeCmdDateFormat           date_format;           // NOTE: internally stored as locale (which not always defaults to UTF8), needs converting from/to UTF8 for editing and cfg load/save
        //  Layout
        gchar                       *list_font;
        gint                         list_row_height;
        GnomeCmdExtDispMode          ext_disp_mode;
        GnomeCmdLayout               layout;
        GnomeCmdColorMode            color_mode;
        gboolean                     use_ls_colors;
        GnomeCmdLsColorsPalette      ls_colors_palette;
        guint                        icon_size;
        GdkInterpType                icon_scale_quality;
        gchar                       *theme_icon_dir;
        //  Tabs
        gboolean                     always_show_tabs;
        int                          tab_lock_indicator;
        //  Confirmation
        gboolean                     confirm_delete;
        GtkButtonsType               confirm_delete_default;
        GnomeCmdConfirmOverwriteMode confirm_copy_overwrite;
        GnomeCmdConfirmOverwriteMode confirm_move_overwrite;
        gboolean                     confirm_mouse_dnd;
        //  Filters
        FilterSettings               filter;
        gchar                       *backup_pattern;
        GList                       *backup_pattern_list;
        //  Programs
        gboolean                     honor_expect_uris;
        gchar                       *viewer;
        gboolean                     use_internal_viewer;
        gchar                       *editor;
        gchar                       *differ;
        gchar                       *sendto;
        gchar                       *termopen;
        gchar                       *termexec;
        GList                       *fav_apps;
        //  Devices
        gboolean                     device_only_icon;
        gboolean                     skip_mounting;

        Options(): gcmd_settings(NULL),
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
                   symlink_prefix(NULL),
                   size_disp_mode(GNOME_CMD_SIZE_DISP_MODE_POWERED),
                   perm_disp_mode(GNOME_CMD_PERM_DISP_MODE_TEXT),
                   date_format(NULL),
                   list_font(NULL),
                   list_row_height(16),
                   ext_disp_mode(GNOME_CMD_EXT_DISP_BOTH),
                   layout(GNOME_CMD_LAYOUT_MIME_ICONS),
                   color_mode(GNOME_CMD_COLOR_DEEP_BLUE),
                   use_ls_colors(FALSE),
                   icon_size(16),
                   icon_scale_quality(GDK_INTERP_HYPER),
                   theme_icon_dir(NULL),
                   always_show_tabs(FALSE),
                   tab_lock_indicator(TAB_LOCK_ICON),
                   confirm_delete(TRUE),
                   confirm_delete_default(GTK_BUTTONS_OK),
                   confirm_copy_overwrite(GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                   confirm_move_overwrite(GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                   confirm_mouse_dnd(TRUE),
                   backup_pattern(NULL),
                   backup_pattern_list(NULL),
                   honor_expect_uris(FALSE),
                   viewer(NULL),
                   use_internal_viewer(TRUE),
                   editor(NULL),
                   differ(NULL),
                   sendto(NULL),
                   termopen(NULL),
                   termexec(NULL),
                   fav_apps(NULL),
                   device_only_icon(FALSE),
                   skip_mounting(FALSE)
        {
            memset(&color_themes, 0, sizeof(color_themes));
            memset(&ls_colors_palette, 0, sizeof(ls_colors_palette));
        }

        Options(const Options &cfg);

        ~Options()
        {
            g_free (symlink_prefix);
            g_free (date_format);
            g_free (list_font);
            g_free (theme_icon_dir);
            g_free (backup_pattern);
            patlist_free (backup_pattern_list);
            g_free (viewer);
            g_free (editor);
            g_free (differ);
            g_free (sendto);
            g_free (termopen);
            g_free (termexec);
        }

        Options &operator = (const Options &cfg);

        GnomeCmdColorTheme *get_current_color_theme()
        {
            return &color_themes[color_mode];
        }

        GnomeCmdColorTheme *get_custom_color_theme()
        {
            return &color_themes[GNOME_CMD_COLOR_CUSTOM];
        }

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

        void set_backup_pattern(const gchar *value)
        {
            g_free (backup_pattern);
            patlist_free (backup_pattern_list);

            backup_pattern = g_strdup (value);
            backup_pattern_list = patlist_new (backup_pattern);
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

        void add_fav_app(GnomeCmdApp *app)
        {
            g_return_if_fail (app != NULL);
            fav_apps = g_list_append (fav_apps, app);
        }

        void remove_fav_app(GnomeCmdApp *app)
        {
            g_return_if_fail (app != NULL);
            fav_apps = g_list_remove (fav_apps, app);
        }

        void set_fav_apps(GList *apps)
        {
            // FIXME:   free fav_apps
            fav_apps = apps;
        }
        gboolean is_name_double(const gchar *name);

        void on_size_display_mode_changed();
    };

    struct Selection
    {
        std::string name;
        std::string filename_pattern;
        Filter::Type syntax;
        int max_depth;
        std::string text_pattern;
        gboolean content_search;
        gboolean match_case;

        Selection(): syntax(Filter::TYPE_REGEX), max_depth(-1), content_search(FALSE), match_case(FALSE)       {}

        const std::string &description() const    {  return filename_pattern;  }
        void reset();

        friend XML::xstream &operator << (XML::xstream &xml, Selection &cfg);
    };

    struct SearchConfig
    {
        gint width, height;

        Selection default_profile;

        History name_patterns;
        History content_patterns;

        std::vector<Selection> &profiles;

        SearchConfig(std::vector<Selection> &selections): width(600), height(400), name_patterns(SEARCH_HISTORY_SIZE), content_patterns(SEARCH_HISTORY_SIZE), profiles(selections)   {  default_profile.name = "Default";  }

        friend XML::xstream &operator << (XML::xstream &xml, SearchConfig &cfg);
    };

    struct AdvrenameConfig
    {
        struct Profile
        {
            std::string name;
            std::string template_string;
            guint counter_start;
            guint counter_width;
            gint counter_step;

            std::vector<GnomeCmd::ReplacePattern> regexes;

            guint case_conversion;
            guint trim_blanks;

            Profile(): template_string("$N"),
                       counter_start(1), counter_width(1), counter_step(1),
                       case_conversion(0), trim_blanks(3)                     {}

            const std::string &description() const {  return template_string;  }
            void reset();
        };

        gint width, height;

        Profile default_profile;
        std::vector<Profile> profiles;

        History templates;

        AdvrenameConfig(): width(600), height(400), templates(ADVRENAME_HISTORY_SIZE)   {}
        ~AdvrenameConfig()                                                              {}

        friend XML::xstream &operator << (XML::xstream &xml, AdvrenameConfig &cfg);
    };

    struct IntViewerConfig
    {
        History text_patterns;
        History hex_patterns;
        gboolean case_sensitive;
        gint search_mode;

        IntViewerConfig(): text_patterns(INTVIEWER_HISTORY_SIZE),
                           hex_patterns(INTVIEWER_HISTORY_SIZE),
                           case_sensitive(FALSE), search_mode(0)    {}
    };

    struct BookmarksConfig
    {
        gint width, height;

        BookmarksConfig(): width(400), height(250)   {}

        friend XML::xstream &operator << (XML::xstream &xml, BookmarksConfig &cfg);
    };

    typedef std::pair<std::string,triple<GnomeCmdFileList::ColumnID,GtkSortType,gboolean> > Tab;

    struct Private;

    Private *priv;

  private:

    GnomeCmdConRemote           *quick_connect;

    void load_auto_load_plugins();
    void load_cmdline_history();
    void load_local_bookmarks();
    void load_rename_history();
    void load_search_defaults();
    void load_intviewer_defaults();
    void load_smb_bookmarks();
    void save_auto_load_plugins();
    void save_cmdline_history();
    void save_intviewer_defaults();
    inline gint get_int (const gchar *path, int def);
    inline void set_int (const gchar *path, int value);
    inline gchar* get_string (const gchar *path, const gchar *def);
    inline void set_string (const gchar *path, const gchar *value);
    inline gboolean get_bool (const gchar *path, gboolean def);
    inline void set_bool (const gchar *path, gboolean value);
    inline void set_color (const gchar *path, GdkColor *color);


  public:

    gboolean                     XML_cfg_has_connections;
    gboolean                     XML_cfg_has_bookmarks;

    Options                      options;
    GcmdSettings                 *settings;

    std::vector<Selection>       selections;

    SearchConfig                 search_defaults;
    AdvrenameConfig              advrename_defaults;
    IntViewerConfig              intviewer_defaults;
    BookmarksConfig              bookmarks_defaults;

    gboolean                     horizontal_orientation;

    gboolean                     show_toolbar;
    gboolean                     show_devbuttons;
    gboolean                     show_devlist;
    gboolean                     cmdline_visibility;
    gboolean                     buttonbar_visibility;
    gboolean                     mainmenu_visibility;

    guint                        dev_icon_size;
    guint                        fs_col_width[GnomeCmdFileList::NUM_COLUMNS];
    guint                        gui_update_rate;

    GList                       *cmdline_history;
    gint                         cmdline_history_length;

    gboolean                     use_gcmd_block;

    gint                         main_win_width;
    gint                         main_win_height;
    GdkWindowState               main_win_state;

    std::map<guint,std::vector<Tab> > tabs;

    mode_t                       umask;

    GnomeCmdData();

    void free();                // FIXME: free() -> ~GnomeCmdData()

    void load();
    void migrate_all_data_to_gsettings();
    gint migrate_data_int_value_into_gsettings(gint user_value, GSettings *settings, const char *key);
    gboolean migrate_data_string_value_into_gsettings(const char* user_value, GSettings *settings, const char *key);
    void load_more();
    inline GList* load_string_history (const gchar *format, gint size);
    void save();
    gint gnome_cmd_data_get_int (const gchar *path, int def);
    void gnome_cmd_data_set_int (const gchar *path, int value);
    gchar* gnome_cmd_data_get_string (const gchar *path, const gchar *def);
    void gnome_cmd_data_set_string (const gchar *path, const gchar *value);
    void gnome_cmd_data_set_bool (const gchar *path, gboolean value);
    void gnome_cmd_data_set_color (const gchar *path, GdkColor *color);
    gboolean gnome_cmd_data_parse_color (const gchar *spec, GdkColor *color);
    gboolean set_color_if_valid_key_value(GdkColor *color, GSettings *settings, const char *key);
    void gnome_cmd_data_get_color_gnome_config (const gchar *path, GdkColor *color);
    gboolean gnome_cmd_data_get_bool (const gchar *path, gboolean def);
    gboolean set_gsettings_when_changed (GSettings *settings, const char *key, gpointer value);
    gboolean set_gsettings_color_when_changed (GSettings *settings, const char *key, GdkColor *color);
    gboolean set_gsettings_enum_when_changed (GSettings *settings, const char *key, gint value);
    inline void gnome_cmd_data_set_string_history (const gchar *format, GList *strings);
    gboolean is_valid_color_string(const char *colorstring);
    gboolean set_valid_color_string(GSettings *settings, const char* key);



    GnomeCmdConRemote *get_quick_connect() const       {  return quick_connect;                     }

    GnomeCmdFileList::ColumnID get_sort_col(FileSelectorID id) const;
    GtkSortType get_sort_direction(FileSelectorID id) const;
};

gpointer gnome_cmd_data_get_con_list ();

const gchar *gnome_cmd_data_get_ftp_anonymous_password ();
void gnome_cmd_data_set_ftp_anonymous_password (const gchar *pw);

GList *gnome_cmd_data_get_auto_load_plugins ();
void gnome_cmd_data_set_auto_load_plugins (GList *plugins);

void gnome_cmd_data_get_main_win_pos (gint *x, gint *y);
void gnome_cmd_data_set_main_win_pos (gint x, gint y);

const gchar *gnome_cmd_data_get_symlink_prefix ();

void on_use_default_font_changed();

extern GnomeCmdData gnome_cmd_data;

extern gchar *start_dir_left;
extern gchar *start_dir_right;
extern gchar *config_dir;

extern DICT<guint> gdk_key_names;
extern DICT<guint> gdk_modifiers_names;

#endif // __GNOME_CMD_DATA_H__
