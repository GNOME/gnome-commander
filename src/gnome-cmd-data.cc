/**
 * @file gnome-cmd-data.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2018 Uwe Scholz\n
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

#include <config.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>

#include <fstream>
#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"
#include "owner.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"
#include "dialogs/gnome-cmd-manage-bookmarks-dialog.h"
#include "gnome-cmd-gkeyfile-utils.h"

using namespace std;


#define MAX_GUI_UPDATE_RATE 1000
#define MIN_GUI_UPDATE_RATE 10
#define DEFAULT_GUI_UPDATE_RATE 100

GnomeCmdData gnome_cmd_data;

//static GnomeCmdData::AdvrenameConfig::Profile xml_adv_profile;

struct GnomeCmdData::Private
{
    GnomeCmdConList *con_list;
    GList           *auto_load_plugins;
    gint             sort_column[2];
    gboolean         sort_direction[2];

    gchar           *ftp_anonymous_password;
};

GSettingsSchemaSource* GnomeCmdData::GetGlobalSchemaSource()
{
    GSettingsSchemaSource   *global_schema_source;
    std::string              g_schema_path(PREFIX);

    g_schema_path.append("/share/glib-2.0/schemas");

    global_schema_source = g_settings_schema_source_get_default ();

    GSettingsSchemaSource *parent = global_schema_source;
    GError *error = NULL;

    global_schema_source = g_settings_schema_source_new_from_directory
                               ((gchar*) g_schema_path.c_str(),
                                parent,
                                FALSE,
                                &error);

    if (global_schema_source == NULL)
    {
        g_printerr(_("Could not load schemas from %s: %s\n"),
                   (gchar*) g_schema_path.c_str(), error->message);
        g_clear_error (&error);
    }

    return global_schema_source;
}

struct _GcmdSettings
{
    GObject parent;

    GSettings *general;
    GSettings *filter;
    GSettings *confirm;
    GSettings *colors;
    GSettings *programs;
    GSettings *network;
    GSettings *internalviewer;
    GSettings *plugins;
};

G_DEFINE_TYPE (GcmdSettings, gcmd_settings, G_TYPE_OBJECT)

static void gcmd_settings_finalize (GObject *object)
{
//    GcmdSettings *gs = GCMD_SETTINGS (object);
//
//    g_free (gs->old_scheme);
//
    G_OBJECT_CLASS (gcmd_settings_parent_class)->finalize (object);
}

static void gcmd_settings_dispose (GObject *object)
{
    GcmdSettings *gs = GCMD_SETTINGS (object);

    g_clear_object (&gs->general);
    g_clear_object (&gs->filter);
    g_clear_object (&gs->confirm);
    g_clear_object (&gs->colors);
    g_clear_object (&gs->programs);
    g_clear_object (&gs->network);
    g_clear_object (&gs->internalviewer);
    g_clear_object (&gs->plugins);

    G_OBJECT_CLASS (gcmd_settings_parent_class)->dispose (object);
}

static void on_size_display_mode_changed ()
{
    gint size_disp_mode;

    size_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    gnome_cmd_data.options.size_disp_mode = (GnomeCmdSizeDispMode) size_disp_mode;

    main_win->update_view();
}

static void on_perm_display_mode_changed ()
{
    gint perm_disp_mode;

    perm_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);
    gnome_cmd_data.options.perm_disp_mode = (GnomeCmdPermDispMode) perm_disp_mode;

    main_win->update_view();
}

static void on_graphical_layout_mode_changed ()
{
    gint graphical_layout_mode;

    graphical_layout_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);
    gnome_cmd_data.options.layout = (GnomeCmdLayout) graphical_layout_mode;

    main_win->update_view();
}

static void on_list_row_height_changed ()
{
    guint list_row_height;

    list_row_height = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);
    gnome_cmd_data.options.list_row_height = list_row_height;

    main_win->update_view();
}

static void on_date_disp_format_changed ()
{
    GnomeCmdDateFormat date_format;

    date_format = (GnomeCmdDateFormat) g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
    g_free(gnome_cmd_data.options.date_format);
    gnome_cmd_data.options.date_format = date_format;

    main_win->update_view();
}

static void on_filter_hide_unknown_changed()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN] = filter;

    main_win->update_view();
}

static void on_filter_hide_regular_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_REGULAR] = filter;

    main_win->update_view();
}

static void on_filter_hide_directory_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY] = filter;

    main_win->update_view();
}

static void on_filter_hide_fifo_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_FIFO);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_FIFO] = filter;

    main_win->update_view();
}

static void on_filter_hide_socket_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SOCKET);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_SOCKET] = filter;

    main_win->update_view();
}

static void on_filter_hide_character_device_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_CHARACTER_DEVICE);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE] = filter;

    main_win->update_view();
}

static void on_filter_hide_block_device_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BLOCK_DEVICE);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE] = filter;

    main_win->update_view();
}

static void on_filter_hide_symbolic_link_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMBOLIC_LINK);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK] = filter;

    main_win->update_view();
}

static void on_filter_dotfile_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_DOTFILE);
    gnome_cmd_data.options.filter.hidden = filter;

    main_win->update_view();
}

static void on_filter_backup_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP);
    gnome_cmd_data.options.filter.backup = filter;

    main_win->update_view();
}

static void on_backup_pattern_changed ()
{
    char *backup_pattern;

    backup_pattern = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN);
    gnome_cmd_data.options.set_backup_pattern(backup_pattern);
    main_win->update_view();
    g_free(backup_pattern);
}

static void on_list_font_changed ()
{
    g_free(gnome_cmd_data.options.list_font);
    gnome_cmd_data.options.list_font = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);

    main_win->update_view();
}

static void on_ext_disp_mode_changed ()
{
    gint ext_disp_mode;

    ext_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
    gnome_cmd_data.options.ext_disp_mode = (GnomeCmdExtDispMode) ext_disp_mode;

    main_win->update_view();
}

static void on_icon_size_changed ()
{
    guint icon_size;

    icon_size = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
    gnome_cmd_data.options.icon_size = icon_size;

    main_win->update_view();
}

static void on_show_devbuttons_changed ()
{
    gboolean show_devbuttons;

    show_devbuttons = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS);
    gnome_cmd_data.show_devbuttons = show_devbuttons;
    main_win->fs(ACTIVE)->update_show_devbuttons();
    main_win->fs(INACTIVE)->update_show_devbuttons();
}

static void on_show_devlist_changed ()
{
    gboolean show_devlist;

    show_devlist = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST);
    gnome_cmd_data.show_devlist = show_devlist;

    main_win->fs(ACTIVE)->update_show_devlist();
    main_win->fs(INACTIVE)->update_show_devlist();
}

static void on_show_cmdline_changed ()
{
    gboolean show_cmdline;

    show_cmdline = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE);
    gnome_cmd_data.cmdline_visibility = show_cmdline;
    main_win->update_cmdline_visibility();
}

static void on_show_toolbar_changed ()
{
    if (gnome_cmd_data.show_toolbar != g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR))
    {
        gnome_cmd_data.show_toolbar = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR);
        main_win->update_show_toolbar();
    }
}

static void on_show_buttonbar_changed ()
{
    if (gnome_cmd_data.buttonbar_visibility != g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR))
    {
        gnome_cmd_data.buttonbar_visibility = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR);
        main_win->update_buttonbar_visibility();
    }
}

static void on_horizontal_orientation_changed ()
{
    gboolean horizontal_orientation;

    horizontal_orientation = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION);
    gnome_cmd_data.horizontal_orientation = horizontal_orientation;

    main_win->update_horizontal_orientation();
    main_win->focus_file_lists();
}

static void on_always_show_tabs_changed ()
{
    gboolean always_show_tabs;

    always_show_tabs = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS);
    gnome_cmd_data.options.always_show_tabs = always_show_tabs;

    main_win->update_style();
}

static void on_tab_lock_indicator_changed ()
{
    gint tab_lock_indicator;

    tab_lock_indicator = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR);
    gnome_cmd_data.options.tab_lock_indicator = tab_lock_indicator;

    main_win->update_style();
}

static void on_confirm_delete_changed ()
{
    gboolean confirm_delete;

    confirm_delete = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE);
    gnome_cmd_data.options.confirm_delete = confirm_delete;
}

static void on_confirm_delete_default_changed ()
{
    gint confirm_delete_default;

    confirm_delete_default = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT);
    gnome_cmd_data.options.confirm_delete_default = (GtkButtonsType) confirm_delete_default;
}

static void on_confirm_copy_overwrite_changed ()
{
    gint confirm_copy_overwrite;

    confirm_copy_overwrite = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE);
    gnome_cmd_data.options.confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) confirm_copy_overwrite;
}

static void on_confirm_move_overwrite_changed ()
{
    gint confirm_move_overwrite;

    confirm_move_overwrite = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE);
    gnome_cmd_data.options.confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) confirm_move_overwrite;
}

static void on_mouse_drag_and_drop_changed ()
{
    gboolean confirm_mouse_dnd;

    confirm_mouse_dnd = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP);
    gnome_cmd_data.options.confirm_mouse_dnd = confirm_mouse_dnd;
}

static void on_select_dirs_changed ()
{
    gboolean select_dirs;

    select_dirs = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
    gnome_cmd_data.options.select_dirs = select_dirs;
}

static void on_case_sensitive_changed ()
{
    gboolean case_sensitive;

    case_sensitive = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);
    gnome_cmd_data.options.case_sens_sort = case_sensitive;
}

static void on_symlink_string_changed ()
{
    g_free(gnome_cmd_data.options.symlink_prefix);
    gnome_cmd_data.options.symlink_prefix = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SYMLINK_PREFIX);
}

static void on_theme_changed()
{
    gint theme;

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);
    gnome_cmd_data.options.color_mode = (GnomeCmdColorMode) theme;

    main_win->update_view();
}

static void on_custom_color_norm_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_NORM_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_custom_color_norm_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_NORM_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_custom_color_alt_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_ALT_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_custom_color_alt_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_ALT_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_custom_color_sel_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_SEL_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_custom_color_sel_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_SEL_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_custom_color_curs_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_CURS_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_custom_color_curs_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_CURS_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

static void on_use_ls_colors_changed()
{
    gboolean use_ls_colors;

    use_ls_colors = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);
    gnome_cmd_data.options.use_ls_colors = use_ls_colors;

    main_win->update_view();
}

static void on_ls_color_black_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.black_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLACK_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_black_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.black_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLACK_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_red_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.red_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_RED_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_red_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.red_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_RED_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_green_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.green_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_GREEN_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_green_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.green_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_GREEN_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_yellow_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.yellow_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_YELLOW_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_yellow_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.yellow_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_YELLOW_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_blue_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.blue_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLUE_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_blue_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.blue_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLUE_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_magenta_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.magenta_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_MAGENTA_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_magenta_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.magenta_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_MAGENTA_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_cyan_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.cyan_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_CYAN_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_cyan_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.cyan_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_CYAN_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_white_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.white_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_WHITE_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_ls_color_white_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.white_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_WHITE_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

static void on_always_download_changed()
{
    gboolean always_download;

    always_download = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD);
    gnome_cmd_data.options.honor_expect_uris = always_download;
}

static void on_multiple_instances_changed()
{
    gboolean allow_multiple_instances;

    allow_multiple_instances = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    gnome_cmd_data.options.allow_multiple_instances = allow_multiple_instances;
}

static void on_use_internal_viewer_changed()
{
    gboolean use_internal_viewer;
    use_internal_viewer = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER);
    gnome_cmd_data.options.use_internal_viewer = use_internal_viewer;
}

static void on_quick_search_shortcut_changed()
{
    GnomeCmdQuickSearchShortcut quick_search;
    quick_search = (GnomeCmdQuickSearchShortcut) g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
    gnome_cmd_data.options.quick_search = quick_search;
}

static void on_quick_search_exact_match_begin_changed()
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
    gnome_cmd_data.options.quick_search_exact_match_begin = quick_search_exact_match;
}

static void on_quick_search_exact_match_end_changed()
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);
    gnome_cmd_data.options.quick_search_exact_match_end = quick_search_exact_match;
}

static void on_dev_skip_mounting_changed()
{
    gboolean skip_mounting;

    skip_mounting = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DEV_SKIP_MOUNTING);
    gnome_cmd_data.options.skip_mounting = skip_mounting;
}

static void on_dev_only_icon_changed()
{
    gboolean dev_only_icon;

    dev_only_icon = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON);
    gnome_cmd_data.options.device_only_icon = dev_only_icon;
}

static void on_mainmenu_visibility_changed()
{
    gboolean mainmenu_visibility;

    mainmenu_visibility = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY);
    gnome_cmd_data.mainmenu_visibility = mainmenu_visibility;
    main_win->update_mainmenu_visibility();
}

static void on_opts_dialog_width_changed()
{
    gnome_cmd_data.opts_dialog_width = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_WIDTH);
}

static void on_opts_dialog_height_changed()
{
    gnome_cmd_data.opts_dialog_height = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_HEIGHT);
}

static void on_viewer_cmd_changed()
{
    gchar *viewer_cmd;
    g_free(gnome_cmd_data.options.viewer);
    viewer_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_VIEWER_CMD);
    gnome_cmd_data.options.viewer = viewer_cmd;
}

static void on_editor_cmd_changed()
{
    gchar *editor_cmd;
    g_free(gnome_cmd_data.options.editor);
    editor_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_EDITOR_CMD);
    gnome_cmd_data.options.editor = editor_cmd;
}

static void on_differ_cmd_changed()
{
    gchar *differ_cmd;
    g_free(gnome_cmd_data.options.differ);
    differ_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_DIFFER_CMD);
    gnome_cmd_data.options.differ = differ_cmd;
}

static void on_sendto_cmd_changed()
{
    gchar *sendto_cmd;
    g_free(gnome_cmd_data.options.sendto);
    sendto_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_SENDTO_CMD);
    gnome_cmd_data.options.sendto = sendto_cmd;
}

static void on_terminal_cmd_changed()
{
    gchar *terminal_cmd;
    g_free(gnome_cmd_data.options.termopen);
    terminal_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_CMD);
    gnome_cmd_data.options.termopen = terminal_cmd;
}

static void on_terminal_exec_cmd_changed()
{
    gchar *terminal_exec_cmd;
    g_free(gnome_cmd_data.options.termexec);
    terminal_exec_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_EXEC_CMD);
    gnome_cmd_data.options.termexec = terminal_exec_cmd;
}

static void on_ftp_anonymous_password_changed()
{
    g_free(gnome_cmd_data.priv->ftp_anonymous_password);
    gnome_cmd_data.priv->ftp_anonymous_password = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->network, GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD);
}

static void on_use_gcmd_block_changed()
{
    gnome_cmd_data.use_gcmd_block = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_USE_GCMD_BLOCK);
}

static void gcmd_settings_class_init (GcmdSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcmd_settings_finalize;
    object_class->dispose = gcmd_settings_dispose;
}

GcmdSettings *gcmd_settings_new ()
{
    return (GcmdSettings *) g_object_new (GCMD_TYPE_SETTINGS, NULL);
}


static void gcmd_connect_gsettings_signals(GcmdSettings *gs)
{
    g_signal_connect (gs->general,
                      "changed::size-display-mode",
                      G_CALLBACK (on_size_display_mode_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::perm-display-mode",
                      G_CALLBACK (on_perm_display_mode_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::graphical-layout-mode",
                      G_CALLBACK (on_graphical_layout_mode_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::list-row-height",
                      G_CALLBACK (on_list_row_height_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::date-disp-format",
                      G_CALLBACK (on_date_disp_format_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::list-font",
                      G_CALLBACK (on_list_font_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-unknown",
                      G_CALLBACK (on_filter_hide_unknown_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-regular",
                      G_CALLBACK (on_filter_hide_regular_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-directory",
                      G_CALLBACK (on_filter_hide_directory_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-fifo",
                      G_CALLBACK (on_filter_hide_fifo_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-socket",
                      G_CALLBACK (on_filter_hide_socket_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-char-device",
                      G_CALLBACK (on_filter_hide_character_device_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-block-device",
                      G_CALLBACK (on_filter_hide_block_device_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-symbolic-link",
                      G_CALLBACK (on_filter_hide_symbolic_link_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-dotfile",
                      G_CALLBACK (on_filter_dotfile_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::hide-backup-files",
                      G_CALLBACK (on_filter_backup_changed),
                      NULL);

    g_signal_connect (gs->filter,
                      "changed::backup-pattern",
                      G_CALLBACK (on_backup_pattern_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::extension-display-mode",
                      G_CALLBACK (on_ext_disp_mode_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::icon-size",
                      G_CALLBACK (on_icon_size_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::show-devbuttons",
                      G_CALLBACK (on_show_devbuttons_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::show-devlist",
                      G_CALLBACK (on_show_devlist_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::show-cmdline",
                      G_CALLBACK (on_show_cmdline_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::show-toolbar",
                      G_CALLBACK (on_show_toolbar_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::show-buttonbar",
                      G_CALLBACK (on_show_buttonbar_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::horizontal-orientation",
                      G_CALLBACK (on_horizontal_orientation_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::symlink-string",
                      G_CALLBACK (on_symlink_string_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::always-show-tabs",
                      G_CALLBACK (on_always_show_tabs_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::tab-lock-indicator",
                      G_CALLBACK (on_tab_lock_indicator_changed),
                      NULL);

    g_signal_connect (gs->confirm,
                      "changed::delete",
                      G_CALLBACK (on_confirm_delete_changed),
                      NULL);

    g_signal_connect (gs->confirm,
                      "changed::delete-default",
                      G_CALLBACK (on_confirm_delete_default_changed),
                      NULL);

    g_signal_connect (gs->confirm,
                      "changed::copy-overwrite",
                      G_CALLBACK (on_confirm_copy_overwrite_changed),
                      NULL);

    g_signal_connect (gs->confirm,
                      "changed::move-overwrite",
                      G_CALLBACK (on_confirm_move_overwrite_changed),
                      NULL);

    g_signal_connect (gs->confirm,
                      "changed::mouse-drag-and-drop",
                      G_CALLBACK (on_mouse_drag_and_drop_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::select-dirs",
                      G_CALLBACK (on_select_dirs_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::case-sensitive",
                      G_CALLBACK (on_case_sensitive_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::theme",
                      G_CALLBACK (on_theme_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-norm-fg",
                      G_CALLBACK (on_custom_color_norm_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-norm-bg",
                      G_CALLBACK (on_custom_color_norm_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-alt-fg",
                      G_CALLBACK (on_custom_color_alt_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-alt-bg",
                      G_CALLBACK (on_custom_color_alt_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-sel-fg",
                      G_CALLBACK (on_custom_color_sel_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-sel-bg",
                      G_CALLBACK (on_custom_color_sel_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-curs-fg",
                      G_CALLBACK (on_custom_color_curs_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::custom-curs-bg",
                      G_CALLBACK (on_custom_color_curs_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::use-ls-colors",
                      G_CALLBACK (on_use_ls_colors_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-black-fg",
                      G_CALLBACK (on_ls_color_black_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-black-bg",
                      G_CALLBACK (on_ls_color_black_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-red-fg",
                      G_CALLBACK (on_ls_color_red_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-red-bg",
                      G_CALLBACK (on_ls_color_red_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-green-fg",
                      G_CALLBACK (on_ls_color_green_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-green-bg",
                      G_CALLBACK (on_ls_color_green_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-yellow-fg",
                      G_CALLBACK (on_ls_color_yellow_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-yellow-bg",
                      G_CALLBACK (on_ls_color_yellow_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-blue-fg",
                      G_CALLBACK (on_ls_color_blue_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-blue-bg",
                      G_CALLBACK (on_ls_color_blue_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-magenta-fg",
                      G_CALLBACK (on_ls_color_magenta_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-magenta-bg",
                      G_CALLBACK (on_ls_color_magenta_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-cyan-fg",
                      G_CALLBACK (on_ls_color_cyan_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-cyan-bg",
                      G_CALLBACK (on_ls_color_cyan_bg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-white-fg",
                      G_CALLBACK (on_ls_color_white_fg_changed),
                      NULL);

    g_signal_connect (gs->colors,
                      "changed::lscm-white-bg",
                      G_CALLBACK (on_ls_color_white_bg_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::dont-download",
                      G_CALLBACK (on_always_download_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::allow-multiple-instances",
                      G_CALLBACK (on_multiple_instances_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::use-internal-viewer",
                      G_CALLBACK (on_use_internal_viewer_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::quick-search",
                      G_CALLBACK (on_quick_search_shortcut_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::quick-search-exact-match-begin",
                      G_CALLBACK (on_quick_search_exact_match_begin_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::quick-search-exact-match-end",
                      G_CALLBACK (on_quick_search_exact_match_end_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::dev-skip-mounting",
                      G_CALLBACK (on_dev_skip_mounting_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::dev-only-icon",
                      G_CALLBACK (on_dev_only_icon_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::mainmenu-visibility",
                      G_CALLBACK (on_mainmenu_visibility_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::opts-dialog-width",
                      G_CALLBACK (on_opts_dialog_width_changed),
                      NULL);

    g_signal_connect (gs->general,
                      "changed::opts-dialog-height",
                      G_CALLBACK (on_opts_dialog_height_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::viewer-cmd",
                      G_CALLBACK (on_viewer_cmd_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::editor-cmd",
                      G_CALLBACK (on_editor_cmd_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::differ-cmd",
                      G_CALLBACK (on_differ_cmd_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::sendto-cmd",
                      G_CALLBACK (on_sendto_cmd_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::terminal-cmd",
                      G_CALLBACK (on_terminal_cmd_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::terminal-exec-cmd",
                      G_CALLBACK (on_terminal_exec_cmd_changed),
                      NULL);

    g_signal_connect (gs->programs,
                      "changed::use-gcmd-block",
                      G_CALLBACK (on_use_gcmd_block_changed),
                      NULL);

    g_signal_connect (gs->network,
                      "changed::ftp-anonymous-password",
                      G_CALLBACK (on_ftp_anonymous_password_changed),
                      NULL);

}


static void gcmd_settings_init (GcmdSettings *gs)
{
    GSettingsSchemaSource   *global_schema_source;
    GSettingsSchema         *global_schema;

    global_schema_source = GnomeCmdData::GetGlobalSchemaSource();

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_GENERAL, FALSE);
    gs->general = g_settings_new_full (global_schema, NULL, NULL);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_FILTER, FALSE);
    gs->filter = g_settings_new_full (global_schema, NULL, NULL);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_CONFIRM, FALSE);
    gs->confirm = g_settings_new_full (global_schema, NULL, NULL);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_COLORS, FALSE);
    gs->colors = g_settings_new_full (global_schema, NULL, NULL);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_PROGRAMS, FALSE);
    gs->programs = g_settings_new_full (global_schema, NULL, NULL);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_NETWORK, FALSE);
    gs->network = g_settings_new_full (global_schema, NULL, NULL);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_INTERNAL_VIEWER, FALSE);
    gs->internalviewer = g_settings_new_full (global_schema, NULL, NULL);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_PLUGINS, FALSE);
    gs->plugins = g_settings_new_full (global_schema, NULL, NULL);

    gcmd_connect_gsettings_signals(gs);
}


DICT<guint> gdk_key_names(GDK_VoidSymbol);
DICT<guint> gdk_modifiers_names;


GnomeCmdData::Options::Options(const Options &cfg)
{
    copy (cfg.color_themes, cfg.color_themes+G_N_ELEMENTS(cfg.color_themes), color_themes);
    left_mouse_button_mode = cfg.left_mouse_button_mode;
    left_mouse_button_unselects = cfg.left_mouse_button_unselects;
    middle_mouse_button_mode = cfg.middle_mouse_button_mode;
    right_mouse_button_mode = cfg.right_mouse_button_mode;
    select_dirs = cfg.select_dirs;
    case_sens_sort = cfg.case_sens_sort;
    quick_search = cfg.quick_search;
    quick_search_exact_match_begin = cfg.quick_search_exact_match_begin;
    quick_search_exact_match_end = cfg.quick_search_exact_match_end;
    allow_multiple_instances = cfg.allow_multiple_instances;
    save_dirs_on_exit = cfg.save_dirs_on_exit;
    save_tabs_on_exit = cfg.save_tabs_on_exit;
    save_dir_history_on_exit = cfg.save_dir_history_on_exit;
    save_cmdline_history_on_exit = cfg.save_cmdline_history_on_exit;
    save_search_history_on_exit = cfg.save_search_history_on_exit;
    symlink_prefix = g_strdup (cfg.symlink_prefix);
    main_win_pos[0] = cfg.main_win_pos[0];
    main_win_pos[1] = cfg.main_win_pos[1];
    size_disp_mode = cfg.size_disp_mode;
    perm_disp_mode = cfg.perm_disp_mode;
    date_format = g_strdup (cfg.date_format);
    list_font = g_strdup (cfg.list_font);
    list_row_height = cfg.list_row_height;
    ext_disp_mode = cfg.ext_disp_mode;
    layout = cfg.layout;
    color_mode = cfg.color_mode;
    use_ls_colors = cfg.use_ls_colors;
    ls_colors_palette = cfg.ls_colors_palette;
    icon_size = cfg.icon_size;
    icon_scale_quality = cfg.icon_scale_quality;
    theme_icon_dir = cfg.theme_icon_dir;
    always_show_tabs = cfg.always_show_tabs;
    tab_lock_indicator = cfg.tab_lock_indicator;
    confirm_delete = cfg.confirm_delete;
    confirm_delete_default = cfg.confirm_delete_default;
    confirm_copy_overwrite = cfg.confirm_copy_overwrite;
    confirm_move_overwrite = cfg.confirm_move_overwrite;
    confirm_mouse_dnd = cfg.confirm_mouse_dnd;
    filter = cfg.filter;
    backup_pattern = g_strdup (cfg.backup_pattern);
    backup_pattern_list = patlist_new (cfg.backup_pattern);
    honor_expect_uris = cfg.honor_expect_uris;
    viewer = g_strdup (cfg.viewer);
    use_internal_viewer = cfg.use_internal_viewer;
    editor = g_strdup (cfg.editor);
    differ = g_strdup (cfg.differ);
    sendto = g_strdup (cfg.sendto);
    termopen = g_strdup (cfg.termopen);
    termexec = g_strdup (cfg.termexec);
    fav_apps = cfg.fav_apps;
    device_only_icon = cfg.device_only_icon;
    skip_mounting = cfg.skip_mounting;
    gcmd_settings = NULL;
}


GnomeCmdData::Options &GnomeCmdData::Options::operator = (const Options &cfg)
{
    if (this != &cfg)
    {
        this->~Options();       //  free allocated data

        copy (cfg.color_themes, cfg.color_themes+G_N_ELEMENTS(cfg.color_themes), color_themes);
        left_mouse_button_mode = cfg.left_mouse_button_mode;
        left_mouse_button_unselects = cfg.left_mouse_button_unselects;
        middle_mouse_button_mode = cfg.middle_mouse_button_mode;
        right_mouse_button_mode = cfg.right_mouse_button_mode;
        select_dirs = cfg.select_dirs;
        case_sens_sort = cfg.case_sens_sort;
        quick_search = cfg.quick_search;
        quick_search_exact_match_begin = cfg.quick_search_exact_match_begin;
        quick_search_exact_match_end = cfg.quick_search_exact_match_end;
        allow_multiple_instances = cfg.allow_multiple_instances;
        save_dirs_on_exit = cfg.save_dirs_on_exit;
        save_tabs_on_exit = cfg.save_tabs_on_exit;
        save_dir_history_on_exit = cfg.save_dir_history_on_exit;
        save_cmdline_history_on_exit = cfg.save_cmdline_history_on_exit;
        save_search_history_on_exit = cfg.save_search_history_on_exit;
        symlink_prefix = g_strdup (cfg.symlink_prefix);
        main_win_pos[0] = cfg.main_win_pos[0];
        main_win_pos[1] = cfg.main_win_pos[1];
        size_disp_mode = cfg.size_disp_mode;
        perm_disp_mode = cfg.perm_disp_mode;
        date_format = g_strdup (cfg.date_format);
        list_font = g_strdup (cfg.list_font);
        list_row_height = cfg.list_row_height;
        ext_disp_mode = cfg.ext_disp_mode;
        layout = cfg.layout;
        color_mode = cfg.color_mode;
        use_ls_colors = cfg.use_ls_colors;
        ls_colors_palette = cfg.ls_colors_palette;
        icon_size = cfg.icon_size;
        icon_scale_quality = cfg.icon_scale_quality;
        theme_icon_dir = cfg.theme_icon_dir;
        always_show_tabs = cfg.always_show_tabs;
        tab_lock_indicator = cfg.tab_lock_indicator;
        confirm_delete = cfg.confirm_delete;
        confirm_copy_overwrite = cfg.confirm_copy_overwrite;
        confirm_move_overwrite = cfg.confirm_move_overwrite;
        confirm_mouse_dnd = cfg.confirm_mouse_dnd;
        filter = cfg.filter;
        backup_pattern = g_strdup (cfg.backup_pattern);
        backup_pattern_list = patlist_new (cfg.backup_pattern);
        honor_expect_uris = cfg.honor_expect_uris;
        viewer = g_strdup (cfg.viewer);
        use_internal_viewer = cfg.use_internal_viewer;
        editor = g_strdup (cfg.editor);
        differ = g_strdup (cfg.differ);
        sendto = g_strdup (cfg.sendto);
        termopen = g_strdup (cfg.termopen);
        termexec = g_strdup (cfg.termexec);
        fav_apps = cfg.fav_apps;
        device_only_icon = cfg.device_only_icon;
        skip_mounting = cfg.skip_mounting;
        gcmd_settings = NULL;
    }

    return *this;
}

/**
 * This function takes a char array and compares it against each app
 * name in the list of gnome_cmd_data.options.fav_apps.
 *
 * @returns A TRUE if the given name is already existing in the list of
 * apps, else FALSE.
 */
gboolean GnomeCmdData::Options::is_name_double(const gchar *name_to_test)
{
    GList *app_pointer;
    gboolean foundstate = FALSE;
    for (app_pointer = gnome_cmd_data.options.fav_apps; app_pointer; app_pointer = app_pointer->next)
    {
        GnomeCmdApp *app = (GnomeCmdApp *) app_pointer->data;
        if (app)
        {
            gchar *app_name = g_strdup(gnome_cmd_app_get_name(app));
            if (!strcmp(app_name, name_to_test))
            foundstate = TRUE;
            g_free (app_name);
        }
    }
    return foundstate;
}

void GnomeCmdData::SearchProfile::reset()
{
    name.clear();
    filename_pattern.clear();
    syntax = Filter::TYPE_REGEX;
    max_depth = -1;
    text_pattern.clear();
    content_search = FALSE;
    match_case = FALSE;
}


void GnomeCmdData::AdvrenameConfig::Profile::reset()
{
    name.clear();
    template_string = "$N";
    regexes.clear();
    counter_start = counter_step = 1;
    counter_width = 0;
    case_conversion = 0;
    trim_blanks = 3;
}


inline gint GnomeCmdData::get_int (const gchar *path, int def)
{
    gboolean b = FALSE;
    gint value = gnome_config_get_int_with_default (path, &b);

    return b ? def : value;
}


inline gchar* GnomeCmdData::get_string (const gchar *path, const gchar *def)
{
    gboolean b = FALSE;
    gchar *value = gnome_config_get_string_with_default (path, &b);
    if (b)
        return g_strdup (def);
    return value;
}


inline void GnomeCmdData::set_string (const gchar *path, const gchar *value)
{
    gnome_config_set_string (path, value);
}


inline gboolean GnomeCmdData::get_bool (const gchar *path, gboolean def)
{
    gboolean b = FALSE;
    gboolean value = gnome_config_get_bool_with_default (path, &b);
    if (b)
        return def;
    return value;
}


inline XML::xstream &operator << (XML::xstream &xml, GnomeCmdBookmark &bookmark)
{
    xml << XML::tag("Bookmark") << XML::attr("name") << XML::escape(bookmark.name);
    xml << XML::attr("path") << XML::escape(bookmark.path) << XML::endtag();

    return xml;
}


static void write(XML::xstream &xml, GnomeCmdCon *con, const gchar *name)
{
    if (!con)
        return;

    GList *bookmarks = gnome_cmd_con_get_bookmarks (con)->bookmarks;

    if (!bookmarks)
        return;

    xml << XML::tag("Group") << XML::attr("name") << name;

    if (GNOME_CMD_IS_CON_REMOTE (con))
        xml << XML::attr("remote") << TRUE;

    for (GList *i=bookmarks; i; i=i->next)
        xml << *(GnomeCmdBookmark *) i->data;

    xml << XML::endtag("Group");
}


/**
 * Save search profiles in gSettings.
 * The first profile is the active profile, i.e. the one which is used currently.
 * The others are profiles which can be choosen in the profile dialoge.
 */
void GnomeCmdData::save_search_profiles ()
{
    GVariantBuilder* gVariantBuilder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
    if (options.save_search_history_on_exit)
        add_search_profile_to_gvariant_builder(gVariantBuilder, search_defaults.default_profile);
    else
    {
        SearchProfile searchProfile;
        add_search_profile_to_gvariant_builder(gVariantBuilder, searchProfile);
    }

    for (auto profile : profiles)
    {
        add_search_profile_to_gvariant_builder(gVariantBuilder, profile);
    }

    GVariant* profilesToStore = g_variant_new(GCMD_SETTINGS_SEARCH_PROFILES_FORMAT_STRING, gVariantBuilder);
    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PROFILES, profilesToStore);
    g_variant_builder_unref(gVariantBuilder);
}


void GnomeCmdData::add_search_profile_to_gvariant_builder(GVariantBuilder *builder, GnomeCmdData::SearchProfile searchProfile)
{
    gchar *nameString = g_strescape (searchProfile.name.c_str(), NULL);
    gchar *filenamePattern = g_strescape (searchProfile.filename_pattern.c_str(), NULL);
    gchar *textPattern = g_strescape (searchProfile.text_pattern.c_str(), NULL);

    g_variant_builder_add(builder, GCMD_SETTINGS_SEARCH_PROFILE_FORMAT_STRING,
        nameString,
        searchProfile.max_depth,
        (gint) searchProfile.syntax,
        filenamePattern,
        searchProfile.content_search,
        searchProfile.match_case,
        textPattern
    );

    g_free(nameString);
    g_free(filenamePattern);
    g_free(textPattern);
}


/**
 * Save advance rename tool profiles in gSettings.
 * The first profile is the active profile, i.e. the one which is used currently.
 * It does not need to have a name != NULL, as it is not actively stored by the user.
 * The others are profiles which can be choosen in the profile dialoge.
 */
void GnomeCmdData::save_advrename_profiles ()
{
    GVariantBuilder* gVariantBuilder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
    add_advrename_profile_to_gvariant_builder(gVariantBuilder, advrename_defaults.default_profile);

    for (auto profile : advrename_defaults.profiles)
    {
        add_advrename_profile_to_gvariant_builder(gVariantBuilder, profile);
    }

    GVariant* profilesToStore = g_variant_new(GCMD_SETTINGS_ADVRENAME_PROFILES_FORMAT_STRING, gVariantBuilder);
    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_PROFILES, profilesToStore);
    g_variant_builder_unref(gVariantBuilder);
}


void GnomeCmdData::add_advrename_profile_to_gvariant_builder(GVariantBuilder *builder, AdvrenameConfig::Profile profile)
{
    gchar *nameString = g_strescape (profile.name.c_str(), NULL);
    gchar *templateString = g_strescape (profile.template_string.empty() ? "$N" : profile.template_string.c_str(), NULL);

    GVariantBuilder *gVariantBuilderAdvrenameFromArray = g_variant_builder_new(G_VARIANT_TYPE("as"));
    GVariantBuilder *gVariantBuilderAdvrenameToList = g_variant_builder_new (G_VARIANT_TYPE("as"));
    GVariantBuilder *gVariantBuilderAdvrenameMatchCaseList = g_variant_builder_new (G_VARIANT_TYPE("ab"));

    // Create arrays out of the patterns, save them in g_variant_builders
    for (auto r : profile.regexes)
    {
        g_variant_builder_add (gVariantBuilderAdvrenameFromArray, "s", r.pattern.c_str());
        g_variant_builder_add (gVariantBuilderAdvrenameToList, "s", r.replacement.c_str());
        g_variant_builder_add (gVariantBuilderAdvrenameMatchCaseList, "b", true);
    }

    g_variant_builder_add(builder, GCMD_SETTINGS_ADVRENAME_PROFILE_FORMAT_STRING,
        nameString,
        templateString,
        profile.counter_start,
        profile.counter_step,
        profile.counter_width,
        profile.case_conversion,
        profile.trim_blanks,
        gVariantBuilderAdvrenameFromArray,
        gVariantBuilderAdvrenameToList,
        gVariantBuilderAdvrenameMatchCaseList
    );

    g_free(nameString);
    g_free(templateString);
}


/**
 * Save devices in gSettings
 */
void GnomeCmdData::save_devices_via_gsettings()
{
    GList *devices;

    devices = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list);
    if (devices)
    {
        GVariant* devicesToStore;
        GVariantBuilder gVariantBuilder;
        g_variant_builder_init (&gVariantBuilder, G_VARIANT_TYPE_ARRAY);

        for (; devices; devices = devices->next)
        {
            GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (devices->data);
            if (device && !gnome_cmd_con_device_get_autovol (device))
            {
                gchar *alias = g_strescape (gnome_cmd_con_device_get_alias (device), NULL);

                gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
                if (device_fn && device_fn[0] != '\0')
                    device_fn = g_strescape (device_fn, NULL);
                else
                    device_fn = g_strdup ("x");

                gchar *mountp = g_strescape (gnome_cmd_con_device_get_mountp (device), NULL);

                gchar *icon_path = (gchar *) gnome_cmd_con_device_get_icon_path (device);
                if (icon_path && icon_path[0] != '\0')
                    icon_path = g_strescape (icon_path, NULL);
                else
                    icon_path = g_strdup ("x");

                g_variant_builder_add (&gVariantBuilder, GCMD_SETTINGS_DEVICES_FORMAT_STRING,
                                        alias,
                                        device_fn,
                                        mountp,
                                        icon_path);

                g_free (alias);
                g_free (device_fn);
                g_free (mountp);
                g_free (icon_path);
            }
        }
        devicesToStore = g_variant_builder_end (&gVariantBuilder);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICES, devicesToStore);
    }
}


/**
 * Save tabs in given gSettings and in given key
 */
static void save_tabs_via_gsettings(GSettings *gSettings, const char *gSettingsKey)
{
    GVariant* fileListTabs;
    GVariantBuilder gVariantBuilder;
    g_variant_builder_init (&gVariantBuilder, G_VARIANT_TYPE_ARRAY);

    for (int fileSelectorIdInt = LEFT; fileSelectorIdInt <= RIGHT; fileSelectorIdInt++)
    {
        FileSelectorID fileSelectorId = static_cast<FileSelectorID>(fileSelectorIdInt);
        GnomeCmdFileSelector gnomeCmdFileSelector = *main_win->fs(fileSelectorId);
        GList *tabs = gnomeCmdFileSelector.GetTabs();

        for (GList *i=tabs; i; i=i->next)
        {
            if (gnome_cmd_data.options.save_tabs_on_exit)
            {
                GnomeCmdFileList *fl = (GnomeCmdFileList *) gtk_bin_get_child (GTK_BIN (i->data));
                if (GNOME_CMD_FILE_LIST (fl) && gnome_cmd_con_is_local (fl->con))
                {
                    gchar* realPath = GNOME_CMD_FILE (fl->cwd)->get_real_path();
                    g_variant_builder_add (&gVariantBuilder, GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING,
                                            realPath,
                                            (guchar) fileSelectorId,
                                            fl->get_sort_column(),
                                            fl->get_sort_order(),
                                            fl->locked);
                    g_free(realPath);
                }
            }
            else
            {
                if (gnome_cmd_data.options.save_dirs_on_exit)
                {
                    GnomeCmdFileList *fl = (GnomeCmdFileList *) gtk_bin_get_child (GTK_BIN (i->data));
                    if (GNOME_CMD_FILE_LIST (fl) && gnome_cmd_con_is_local (fl->con) && (fl==gnomeCmdFileSelector.file_list() || fl->locked))
                    {
                        gchar* realPath = GNOME_CMD_FILE (fl->cwd)->get_real_path();
                        g_variant_builder_add (&gVariantBuilder, GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING,
                                                realPath,
                                                (guchar) fileSelectorId,
                                                fl->get_sort_column(),
                                                fl->get_sort_order(),
                                                fl->locked);
                        g_free(realPath);
                    }
                }
                else
                {
                    GnomeCmdFileList *fl = (GnomeCmdFileList *) gtk_bin_get_child (GTK_BIN (i->data));
                    if (GNOME_CMD_FILE_LIST (fl) && gnome_cmd_con_is_local (fl->con) && fl->locked)
                    {
                        gchar* realPath = GNOME_CMD_FILE (fl->cwd)->get_real_path();
                        g_variant_builder_add (&gVariantBuilder, GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING,
                                                realPath,
                                                (guchar) fileSelectorId,
                                                fl->get_sort_column(),
                                                fl->get_sort_order(),
                                                fl->locked);
                        g_free(realPath);
                    }
                }
            }
        }
        g_list_free (tabs);
    }
    fileListTabs = g_variant_builder_end (&gVariantBuilder);
    g_settings_set_value(gSettings, gSettingsKey, fileListTabs);
}


/**
 * Save devices
 */
 void GnomeCmdData::save_fav_apps_via_gsettings()
{
    if (gnome_cmd_data.options.fav_apps)
    {
        GVariant* favAppsToStore;
        GVariantBuilder gVariantBuilder;
        g_variant_builder_init (&gVariantBuilder, G_VARIANT_TYPE_ARRAY);

        for (GList *i = gnome_cmd_data.options.fav_apps; i; i = i->next)
        {
            GnomeCmdApp *app = (GnomeCmdApp *) i->data;
            if (app)
            {
                gchar *iconPath = g_strdup(gnome_cmd_app_get_icon_path(app));
                if (!iconPath)
                    iconPath = g_strdup("");

                g_variant_builder_add (&gVariantBuilder, GCMD_SETTINGS_FAV_APPS_FORMAT_STRING,
                                        gnome_cmd_app_get_name(app),
                                        gnome_cmd_app_get_command(app),
                                        iconPath,
                                        gnome_cmd_app_get_pattern_string(app),
                                        gnome_cmd_app_get_target(app),
                                        gnome_cmd_app_get_handles_uris(app),
                                        gnome_cmd_app_get_handles_multiple(app),
                                        gnome_cmd_app_get_requires_terminal(app));

                g_free (iconPath);
            }
        }
        favAppsToStore = g_variant_builder_end (&gVariantBuilder);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_FAV_APPS, favAppsToStore);
    }
}


/**
 * Save favourite applications in the given file by means of GKeyFile.
 */
static void save_devices_old (const gchar *fname)
{
    GList *devices;
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    GKeyFile *key_file;
    key_file = g_key_file_new ();

    devices = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list);

    if (devices)
    {
        for (; devices; devices = devices->next)
        {
            GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (devices->data);
            if (device && !gnome_cmd_con_device_get_autovol (device))
            {
                gchar *alias = g_strescape (gnome_cmd_con_device_get_alias (device), NULL);

                gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
                if (device_fn && device_fn[0] != '\0')
                    device_fn = g_strescape (device_fn, NULL);
                else
                    device_fn = g_strdup ("x");

                gchar *mountp = g_strescape (gnome_cmd_con_device_get_mountp (device), NULL);

                gchar *icon_path = (gchar *) gnome_cmd_con_device_get_icon_path (device);
                if (icon_path && icon_path[0] != '\0')
                    icon_path = g_strescape (icon_path, NULL);
                else
                    icon_path = g_strdup ("x");

                g_key_file_set_string(key_file, alias, DEVICES_DEVICE, device_fn);
                g_key_file_set_string(key_file, alias, DEVICES_MOUNT_POINT, mountp);
                g_key_file_set_string(key_file, alias, DEVICES_ICON_PATH, icon_path);

                g_free (alias);
                g_free (device_fn);
                g_free (mountp);
                g_free (icon_path);
            }
        }

        gcmd_key_file_save_to_file (path, key_file);
    }

    g_key_file_free(key_file);
    g_free (path);
}


/**
 * Save favourite applications in the given file by means of GKeyFile.
 */
static void save_fav_apps_old (const gchar *fname)
{
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    GKeyFile *key_file;
    key_file = g_key_file_new ();

    /* Now save the list */
    for (GList *i = gnome_cmd_data.options.fav_apps; i; i = i->next)
    {
        GnomeCmdApp *app = (GnomeCmdApp *) i->data;
        if (app)
        {
            gchar *group_name = g_strdup(gnome_cmd_app_get_name(app));
            gchar *icon       = g_strdup(gnome_cmd_app_get_icon_path(app));
            if (!icon)
                icon = g_strdup("");

            g_key_file_set_string(key_file, group_name, FAV_APPS_CMD, gnome_cmd_app_get_command(app));
            g_key_file_set_string(key_file, group_name, FAV_APPS_ICON, icon);
            g_key_file_set_string(key_file, group_name, FAV_APPS_PATTERN, gnome_cmd_app_get_pattern_string(app));
            g_key_file_set_integer(key_file, group_name, FAV_APPS_TARGET, gnome_cmd_app_get_target(app));
            g_key_file_set_integer(key_file, group_name, FAV_APPS_HANDLES_URIS, gnome_cmd_app_get_handles_uris(app));
            g_key_file_set_integer(key_file, group_name, FAV_APPS_HANDLES_MULTIPLE, gnome_cmd_app_get_handles_multiple(app));
            g_key_file_set_integer(key_file, group_name, FAV_APPS_REQUIRES_TERMINAL, gnome_cmd_app_get_requires_terminal(app));

            g_free (icon);
            g_free (group_name);
        }
    }

    if (gnome_cmd_data.options.fav_apps)
        gcmd_key_file_save_to_file (path, key_file);

    g_key_file_free(key_file);
    g_free (path);
}


inline gboolean vfs_is_uri_local (const char *uri)
{
    GnomeVFSURI *pURI = gnome_vfs_uri_new (uri);

    if (!pURI)
        return FALSE;

    gboolean b = gnome_vfs_uri_is_local (pURI);
    gnome_vfs_uri_unref (pURI);

    // make sure this is actually a local path (gnome treats "burn://" as local too, and we don't want that)
    if (g_ascii_strncasecmp (uri, "file:/", 6)!=0)
        b = FALSE;

    DEBUG('m',"uri (%s) is %slocal\n", uri, b?"":"NOT ");

    return b;
}


inline void remove_vfs_volume (GnomeVFSVolume *volume)
{
    char *path, *uri, *localpath;

    if (!gnome_vfs_volume_is_user_visible (volume))
        return;

    uri = gnome_vfs_volume_get_activation_uri (volume);
    if (!vfs_is_uri_local (uri))
    {
        g_free (uri);
        return;
    }

    path = gnome_vfs_volume_get_device_path (volume);
    localpath = gnome_vfs_get_local_path_from_uri (uri);

    for (GList *i = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list); i; i = i->next)
    {
        GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (i->data);
        if (device && gnome_cmd_con_device_get_autovol (device))
        {
            gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
            const gchar *mountp = gnome_cmd_con_device_get_mountp (device);

            if ((strcmp(device_fn, path)==0) && (strcmp(mountp, localpath)==0))
            {
                DEBUG('m',"Remove Volume:\ndevice_fn = %s\tmountp = %s\n",
                device_fn,mountp);
                gnome_cmd_data.priv->con_list->remove(device);
                break;
            }
        }
    }

    g_free (path);
    g_free (uri);
    g_free (localpath);
}


inline gboolean device_mount_point_exists (GnomeCmdConList *list, const gchar *mountpoint)
{
    gboolean rc = FALSE;

    for (GList *tmp = gnome_cmd_con_list_get_all_dev (list); tmp; tmp = tmp->next)
    {
        GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (tmp->data);
        if (device && !gnome_cmd_con_device_get_autovol (device))
        {
            gchar *mountp = g_strescape (gnome_cmd_con_device_get_mountp (device), NULL);
            gchar *mountp2= gnome_vfs_unescape_string (mountp, NULL);

            rc = strcmp(mountp2, mountpoint)==0;

            g_free (mountp);
            g_free (mountp2);

            if (rc)
                break;
        }
    }

    return rc;
}


static void add_vfs_volume (GnomeVFSVolume *volume)
{
    if (!gnome_vfs_volume_is_user_visible (volume))
        return;

    char *uri = gnome_vfs_volume_get_activation_uri (volume);

    if (!vfs_is_uri_local (uri))
    {
        g_free (uri);
        return;
    }

    char *path = gnome_vfs_volume_get_device_path (volume);
    char *icon = gnome_vfs_volume_get_icon (volume);
    char *name = gnome_vfs_volume_get_display_name (volume);
    GnomeVFSDrive *drive = gnome_vfs_volume_get_drive (volume);

    // Try to load the icon, using current theme
    const gchar *iconpath = NULL;
    GtkIconTheme *icontheme = gtk_icon_theme_get_default();
    if (icontheme)
    {
        GtkIconInfo *iconinfo = gtk_icon_theme_lookup_icon (icontheme, icon, 16, GTK_ICON_LOOKUP_USE_BUILTIN);
        // This returned string should not be free, see gtk documentation
        if (iconinfo)
            iconpath = gtk_icon_info_get_filename (iconinfo);
    }

    char *localpath = gnome_vfs_get_local_path_from_uri (uri);

    DEBUG('m',"name = %s\n", name);
    DEBUG('m',"path = %s\n", path);
    DEBUG('m',"uri = %s\n", uri);
    DEBUG('m',"local = %s\n", localpath);
    DEBUG('m',"icon = %s (full path = %s)\n", icon, iconpath);

    // Don't create a new device connect if one already exists. This can happen if the user manually added the same device in "Options|Devices" menu
    if (!device_mount_point_exists (gnome_cmd_data.priv->con_list, localpath))
    {
        GnomeCmdConDevice *ConDev = gnome_cmd_con_device_new (name, path?path:NULL, localpath, iconpath);
        gnome_cmd_con_device_set_autovol (ConDev, TRUE);
        gnome_cmd_con_device_set_vfs_volume (ConDev, volume);
        gnome_cmd_data.priv->con_list->add(ConDev);
    }
    else
        DEBUG('m', "Device for mountpoint(%s) already exists. AutoVolume not added\n", localpath);

    g_free (path);
    g_free (uri);
    g_free (icon);
    g_free (name);
    g_free (localpath);

    gnome_vfs_drive_unref (drive);
}


static void volume_mounted (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume)
{
    add_vfs_volume (volume);
}


static void volume_unmounted (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume)
{
    remove_vfs_volume (volume);
}

inline void set_vfs_volume_monitor ()
{
    GnomeVFSVolumeMonitor *monitor = gnome_vfs_get_volume_monitor ();

    g_signal_connect (monitor, "volume-mounted", G_CALLBACK (volume_mounted), NULL);
    g_signal_connect (monitor, "volume-unmounted", G_CALLBACK (volume_unmounted), NULL);
}


static void load_vfs_auto_devices ()
{
    GnomeVFSVolumeMonitor *monitor = gnome_vfs_get_volume_monitor ();
    GList *volumes = gnome_vfs_volume_monitor_get_mounted_volumes (monitor);

    for (GList *l = volumes; l; l = l->next)
    {
        add_vfs_volume ((GnomeVFSVolume *) l->data);
        gnome_vfs_volume_unref ((GnomeVFSVolume *) l->data);
    }
    g_list_free (volumes);

}


/**
 * This function reads the given file and sets up additional devices by
 * means of GKeyFile.
 */
static gboolean load_devices (const gchar *fname)
{
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);

    ifstream f(path);

    //Device file does not exist
    if(f.fail())
    {
        g_free (path);
        return FALSE;
    }

    GKeyFile *keyfile;
    gsize length;
    gchar **groups;

    keyfile = gcmd_key_file_load_from_file(path, 0);

    // keyfile exists but it is empty, no need to do anything here
    if (keyfile == NULL)
    {
        remove(path);
        g_free (path);
        return TRUE;
    }

    groups = g_key_file_get_groups (keyfile, &length);

    for (guint i = 0; i < length; i++)
    {
        gchar *alias     = NULL;
        gchar *device_fn = NULL;
        gchar *mountp    = NULL;
        gchar *icon_path = NULL;
        GError *error = NULL;

        alias     = g_strdup(groups[i]);
        device_fn = g_key_file_get_string (keyfile, groups[i], DEVICES_DEVICE, &error);
        mountp    = g_key_file_get_string (keyfile, groups[i], DEVICES_MOUNT_POINT, &error);
        icon_path = g_key_file_get_string (keyfile, groups[i], DEVICES_ICON_PATH, &error);

        if (error != NULL)
        {
             fprintf (stderr, "Unable to read file: %s\n", error->message);
             g_error_free (error);
        }
        else
        {
             gnome_cmd_data.priv->con_list->add (gnome_cmd_con_device_new (alias, device_fn, mountp, icon_path));
        }
        g_free(alias);
        g_free(device_fn);
        g_free(mountp);
        g_free(icon_path);
    }

    load_vfs_auto_devices ();

    // delete deprecated file
    remove(path);

    g_free (path);

    save_devices_old ("devices.gkeyfile_deprecated");

    return TRUE;
}

/**
 * This method reads the gsettings section of the advance rename tool
 */
void GnomeCmdData::load_advrename_profiles ()
{
    GVariant *gVariantProfiles = g_settings_get_value (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_PROFILES);

    g_autoptr(GVariantIter) iter1 = NULL;
    g_autoptr(GVariantIter) iter2 = NULL;
    g_autoptr(GVariantIter) iter3 = NULL;
    g_autoptr(GVariantIter) iter4 = NULL;

    g_variant_get (gVariantProfiles, GCMD_SETTINGS_ADVRENAME_PROFILES_FORMAT_STRING, &iter1);

    g_autofree gchar *name = NULL;
    gchar *template_string = NULL;
    guint counter_start = 0;
    guint counter_step  = 0;
    guint counter_width = 0;
    gboolean case_conversion = false;
    guint trim_blanks = 0;
    gboolean isGVariantEmpty = true;
    guint profileNumber = 0;

    while (g_variant_iter_loop (iter1,
            GCMD_SETTINGS_ADVRENAME_PROFILE_FORMAT_STRING,
            &name,
            &template_string,
            &counter_start,
            &counter_step,
            &counter_width,
            &case_conversion,
            &trim_blanks,
            &iter2,
            &iter3,
            &iter4))
    {
        isGVariantEmpty = false;

        AdvrenameConfig::Profile p;

        p.reset();

        p.name = name;
        p.template_string = template_string;
        p.counter_start = counter_start;
        p.counter_step  = counter_step;
        p.counter_width = counter_width;
        p.case_conversion = case_conversion;
        p.trim_blanks = trim_blanks;

        auto regexes_from = new vector<string>;
        auto regexes_to = new vector<string>;
        auto regexes_match_case = new vector<gboolean>;

        gchar *string;
        while (g_variant_iter_loop (iter2, "s", &string))
        {
            regexes_from->push_back(string);
        }
        while (g_variant_iter_loop (iter3, "s", &string))
        {
            regexes_to->push_back(string);
        }
        gboolean match_case;
        while (g_variant_iter_loop (iter4, "b", &match_case))
            regexes_match_case->push_back(match_case);

        // as the lenght in each string_list is the same, we only need one upper limit for this loop
        for (gsize ii = 0; ii < regexes_from->size(); ii++)
        {
            p.regexes.push_back(GnomeCmd::ReplacePattern(regexes_from->at(ii),
                                                         regexes_to->at(ii),
                                                         regexes_match_case->at(ii)));
        }

        if (profileNumber == 0)
            advrename_defaults.default_profile = p;
        else
            advrename_defaults.profiles.push_back(p);

        ++profileNumber;
        delete(regexes_from);
        delete(regexes_to);
        delete(regexes_match_case);
    }

    g_variant_unref(gVariantProfiles);

    // Add two sample profiles here - for new users
    if (isGVariantEmpty)
    {
        AdvrenameConfig::Profile p;

        p.reset();
        p.name = "Audio Files";
        p.template_string = "$T(Audio.AlbumArtist) - $T(Audio.Title).$e";
        p.regexes.push_back(GnomeCmd::ReplacePattern("[ _]+", " ", 0));
        p.regexes.push_back(GnomeCmd::ReplacePattern("[fF]eat\\.", "fr.", 1));
        p.counter_width = 1;
        advrename_defaults.profiles.push_back(p);

        p.reset();
        p.name = "CamelCase";
        p.regexes.push_back(GnomeCmd::ReplacePattern("\\s*\\b(\\w)(\\w*)\\b", "\\u\\1\\L\\2\\E", 0));
        p.regexes.push_back(GnomeCmd::ReplacePattern("\\.(.+)$", ".\\L\\1", 0));
        advrename_defaults.profiles.push_back(p);
    }
}


/**
 * This method reads the gsettings section of the search tool profiles
 */
void GnomeCmdData::load_search_profiles ()
{
    GVariant *gVariantProfiles = g_settings_get_value (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PROFILES);

    g_autoptr(GVariantIter) iter1 = nullptr;

    g_variant_get (gVariantProfiles, GCMD_SETTINGS_SEARCH_PROFILES_FORMAT_STRING, &iter1);

    g_autofree gchar *name = nullptr;
    gint maxDepth = 0;
    gint syntax = 0;
    gchar* filenamePattern = nullptr;
    gboolean contentSearch = false;
    gboolean matchCase = false;
    gchar* textPattern = nullptr;
    guint profileNumber = 0;

    while (g_variant_iter_loop (iter1,
            GCMD_SETTINGS_SEARCH_PROFILE_FORMAT_STRING,
            &name,
            &maxDepth,
            &syntax,
            &filenamePattern,
            &contentSearch,
            &matchCase,
            &textPattern))
    {
        SearchProfile searchProfile;

        searchProfile.name             = name;
        searchProfile.max_depth        = maxDepth;
        searchProfile.syntax           = syntax == 0 ? Filter::TYPE_REGEX : Filter::TYPE_FNMATCH;
        searchProfile.filename_pattern = filenamePattern;
        searchProfile.content_search   = contentSearch;
        searchProfile.match_case       = matchCase;
        searchProfile.text_pattern     = textPattern;

        if (profileNumber == 0)
            search_defaults.default_profile = searchProfile;
        else
            profiles.push_back(searchProfile);

        ++profileNumber;
    }

    g_variant_unref(gVariantProfiles);
}


/**
 * Loads devices from gSettings into gcmd options
 */
void GnomeCmdData::load_fav_apps_from_gsettings()
{
    GVariant *gvFavApps, *favApp;
    GVariantIter iter;

    gvFavApps = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_FAV_APPS);

    g_variant_iter_init (&iter, gvFavApps);

    gnome_cmd_data.options.fav_apps = NULL;

	while ((favApp = g_variant_iter_next_value (&iter)) != NULL)
    {
        gchar *name, *command, *iconPath, *pattern;
        guint target;
        gboolean handlesUris, handlesMutiple, requiresTerminal;

		g_assert (g_variant_is_of_type (favApp, G_VARIANT_TYPE (GCMD_SETTINGS_FAV_APPS_FORMAT_STRING)));
		g_variant_get(favApp, GCMD_SETTINGS_FAV_APPS_FORMAT_STRING, &name, &command, &iconPath, &pattern, &target, &handlesUris, &handlesMutiple, &requiresTerminal);

        gnome_cmd_data.options.fav_apps = g_list_append (
            gnome_cmd_data.options.fav_apps,
            gnome_cmd_app_new_with_values (
            name, command, iconPath, (AppTarget) target, pattern,
            handlesUris, handlesMutiple, requiresTerminal));

		g_variant_unref(favApp);
        g_free (name);
        g_free (command);
        g_free (iconPath);
        g_free (pattern);
    }
    g_variant_unref(gvFavApps);
}


/**
 * This function reads the given file and sets up favourite applications
 * by means of GKeyFile and by filling gnome_cmd_data.options.fav_apps.
 */
static gboolean load_fav_apps_old (const gchar *fname)
{
    GKeyFile *keyfile;
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);

    gnome_cmd_data.options.fav_apps = NULL;

    keyfile = gcmd_key_file_load_from_file(path, 0);

    if (keyfile == NULL)
    {
        g_free(path);
        return FALSE;
    }

    gsize length;
    gchar **groups;

    groups = g_key_file_get_groups (keyfile, &length);

    for (guint i = 0; i < length; i++)
    {
        gchar *name      = NULL;
        gchar *cmd       = NULL;
        gchar *icon_path = NULL;
        gchar *pattern   = NULL;
        guint target;
        guint handles_uris;
        guint handles_multiple;
        guint requires_terminal;
        GError *error = NULL;

        name              = g_strdup(groups[i]);
        cmd               = g_key_file_get_string (keyfile, groups[i], FAV_APPS_CMD, &error);
        icon_path         = g_key_file_get_string (keyfile, groups[i], FAV_APPS_ICON, &error);
        pattern           = g_key_file_get_string (keyfile, groups[i], FAV_APPS_PATTERN, &error);
        target            = g_key_file_get_integer (keyfile, groups[i], FAV_APPS_TARGET, &error);
        handles_uris      = g_key_file_get_boolean (keyfile, groups[i], FAV_APPS_HANDLES_URIS, &error);
        handles_multiple  = g_key_file_get_boolean (keyfile, groups[i], FAV_APPS_HANDLES_MULTIPLE, &error);
        requires_terminal = g_key_file_get_boolean (keyfile, groups[i], FAV_APPS_REQUIRES_TERMINAL, &error);

        if (error != NULL)
        {
             fprintf (stderr, "Unable to read file: %s\n", error->message);
             g_error_free (error);
        }
        else
        {
             gnome_cmd_data.options.fav_apps = g_list_append (
                 gnome_cmd_data.options.fav_apps,
                 gnome_cmd_app_new_with_values (
                 name, cmd, icon_path, (AppTarget) target, pattern,
                 handles_uris, handles_multiple, requires_terminal));
        }

        g_free (name);
        g_free (cmd);
        g_free (icon_path);
        g_free (pattern);
    }

    remove(path);

    g_free(path);
    g_strfreev(groups);
    g_key_file_free(keyfile);

    save_fav_apps_old("fav-apps.gkeyfile_deprecated");
    return TRUE;
}

/**
 * This function converts a GList into a NULL terminated array of char pointers.
 * This array is stored into the given GSettings key.
 * @returns The return value of g_settings_set_strv if the length of the GList is > 0, else true.
 */
gboolean GnomeCmdData::set_gsettings_string_array_from_glist (GSettings *settings_given, const gchar *key, GList *strings)
{
    gboolean rv = true;

    if (strings == nullptr)
    {
        rv = g_settings_set_strv(settings_given, key, nullptr);
    }
    else
    {
        guint ii;
        auto numberOfStrings = g_list_length (strings);
        gchar** str_array;
        str_array = new char * [numberOfStrings + 1];

        // Build up a NULL terminated char array for storage in GSettings
        for (ii = 0; strings; strings = strings->next, ++ii)
        {
            str_array[ii] = (gchar*) strings->data;
        }
        str_array[ii] = nullptr;

        rv = g_settings_set_strv(settings_given, key, str_array);

        delete[](str_array);
    }
    return rv;
}


void GnomeCmdData::save_cmdline_history()
{
    if (options.save_cmdline_history_on_exit)
    {
        cmdline_history = gnome_cmd_cmdline_get_history (main_win->get_cmdline());
        set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, cmdline_history);
    }
    else
    {
        set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, NULL);
    }
}


void GnomeCmdData::save_directory_history()
{
    if (!options.save_dir_history_on_exit)
        return;

    set_gsettings_string_array_from_glist(
        options.gcmd_settings->general,
        GCMD_SETTINGS_DIRECTORY_HISTORY,
        gnome_cmd_con_get_dir_history (priv->con_list->get_home())->ents);
}


void GnomeCmdData::save_search_history()
{
    if (!gnome_cmd_data.options.save_search_history_on_exit)
        return;

    set_gsettings_string_array_from_glist(
        options.gcmd_settings->general,
        GCMD_SETTINGS_SEARCH_PATTERN_HISTORY,
        search_defaults.name_patterns.ents);

    set_gsettings_string_array_from_glist(
        options.gcmd_settings->general,
        GCMD_SETTINGS_SEARCH_TEXT_HISTORY,
        search_defaults.content_patterns.ents);
}


inline void GnomeCmdData::save_intviewer_defaults()
{
    set_gsettings_string_array_from_glist(options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT, intviewer_defaults.text_patterns.ents);
    set_gsettings_string_array_from_glist(options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_HEX, intviewer_defaults.hex_patterns.ents);
    set_gsettings_when_changed      (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_CASE_SENSITIVE, &(intviewer_defaults.case_sensitive));
    set_gsettings_enum_when_changed (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_MODE, intviewer_defaults.search_mode);
}

/**
 * This function saves all entries of the auto load plugin list in the associated GSettings string array.
 * @returns the return value of set_gsettings_string_array_from_glist() if there is anything to store, otherwise true.
 */
inline gboolean GnomeCmdData::save_auto_load_plugins()
{
    gboolean rv = true;

    if (g_list_length (priv->auto_load_plugins) > 0)
        rv = set_gsettings_string_array_from_glist(options.gcmd_settings->plugins, GCMD_SETTINGS_PLUGINS_AUTOLOAD, priv->auto_load_plugins);

    return rv;
}


#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
inline GList* GnomeCmdData::load_string_history (const gchar *format, gint size)
{
    GList *list = NULL;

    for (gint i=0; i<size || size==-1; ++i)
    {
        gchar *key = g_strdup_printf (format, i);
        gchar *value = gnome_cmd_data_get_string (key, NULL);
        g_free (key);
        if (!value)
            break;
        list = g_list_append (list, value);
    }

    return list;
}
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif


/**
 * Returns a GList with newly allocated char strings
 */
inline GList* GnomeCmdData::get_list_from_gsettings_string_array (GSettings *settings_given, const gchar *key)
{
    GList *list = NULL;
    gchar** gsettings_array;
    gsettings_array = g_settings_get_strv (settings_given, key);

    for(gint i = 0; gsettings_array[i]; ++i)
    {
        list = g_list_append (list, gsettings_array[i]);
    }

    g_free(gsettings_array);
    return list;
}


inline void GnomeCmdData::load_cmdline_history()
{
    g_list_free(cmdline_history);

    cmdline_history = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY);
}


inline void GnomeCmdData::load_directory_history()
{
    GList* directories = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY);

    for (GList *i=directories; i; i=i->next)
    {
        gnome_cmd_con_get_dir_history (get_home_con())->add((const gchar *) i->data);
        // the add method above copies the char strings
        g_free(i->data);
    }
    g_list_free(directories);
}


inline void GnomeCmdData::load_intviewer_defaults()
{
    intviewer_defaults.text_patterns = get_list_from_gsettings_string_array (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT);
    intviewer_defaults.hex_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_HEX);
    intviewer_defaults.case_sensitive = g_settings_get_boolean (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_CASE_SENSITIVE);
    intviewer_defaults.search_mode = g_settings_get_enum (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_MODE);
}


/**
 * This function pushes the list of plugins to be automatically loaded into the
 * associated Glist.
 */
inline void GnomeCmdData::load_auto_load_plugins()
{
    g_list_free(priv->auto_load_plugins);

    priv->auto_load_plugins = get_list_from_gsettings_string_array (options.gcmd_settings->plugins, GCMD_SETTINGS_PLUGINS_AUTOLOAD);
}


GnomeCmdData::GnomeCmdData(): search_defaults(profiles)
{
    quick_connect = NULL;

    XML_cfg_has_bookmarks = FALSE;

    horizontal_orientation = FALSE;

    show_toolbar = TRUE;
    show_devbuttons = TRUE;
    show_devlist = TRUE;
    cmdline_visibility = TRUE;
    buttonbar_visibility = TRUE;
    mainmenu_visibility = TRUE;

    //TODO: Include into GnomeCmdData::Options
    dev_icon_size = 16;
    memset(fs_col_width, 0, sizeof(fs_col_width));
    gui_update_rate = DEFAULT_GUI_UPDATE_RATE;

    cmdline_history = NULL;
    cmdline_history_length = 0;

    use_gcmd_block = TRUE;

    main_win_width = 600;
    main_win_height = 400;

    main_win_state = GDK_WINDOW_STATE_MAXIMIZED;

    umask = ::umask(0);
    ::umask(umask);

    priv = NULL;
    settings = NULL;
}


GnomeCmdData::~GnomeCmdData()
{
    if (priv)
    {
        // free the connections
        g_object_ref_sink (priv->con_list);
        g_object_unref (priv->con_list);

        // close quick connect
        if (quick_connect)
        {
            gnome_cmd_con_close (GNOME_CMD_CON (quick_connect));
            // gtk_object_destroy (GTK_OBJECT (quick_connect));
        }

        // free the anonymous password string
        g_free (priv->ftp_anonymous_password);

        g_free (priv);
    }
}

void GnomeCmdData::gsettings_init()
{
    options.gcmd_settings = gcmd_settings_new();
}

/**
 * This function checks if the given GSettings keys enholds a valid color string. If not,
 * the keys value is resetted to the default value.
 * @returns TRUE if the current value is resetted by the default value, else FALSE
 */
gboolean GnomeCmdData::set_valid_color_string(GSettings *settings_given, const char* key)
{
    gchar *colorstring;
    gboolean return_value;

    colorstring = g_settings_get_string (settings_given, key);
    if (!is_valid_color_string(colorstring))
    {
        GVariant *variant;
        variant = g_settings_get_default_value (settings_given, key);
        g_warning("Illegal color string \'%s\' for gsettings key %s. Resetting to default value \'%s\'",
                  colorstring, key, g_variant_get_string(variant, NULL));
        g_settings_set_string (settings_given, key, g_variant_get_string(variant, NULL));
        g_variant_unref (variant);
        return_value = TRUE;
    }
    else
        return_value = FALSE;

    g_free(colorstring);

    return return_value;
}


/**
 * Loads tabs from gSettings into gcmd options
 */
void GnomeCmdData::load_tabs_from_gsettings()
{
    GVariant *gvTabs, *tab;
    GVariantIter iter;

    gvTabs = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_FILE_LIST_TABS);

    g_variant_iter_init (&iter, gvTabs);

	while ((tab = g_variant_iter_next_value (&iter)) != NULL)
    {
        gchar *path;
        gboolean sort_order, locked;
        guchar fileSelectorId, sort_column;

		g_assert (g_variant_is_of_type (tab, G_VARIANT_TYPE (GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING)));
		g_variant_get(tab, GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING, &path, &fileSelectorId, &sort_column, &sort_order, &locked);
        string directory_path(path);
        if (!directory_path.empty() && sort_column < GnomeCmdFileList::NUM_COLUMNS)
        {
            this->tabs[(FileSelectorID) fileSelectorId].push_back(make_pair(directory_path, make_triple((GnomeCmdFileList::ColumnID) sort_column, (GtkSortType) sort_order, locked)));
        }
		g_variant_unref(tab);
        g_free(path);
    }
    g_variant_unref(gvTabs);
}

/**
 * Loads devices from gSettings into gcmd options
 */
void GnomeCmdData::load_devices_from_gsettings()
{
    GVariant *gvDevices, *device;
    GVariantIter iter;

    gvDevices = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICES);

    g_variant_iter_init (&iter, gvDevices);

	while ((device = g_variant_iter_next_value (&iter)) != NULL)
    {
        gchar *alias, *device_fn, *mountPoint, *iconPath;

		g_assert (g_variant_is_of_type (device, G_VARIANT_TYPE (GCMD_SETTINGS_DEVICES_FORMAT_STRING)));
		g_variant_get(device, GCMD_SETTINGS_DEVICES_FORMAT_STRING, &alias, &device_fn, &mountPoint, &iconPath);

        gnome_cmd_data.priv->con_list->add (gnome_cmd_con_device_new (alias, device_fn, mountPoint, iconPath));

		g_variant_unref(device);
        g_free (alias);
        g_free (device_fn);
        g_free (mountPoint);
        g_free (iconPath);
    }
    g_variant_unref(gvDevices);
    load_vfs_auto_devices ();
}


void GnomeCmdData::load()
{
    gchar *colorstring;

    if (!priv)
        priv = g_new0 (Private, 1);

    options.use_ls_colors = g_settings_get_boolean (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);

    options.color_themes[GNOME_CMD_COLOR_CUSTOM].respect_theme = FALSE;

    /* Initialization */
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg = gdk_color_new (0,0,0);
    options.ls_colors_palette.black_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.black_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.red_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.red_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.green_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.green_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.yellow_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.yellow_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.blue_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.blue_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.magenta_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.magenta_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.cyan_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.cyan_bg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.white_fg = gdk_color_new (0, 0, 0);
    options.ls_colors_palette.white_bg = gdk_color_new (0, 0, 0);
    /* Loading of actual values */
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
        g_free(colorstring);
    }

    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.black_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.black_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.red_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.red_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.green_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.green_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.yellow_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.yellow_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.blue_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.blue_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.magenta_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.magenta_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.cyan_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.cyan_bg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_FG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_FG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.white_fg);
        g_free(colorstring);
    }
    if (set_valid_color_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_BG) == FALSE)
    {
        colorstring = g_settings_get_string (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_BG);
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring, options.ls_colors_palette.white_bg);
        g_free(colorstring);
    }
    colorstring = NULL;

    options.color_themes[GNOME_CMD_COLOR_MODERN].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_MODERN].norm_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_MODERN].norm_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    options.color_themes[GNOME_CMD_COLOR_MODERN].alt_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_MODERN].alt_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    options.color_themes[GNOME_CMD_COLOR_MODERN].sel_fg = gdk_color_new (0xffff,0,0);
    options.color_themes[GNOME_CMD_COLOR_MODERN].sel_bg = gdk_color_new (0xdddd,0xdddd,0xdddd);
    options.color_themes[GNOME_CMD_COLOR_MODERN].curs_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_MODERN].curs_bg = gdk_color_new (0,0,0x4444);

    options.color_themes[GNOME_CMD_COLOR_FUSION].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_FUSION].norm_fg = gdk_color_new (0x8080,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_FUSION].norm_bg = gdk_color_new (0,0x4040,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].alt_fg = gdk_color_new (0x8080,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_FUSION].alt_bg = gdk_color_new (0,0x4040,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].sel_fg = gdk_color_new (0xffff,0xffff,0);
    options.color_themes[GNOME_CMD_COLOR_FUSION].sel_bg = gdk_color_new (0,0x4040,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].curs_fg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_FUSION].curs_bg = gdk_color_new (0,0x8080,0x8080);

    options.color_themes[GNOME_CMD_COLOR_CLASSIC].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].norm_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].norm_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].alt_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].alt_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].sel_fg = gdk_color_new (0xffff,0xffff,0);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].sel_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_CLASSIC].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_fg = gdk_color_new (0,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].norm_bg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].alt_fg = gdk_color_new (0,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].alt_bg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_fg = gdk_color_new (0xffff,0xffff,0);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].sel_bg = gdk_color_new (0,0,0x8080);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_DEEP_BLUE].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].norm_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].norm_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].alt_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].alt_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].sel_fg = gdk_color_new (0xffff,0xcfcf,0x3636);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].sel_bg = gdk_color_new (0x199a,0x1530,0x11a8);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].curs_fg = gdk_color_new (0xe4e4,0xdede,0xd5d5);
    options.color_themes[GNOME_CMD_COLOR_CAFEZINHO].curs_bg = gdk_color_new (0x4d4d,0x4d4d,0x4d4d);

    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].norm_fg = gdk_color_new (0xffff,0xc644,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].norm_bg = gdk_color_new (0x1919,0x2e2e,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].alt_fg = gdk_color_new (0xffff,0xc6c6,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].alt_bg = gdk_color_new (0x1f1f,0x3939,0x101);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].sel_fg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].sel_bg = gdk_color_new (0,0,0x4444);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_GREEN_TIGER].curs_bg = gdk_color_new (0xaaaa,0xaaaa,0xaaaa);

    options.color_themes[GNOME_CMD_COLOR_WINTER].respect_theme = FALSE;
    options.color_themes[GNOME_CMD_COLOR_WINTER].norm_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_WINTER].norm_bg = gdk_color_new (0xffff,0xffff,0xffff);
    options.color_themes[GNOME_CMD_COLOR_WINTER].alt_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_WINTER].alt_bg = gdk_color_new (0xf0f0, 0xf0f0, 0xf0f0);
    options.color_themes[GNOME_CMD_COLOR_WINTER].sel_fg = gdk_color_new (0,0,0xffff);
    options.color_themes[GNOME_CMD_COLOR_WINTER].sel_bg = gdk_color_new (0xc8c8,0xc8c8,0xc8c8);
    options.color_themes[GNOME_CMD_COLOR_WINTER].curs_fg = gdk_color_new (0,0,0);
    options.color_themes[GNOME_CMD_COLOR_WINTER].curs_bg = gdk_color_new (0,0xffff,0xffff);

    options.color_themes[GNOME_CMD_COLOR_NONE].respect_theme = TRUE;
    options.color_themes[GNOME_CMD_COLOR_NONE].norm_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].norm_bg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].alt_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].alt_bg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].sel_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].sel_bg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].curs_fg = NULL;
    options.color_themes[GNOME_CMD_COLOR_NONE].curs_bg = NULL;

    options.size_disp_mode = (GnomeCmdSizeDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    options.perm_disp_mode = (GnomeCmdPermDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);

    gchar *utf8_date_format = g_settings_get_string (options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
    options.date_format = g_locale_from_utf8 (utf8_date_format, -1, NULL, NULL, NULL);
    g_free (utf8_date_format);

    options.layout = (GnomeCmdLayout) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);

    options.list_row_height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);

    options.confirm_delete = g_settings_get_boolean (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE);
    options.confirm_delete_default = (GtkButtonsType) g_settings_get_enum (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT);
    options.confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) g_settings_get_enum (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE);
    options.confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) g_settings_get_enum (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE);
    options.confirm_mouse_dnd = g_settings_get_boolean (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP);

    options.filter.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_REGULAR] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_FIFO] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_FIFO);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_SOCKET] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SOCKET);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_CHARACTER_DEVICE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BLOCK_DEVICE);
    options.filter.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMBOLIC_LINK);
    options.filter.hidden = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_DOTFILE);
    options.filter.backup = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP);

    options.select_dirs = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
    options.case_sens_sort = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);

    main_win_width = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_WIDTH);
    main_win_height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_HEIGHT);
    opts_dialog_width = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_WIDTH);
    opts_dialog_height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_HEIGHT);
    fs_col_width[0] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_ICON);
    fs_col_width[1] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_NAME);
    fs_col_width[2] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_EXT);
    fs_col_width[3] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_DIR);
    fs_col_width[4] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_SIZE);
    fs_col_width[5] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_DATE);
    fs_col_width[6] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_PERM);
    fs_col_width[7] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_OWNER);
    fs_col_width[8] = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_GROUP);

    options.color_mode = gcmd_owner.is_root() ? (GnomeCmdColorMode) GNOME_CMD_COLOR_DEEP_BLUE
                                              : (GnomeCmdColorMode) g_settings_get_enum (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    options.list_font = g_settings_get_string (options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);

    options.ext_disp_mode = (GnomeCmdExtDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
    options.left_mouse_button_mode = (LeftMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM);
    options.left_mouse_button_unselects = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS);
    options.middle_mouse_button_mode = (MiddleMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE);
    options.right_mouse_button_mode = (RightMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE);
    options.icon_size = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
    dev_icon_size = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_DEV_ICON_SIZE);
    options.icon_scale_quality = (GdkInterpType) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SCALE_QUALITY);
    options.theme_icon_dir = g_settings_get_string(options.gcmd_settings->general, GCMD_SETTINGS_MIME_ICON_DIR);
    cmdline_history_length = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH);
    horizontal_orientation = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION);
    gui_update_rate = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE);
    options.main_win_pos[0] = g_settings_get_int (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_POS_X);
    options.main_win_pos[1] = g_settings_get_int (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_POS_Y);

    show_toolbar = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR);
    show_devbuttons = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS);
    show_devlist = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST);
    cmdline_visibility = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE);
    buttonbar_visibility = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR);
    mainmenu_visibility = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY);

    options.honor_expect_uris = g_settings_get_boolean (options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD);
    options.allow_multiple_instances = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    options.use_internal_viewer = g_settings_get_boolean (options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER);
    options.quick_search = (GnomeCmdQuickSearchShortcut) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
    options.quick_search_exact_match_begin = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
    options.quick_search_exact_match_end = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);

    options.skip_mounting = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_DEV_SKIP_MOUNTING);
    options.device_only_icon = g_settings_get_boolean(options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON);

    options.symlink_prefix = g_settings_get_string(options.gcmd_settings->general, GCMD_SETTINGS_SYMLINK_PREFIX);
    if (!*options.symlink_prefix || strcmp(options.symlink_prefix, _("link to %s"))==0)
    {
        g_free (options.symlink_prefix);
        options.symlink_prefix = NULL;
    }

    options.viewer = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_VIEWER_CMD);
    options.editor = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_EDITOR_CMD);
    options.differ = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_DIFFER_CMD);
    options.sendto = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_SENDTO_CMD);
    options.termopen = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_CMD);
    options.termexec = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_EXEC_CMD);
    use_gcmd_block = g_settings_get_boolean(options.gcmd_settings->programs, GCMD_SETTINGS_USE_GCMD_BLOCK);

    options.save_dirs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT);
    options.save_tabs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT);
    options.save_dir_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT);
    options.save_cmdline_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_CMDLINE_HISTORY_ON_EXIT);
    options.save_search_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_SEARCH_HISTORY_ON_EXIT);
    search_defaults.height = g_settings_get_uint(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_HEIGHT);
    search_defaults.width = g_settings_get_uint(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_WIDTH);
    search_defaults.content_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_TEXT_HISTORY);
    search_defaults.name_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PATTERN_HISTORY);

    options.always_show_tabs = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS);
    options.tab_lock_indicator = (TabLockIndicator) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR);

    options.backup_pattern = g_settings_get_string (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN);
    options.backup_pattern_list = patlist_new (options.backup_pattern);

    main_win_state = (GdkWindowState) g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_STATE);

    priv->ftp_anonymous_password = g_settings_get_string (options.gcmd_settings->network, GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD);

    advrename_defaults.width = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_WIDTH);
    advrename_defaults.height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_HEIGHT);

    advrename_defaults.templates.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_TEMPLATE_HISTORY);

    load_tabs_from_gsettings();

    static struct
    {
        guint code;
        const gchar *name;
    }
    gdk_key_names_data[] = {
                            {GDK_ampersand, "ampersand"},
                            {GDK_apostrophe, "apostrophe"},
                            {GDK_asciicircum, "asciicircum"},
                            {GDK_asciitilde, "asciitilde"},
                            {GDK_asterisk, "asterisk"},
                            {GDK_at, "at"},
                            {GDK_backslash, "backslash"},
                            {GDK_bar, "bar"},
                            {GDK_braceleft, "braceleft"},
                            {GDK_braceright, "braceright"},
                            {GDK_bracketleft, "bracketleft"},
                            {GDK_bracketright, "bracketright"},
                            {GDK_colon, "colon"},
                            {GDK_comma, "comma"},
                            {GDK_dollar, "dollar"},
                            {GDK_equal, "equal"},
                            {GDK_exclam, "exclam"},
                            {GDK_greater, "greater"},
                            {GDK_grave, "grave"},
                            {GDK_less, "less"},
                            {GDK_minus, "minus"},
                            {GDK_numbersign, "numbersign"},
                            {GDK_parenleft, "parenleft"},
                            {GDK_parenright, "parenright"},
                            {GDK_percent, "percent"},
                            {GDK_period, "period"},
                            {GDK_plus, "plus"},
                            {GDK_question, "question"},
                            {GDK_quotedbl, "quotedbl"},
                            {GDK_quoteleft, "quoteleft"},
                            {GDK_quoteright, "quoteright"},
                            {GDK_semicolon, "semicolon"},
                            {GDK_slash, "slash"},
                            {GDK_space, "space"},
                            {GDK_underscore, "underscore"},

                            {GDK_F1, "f1"},
                            {GDK_F2, "f2"},
                            {GDK_F3, "f3"},
                            {GDK_F4, "f4"},
                            {GDK_F5, "f5"},
                            {GDK_F6, "f6"},
                            {GDK_F7, "f7"},
                            {GDK_F8, "f8"},
                            {GDK_F9, "f9"},
                            {GDK_F10, "f10"},
                            {GDK_F11, "f11"},
                            {GDK_F12, "f12"},
                            {GDK_F13, "f13"},
                            {GDK_F14, "f14"},
                            {GDK_F15, "f15"},
                            {GDK_F16, "f16"},
                            {GDK_F17, "f17"},
                            {GDK_F18, "f18"},
                            {GDK_F19, "f19"},
                            {GDK_F20, "f20"},
                            {GDK_F21, "f21"},
                            {GDK_F22, "f22"},
                            {GDK_F23, "f23"},
                            {GDK_F24, "f24"},
                            {GDK_F25, "f25"},
                            {GDK_F26, "f26"},
                            {GDK_F27, "f27"},
                            {GDK_F28, "f28"},
                            {GDK_F29, "f29"},
                            {GDK_F30, "f30"},
                            {GDK_F31, "f31"},
                            {GDK_F32, "f32"},
                            {GDK_F33, "f33"},
                            {GDK_F34, "f34"},
                            {GDK_F35, "f35"},

                            {GDK_KP_0, "kp.0"},
                            {GDK_KP_1, "kp.1"},
                            {GDK_KP_2, "kp.2"},
                            {GDK_KP_3, "kp.3"},
                            {GDK_KP_4, "kp.4"},
                            {GDK_KP_5, "kp.5"},
                            {GDK_KP_6, "kp.6"},
                            {GDK_KP_7, "kp.7"},
                            {GDK_KP_8, "kp.8"},
                            {GDK_KP_9, "kp.9"},
                            {GDK_KP_Add, "kp.add"},
                            {GDK_KP_Begin, "kp.begin"},
                            {GDK_KP_Decimal, "kp.decimal"},
                            {GDK_KP_Delete, "kp.delete"},
                            {GDK_KP_Divide, "kp.divide"},
                            {GDK_KP_Down, "kp.down"},
                            {GDK_KP_End, "kp.end"},
                            {GDK_KP_Enter, "kp.enter"},
                            {GDK_KP_Equal, "kp.equal"},
                            {GDK_KP_F1, "kp.f1"},
                            {GDK_KP_F2, "kp.f2"},
                            {GDK_KP_F3, "kp.f3"},
                            {GDK_KP_F4, "kp.f4"},
                            {GDK_KP_Home, "kp.home"},
                            {GDK_KP_Insert, "kp.insert"},
                            {GDK_KP_Left, "kp.left"},
                            {GDK_KP_Multiply, "kp.multiply"},
                            {GDK_KP_Next, "kp.next"},
                            {GDK_KP_Page_Down, "kp.page.down"},
                            {GDK_KP_Page_Up, "kp.page.up"},
                            {GDK_KP_Prior, "kp.prior"},
                            {GDK_KP_Right, "kp.right"},
                            {GDK_KP_Separator, "kp.separator"},
                            {GDK_KP_Space, "kp.space"},
                            {GDK_KP_Subtract, "kp.subtract"},
                            {GDK_KP_Tab, "kp.tab"},
                            {GDK_KP_Up, "kp.up"},

                            {GDK_Caps_Lock, "caps.lock"},
                            {GDK_Num_Lock, "num.lock"},
                            {GDK_Scroll_Lock, "scroll.lock"},
                            {GDK_Shift_Lock, "shift.lock"},

                            {GDK_BackSpace, "backspace"},
                            {GDK_Begin, "begin"},
                            {GDK_Break, "break"},
                            {GDK_Cancel, "cancel"},
                            {GDK_Clear, "clear"},
                            {GDK_Codeinput, "codeinput"},
                            {GDK_Delete, "delete"},
                            {GDK_Down, "down"},
                            {GDK_Eisu_Shift, "eisu.shift"},
                            {GDK_Eisu_toggle, "eisu.toggle"},
                            {GDK_End, "end"},
                            {GDK_Escape, "escape"},
                            {GDK_Execute, "execute"},
                            {GDK_Find, "find"},
                            {GDK_First_Virtual_Screen, "first.virtual.screen"},
                            {GDK_Help, "help"},
                            {GDK_Home, "home"},
                            {GDK_Hyper_L, "hyper.l"},
                            {GDK_Hyper_R, "hyper.r"},
                            {GDK_Insert, "insert"},
                            {GDK_Last_Virtual_Screen, "last.virtual.screen"},
                            {GDK_Left, "left"},
                            {GDK_Linefeed, "linefeed"},
                            {GDK_Menu, "menu"},
                            {GDK_Meta_L, "meta.l"},
                            {GDK_Meta_R, "meta.r"},
                            {GDK_Mode_switch, "mode.switch"},
                            {GDK_MultipleCandidate, "multiplecandidate"},
                            {GDK_Multi_key, "multi.key"},
                            {GDK_Next, "next"},
                            {GDK_Next_Virtual_Screen, "next.virtual.screen"},
                            {GDK_Page_Down, "page.down"},
                            {GDK_Page_Up, "page.up"},
                            {GDK_Pause, "pause"},
                            {GDK_PreviousCandidate, "previouscandidate"},
                            {GDK_Prev_Virtual_Screen, "prev.virtual.screen"},
                            {GDK_Print, "print"},
                            {GDK_Prior, "prior"},
                            {GDK_Redo, "redo"},
                            {GDK_Return, "return"},
                            {GDK_Right, "right"},
                            {GDK_script_switch, "script.switch"},
                            {GDK_Select, "select"},
                            {GDK_SingleCandidate, "singlecandidate"},
                            {GDK_Super_L, "super.l"},
                            {GDK_Super_R, "super.r"},
                            {GDK_Sys_Req, "sys.req"},
                            {GDK_Tab, "tab"},
                            {GDK_Terminate_Server, "terminate.server"},
                            {GDK_Undo, "undo"},
                            {GDK_Up, "up"}
                           };

    load_data (gdk_key_names, gdk_key_names_data, G_N_ELEMENTS(gdk_key_names_data));

    static struct
    {
        guint code;
        const gchar *name;
    }
    gdk_mod_names_data[] = {
                            {GDK_SHIFT_MASK, "<shift>"},
                            {GDK_CONTROL_MASK, "<control>"},
                            {GDK_MOD1_MASK, "<alt>"},
                            {GDK_SUPER_MASK, "<super>"},
                            {GDK_SUPER_MASK, "<win>"},
                            {GDK_SUPER_MASK, "<mod4>"},
                            {GDK_HYPER_MASK, "<hyper>"},
                            {GDK_META_MASK, "<meta>"},
                            {GDK_MOD1_MASK, "<mod1>"},
                            {GDK_MOD4_MASK, "<super>"},
                            {GDK_MOD4_MASK, "<win>"},
                            {GDK_MOD4_MASK, "<mod4>"}
                           };

    load_data (gdk_modifiers_names, gdk_mod_names_data, G_N_ELEMENTS(gdk_mod_names_data));

    load_cmdline_history();

    if (!priv->con_list)
        priv->con_list = gnome_cmd_con_list_new ();
    else
    {
        gnome_cmd_con_erase_bookmark (priv->con_list->get_home());
#ifdef HAVE_SAMBA
        gnome_cmd_con_erase_bookmark (priv->con_list->get_smb());
#endif
        advrename_defaults.profiles.clear();
    }

    priv->con_list->lock();
    if (load_devices (DEVICES_FILENAME) == FALSE)
        load_devices_from_gsettings();
    else // This is done for migration to gSettings. Can be deleted in gcmd 1.9.
        save_devices_via_gsettings();

    gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

    // ToDo: Remove the check for xml cfg file in gcmd version >= 1.9.0
    if (gnome_cmd_xml_config_load (xml_cfg_path, *this))
    {
        // Convert advrename history from xml into gsettings
        set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_TEMPLATE_HISTORY, advrename_defaults.templates.ents);

        // Convert xml to keyfiles by using the save methods.
        save_advrename_profiles();
        save_search_profiles();

        // Convert directory history
        save_directory_history();

        // Move gnome-commander.xml to gnome-commander.xml.deprecated
        gchar *xml_cfg_path_old = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);
        gchar *xml_cfg_path_new = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml.deprecated", NULL);
        int rv;
        rv = rename (xml_cfg_path_old, xml_cfg_path_new);
        if (rv == -1)
        {
            g_warning("xml config file could not be renamed to \"%s\".", xml_cfg_path_new);
        }
        g_free (xml_cfg_path_old);
        g_free (xml_cfg_path_new);
    }

    g_free (xml_cfg_path);

    load_advrename_profiles ();
    load_search_profiles    ();

    if (load_fav_apps_old(FAV_APPS_FILENAME) == FALSE)
        load_fav_apps_from_gsettings();
    else // This is done for migration to gSettings. Can be deleted in gcmd 1.9.
        save_fav_apps_via_gsettings();

    priv->con_list->unlock();

    load_directory_history();

    gchar *quick_connect_uri = g_settings_get_string(options.gcmd_settings->network, GCMD_SETTINGS_QUICK_CONNECT_URI);
    quick_connect = gnome_cmd_con_remote_new (NULL, quick_connect_uri);
    g_free (quick_connect_uri);

    load_intviewer_defaults();
    load_auto_load_plugins();

    set_vfs_volume_monitor ();
}

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
/**
 * This method returns an int value which is either the given user_value or,
 * the default integer value of the given GSettings key. The user_value is returned
 * if it is different from the default value of the GSettings key.
 * @param user_value An integer value
 * @param settings_given A GSettings pointer
 * @param key a GSettings key path given as a char array
 */
gint GnomeCmdData::migrate_data_int_value_into_gsettings(int user_value, GSettings *settings_given, const char *key)
{
    GVariant *variant;
    gint default_value;
    gint return_value;

    variant = g_settings_get_default_value (settings_given, key);

    switch (g_variant_classify(variant))
    {
        // In all of the following cases it is assumed that the value behind 'default_value' is the actual
        // default value, i.e. nobody changed the given key before gcmd data migration was started.
        case G_VARIANT_CLASS_STRING:
        {
            default_value = g_settings_get_enum (settings_given, key);

            if (user_value != default_value)
                g_settings_set_enum (settings_given, key, user_value);

            return_value = g_settings_get_enum(settings_given, key);

            break;
        }
        case G_VARIANT_CLASS_UINT32:
        {
            default_value = g_variant_get_uint32 (variant);

            if (user_value != default_value)
                g_settings_set_uint (settings_given, key, user_value);

            return_value = g_settings_get_uint(settings_given, key);

            break;
        }
        case G_VARIANT_CLASS_INT32:
        {
            default_value = g_variant_get_int32 (variant);

            if (user_value != default_value)
                g_settings_set_int (settings_given, key, user_value);

            return_value = g_settings_get_int(settings_given, key);

            break;
        }
        case G_VARIANT_CLASS_BOOLEAN:
        {
            gboolean bdef_value;
            gboolean buser_value;
            bdef_value = g_variant_get_boolean (variant);
            buser_value = user_value == 1 ? TRUE : FALSE;

            if (buser_value != bdef_value)
                g_settings_set_boolean (settings_given, key, buser_value);

            return_value = g_settings_get_boolean (settings_given, key) ? 1 : 0;

            break;
        }
        default:
        {
            g_warning("Could not translate key value of type '%s'\n", g_variant_get_type_string (variant));
            default_value = -9999;
            return_value = default_value;
            break;
        }
    }
    g_variant_unref (variant);

    return return_value;
}
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

void GnomeCmdData::save_xml ()
{
    gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

    ofstream f(xml_cfg_path);
    XML::xstream xml(f);

    xml << *main_win;

    xml << search_defaults;
    xml << bookmarks_defaults;

    xml << XML::tag("Connections");

    for (GList *i=gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list); i; i=i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);

        if (con)
            xml << *con;
    }

    xml << XML::endtag("Connections");

    xml << XML::tag("Bookmarks");

    write (xml, priv->con_list->get_home(), "Home");
#ifdef HAVE_SAMBA
    write (xml, priv->con_list->get_smb(), "SMB");
#endif
    for (GList *i=gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list); i; i=i->next)
    {
        GnomeCmdCon *con = GNOME_CMD_CON (i->data);
        write (xml, con, XML::escape(gnome_cmd_con_get_alias (con)));
    }

    xml << XML::endtag("Bookmarks");

    xml << XML::tag("Selections");
    for (vector<SearchProfile>::iterator i=profiles.begin(); i!=profiles.end(); ++i)
        xml << *i;
    xml << XML::endtag("Selections");

    xml << gcmd_user_actions;

    xml << XML::endtag("GnomeCommander");

    g_free (xml_cfg_path);
}


/**
 * This method sets the value of a given GSettings key to the string stored in user_value or to the default value,
 * depending on the value of user_value. If the class of the key is not a string but a string array, the first
 * entry of the array is set to user_value.
 * @returns FALSE if an error occured setting the key value to a new string.
 */
gboolean GnomeCmdData::migrate_data_string_value_into_gsettings(const char* user_value, GSettings *settings_given, const char *key)
{
    GVariant *variant;
    gboolean rv = true;

    variant = g_settings_get_default_value (settings_given, key);

    if (g_variant_classify(variant) == G_VARIANT_CLASS_STRING)
    {
        gchar *default_value;

        // In the following it is assumed that the value behind 'default_value' is the actual
        // default value, i.e. nobody changed the given key before gcmd data migration was started.
        default_value = g_settings_get_string (settings_given, key);

        if (strcmp(user_value, default_value) != 0)
            rv = g_settings_set_string (settings_given, key, user_value);
    }
    else if (g_variant_classify(variant) == G_VARIANT_CLASS_ARRAY)
    {
        gchar** str_array;
        str_array = (gchar**) g_malloc (2*sizeof(char*));
        str_array[0] = g_strdup(user_value);
        str_array[1] = NULL;

        rv = g_settings_set_strv(settings_given, key, str_array);

        g_free(str_array[0]);
        g_free(str_array[1]);
        g_free(str_array);
    }
    else
    {
        g_warning("Could not translate key value of type '%s'\n", g_variant_get_type_string (variant));
        rv = false;
    }
    g_variant_unref (variant);

    return rv;
}


void GnomeCmdData::save()
{
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE, options.size_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE, options.perm_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE, options.layout);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT, &(options.list_row_height));

    gchar *utf8_date_format = g_locale_to_utf8 (options.date_format, -1, NULL, NULL, NULL);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT, utf8_date_format);
    g_free (utf8_date_format);

    set_gsettings_when_changed      (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE, &(options.confirm_delete));
    set_gsettings_enum_when_changed (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT, options.confirm_delete_default);
    set_gsettings_enum_when_changed (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE, options.confirm_copy_overwrite);
    set_gsettings_enum_when_changed (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE, options.confirm_move_overwrite);
    set_gsettings_when_changed      (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP, &(options.confirm_mouse_dnd));

    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_REGULAR]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_FIFO, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_FIFO]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SOCKET, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_SOCKET]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_CHARACTER_DEVICE, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BLOCK_DEVICE, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMBOLIC_LINK, &(options.filter.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_DOTFILE, &(options.filter.hidden));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP, &(options.filter.backup));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS, &(options.select_dirs));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE, &(options.case_sens_sort));

    set_gsettings_enum_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME, options.color_mode);

    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_FG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_BG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_FG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_BG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_FG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_BG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_FG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_BG, options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg);

    set_gsettings_when_changed      (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS, &(options.use_ls_colors));

    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_FG,   options.ls_colors_palette.black_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_BG,   options.ls_colors_palette.black_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_FG,     options.ls_colors_palette.red_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_BG,     options.ls_colors_palette.red_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_FG,   options.ls_colors_palette.green_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_BG,   options.ls_colors_palette.green_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_FG,  options.ls_colors_palette.yellow_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_BG,  options.ls_colors_palette.yellow_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_FG,    options.ls_colors_palette.blue_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_BG,    options.ls_colors_palette.blue_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_FG, options.ls_colors_palette.magenta_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_BG, options.ls_colors_palette.magenta_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_FG,    options.ls_colors_palette.cyan_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_BG,    options.ls_colors_palette.cyan_bg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_FG,   options.ls_colors_palette.white_fg);
    set_gsettings_color_when_changed (options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_BG,   options.ls_colors_palette.white_bg);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT, options.list_font);

    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE, options.ext_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM, options.left_mouse_button_mode);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS, &(options.left_mouse_button_unselects));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE, options.middle_mouse_button_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE, options.right_mouse_button_mode);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE, &(options.icon_size));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_DEV_ICON_SIZE, &(dev_icon_size));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SCALE_QUALITY, options.icon_scale_quality);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MIME_ICON_DIR, options.theme_icon_dir);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH, &(cmdline_history_length));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION, &(horizontal_orientation));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE, &(gui_update_rate));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES, &(options.allow_multiple_instances));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT, options.quick_search);

    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD, &(options.honor_expect_uris));
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER, &(options.use_internal_viewer));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN, &(options.quick_search_exact_match_begin));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END, &(options.quick_search_exact_match_end));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_DEV_SKIP_MOUNTING, &(options.skip_mounting));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON, &(options.device_only_icon));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR, &(show_toolbar));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS, &(show_devbuttons));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST, &(show_devlist));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE, &(cmdline_visibility));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR, &(buttonbar_visibility));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY, &(mainmenu_visibility));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_POS_X, &(options.main_win_pos[0]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_POS_Y, &(options.main_win_pos[1]));

    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_VIEWER_CMD, options.viewer);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_EDITOR_CMD, options.editor);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_DIFFER_CMD, options.differ);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_SENDTO_CMD, options.sendto);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_CMD, options.termopen);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_EXEC_CMD, options.termexec);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_USE_GCMD_BLOCK, &(use_gcmd_block));

    const gchar *quick_connect_uri = gnome_cmd_con_get_uri (GNOME_CMD_CON (quick_connect));

    if (quick_connect_uri)
        set_gsettings_when_changed (options.gcmd_settings->network, GCMD_SETTINGS_QUICK_CONNECT_URI, (gpointer) quick_connect_uri);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_WIDTH, &(main_win_width));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_HEIGHT, &(main_win_height));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_WIDTH, &(opts_dialog_width));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_OPTS_DIALOG_HEIGHT, &(opts_dialog_height));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_ICON, &(fs_col_width[0]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_NAME, &(fs_col_width[1]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_EXT, &(fs_col_width[2]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_DIR, &(fs_col_width[3]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_SIZE, &(fs_col_width[4]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_DATE, &(fs_col_width[5]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_PERM, &(fs_col_width[6]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_OWNER, &(fs_col_width[7]));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_COLUMN_WIDTH_GROUP, &(fs_col_width[8]));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT, &(options.save_dirs_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT, &(options.save_tabs_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT, &(options.save_dir_history_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_CMDLINE_HISTORY_ON_EXIT, &(options.save_cmdline_history_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_SEARCH_HISTORY_ON_EXIT, &(options.save_search_history_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_WIDTH, &(search_defaults.width));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_HEIGHT, &(search_defaults.height));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS, &(options.always_show_tabs));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR, options.tab_lock_indicator);

    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN, options.backup_pattern);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_STATE, &(main_win_state));

    set_gsettings_when_changed      (options.gcmd_settings->network, GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD, priv->ftp_anonymous_password);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_WIDTH, &(advrename_defaults.width));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_HEIGHT, &(advrename_defaults.height));
    set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_TEMPLATE_HISTORY, advrename_defaults.templates.ents);

    save_tabs_via_gsettings         (options.gcmd_settings->general, GCMD_SETTINGS_FILE_LIST_TABS);
    save_devices_via_gsettings      ();
    save_fav_apps_via_gsettings     ();
    save_cmdline_history            ();
    save_directory_history          ();
    save_search_history             ();
    save_search_profiles            ();

    save_advrename_profiles();
    save_intviewer_defaults();

    save_auto_load_plugins();
    g_settings_sync ();
}


gint GnomeCmdData::gnome_cmd_data_get_int (const gchar *path, int def)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gint v = get_int (s, def);

    g_free (s);

    return v;
}


gchar* GnomeCmdData::gnome_cmd_data_get_string (const gchar *path, const gchar *def)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gchar *v = get_string (s, def);

    g_free (s);

    return v;
}

void GnomeCmdData::gnome_cmd_data_set_string (const gchar *path, const gchar *value)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_string (s, value);

    g_free (s);
}

gboolean GnomeCmdData::gnome_cmd_data_get_bool (const gchar *path, gboolean def)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gboolean v = get_bool (s, def);

    g_free (s);

    return v;
}

/**
 * This function tests if the given colorstring enholds a valid color-describing string.
 * See documentation of gdk_color_parse() for valid strings.
 * @returns TRUE if the string is a valid color representation, else FALSE.
 */
gboolean GnomeCmdData::is_valid_color_string(const char *colorstring)
{
    g_return_val_if_fail(colorstring, FALSE);

    gboolean return_value;
    GdkColor *test_color;

    test_color = gdk_color_new (0,0,0);
    return_value = gdk_color_parse (colorstring, test_color);
    g_free(test_color);

    return return_value;
}

/**
 * This function loads a color specification, stored at the char pointer spec,
 * into *color if it is a valid color specification.
 * @returns the return value of gdk_color_parse function.
 */
gboolean GnomeCmdData::gnome_cmd_data_parse_color (const gchar *spec, GdkColor *color)
{
    g_return_val_if_fail(spec,FALSE);
    g_return_val_if_fail(color,FALSE);

    if (is_valid_color_string(spec) == TRUE)
        return gdk_color_parse (spec, color);
    else
        return FALSE;
}

/**
 * The task of this function is to store red, green and blue color
 * values in the GdkColor variable to which color is pointing to, based
 * on the GSettings value of key. First, it is tested if this value is a
 * valid GdkColor string. If yes, color is updated; if not, the current
 * string representing color is used to set back the string in the
 * GSettings key.
 */
gboolean GnomeCmdData::set_color_if_valid_key_value(GdkColor *color, GSettings *settings_given, const char *key)
{
    gboolean return_value;
    gchar *colorstring_new;

    colorstring_new = g_settings_get_string (settings_given, key);
    if (!gnome_cmd_data.is_valid_color_string(colorstring_new))
    {
        gchar *colorstring_old;

        colorstring_old = gdk_color_to_string (color);
        g_settings_set_string (settings_given, key, colorstring_old);
        g_warning("Illegal color string \'%s\'. Resetting to old value \'%s\'", colorstring_new, colorstring_old);
        g_free(colorstring_old);
        return_value = TRUE;
    }
    else
    {
        gnome_cmd_data.gnome_cmd_data_parse_color(colorstring_new, color);
        return_value = FALSE;
    }
    g_free(colorstring_new);
    return return_value;
}

/**
 * As GSettings enum-type is of GVARIANT_CLASS String, we need a seperate function for
 * finding out if a key value has changed. This is done here. For storing the other GSettings
 * types, see @link set_gsettings_when_changed @endlink .
 * @returns TRUE if new value could be stored, else FALSE
 */
gboolean GnomeCmdData::set_gsettings_enum_when_changed (GSettings *settings_given, const char *key, gint new_value)
{
    GVariant *default_val;
    gboolean rv = true;

    default_val = g_settings_get_default_value (settings_given, key);

    // An enum key must be of type G_VARIANT_CLASS_STRING
    if (g_variant_classify(default_val) == G_VARIANT_CLASS_STRING)
    {
        gint old_value;
        old_value = g_settings_get_enum(settings_given, key);
        if (old_value != new_value)
            rv = g_settings_set_enum (settings_given, key, new_value);
    }
    else
    {
        g_warning("Could not store value of type '%s' for key '%s'\n", g_variant_get_type_string (default_val), key);
        rv = false;
    }

    if (default_val)
        g_variant_unref (default_val);

    return rv;
}


#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
/**
 * This method stores the value for a given key if the value is different from the currently stored one
 * under the keys value. This function is able of storing several types of GSettings values (except enums
 * which is done in @link set_gsettings_enum_when_changed @endlink, and complex variant types).
 * Therefore, it first checks the type of GVariant of the default value of the given key. Depending on
 * the result, the gpointer is than casted to the correct type so that *value can be saved.
 * @returns TRUE if new value could be stored, else FALSE
 */
gboolean GnomeCmdData::set_gsettings_when_changed (GSettings *settings_given, const char *key, gpointer value)
{
    GVariant *default_val;
    gboolean rv = true;
    default_val = g_settings_get_default_value (settings_given, key);

    switch (g_variant_classify(default_val))
    {
        case G_VARIANT_CLASS_INT32:
        {
            gint old_value;
            gint new_value = *(gint*) value;

            old_value = g_settings_get_int (settings_given, key);
            if (old_value != new_value)
                rv = g_settings_set_int (settings_given, key, new_value);
            break;
        }
        case G_VARIANT_CLASS_UINT32:
        {
            gint old_value;
            gint new_value = *(gint*) value;

            old_value = g_settings_get_uint (settings_given, key);
            if (old_value != new_value)
                rv = g_settings_set_uint (settings_given, key, new_value);
            break;
        }
        case G_VARIANT_CLASS_STRING:
        {
            gchar *old_value;
            gchar *new_value = (char*) value;

            old_value = g_settings_get_string (settings_given, key);
            if (strcmp(old_value, new_value) != 0)
                rv = g_settings_set_string (settings_given, key, new_value);
            g_free(old_value);
            break;
        }
        case G_VARIANT_CLASS_BOOLEAN:
        {
            gboolean old_value;
            gboolean new_value = *(gboolean*) value;

            old_value = g_settings_get_boolean (settings_given, key);
            if (old_value != new_value)
                rv = g_settings_set_boolean (settings_given, key, new_value);
            break;
        }
        default:
        {
            g_warning("Could not store value of type '%s' for key '%s'\n", g_variant_get_type_string (default_val), key);
            rv = false;
            break;
        }
    }
    if (default_val)
        g_variant_unref (default_val);

    return rv;
}
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

gboolean GnomeCmdData::set_gsettings_color_when_changed (GSettings *settings_given, const char *key, GdkColor *color)
{
    gboolean return_value;
    gchar *colorstring;

    colorstring = gdk_color_to_string (color);
    return_value = set_gsettings_when_changed (settings_given, key, colorstring);
    g_free(colorstring);

    return return_value;
}


GnomeCmdFileList::ColumnID GnomeCmdData::get_sort_col(FileSelectorID id) const
{
    return (GnomeCmdFileList::ColumnID) priv->sort_column[id];
}


GtkSortType GnomeCmdData::get_sort_direction(FileSelectorID id) const
{
    return (GtkSortType) priv->sort_direction[id];
}


gpointer gnome_cmd_data_get_con_list ()
{
    return gnome_cmd_data.priv->con_list;
}


const gchar *gnome_cmd_data_get_ftp_anonymous_password ()
{
    return gnome_cmd_data.priv->ftp_anonymous_password;
}


void gnome_cmd_data_set_ftp_anonymous_password (const gchar *pw)
{
    g_free (gnome_cmd_data.priv->ftp_anonymous_password);

    gnome_cmd_data.priv->ftp_anonymous_password = g_strdup (pw);
}


GList *gnome_cmd_data_get_auto_load_plugins ()
{
    return gnome_cmd_data.priv->auto_load_plugins;
}


void gnome_cmd_data_set_auto_load_plugins (GList *plugins)
{
    gnome_cmd_data.priv->auto_load_plugins = plugins;
}


void gnome_cmd_data_set_main_win_pos (gint x, gint y)
{
    gnome_cmd_data.options.main_win_pos[0] = x;
    gnome_cmd_data.options.main_win_pos[1] = y;
}


void gnome_cmd_data_get_main_win_pos (gint *x, gint *y)
{
    *x = gnome_cmd_data.options.main_win_pos[0];
    *y = gnome_cmd_data.options.main_win_pos[1];
}


const gchar *gnome_cmd_data_get_symlink_prefix ()
{
    char *symlink_prefix;
    symlink_prefix = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SYMLINK_PREFIX);
    return (strlen(symlink_prefix) > 0) ? symlink_prefix : _("link to %s");
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::SearchProfile &cfg)
{
    xml << XML::tag("Profile") << XML::attr("name") << XML::escape(cfg.name);

        xml << XML::tag("Pattern") << XML::attr("syntax") << (cfg.syntax==Filter::TYPE_REGEX ? "regex" : "shell")
                                   << XML::attr("match-case") << 0 << XML::chardata();

        if (!strcmp("Default", cfg.name.c_str()) && gnome_cmd_data.options.save_search_history_on_exit)
            xml << XML::escape(cfg.filename_pattern);
        else
            xml << "";

        xml << XML::endtag();

        xml << XML::tag("Subdirectories") << XML::attr("max-depth") << cfg.max_depth << XML::endtag();
        xml << XML::tag("Text") << XML::attr("content-search") << cfg.content_search << XML::attr("match-case") << cfg.match_case << XML::chardata() << XML::escape(cfg.text_pattern) << XML::endtag();

    xml << XML::endtag();

    return xml;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::SearchConfig &cfg)
{
    xml << XML::tag("SearchTool");

        xml << XML::tag("WindowSize") << XML::attr("width") << cfg.width << XML::attr("height") << cfg.height << XML::endtag();
        xml << cfg.default_profile;

        if (gnome_cmd_data.options.save_search_history_on_exit)
        {
            xml << XML::tag("History");

            for (GList *i=cfg.name_patterns.ents; i; i=i->next)
                xml << XML::tag("Pattern") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();

            for (GList *i=cfg.content_patterns.ents; i; i=i->next)
                xml << XML::tag("Text") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();

            xml << XML::endtag();
        }

    xml << XML::endtag();

    return xml;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::BookmarksConfig &cfg)
{
    xml << XML::tag("BookmarksTool");

        xml << XML::tag("WindowSize") << XML::attr("width") << cfg.width << XML::attr("height") << cfg.height << XML::endtag();

    xml << XML::endtag();

    return xml;
}

