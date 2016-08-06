/**
 * @file gnome-cmd-data.cc
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

#include <config.h>
#include <glib.h>
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
#include "gnome-cmd-gkeyfile-utils.h"

using namespace std;


#define MAX_GUI_UPDATE_RATE 1000
#define MIN_GUI_UPDATE_RATE 10
#define DEFAULT_GUI_UPDATE_RATE 100

GnomeCmdData gnome_cmd_data;
GnomeVFSVolumeMonitor *monitor = NULL;

struct GnomeCmdData::Private
{
    GnomeCmdConList *con_list;
    GList           *auto_load_plugins;
    gint             sort_column[2];
    gboolean         sort_direction[2];

    gchar           *ftp_anonymous_password;
};


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

void on_size_display_mode_changed ()
{
    gint size_disp_mode;

    size_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    gnome_cmd_data.options.size_disp_mode = (GnomeCmdSizeDispMode) size_disp_mode;

    main_win->update_view();
}

void on_perm_display_mode_changed ()
{
    gint perm_disp_mode;

    perm_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);
    gnome_cmd_data.options.perm_disp_mode = (GnomeCmdPermDispMode) perm_disp_mode;

    main_win->update_view();
}

void on_graphical_layout_mode_changed ()
{
    gint graphical_layout_mode;

    graphical_layout_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);
    gnome_cmd_data.options.layout = (GnomeCmdLayout) graphical_layout_mode;

    main_win->update_view();
}

void on_list_row_height_changed ()
{
    guint list_row_height;

    list_row_height = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);
    gnome_cmd_data.options.list_row_height = list_row_height;

    main_win->update_view();
}

void on_date_disp_format_changed ()
{
    GnomeCmdDateFormat date_format;

    date_format = (GnomeCmdDateFormat) g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
    g_free(gnome_cmd_data.options.date_format);
    gnome_cmd_data.options.date_format = date_format;

    main_win->update_view();
}

void on_filter_hide_unknown_changed()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_UNKNOWN] = filter;

    main_win->update_view();
}

void on_filter_hide_regular_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_REGULAR] = filter;

    main_win->update_view();
}

void on_filter_hide_directory_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_DIRECTORY] = filter;

    main_win->update_view();
}

void on_filter_hide_fifo_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_FIFO);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_FIFO] = filter;

    main_win->update_view();
}

void on_filter_hide_socket_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SOCKET);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_SOCKET] = filter;

    main_win->update_view();
}

void on_filter_hide_character_device_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_CHARACTER_DEVICE);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE] = filter;

    main_win->update_view();
}

void on_filter_hide_block_device_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BLOCK_DEVICE);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_BLOCK_DEVICE] = filter;

    main_win->update_view();
}

void on_filter_hide_symbolic_link_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMBOLIC_LINK);
    gnome_cmd_data.options.filter.file_types[GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK] = filter;

    main_win->update_view();
}

void on_filter_dotfile_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_DOTFILE);
    gnome_cmd_data.options.filter.hidden = filter;

    main_win->update_view();
}

void on_filter_backup_changed ()
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP);
    gnome_cmd_data.options.filter.backup = filter;

    main_win->update_view();
}

void on_backup_pattern_changed ()
{
    char *backup_pattern;

    backup_pattern = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN);
    gnome_cmd_data.options.set_backup_pattern(backup_pattern);
    main_win->update_view();
    g_free(backup_pattern);
}

void on_list_font_changed ()
{
    g_free(gnome_cmd_data.options.list_font);
    gnome_cmd_data.options.list_font = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);

    main_win->update_view();
}

void on_ext_disp_mode_changed ()
{
    gint ext_disp_mode;

    ext_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
    gnome_cmd_data.options.ext_disp_mode = (GnomeCmdExtDispMode) ext_disp_mode;

    main_win->update_view();
}

void on_icon_size_changed ()
{
    guint icon_size;

    icon_size = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
    gnome_cmd_data.options.icon_size = icon_size;

    main_win->update_view();
}

void on_show_devbuttons_changed ()
{
    gboolean show_devbuttons;

    show_devbuttons = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS);
    gnome_cmd_data.show_devbuttons = show_devbuttons;
    main_win->fs(ACTIVE)->update_show_devbuttons();
    main_win->fs(INACTIVE)->update_show_devbuttons();
}

void on_show_devlist_changed ()
{
    gboolean show_devlist;

    show_devlist = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST);
    gnome_cmd_data.show_devlist = show_devlist;

    main_win->fs(ACTIVE)->update_show_devlist();
    main_win->fs(INACTIVE)->update_show_devlist();
}

void on_show_cmdline_changed ()
{
    gboolean show_cmdline;

    show_cmdline = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE);
    gnome_cmd_data.cmdline_visibility = show_cmdline;
    main_win->update_cmdline_visibility();
}

void on_show_toolbar_changed ()
{
    if (gnome_cmd_data.show_toolbar != g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR))
    {
        gnome_cmd_data.show_toolbar = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR);
        main_win->update_show_toolbar();
    }
}

void on_show_buttonbar_changed ()
{
    if (gnome_cmd_data.buttonbar_visibility != g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR))
    {
        gnome_cmd_data.buttonbar_visibility = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR);
        main_win->update_buttonbar_visibility();
    }
}

void on_horizontal_orientation_changed ()
{
    gboolean horizontal_orientation;

    horizontal_orientation = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION);
    gnome_cmd_data.horizontal_orientation = horizontal_orientation;

    main_win->update_horizontal_orientation();
    main_win->focus_file_lists();
}

void on_always_show_tabs_changed ()
{
    gboolean always_show_tabs;

    always_show_tabs = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS);
    gnome_cmd_data.options.always_show_tabs = always_show_tabs;

    main_win->update_style();
}

void on_tab_lock_indicator_changed ()
{
    gint tab_lock_indicator;

    tab_lock_indicator = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR);
    gnome_cmd_data.options.tab_lock_indicator = tab_lock_indicator;

    main_win->update_style();
}

void on_confirm_delete_changed ()
{
    gboolean confirm_delete;

    confirm_delete = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE);
    gnome_cmd_data.options.confirm_delete = confirm_delete;
}

void on_confirm_delete_default_changed ()
{
    gint confirm_delete_default;

    confirm_delete_default = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT);
    gnome_cmd_data.options.confirm_delete_default = (GtkButtonsType) confirm_delete_default;
}

void on_confirm_copy_overwrite_changed ()
{
    gint confirm_copy_overwrite;

    confirm_copy_overwrite = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE);
    gnome_cmd_data.options.confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) confirm_copy_overwrite;
}

void on_confirm_move_overwrite_changed ()
{
    gint confirm_move_overwrite;

    confirm_move_overwrite = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE);
    gnome_cmd_data.options.confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) confirm_move_overwrite;
}

void on_mouse_drag_and_drop_changed ()
{
    gboolean confirm_mouse_dnd;

    confirm_mouse_dnd = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP);
    gnome_cmd_data.options.confirm_mouse_dnd = confirm_mouse_dnd;
}

void on_select_dirs_changed ()
{
    gboolean select_dirs;

    select_dirs = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
    gnome_cmd_data.options.select_dirs = select_dirs;
}

void on_case_sensitive_changed ()
{
    gboolean case_sensitive;

    case_sensitive = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);
    gnome_cmd_data.options.case_sens_sort = case_sensitive;
}

void on_symlink_string_changed ()
{
    g_free(gnome_cmd_data.options.symlink_prefix);
    gnome_cmd_data.options.symlink_prefix = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SYMLINK_PREFIX);
}

void on_theme_changed()
{
    gint theme;

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);
    gnome_cmd_data.options.color_mode = (GnomeCmdColorMode) theme;

    main_win->update_view();
}

void on_custom_color_norm_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_NORM_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_custom_color_norm_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].norm_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_NORM_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_custom_color_alt_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_ALT_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_custom_color_alt_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].alt_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_ALT_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_custom_color_sel_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_SEL_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_custom_color_sel_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].sel_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_SEL_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_custom_color_curs_fg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_CURS_FG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_custom_color_curs_bg_changed()
{
    gint theme;

    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.color_themes[GNOME_CMD_COLOR_CUSTOM].curs_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_COLORS_CURS_BG);

    theme = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);

    if ((GnomeCmdColorMode) theme == GNOME_CMD_COLOR_CUSTOM)
        main_win->update_view();
}

void on_use_ls_colors_changed()
{
    gboolean use_ls_colors;

    use_ls_colors = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);
    gnome_cmd_data.options.use_ls_colors = use_ls_colors;

    main_win->update_view();
}

void on_ls_color_black_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.black_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLACK_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_black_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.black_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLACK_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_red_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.red_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_RED_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_red_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.red_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_RED_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_green_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.green_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_GREEN_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_green_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.green_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_GREEN_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_yellow_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.yellow_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_YELLOW_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_yellow_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.yellow_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_YELLOW_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_blue_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.blue_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLUE_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_blue_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.blue_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_BLUE_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_magenta_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.magenta_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_MAGENTA_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_magenta_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.magenta_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_MAGENTA_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_cyan_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.cyan_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_CYAN_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_cyan_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.cyan_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_CYAN_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_white_fg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.white_fg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_WHITE_FG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_ls_color_white_bg_changed()
{
    gnome_cmd_data.set_color_if_valid_key_value(gnome_cmd_data.options.ls_colors_palette.white_bg,
                                                gnome_cmd_data.options.gcmd_settings->colors,
                                                GCMD_SETTINGS_LS_COLORS_WHITE_BG);

    if (g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS))
        main_win->update_view();
}

void on_always_download_changed()
{
    gboolean always_download;

    always_download = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD);
    gnome_cmd_data.options.honor_expect_uris = always_download;
}

void on_multiple_instances_changed()
{
    gboolean allow_multiple_instances;

    allow_multiple_instances = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    gnome_cmd_data.options.allow_multiple_instances = allow_multiple_instances;
}

void on_use_internal_viewer_changed()
{
    gboolean use_internal_viewer;
    use_internal_viewer = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER);
    gnome_cmd_data.options.use_internal_viewer = use_internal_viewer;
}

void on_quick_search_shortcut_changed()
{
    gint quick_search;
    quick_search = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
    gnome_cmd_data.options.quick_search = (GnomeCmdQuickSearchShortcut) quick_search;
}

void on_quick_search_exact_match_begin_changed()
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
    gnome_cmd_data.options.quick_search_exact_match_begin = quick_search_exact_match;
}

void on_quick_search_exact_match_end_changed()
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);
    gnome_cmd_data.options.quick_search_exact_match_end = quick_search_exact_match;
}

void on_dev_skip_mounting_changed()
{
    gboolean skip_mounting;

    skip_mounting = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DEV_SKIP_MOUNTING);
    gnome_cmd_data.options.skip_mounting = skip_mounting;
}

void on_dev_only_icon_changed()
{
    gboolean dev_only_icon;

    dev_only_icon = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON);
    gnome_cmd_data.options.device_only_icon = dev_only_icon;
}

void on_mainmenu_visibility_changed()
{
    gboolean mainmenu_visibility;

    mainmenu_visibility = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY);
    gnome_cmd_data.mainmenu_visibility = mainmenu_visibility;
    main_win->update_mainmenu_visibility();
}

void on_viewer_cmd_changed()
{
    gchar *viewer_cmd;
    g_free(gnome_cmd_data.options.viewer);
    viewer_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_VIEWER_CMD);
    gnome_cmd_data.options.viewer = viewer_cmd;
}

void on_editor_cmd_changed()
{
    gchar *editor_cmd;
    g_free(gnome_cmd_data.options.editor);
    editor_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_EDITOR_CMD);
    gnome_cmd_data.options.editor = editor_cmd;
}

void on_differ_cmd_changed()
{
    gchar *differ_cmd;
    g_free(gnome_cmd_data.options.differ);
    differ_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_DIFFER_CMD);
    gnome_cmd_data.options.differ = differ_cmd;
}

void on_sendto_cmd_changed()
{
    gchar *sendto_cmd;
    g_free(gnome_cmd_data.options.sendto);
    sendto_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_SENDTO_CMD);
    gnome_cmd_data.options.sendto = sendto_cmd;
}

void on_terminal_cmd_changed()
{
    gchar *terminal_cmd;
    g_free(gnome_cmd_data.options.termopen);
    terminal_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_CMD);
    gnome_cmd_data.options.termopen = terminal_cmd;
}

void on_terminal_exec_cmd_changed()
{
    gchar *terminal_exec_cmd;
    g_free(gnome_cmd_data.options.termexec);
    terminal_exec_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_EXEC_CMD);
    gnome_cmd_data.options.termexec = terminal_exec_cmd;
}

void on_ftp_anonymous_password_changed()
{
    g_free(gnome_cmd_data.priv->ftp_anonymous_password);
    gnome_cmd_data.priv->ftp_anonymous_password = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->network, GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD);
}

void on_use_gcmd_block_changed()
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
    gs->general        = g_settings_new (GCMD_PREF_GENERAL);
    gs->filter         = g_settings_new (GCMD_PREF_FILTER);
    gs->confirm        = g_settings_new (GCMD_PREF_CONFIRM);
    gs->colors         = g_settings_new (GCMD_PREF_COLORS);
    gs->programs       = g_settings_new (GCMD_PREF_PROGRAMS);
    gs->network        = g_settings_new (GCMD_PREF_NETWORK);
    gs->internalviewer = g_settings_new (GCMD_PREF_INTERNAL_VIEWER);
    gs->plugins        = g_settings_new (GCMD_PREF_PLUGINS);
    //TODO: Activate the following function in GCMD > 1.6
    //gcmd_connect_gsettings_signals(gs);
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

void GnomeCmdData::Selection::reset()
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


inline void GnomeCmdData::set_int (const gchar *path, int value)
{
    gnome_config_set_int (path, value);
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


inline void GnomeCmdData::set_bool (const gchar *path, gboolean value)
{
    gnome_config_set_bool (path, value);
}


inline void GnomeCmdData::set_color (const gchar *path, GdkColor *color)
{
    gchar *color_str;
    color_str = g_strdup_printf ("%d %d %d", color->red, color->green, color->blue);
    set_string (path, color_str);
    g_free (color_str);
}


inline XML::xstream &operator << (XML::xstream &xml, GnomeCmdBookmark &bookmark)
{
    xml << XML::tag("Bookmark") << XML::attr("name") << XML::escape(bookmark.name);
    xml << XML::attr("path") << XML::escape(bookmark.path) << XML::endtag();

    return xml;
}


inline void write(XML::xstream &xml, GnomeCmdCon *con, const gchar *name)
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
 * Save favourite applications in the given file by means of GKeyFile.
 */
inline void save_devices (const gchar *fname)
{
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    GKeyFile *key_file;
    key_file = g_key_file_new ();

    for (GList *i = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list); i; i = i->next)
    {
        GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (i->data);
        if (device && !gnome_cmd_con_device_get_autovol (device))
        {
            gchar *alias = gnome_vfs_escape_string (gnome_cmd_con_device_get_alias (device));

            gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
            if (device_fn && device_fn[0] != '\0')
                device_fn = gnome_vfs_escape_path_string (device_fn);
            else
                device_fn = g_strdup ("x");

            gchar *mountp = gnome_vfs_escape_path_string (gnome_cmd_con_device_get_mountp (device));

            gchar *icon_path = (gchar *) gnome_cmd_con_device_get_icon_path (device);
            if (icon_path && icon_path[0] != '\0')
                icon_path = gnome_vfs_escape_path_string (icon_path);
            else
                icon_path = g_strdup ("x");

            printf("%s\n",device_fn);
            g_key_file_set_string(key_file,alias,"device",device_fn);
            g_key_file_set_string(key_file,alias,"mount_point",mountp);
            g_key_file_set_string(key_file,alias,"icon_path",icon_path);

            g_free (alias);
            g_free (device_fn);
            g_free (mountp);
            g_free (icon_path);
        }
    }

    gcmd_key_file_save_to_file (path, key_file);

    g_key_file_free(key_file);
    g_free (path);
}

/**
 * Save devices in the given file with the file format prior to gcmd-v.1.6.
 *
 * @note This function should be deleted a while after
 * the release of gcmd-v1.6, see @link load_fav_apps @endlink.
 */
inline void save_devices_old (const gchar *fname)
{
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *i = gnome_cmd_con_list_get_all_dev (gnome_cmd_data.priv->con_list); i; i = i->next)
        {
            GnomeCmdConDevice *device = GNOME_CMD_CON_DEVICE (i->data);
            if (device && !gnome_cmd_con_device_get_autovol (device))
            {
                gchar *alias = gnome_vfs_escape_string (gnome_cmd_con_device_get_alias (device));
                gchar *device_fn = (gchar *) gnome_cmd_con_device_get_device_fn (device);
                gchar *mountp = gnome_vfs_escape_string (gnome_cmd_con_device_get_mountp (device));
                gchar *icon_path = (gchar *) gnome_cmd_con_device_get_icon_path (device);

                if (device_fn && device_fn[0] != '\0')
                    device_fn = gnome_vfs_escape_string (device_fn);
                else
                    device_fn = g_strdup ("x");

                if (icon_path && icon_path[0] != '\0')
                    icon_path = gnome_vfs_escape_string (icon_path);
                else
                    icon_path = g_strdup ("x");

                fprintf (fd, "%s %s %s %s\n", alias, device_fn, mountp, icon_path);

                g_free (alias);
                g_free (device_fn);
                g_free (mountp);
                g_free (icon_path);
            }
        }

        fclose (fd);
    }
    else
        g_warning ("Failed to open the file %s for writing", path);

    g_free (path);
}

/**
 * Save favourite applications in the given file with the file format
 * prior to gcmd-v.1.6.
 *
 * @note This function should be deleted a while after
 * the release of gcmd-v1.6, see @link load_fav_apps @endlink.
 */
static void save_fav_apps_old (const gchar *fname)
{
    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    FILE *fd = fopen (path, "w");

    if (fd)
    {
        for (GList *i = gnome_cmd_data.options.fav_apps; i; i = i->next)
        {
            GnomeCmdApp *app = (GnomeCmdApp *) i->data;
            if (app)
            {
                gchar *name = gnome_vfs_escape_string (gnome_cmd_app_get_name (app));
                gchar *cmd = gnome_vfs_escape_string (gnome_cmd_app_get_command (app));
                gchar *icon_path = gnome_vfs_escape_string (gnome_cmd_app_get_icon_path (app));
                gint target = gnome_cmd_app_get_target (app);
                gchar *pattern_string = gnome_vfs_escape_string (gnome_cmd_app_get_pattern_string (app));
                gint handles_uris = gnome_cmd_app_get_handles_uris (app);
                gint handles_multiple = gnome_cmd_app_get_handles_multiple (app);
                gint requires_terminal = gnome_cmd_app_get_requires_terminal (app);

                fprintf (fd, "%s\t%s\t%s\t%d\t%s\t%d\t%d\t%d\n",
                         name, cmd, icon_path,
                         target, pattern_string,
                         handles_uris, handles_multiple, requires_terminal);

                g_free (name);
                g_free (cmd);
                g_free (icon_path);
                g_free (pattern_string);
            }
        }

        fclose (fd);
    }
    else
        g_warning ("Failed to open the file %s for writing", path);

    g_free (path);
}

/**
 * Save favourite applications in the given file by means of GKeyFile.
 */
static void save_fav_apps (const gchar *fname)
{
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    GKeyFile *key_file;
    key_file = g_key_file_new ();

    /* Check if there are groups with the same name -> This is not
       allowed for GKeyFile but can happen when calling save_fav_apps
       the first time after an upgrade from gcmd-v1.4 to gcmd-v1.6. This
       loop should be deleted someday!*/
    for (GList *i = gnome_cmd_data.options.fav_apps; i; i = i->next)
    {
        GnomeCmdApp *app1 = (GnomeCmdApp *) i->data;
        if (app1)
        {
            gchar *group_name_to_test = g_strdup(gnome_cmd_app_get_name(app1));

            for (GList *j = i->next; j; j = j->next)
            {
                GnomeCmdApp *app2 = (GnomeCmdApp *) j->data;
                if (app2)
                {
                    gchar *group_name = g_strdup(gnome_cmd_app_get_name(app2));
                    int name_occurence = 2;

                    /* Are the names equal? -> Change the name */
                    if (!strcmp(group_name_to_test, group_name))
                    {
                        gchar *new_name = g_strdup_printf("%s_%d",
                                            gnome_cmd_app_get_name(app2),
                                            name_occurence);
                        name_occurence++;
                        gnome_cmd_app_set_name (app2, new_name);
                        g_free (new_name);
                    }
                    g_free (group_name);
                }
            }
            g_free (group_name_to_test);
        }
    }

    /* Now save the list */
    for (GList *i = gnome_cmd_data.options.fav_apps; i; i = i->next)
    {
        GnomeCmdApp *app = (GnomeCmdApp *) i->data;
        if (app)
        {
            gchar *group_name = g_strdup(gnome_cmd_app_get_name(app));

            g_key_file_set_string(key_file,group_name,"cmd",gnome_cmd_app_get_command(app));
            g_key_file_set_string(key_file,group_name,"icon",gnome_cmd_app_get_icon_path(app));
            g_key_file_set_string(key_file,group_name,"pattern",gnome_cmd_app_get_pattern_string(app));
            g_key_file_set_integer(key_file,group_name,"target",gnome_cmd_app_get_target(app));
            g_key_file_set_integer(key_file,group_name,"handles_uris",gnome_cmd_app_get_handles_uris(app));
            g_key_file_set_integer(key_file,group_name,"handles_multiple",gnome_cmd_app_get_handles_multiple(app));
            g_key_file_set_integer(key_file,group_name,"requires_terminal",gnome_cmd_app_get_requires_terminal(app));

           g_free (group_name);
        }
    }

    gcmd_key_file_save_to_file (path, key_file);

    g_key_file_free(key_file);
    g_free (path);
}


inline gboolean load_connections (const gchar *fname)
{
    guint prev_ftp_cons_no = g_list_length (gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list));

    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);
    FILE  *fd = fopen (path, "r");

    if (fd)
    {
        gchar line[1024];

        while (fgets (line, sizeof(line), fd) != NULL)
        {
            GnomeCmdConRemote *server = NULL;

            gchar *s = strchr (line, '\n');             // g_utf8_strchr (line, -1, '\n') ???

            if (s)
                *s = 0;

            switch (line[0])
            {
                case '\0':              // do not warn about empty lines
                case '#' :              // do not warn about comments
                    break;

                case 'U':               // format       U:<tab>alias<tab>uri
                    {
                        vector<string> a;

                        split(line, a, "\t");

                        if (a.size()!=3)
                        {
                            g_warning ("Invalid line in the '%s' file - skipping it...", path);
                            g_warning ("\t... %s", line);
                            break;
                        }

                        gchar *alias = gnome_vfs_unescape_string (a[1].c_str(), NULL);

                        if (gnome_cmd_data.priv->con_list->has_alias(alias))
                            g_warning ("%s: ignored duplicate entry: %s", path, alias);
                        else
                        {
                            server = gnome_cmd_con_remote_new (alias, a[2]);

                            if (!server)
                            {
                                g_warning ("Invalid URI in the '%s' file", path);
                                g_warning ("\t... %s", line);
                            }
                            else
                                gnome_cmd_data.priv->con_list->add(server);
                        }

                        g_free (alias);
                    }
                    break;

                case 'B':
                    if (server)
                    {
                        gchar name[256], path[256];
                        gint ret = sscanf (line, "B: %256s %256s\n", name, path);

                        if (ret == 2)
                            gnome_cmd_con_add_bookmark (GNOME_CMD_CON (server), gnome_vfs_unescape_string (name, NULL), gnome_vfs_unescape_string (path, NULL));
                    }
                    break;

                default:
                    g_warning ("Invalid line in the '%s' file - skipping it...", path);
                    g_warning ("\t... %s", line);
                    break;
            }
        }

        fclose (fd);
    }
    else
        if (errno != ENOENT)
            g_warning ("Failed to open the file %s for reading", path);

    g_free (path);

    if (!g_list_length (gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list)))
    {
        GnomeCmdConRemote *server = gnome_cmd_con_remote_new (_("GNOME Commander"), "ftp://anonymous@ftp.gnome.org/pub/GNOME/sources/gnome-commander/", GnomeCmdCon::NOT_REQUIRED);
        gnome_cmd_data.priv->con_list->add(server);
    }

    return fd!=NULL && g_list_length (gnome_cmd_con_list_get_all_remote (gnome_cmd_data.priv->con_list))>prev_ftp_cons_no;
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
            gchar *mountp = gnome_vfs_escape_string (gnome_cmd_con_device_get_mountp (device));
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


inline void add_vfs_volume (GnomeVFSVolume *volume)
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

#if 0
inline void add_vfs_drive (GnomeVFSDrive *drive)
{
    if (!gnome_vfs_drive_is_user_visible (drive))
        return;

    char *uri = gnome_vfs_drive_get_activation_uri (drive);

    if (!vfs_is_uri_local (uri))
    {
        g_free (uri);
        return;
    }

    char *path = gnome_vfs_drive_get_device_path (drive);
    char *icon = gnome_vfs_drive_get_icon (drive);
    char *name = gnome_vfs_drive_get_display_name (drive);
    GnomeVFSVolume *volume = gnome_vfs_drive_get_mounted_volume (drive);

    char *localpath = gnome_vfs_get_local_path_from_uri (uri);

    DEBUG('m',"name = %s\tpath = %s\turi = %s\tlocal = %s\n",name,path,uri,localpath);

    GnomeCmdConDevice *ConDev = gnome_cmd_con_device_new (name, path, localpath, icon);

    gnome_cmd_con_device_set_autovol (ConDev, TRUE);

    gnome_cmd_data.priv->con_list->add(ConDev);

    g_free (path);
    g_free (uri);
    g_free (icon);
    g_free (name);
    g_free (localpath);

    gnome_vfs_volume_unref (volume);
}
#endif

static void volume_mounted (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume)
{
    add_vfs_volume (volume);
}


static void volume_unmounted (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSVolume *volume)
{
    remove_vfs_volume (volume);
}

#if 0
static void drive_connected (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSDrive *drive)
{
    add_vfs_drive (drive);
}

static void drive_disconnected (GnomeVFSVolumeMonitor *volume_monitor, GnomeVFSDrive *drive)
{
    // TODO: Remove from Drives combobox
}
#endif

inline void set_vfs_volume_monitor ()
{
    monitor = gnome_vfs_get_volume_monitor ();

    g_signal_connect (monitor, "volume-mounted", G_CALLBACK (volume_mounted), NULL);
    g_signal_connect (monitor, "volume-unmounted", G_CALLBACK (volume_unmounted), NULL);
#if 0
    g_signal_connect (monitor, "drive-connected", G_CALLBACK (drive_connected), NULL);
    g_signal_connect (monitor, "drive-disconnected", G_CALLBACK (drive_disconnected), NULL);
#endif
}


inline void load_vfs_auto_devices ()
{
    GnomeVFSVolumeMonitor *monitor = gnome_vfs_get_volume_monitor ();
    GList *volumes = gnome_vfs_volume_monitor_get_mounted_volumes (monitor);

    for (GList *l = volumes; l; l = l->next)
    {
        add_vfs_volume ((GnomeVFSVolume *) l->data);
        gnome_vfs_volume_unref ((GnomeVFSVolume *) l->data);
    }
    g_list_free (volumes);

#if 0
    GList *drives = gnome_vfs_volume_monitor_get_connected_drives (monitor);
    for (GList *l = drives; l; l = l->next)
    {
        add_vfs_drive (l->data);
        gnome_vfs_drive_unref (l->data);
    }
    g_list_free (drives);
#endif
}


/**
 * This function reads the given file and sets up additional devices by
 * means of GKeyFile.
 */
static inline void load_devices (const gchar *fname)
{
    GKeyFile *keyfile;
    gsize length;
    gchar **groups;
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);

    keyfile = gcmd_key_file_load_from_file(path, 0);
    g_return_if_fail(keyfile != NULL);

    groups = g_key_file_get_groups (keyfile, &length);

    for (guint i = 0; i < length; i++)
    {
        gchar *alias     = NULL;
        gchar *device_fn = NULL;
        gchar *mountp    = NULL;
        gchar *icon_path = NULL;
        GError *error = NULL;

        alias     = g_strdup(groups[i]);
        device_fn = g_key_file_get_string (keyfile, groups[i], "device", &error);
        mountp    = g_key_file_get_string (keyfile, groups[i], "mount_point", &error);
        icon_path = g_key_file_get_string (keyfile, groups[i], "icon_path", &error);

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

    g_free (path);
}

/**
 * This function reads the given file and sets up additional devices.
 *
 * @note Beginning with gcmd-v1.6 GKeyFile is used for storing and
 * loading configuration files. For compatibility reasons, this
 * functions tries to load devices from the given file
 * with the old format prior to gcmd-v1.6. Therefore it checks if the
 * very first letter in fname is alphanumeric. If "yes", the given file
 * has a pre-v1.6 format and the file is loaded as in gcmd-v1.4. Also, a
 * backup configuration is stored in @c devices.backup in the old file
 * format. If "no", then nothing happens and FALSE is returned.
 *
 * @note In later versions of gcmd (later than v1.6), this function
 * might be removed, because when saving the configuration in @link
 * save_device() @endlink, GKeyFile is used and the old file
 * format isn't used anymore.
 *
 * @returns FALSE if the very first letter of the given file is not
 * alphanumeric and TRUE if it is alphanumeric.
 */
static gboolean load_devices_old (const gchar *fname)
{
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);

    ifstream f(path);
    string line;
    int i = 0;

    while (getline(f,line))
    {
        /* Is the file using the new storage format? If yes, stop here
         * and return FALSE, i.e. old file format is not used*/
        gchar **a = g_strsplit_set (line.c_str()," ",-1);
        if (i == 0)
        {
            if (!isalnum(a[0][0]))
            {
                g_strfreev (a);
                g_free (path);
                return FALSE;
            }
            i++;
        }

        if (g_strv_length (a) == 4)
        {
            gchar *alias  = gnome_vfs_unescape_string (a[0], NULL);
            gchar *device_fn  = gnome_vfs_unescape_string (a[1], NULL);
            gchar *mountp  = gnome_vfs_unescape_string (a[2], NULL);
            gchar *icon_path  = gnome_vfs_unescape_string (a[3], NULL);

            gnome_cmd_data.priv->con_list->add (gnome_cmd_con_device_new (alias, device_fn, mountp, icon_path));

            g_free (alias);
            g_free (device_fn);
            g_free (mountp);
            g_free (icon_path);
        }
    }

    g_free (path);

    load_vfs_auto_devices ();
    save_devices_old ("devices.backup");
    return TRUE;
}


/**
 * This function reads the given file and sets up favourite applications
 * by means of GKeyFile and by filling gnome_cmd_data.options.fav_apps.
 */
static void load_fav_apps (const gchar *fname)
{
    GKeyFile *keyfile;
    gsize length;
    gchar **groups;
    gchar *path = config_dir ?
        g_build_filename (config_dir, fname, NULL) :
        g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);

    gnome_cmd_data.options.fav_apps = NULL;

    keyfile = gcmd_key_file_load_from_file(path, 0);
    g_return_if_fail(keyfile != NULL);

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
        cmd               = g_key_file_get_string (keyfile, groups[i], "cmd", &error);
        icon_path         = g_key_file_get_string (keyfile, groups[i], "icon", &error);
        pattern           = g_key_file_get_string (keyfile, groups[i], "pattern", &error);
        target            = g_key_file_get_integer (keyfile, groups[i], "target", &error);
        handles_uris      = g_key_file_get_boolean (keyfile, groups[i], "handles_uris", &error);
        handles_multiple  = g_key_file_get_boolean (keyfile, groups[i], "handles_multiple", &error);
        requires_terminal = g_key_file_get_boolean (keyfile, groups[i], "requires_terminal", &error);

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
    g_free(path);
    g_strfreev(groups);
    g_key_file_free(keyfile);
}

/**
 * This function reads the given file and sets up favourite applications
 * by filling gnome_cmd_data.options.fav_apps.
 *
 * @note Beginning with gcmd-v1.6 GKeyFile is used for storing and
 * loading configuration files. For compatibility reasons, this
 * functions tries to load favourite applications from the given file
 * with the old format prior to gcmd-v1.6. Therefore it checks if the
 * very first letter in fname is alphanumeric. If yes, the given file
 * has a pre-v1.6 format and the file is loaded as in gcmd-v1.4. Also, a
 * backup configuration is stored in @c fav-apps.backup in the old file
 * format. If the result is no, then nothing happens and FALSE is
 * returned.
 *
 * @note In later versions of gcmd (later than v1.6), this function
 * might be removed, because when saving the configuration in @link
 * save_fav_apps() @endlink, GKeyFile is used and the old file
 * format isn't used anymore.
 *
 * @returns FALSE if the very first letter of the given file is not
 * alphanumeric and TRUE if it is alphanumeric.
 */
static gboolean load_fav_apps_old (const gchar *fname)
{
    gnome_cmd_data.options.fav_apps = NULL;
    gchar *path = config_dir ? g_build_filename (config_dir, fname, NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, fname, NULL);

    ifstream f(path);
    string line;
    int i = 0;

    while (getline(f,line))
    {
        /* Is the file using the new storage format? If yes, stop here
         * and return FALSE, i.e. old file format is not used */
        gchar **a = g_strsplit_set (line.c_str()," \t",-1);
        if (i == 0)
        {
            if (!isalnum(a[0][0]))
            {
                g_strfreev (a);
                g_free (path);
                return FALSE;
            }
            i++;
        }

        if (g_strv_length (a)==8)
        {
            guint target, handles_uris, handles_multiple, requires_terminal;

            if (string2uint (a[3], target) &&
                string2uint (a[5], handles_uris) &&
                string2uint (a[6], handles_multiple) &&
                string2uint (a[7], requires_terminal))
            {
                gchar *name      = gnome_vfs_unescape_string (a[0], NULL);
                gchar *cmd       = gnome_vfs_unescape_string (a[1], NULL);
                gchar *icon_path = gnome_vfs_unescape_string (a[2], NULL);
                gchar *pattern   = gnome_vfs_unescape_string (a[4], NULL);

                gnome_cmd_data.options.fav_apps = g_list_append (
                    gnome_cmd_data.options.fav_apps,
                    gnome_cmd_app_new_with_values (
                        name, cmd, icon_path, (AppTarget) target, pattern, handles_uris, handles_multiple, requires_terminal));

                g_free (name);
                g_free (cmd);
                g_free (icon_path);
                g_free (pattern);
            }
        }

        g_strfreev (a);
    }

    g_free (path);
    save_fav_apps_old ("fav-apps.backup");
    return TRUE;
}


inline void GnomeCmdData::gnome_cmd_data_set_string_history (const gchar *format, GList *strings)
{
    gchar key[128];

    for (gint i=0; strings; strings=strings->next, ++i)
    {
        snprintf (key, sizeof (key), format, i);
        gnome_cmd_data_set_string (key, (gchar *) strings->data);
    }
}

/**
 * This function converts a GList into a NULL terminated array of char pointers.
 * This array is stored into the given GSettings key.
 * @returns The return value of g_settings_set_strv if the length of the GList is > 0, else true.
 */
gboolean GnomeCmdData::set_gsettings_string_array_from_glist (GSettings *settings, const gchar *key, GList *strings)
{
    gboolean rv = true;
    guint number_of_strings = g_list_length (strings);
    if (number_of_strings > 0)
    {
        gint ii;
        gchar** str_array;
        str_array = (gchar**) g_malloc ((number_of_strings + 1) * sizeof(char*));
        GList *str_list = strings;

        // build up a char array for storage in GSettings
        for (ii = 0; str_list; str_list = str_list->next, ++ii)
        {
            str_array[ii] = g_strdup((const gchar*) str_list->data);
        }
        str_array[ii] = NULL;

        // store the NULL terminated str_array in GSettings
        rv = g_settings_set_strv(settings, key, str_array);

        g_free(str_array);
    }
    return rv;
}


inline void GnomeCmdData::save_cmdline_history()
{
    if (!cmdline_visibility)
        return;

    cmdline_history = gnome_cmd_cmdline_get_history (main_win->get_cmdline());

    set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, cmdline_history);
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


inline GList* GnomeCmdData::get_list_from_gsettings_string_array (GSettings *settings, const gchar *key)
{
    GList *list = NULL;

    gchar** gsettings_array;
    gsettings_array = g_settings_get_strv (settings, key);

    for(gint i = 0; gsettings_array[i]; ++i)
    {
        gchar *value;
        value = g_strdup (gsettings_array[i]);
        list = g_list_append (list, value);
    }
    g_free(gsettings_array);

    return list;
}


inline void GnomeCmdData::load_cmdline_history()
{
    g_list_free(cmdline_history);

    cmdline_history = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY);
}


inline void GnomeCmdData::load_intviewer_defaults()
{
    intviewer_defaults.text_patterns = get_list_from_gsettings_string_array (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT);
    intviewer_defaults.hex_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_HEX);
    intviewer_defaults.case_sensitive = g_settings_get_boolean (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_CASE_SENSITIVE);
    intviewer_defaults.search_mode = g_settings_get_enum (options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_MODE);
}


inline void GnomeCmdData::load_rename_history()
{
    gint size;
    GList *from=NULL, *to=NULL, *csens=NULL;

    advrename_defaults.width = gnome_cmd_data_get_int ("/advrename/width", 640);
    advrename_defaults.height = gnome_cmd_data_get_int ("/advrename/height", 400);

    size = gnome_cmd_data_get_int ("/template-history/size", 0);
    advrename_defaults.templates = load_string_history ("/template-history/template%d", size);

    advrename_defaults.default_profile.counter_start = gnome_cmd_data_get_int ("/advrename/counter_start", 1);
    advrename_defaults.default_profile.counter_width = gnome_cmd_data_get_int ("/advrename/counter_precision", 1);
    advrename_defaults.default_profile.counter_step = gnome_cmd_data_get_int ("/advrename/counter_increment", 1);

    size = gnome_cmd_data_get_int ("/rename-history/size", 0);

    GList *tmp_from = from = load_string_history ("/rename-history/from%d", size);
    GList *tmp_to = to = load_string_history ("/rename-history/to%d", size);
    GList *tmp_csens = csens = load_string_history ("/rename-history/csens%d", size);

    for (; tmp_from && size > 0; --size)
    {
        advrename_defaults.default_profile.regexes.push_back(GnomeCmd::ReplacePattern((gchar *) tmp_from->data,
                                                                                      (gchar *) tmp_to->data,
                                                                                      *((gchar *) tmp_csens->data)=='T'));
        tmp_from = tmp_from->next;
        tmp_to = tmp_to->next;
        tmp_csens = tmp_csens->next;
    }

    g_list_free (from);
    g_list_free (to);
    g_list_free (csens);
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


GnomeCmdData::GnomeCmdData(): search_defaults(selections)
{
    quick_connect = NULL;

    XML_cfg_has_connections = FALSE;
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

    use_gcmd_block = FALSE;

    main_win_width = 600;
    main_win_height = 400;

    main_win_state = GDK_WINDOW_STATE_MAXIMIZED;

    umask = ::umask(0);
    ::umask(umask);
}


void GnomeCmdData::free()
{
    if (priv)
    {
        // free the connections
        // g_object_unref (priv->con_list);

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

/**
 * This method converts user settings from gcmds old config files prior to v1.6.0 to
 * GSettings. Therefore, it first looks for those files in question and then converts the data.
 */
void GnomeCmdData::migrate_all_data_to_gsettings()
{
    guint temp_value;
    options.gcmd_settings = gcmd_settings_new();
    gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);
    gchar *package_config_path = gnome_config_get_real_path(PACKAGE);

    //TODO: migrate xml stuff
    /////////////////////////////////////////////////////////////
    //// Data migration from .gnome-commander/gnome-commander.xml
    /////////////////////////////////////////////////////////////
    //FILE *fd = fopen (xml_cfg_path, "r");
    //if (fd)
    //{
    //    // TODO: Data migration from xml-file
    //    fclose (fd);
    //
    //}
    //else
    //{
    //    g_warning ("Failed to open the file %s for reading, skipping data migration", xml_cfg_path);
    //
    //    options.size_disp_mode = (GnomeCmdSizeDispMode) g_settings_get_int (gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    //}
    g_free (xml_cfg_path);

    ///////////////////////////////////////////////////////////////////////
    // Data migration from .gnome2/gnome-commander, created by gnome_config
    ///////////////////////////////////////////////////////////////////////
    FILE *fd = fopen (package_config_path, "r");
    if (fd)
    {
        // size_disp_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/size_disp_mode", GNOME_CMD_SIZE_DISP_MODE_POWERED),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
        // perm_disp_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/perm_disp_mode", GNOME_CMD_PERM_DISP_MODE_TEXT),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);
        // layout
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/layout", GNOME_CMD_LAYOUT_MIME_ICONS),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);
        //list-row-height
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/list_row_height", 16),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);
        //date_disp_mode
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/options/date_disp_mode", "%x %R"),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
        //show_unknown
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_unknown", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN);
        //show_regular
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_regular", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR);
        //show_directory
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_directory", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY);
        //show_fifo
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_fifo", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_FIFO);
        //show_socket
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_socket", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SOCKET);
        //show_char_device
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_char_device", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_CHARACTER_DEVICE);
        //show_block_device
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_block_device", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BLOCK_DEVICE);
        //show_symbolic_link
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/show_symbolic_link", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMBOLIC_LINK);
        //hidden_filter
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/hidden_filter", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_DOTFILE);
        //hidden_filter
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/backup_filter", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP);
        //backup_pattern
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/defaults/backup_pattern", "*~;*.bak"),
                                                        options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN);
        //list_font
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/options/list_font", "DejaVu Sans Mono 8"),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);
        //ext_disp_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/ext_disp_mode", GNOME_CMD_EXT_DISP_BOTH),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
        //left_mouse_button_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/left_mouse_button_mode", LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM);
        //left_mouse_button_unselects
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/left_mouse_button_unselects", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS);
        //right_mouse_button_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/right_mouse_button_mode", RIGHT_BUTTON_POPUPS_MENU),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE);
        //icon_size
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/icon_size", 16),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
        //dev-icon_size
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/dev_icon_size", 16),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_DEV_ICON_SIZE);
        //icon_scale_quality
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/icon_scale_quality", GDK_INTERP_HYPER),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_ICON_SCALE_QUALITY);
        //theme_icon_dir
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/options/theme_icon_dir", "/usr/local/share/pixmaps/gnome-commander/mime-icons"),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_MIME_ICON_DIR);
        //cmdline_history_length
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/cmdline_history_length", 16),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH);
        //middle_mouse_button_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/middle_mouse_button_mode", MIDDLE_BUTTON_GOES_UP_DIR),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE);
        //list_orientation
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/list_orientation", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION);
        //conbuttons_visibility
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/conbuttons_visibility", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS);
        //con_list_visibility
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/con_list_visibility", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST);
        //cmdline_visibility
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/cmdline_visibility", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE);
        //toolbar_visibility
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/toolbar_visibility", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR);
        //buttonbar_visibility
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/buttonbar_visibility", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR);
        //gui_update_rate
        temp_value = gnome_cmd_data_get_int ("/options/gui_update_rate", 100);
        if (temp_value < MIN_GUI_UPDATE_RATE)
            temp_value = MIN_GUI_UPDATE_RATE;
        if (temp_value > MAX_GUI_UPDATE_RATE)
            temp_value = MAX_GUI_UPDATE_RATE;
        migrate_data_int_value_into_gsettings(temp_value, options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE);
        //symlink_prefix
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/options/symlink_prefix", ""),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SYMLINK_PREFIX);
        //main_win_pos_x
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/main_win_pos_x", 0),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_POS_X);
        //main_win_pos_y
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/main_win_pos_y", 25),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_POS_Y);
        //save_dirs_on_exit
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/save_dirs_on_exit", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT);
        //save_tabs_on_exit
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/save_tabs_on_exit", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT);
        //save_dir_history_on_exit
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/save_dir_history_on_exit", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT);
        //always_show_tabs
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/options/always_show_tabs", FALSE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS);
        //tab_lock_indicator
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/tab_lock_indicator", TAB_LOCK_ICON),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR);
        //main_win_state
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/options/main_win_state", (gint) GDK_WINDOW_STATE_MAXIMIZED),
                                                        options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_STATE);
        //delete
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/confirm/delete", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE);
        //delete_default
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/confirm/delete_default", 1),
                                                        options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT);
        //confirm_copy_overwrite
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/confirm/copy_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                                                        options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE);
        //confirm_move_overwrite
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/confirm/move_overwrite", GNOME_CMD_CONFIRM_OVERWRITE_QUERY),
                                                        options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE);
        //confirm_mouse_dnd
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/confirm/confirm_mouse_dnd", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP);
        //select_dirs
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/sort/select_dirs", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
        //case_sensitive
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/sort/case_sensitive", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);
        //mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/colors/mode", GNOME_CMD_COLOR_GREEN_TIGER),
                                                        options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);
        GdkColor *color = g_new0 (GdkColor, 1);
        //norm_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/norm_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_FG);
        //norm_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/norm_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_NORM_BG);
        //alt_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/alt_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_FG);
        //alt_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/alt_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_ALT_BG);
        //sel_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/sel_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_FG);
        //sel_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/sel_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_SEL_BG);
        //curs_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/curs_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_FG);
        //curs_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/curs_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_CURS_BG);
        //use_ls_colors
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/colors/use_ls_colors", TRUE) ? 1 : 0,
                                              options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);
        //ls_colors_black_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_black_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_FG);
        //ls_colors_black_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_black_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLACK_BG);
        //ls_colors_red_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_red_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_FG);
        //ls_colors_red_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_red_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_RED_BG);
        //ls_colors_green_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_green_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_FG);
        //ls_colors_green_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_green_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_GREEN_BG);
        //ls_colors_yellow_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_yellow_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_FG);
        //ls_colors_yellow_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_yellow_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_YELLOW_BG);
        //ls_colors_blue_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_blue_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_FG);
        //ls_colors_blue_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_blue_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_BLUE_BG);
        //ls_colors_magenta_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_magenta_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_FG);
        //ls_colors_magenta_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_magenta_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_MAGENTA_BG);
        //ls_colors_cyan_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_cyan_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_FG);
        //ls_colors_cyan_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_cyan_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_CYAN_BG);
        //ls_colors_white_fg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_white_fg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_FG);
        //ls_colors_white_bg
        gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_white_bg", color);
        migrate_data_string_value_into_gsettings(gdk_color_to_string (color),
                                                 options.gcmd_settings->colors, GCMD_SETTINGS_LS_COLORS_WHITE_BG);
        //honor_expect_uris
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/honor_expect_uris", FALSE) ? 1 : 0,
                                              options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD);
        //allow_multiple_instances
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/allow_multiple_instances", FALSE) ? 1 : 0,
                                              options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
        //use_internal_viewer
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/use_internal_viewer", TRUE) ? 1 : 0,
                                              options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER);
        //alt_quick_search
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/alt_quick_search", FALSE) ? 1 : 0,
                                              options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
        //quick_search_exact_match_begin
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/quick_search_exact_match_begin", TRUE) ? 1 : 0,
                                              options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
        //quick_search_exact_match_end
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/quick_search_exact_match_end", FALSE) ? 1 : 0,
                                              options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);
        //skip_mounting
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/skip_mounting", FALSE) ? 1 : 0,
                                              options.gcmd_settings->general, GCMD_SETTINGS_DEV_SKIP_MOUNTING);
        //viewer
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/programs/viewer", "gedit %s"),
                                                        options.gcmd_settings->programs, GCMD_SETTINGS_VIEWER_CMD);
        //editor
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/programs/editor", "gedit %s"),
                                                        options.gcmd_settings->programs, GCMD_SETTINGS_EDITOR_CMD);
        //differ
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/programs/differ", "meld %s"),
                                                        options.gcmd_settings->programs, GCMD_SETTINGS_DIFFER_CMD);
        //sendto
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/programs/sendto", "nautilus-sendto %s"),
                                                        options.gcmd_settings->programs, GCMD_SETTINGS_SENDTO_CMD);
        //terminal_open
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/programs/terminal_open", "gnome-terminal"),
                                                        options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_CMD);
        //terminal_exec
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/programs/terminal_exec", "gnome-terminal -e %s"),
                                                        options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_EXEC_CMD);
        //use_gcmd_block
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/use_gcmd_block", FALSE) ? 1 : 0,
                                              options.gcmd_settings->programs, GCMD_SETTINGS_USE_GCMD_BLOCK);
        //only_icon
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/devices/only_icon", FALSE) ? 1 : 0,
                                              options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON);
        //mainmenu_visibility
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/programs/mainmenu_visibility", FALSE) ? 1 : 0,
                                              options.gcmd_settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY);
        //uri
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/quick-connect/uri", "ftp://anonymous@ftp.gnome.org/pub/GNOME/"),
                                                        options.gcmd_settings->network, GCMD_SETTINGS_QUICK_CONNECT_URI);
        //ftp_anonymous_password
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/network/ftp_anonymous_password", "you@provider.com"),
                                                        options.gcmd_settings->network, GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD);
        //auto_load0 -> migrate the string into a gsettings string array
        if (gnome_cmd_data_get_int ("/plugins/count", 0))
        {
            migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/plugins/auto_load0", "libfileroller.so"),
                                                            options.gcmd_settings->plugins, GCMD_SETTINGS_PLUGINS_AUTOLOAD);
        }
        //cmdline-history/lineXX -> migrate the string into a gsettings string array
        GList *cmdline_history_for_migration = NULL;
        cmdline_history_for_migration = load_string_history ("/cmdline-history/line%d", -1);
        if (cmdline_history_for_migration && cmdline_history_for_migration->data)
        {
            GList *list_pointer;
            list_pointer = cmdline_history_for_migration;
            gint ii, array_size = 0;
            for (;list_pointer; list_pointer=list_pointer->next)
                ++array_size;
            gchar** str_array;
            str_array = (gchar**) g_malloc ((array_size + 1)*sizeof(char*));

            list_pointer = cmdline_history_for_migration;
            for (ii = 0; list_pointer; list_pointer=list_pointer->next, ++ii)
            {
                str_array[ii] = g_strdup((const gchar*) list_pointer->data);
            }
            str_array[ii] = NULL;
            g_settings_set_strv(options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, str_array);

            g_free(str_array);
            g_list_free(cmdline_history_for_migration);
        }
        /////////////////////// internal viewer specs ///////////////////////
        //case_sens
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/internal_viewer/case_sens", FALSE) ? 1 : 0,
                                              options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_CASE_SENSITIVE);
        //last_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/last_mode", 0),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_MODE);
        //charset
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/internal_viewer/charset", "UTF8"),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_CHARSET);
        //fixed_font_name
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/internal_viewer/fixed_font_name", "Monospace"),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_FIXED_FONT_NAME);
        //variable_font_name
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/internal_viewer/variable_font_name", "Sans"),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_VARIABLE_FONT_NAME);
        //hex_offset_display
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/internal_viewer/hex_offset_display", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_DISPLAY_HEX_OFFSET);
        //wrap_mode
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_bool ("/internal_viewer/wrap_mode", TRUE) ? 1 : 0,
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_WRAP_MODE);
        //font_size
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/font_size", 12),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_FONT_SIZE);
        //tab_size
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/tab_size", 8),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_TAB_SIZE);
        //binary_bytes_per_line
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/binary_bytes_per_line", 80),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_BINARY_BYTES_PER_LINE);
        //x
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/x", 20),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_X_OFFSET);
        //y
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/y", 20),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_Y_OFFSET);
        //width
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/width", 400),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_WIDTH);
        //height
        migrate_data_int_value_into_gsettings(gnome_cmd_data_get_int ("/internal_viewer/height", 400),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_HEIGHT);
        //text_pattern0
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/internal_viewer/text_pattern0", ""),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_TEXT);
        //hex_pattern0
        migrate_data_string_value_into_gsettings(gnome_cmd_data_get_string ("/internal_viewer/hex_pattern0", ""),
                                                        options.gcmd_settings->internalviewer, GCMD_SETTINGS_IV_SEARCH_PATTERN_HEX);
        g_free(color);
        // ToDo: Move old xml-file to ~/.gnome-commander/gnome-commander.xml.backup
        //       à la save_devices_old ("devices.backup");
        //       and move .gnome2/gnome-commander to .gnome2/gnome-commander.backup
    }
    g_free(package_config_path);

    // Activate gcmd gsettings signals
    gcmd_connect_gsettings_signals(gnome_cmd_data.options.gcmd_settings);
}

/**
 * This function checks if the given GSettings keys enholds a valid color string. If not,
 * the keys value is resetted to the default value.
 * @returns TRUE if the current value is resetted by the default value, else FALSE
 */
gboolean GnomeCmdData::set_valid_color_string(GSettings *settings, const char* key)
{
    gchar *colorstring;
    gboolean return_value;

    colorstring = g_settings_get_string (settings, key);
    if (!is_valid_color_string(colorstring))
    {
        GVariant *variant;
        variant = g_settings_get_default_value (settings, key);
        g_warning("Illegal color string \'%s\' for gsettings key %s. Resetting to default value \'%s\'",
                  colorstring, key, g_variant_get_string(variant, NULL));
        g_settings_set_string (settings, key, g_variant_get_string(variant, NULL));
        g_variant_unref (variant);
        return_value = TRUE;
    }
    else
        return_value = FALSE;

    g_free(colorstring);

    return return_value;
}

void GnomeCmdData::load()
{
    gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);
    gchar *colorstring;

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

    main_win_width = get_int ("/gnome-commander-size/main_win/width", 600);
    main_win_height = get_int ("/gnome-commander-size/main_win/height", 400);

    for (gint i=0; i<GnomeCmdFileList::NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/fs_col_width%d", i);
        fs_col_width[i] = get_int (tmp, GnomeCmdFileList::get_column_default_width((GnomeCmdFileList::ColumnID) i));
        g_free (tmp);
    }

    options.color_mode = gcmd_owner.is_root() ? (GnomeCmdColorMode) g_settings_get_enum (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME)
                                              : (GnomeCmdColorMode) GNOME_CMD_COLOR_DEEP_BLUE;

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

    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_black_fg", options.ls_colors_palette.black_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_black_bg", options.ls_colors_palette.black_bg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_red_fg", options.ls_colors_palette.red_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_red_bg", options.ls_colors_palette.red_bg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_green_fg", options.ls_colors_palette.green_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_green_bg", options.ls_colors_palette.green_bg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_yellow_fg", options.ls_colors_palette.yellow_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_yellow_bg", options.ls_colors_palette.yellow_bg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_blue_fg", options.ls_colors_palette.blue_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_blue_bg", options.ls_colors_palette.blue_bg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_magenta_fg", options.ls_colors_palette.magenta_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_magenta_bg", options.ls_colors_palette.magenta_bg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_cyan_fg", options.ls_colors_palette.cyan_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_cyan_bg", options.ls_colors_palette.cyan_bg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_white_fg", options.ls_colors_palette.white_fg);
    gnome_cmd_data_get_color_gnome_config ("/colors/ls_colors_white_bg", options.ls_colors_palette.white_bg);

    options.save_dirs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT);
    options.save_tabs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT);
    options.save_dir_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT);

    options.always_show_tabs = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS);
    options.tab_lock_indicator = (TabLockIndicator) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR);

    options.backup_pattern = g_settings_get_string (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN);
    options.backup_pattern_list = patlist_new (options.backup_pattern);

    main_win_state = (GdkWindowState) g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_STATE);

    priv->ftp_anonymous_password = g_settings_get_string (options.gcmd_settings->network, GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD);

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
#if GTK_CHECK_VERSION (2, 10, 0)
                            {GDK_SUPER_MASK, "<super>"},
                            {GDK_SUPER_MASK, "<win>"},
                            {GDK_SUPER_MASK, "<mod4>"},
                            {GDK_HYPER_MASK, "<hyper>"},
                            {GDK_META_MASK, "<meta>"},
#endif
                            {GDK_MOD1_MASK, "<mod1>"},
                            {GDK_MOD4_MASK, "<super>"},
                            {GDK_MOD4_MASK, "<win>"},
                            {GDK_MOD4_MASK, "<mod4>"}
                           };

    load_data (gdk_modifiers_names, gdk_mod_names_data, G_N_ELEMENTS(gdk_mod_names_data));

    load_cmdline_history();
    //load_dir_history ();

    priv->con_list = gnome_cmd_con_list_new ();

    priv->con_list->lock();
    if (load_devices_old ("devices") == FALSE)
        load_devices ("devices");

    if (!gnome_cmd_xml_config_load (xml_cfg_path, *this))
    {
        load_rename_history();

        // add a few default templates here - for new users
#if GLIB_CHECK_VERSION (2, 14, 0)
        {
            AdvrenameConfig::Profile p;

            p.name = _("Audio Files");
            p.template_string = "$T(Audio.AlbumArtist) - $T(Audio.Title).$e";
            p.regexes.push_back(GnomeCmd::ReplacePattern("[ _]+", " ", FALSE));
            p.regexes.push_back(GnomeCmd::ReplacePattern("[fF]eat\\.", "fr.", TRUE));

            advrename_defaults.profiles.push_back(p);

            p.reset();
            p.name = _("CamelCase");
            p.template_string = "$N";
            p.regexes.push_back(GnomeCmd::ReplacePattern("\\s*\\b(\\w)(\\w*)\\b", "\\u\\1\\L\\2\\E", FALSE));
            p.regexes.push_back(GnomeCmd::ReplacePattern("\\.(.+)$", ".\\L\\1", FALSE));

            advrename_defaults.profiles.push_back(p);
        }
#endif

    }

    if (!XML_cfg_has_connections)
        load_connections ("connections");

    priv->con_list->unlock();

    gchar *quick_connect_uri = g_settings_get_string(options.gcmd_settings->network, GCMD_SETTINGS_QUICK_CONNECT_URI);
    quick_connect = gnome_cmd_con_remote_new (NULL, quick_connect_uri);
    g_free (quick_connect_uri);

    // if number of registered user actions does not exceed 10 (nothing has been read), try to read old cfg file
    if (gcmd_user_actions.size()<10)
        gcmd_user_actions.load("key-bindings");

    load_intviewer_defaults();
    load_auto_load_plugins();

    set_vfs_volume_monitor ();

    g_free (xml_cfg_path);
}

/**
 * This method returns an int value which is either the given user_value or,
 * the default integer value of the given GSettings key. The user_value is returned
 * if it is different from the default value of the GSettings key.
 * @param user_value An integer value
 * @param settings A GSettings pointer
 * @param key a GSettings key path given as a char array
 */
gint GnomeCmdData::migrate_data_int_value_into_gsettings(int user_value, GSettings *settings, const char *key)
{
    GVariant *variant;
    gint default_value;
    gint return_value;

    variant = g_settings_get_default_value (settings, key);

    switch (g_variant_classify(variant))
    {
        // In all of the following cases it is assumed that the value behind 'default_value' is the actual
        // default value, i.e. nobody changed the given key before gcmd data migration was started.
        case G_VARIANT_CLASS_STRING:
        {
            default_value = g_settings_get_enum (settings, key);

            if (user_value != default_value)
                g_settings_set_enum (settings, key, user_value);

            return_value = g_settings_get_enum(settings, key);

            break;
        }
        case G_VARIANT_CLASS_UINT32:
        {
            default_value = g_variant_get_uint32 (variant);

            if (user_value != default_value)
                g_settings_set_uint (settings, key, user_value);

            return_value = g_settings_get_uint(settings, key);

            break;
        }
        case G_VARIANT_CLASS_INT32:
        {
            default_value = g_variant_get_int32 (variant);

            if (user_value != default_value)
                g_settings_set_int (settings, key, user_value);

            return_value = g_settings_get_int(settings, key);

            break;
        }
        case G_VARIANT_CLASS_BOOLEAN:
        {
            gboolean bdef_value;
            gboolean buser_value;
            bdef_value = g_variant_get_boolean (variant);
            buser_value = user_value == 1 ? TRUE : FALSE;

            if (buser_value != bdef_value)
                g_settings_set_boolean (settings, key, buser_value);

            return_value = g_settings_get_boolean (settings, key) ? 1 : 0;

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

/**
 * This method sets the value of a given GSettings key to the string stored in user_value or to the default value,
 * depending on the value of user_value. If the class of the key is not a string but a string array, the first
 * entry of the array is set to user_value.
 * @returns FALSE if an error occured setting the key value to a new string.
 */
gboolean GnomeCmdData::migrate_data_string_value_into_gsettings(const char* user_value, GSettings *settings, const char *key)
{
    GVariant *variant;
    gchar *default_value;
    gint rv = true;

    variant = g_settings_get_default_value (settings, key);

    if (g_variant_classify(variant) == G_VARIANT_CLASS_STRING)
    {
        // In the following it is assumed that the value behind 'default_value' is the actual
        // default value, i.e. nobody changed the given key before gcmd data migration was started.
        default_value = g_settings_get_string (settings, key);

        if (strcmp(user_value, default_value) != 0)
            rv = g_settings_set_string (settings, key, user_value);
    }
    else if (g_variant_classify(variant) == G_VARIANT_CLASS_ARRAY)
    {
        gchar** str_array;
        str_array = (gchar**) g_malloc (2*sizeof(char*));
        str_array[0] = g_strdup(user_value);
        str_array[1] = NULL;

        rv = (gint) g_settings_set_strv(settings, key, str_array);

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

void GnomeCmdData::load_more()
{
    if (load_fav_apps_old ("fav-apps") == FALSE)
        load_fav_apps("fav-apps");
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

    gnome_config_set_int ("/gnome-commander-size/main_win/width", main_win_width);
    gnome_config_set_int ("/gnome-commander-size/main_win/height", main_win_height);

    for (gint i=0; i<GnomeCmdFileList::NUM_COLUMNS; i++)
    {
        gchar *tmp = g_strdup_printf ("/gnome-commander-size/column-widths/fs_col_width%d", i);
        gnome_config_set_int (tmp, fs_col_width[i]);
        g_free (tmp);
    }

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT, &(options.save_dirs_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT, &(options.save_tabs_on_exit));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT, &(options.save_dir_history_on_exit));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS, &(options.always_show_tabs));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR, options.tab_lock_indicator);

    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN, options.backup_pattern);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_STATE, &(main_win_state));

    set_gsettings_when_changed      (options.gcmd_settings->network, GCMD_SETTINGS_FTP_ANONYMOUS_PASSWORD, priv->ftp_anonymous_password);

    save_cmdline_history();
    //write_dir_history ();

    save_devices ("devices");
    save_fav_apps ("fav-apps");
    save_intviewer_defaults();

    {
        gchar *xml_cfg_path = config_dir ? g_build_filename (config_dir, PACKAGE ".xml", NULL) : g_build_filename (g_get_home_dir (), "." PACKAGE, PACKAGE ".xml", NULL);

        ofstream f(xml_cfg_path);
        XML::xstream xml(f);

        xml << XML::comment("Created with GNOME Commander (http://gcmd.github.io/)");
        xml << XML::tag("GnomeCommander") << XML::attr("version") << VERSION;

        xml << *main_win;

        xml << XML::tag("History");

        if (options.save_dir_history_on_exit)
        {
            xml << XML::tag("Directories");

            for (GList *i=gnome_cmd_con_get_dir_history (priv->con_list->get_home())->ents; i; i=i->next)
                xml << XML::tag("Directory") << XML::attr("path") << XML::escape((const gchar *) i->data) << XML::endtag();

            xml << XML::endtag("Directories");
        }

        xml << XML::endtag("History");

        xml << advrename_defaults;
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
        for (vector<Selection>::iterator i=selections.begin(); i!=selections.end(); ++i)
            xml << *i;
        xml << XML::endtag("Selections");

        xml << gcmd_user_actions;

        xml << XML::endtag("GnomeCommander");

        g_free (xml_cfg_path);
    }

    save_auto_load_plugins();

    gnome_config_sync ();
}

gint GnomeCmdData::gnome_cmd_data_get_int (const gchar *path, int def)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gint v = get_int (s, def);

    g_free (s);

    return v;
}


void GnomeCmdData::gnome_cmd_data_set_int (const gchar *path, int value)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_int (s, value);

    g_free (s);
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

void GnomeCmdData::gnome_cmd_data_set_bool (const gchar *path, gboolean value)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_bool (s, value);

    g_free (s);
}

void GnomeCmdData::gnome_cmd_data_set_color (const gchar *path, GdkColor *color)
{
    gchar *s = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    set_color (s, color);

    g_free (s);
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
gboolean GnomeCmdData::set_color_if_valid_key_value(GdkColor *color, GSettings *settings, const char *key)
{
    gboolean return_value;
    gchar *colorstring_new;
    gchar *colorstring_old;

    colorstring_new = g_settings_get_string (settings, key);
    if (!gnome_cmd_data.is_valid_color_string(colorstring_new))
    {
        colorstring_old = gdk_color_to_string (color);
        g_settings_set_string (settings, key, colorstring_old);
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
 * This function loads a color specification into color by using gnome_config.
 * It will be obsolete in GCMD > 1.6.0
 */
void GnomeCmdData::gnome_cmd_data_get_color_gnome_config (const gchar *path, GdkColor *color)
{
    gchar *def = g_strdup_printf ("%d %d %d",
                                  color->red, color->green, color->blue);

    gchar *gcmd_path = g_build_path (G_DIR_SEPARATOR_S, PACKAGE, path, NULL);

    gchar *color_str = get_string (gcmd_path, def);

    gint red, green, blue;
    if (sscanf (color_str, "%u %u %u", &red, &green, &blue) != 3)
        g_printerr ("Illegal color in config file\n");

    if (color_str != def)
        g_free (color_str);
    g_free (def);

    color->red   = (gushort) red;
    color->green = (gushort) green;
    color->blue  = (gushort) blue;

    g_free (gcmd_path);
}

/**
 * As GSettings enum-type is of GVARIANT_CLASS String, we need a seperate function for
 * finding out if a key value has changed. This is done here. For storing the other GSettings
 * types, see @link set_gsettings_when_changed @endlink .
 * @returns TRUE if new value could be stored, else FALSE
 */
gboolean GnomeCmdData::set_gsettings_enum_when_changed (GSettings *settings, const char *key, gint new_value)
{
    GVariant *default_val;
    gboolean rv = true;

    default_val = g_settings_get_default_value (settings, key);

    // An enum key must be of type G_VARIANT_CLASS_STRING
    if (g_variant_classify(default_val) == G_VARIANT_CLASS_STRING)
    {
        gint old_value;
        old_value = g_settings_get_enum(settings, key);
        if (old_value != new_value)
            rv = g_settings_set_enum (settings, key, new_value);
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


/**
 * This method stores the value for a given key if the value is different from the  currently stored one
 * under the keys value. This function is able of storing several types of GSettings values (except enums
 * which is done in @link set_gsettings_enum_when_changed @endlink ). Therefore, it first checks the type
 * of GVariant of the default value of the given key. Depending on the result, the gpointer is than casted
 * to the correct type so that *value can be saved.
 * @returns TRUE if new value could be stored, else FALSE
 */
gboolean GnomeCmdData::set_gsettings_when_changed (GSettings *settings, const char *key, gpointer value)
{
    GVariant *default_val;
    gboolean rv = true;
    default_val = g_settings_get_default_value (settings, key);

    switch (g_variant_classify(default_val))
    {
        case G_VARIANT_CLASS_INT32:
        {
            gint old_value;
            gint new_value = *(gint*) value;

            old_value = g_settings_get_int (settings, key);
            if (old_value != new_value)
                rv = g_settings_set_int (settings, key, new_value);
            break;
        }
        case G_VARIANT_CLASS_UINT32:
        {
            gint old_value;
            gint new_value = *(gint*) value;

            old_value = g_settings_get_uint (settings, key);
            if (old_value != new_value)
                rv = g_settings_set_uint (settings, key, new_value);
            break;
        }
        case G_VARIANT_CLASS_STRING:
        {
            gchar *old_value;
            gchar *new_value = (char*) value;

            old_value = g_settings_get_string (settings, key);
            if (strcmp(old_value, new_value) != 0)
                rv = g_settings_set_string (settings, key, new_value);
            g_free(old_value);
            break;
        }
        case G_VARIANT_CLASS_BOOLEAN:
        {
            gboolean old_value;
            gboolean new_value = *(gboolean*) value;

            old_value = g_settings_get_boolean (settings, key);
            if (old_value != new_value)
                rv = g_settings_set_boolean (settings, key, new_value);
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

gboolean GnomeCmdData::set_gsettings_color_when_changed (GSettings *settings, const char *key, GdkColor *color)
{
    gboolean return_value;
    gchar *colorstring;


    colorstring = gdk_color_to_string (color);
    return_value = set_gsettings_when_changed (settings, key, colorstring);
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


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::AdvrenameConfig &cfg)
{
    xml << XML::tag("AdvancedRenameTool");

        xml << XML::tag("WindowSize") << XML::attr("width") << cfg.width << XML::attr("height") << cfg.height << XML::endtag();

        xml << XML::tag("Profile") << XML::attr("name") << "Default";

            xml << XML::tag("Template") << XML::chardata() << XML::escape(cfg.templates.empty()  ? "$N" : cfg.templates.front()) << XML::endtag();
            xml << XML::tag("Counter") << XML::attr("start") << cfg.default_profile.counter_start
                                       << XML::attr("step") << cfg.default_profile.counter_step
                                       << XML::attr("width") << cfg.default_profile.counter_width << XML::endtag();

            xml << XML::tag("Regexes");
            for (vector<GnomeCmd::ReplacePattern>::const_iterator r=cfg.default_profile.regexes.begin(); r!=cfg.default_profile.regexes.end(); ++r)
            {
                xml << XML::tag("Regex") << XML::attr("pattern") << XML::escape(r->pattern);
                xml << XML::attr("replace") << XML::escape(r->replacement) << XML::attr("match-case") << r->match_case << XML::endtag();
            }
            xml << XML::endtag();

            xml << XML::tag("CaseConversion") << XML::attr("use") << cfg.default_profile.case_conversion << XML::endtag();
            xml << XML::tag("TrimBlanks") << XML::attr("use") << cfg.default_profile.trim_blanks << XML::endtag();

        xml << XML::endtag();

        for (vector<GnomeCmdData::AdvrenameConfig::Profile>::const_iterator p=cfg.profiles.begin(); p!=cfg.profiles.end(); ++p)
        {
            xml << XML::tag("Profile") << XML::attr("name") << p->name;
                xml << XML::tag("Template") << XML::chardata() << XML::escape(p->template_string.empty() ? "$N" : p->template_string) << XML::endtag();
                xml << XML::tag("Counter") << XML::attr("start") << p->counter_start
                                           << XML::attr("step") << p->counter_step
                                           << XML::attr("width") << p->counter_width << XML::endtag();
                xml << XML::tag("Regexes");
                for (vector<GnomeCmd::ReplacePattern>::const_iterator r=p->regexes.begin(); r!=p->regexes.end(); ++r)
                {
                    xml << XML::tag("Regex") << XML::attr("pattern") << XML::escape(r->pattern);
                    xml << XML::attr("replace") << XML::escape(r->replacement) << XML::attr("match-case") << r->match_case << XML::endtag();
                }
                xml << XML::endtag();
                xml << XML::tag("CaseConversion") << XML::attr("use") << p->case_conversion << XML::endtag();
                xml << XML::tag("TrimBlanks") << XML::attr("use") << p->trim_blanks << XML::endtag();
            xml << XML::endtag();
        }

        xml << XML::tag("History");
        for (GList *i=cfg.templates.ents; i; i=i->next)
            xml << XML::tag("Template") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();
        xml << XML::endtag();

    xml << XML::endtag();

    return xml;
}


XML::xstream &operator << (XML::xstream &xml, GnomeCmdData::Selection &cfg)
{
    xml << XML::tag("Profile") << XML::attr("name") << XML::escape(cfg.name);

        xml << XML::tag("Pattern") << XML::attr("syntax") << (cfg.syntax==Filter::TYPE_REGEX ? "regex" : "shell")
                                   << XML::attr("match-case") << 0 << XML::chardata() << XML::escape(cfg.filename_pattern) << XML::endtag();
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

        xml << XML::tag("History");

        for (GList *i=cfg.name_patterns.ents; i; i=i->next)
            xml << XML::tag("Pattern") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();

        for (GList *i=cfg.content_patterns.ents; i; i=i->next)
            xml << XML::tag("Text") << XML::chardata() << XML::escape((const gchar *) i->data) << XML::endtag();

        xml << XML::endtag();

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
