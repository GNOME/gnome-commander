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


struct GnomeCmdData
{
    static GSettingsSchemaSource* GetGlobalSchemaSource();

  private:

    void load_directory_history();
    gboolean save_auto_load_plugins();
    void load_connections();
    void save_connections();
    void save_directory_history(bool save_dir_history);
    void save_devices();

  public:

    GcmdSettings                 *settings {nullptr};

    void init();

    void load_devices();
    void save_bookmarks();
    void load_bookmarks();
    void save(bool save_dir_history);
};

extern GnomeCmdData gnome_cmd_data;
