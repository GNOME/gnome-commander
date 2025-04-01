/**
 * @file gnome-cmd-data.cc
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

#include <config.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <fstream>
#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"

using namespace std;

#define DEFAULT_GUI_UPDATE_RATE 100

GnomeCmdData gnome_cmd_data;

//static GnomeCmdData::AdvrenameConfig::Profile xml_adv_profile;

struct GnomeCmdData::Private
{
    GnomeCmdConList *con_list;
};

GSettingsSchemaSource* GnomeCmdData::GetGlobalSchemaSource()
{
    GSettingsSchemaSource   *global_schema_source;
    std::string              g_schema_path(PREFIX);

    g_schema_path.append("/share/glib-2.0/schemas");

    global_schema_source = g_settings_schema_source_get_default ();

    GSettingsSchemaSource *parent = global_schema_source;
    GError *error = nullptr;

    global_schema_source = g_settings_schema_source_new_from_directory
                               ((gchar*) g_schema_path.c_str(),
                                parent,
                                FALSE,
                                &error);

    if (global_schema_source == nullptr)
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

    G_OBJECT_CLASS (gcmd_settings_parent_class)->dispose (object);
}

static void on_bookmarks_changed (GnomeCmdMainWin *main_win)
{
    gnome_cmd_data.load_bookmarks();
    main_win->update_bookmarks ();
}

static void on_size_display_mode_changed (GnomeCmdMainWin *main_win)
{
    gint size_disp_mode;

    size_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    gnome_cmd_data.options.size_disp_mode = (GnomeCmdSizeDispMode) size_disp_mode;

    main_win->update_view();
}

static void on_perm_display_mode_changed (GnomeCmdMainWin *main_win)
{
    gint perm_disp_mode;

    perm_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);
    gnome_cmd_data.options.perm_disp_mode = (GnomeCmdPermDispMode) perm_disp_mode;

    main_win->update_view();
}

static void on_graphical_layout_mode_changed (GnomeCmdMainWin *main_win)
{
    gint graphical_layout_mode;

    graphical_layout_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);
    gnome_cmd_data.options.layout = (GnomeCmdLayout) graphical_layout_mode;

    main_win->update_view();
}

static void on_list_row_height_changed (GnomeCmdMainWin *main_win)
{
    guint list_row_height;

    list_row_height = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);
    gnome_cmd_data.options.list_row_height = list_row_height;

    main_win->update_view();
}

static void on_date_disp_format_changed (GnomeCmdMainWin *main_win)
{
    GnomeCmdDateFormat date_format;

    date_format = (GnomeCmdDateFormat) g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
    g_free(gnome_cmd_data.options.date_format);
    gnome_cmd_data.options.date_format = date_format;

    main_win->update_view();
}

static void on_filter_hide_unknown_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_UNKNOWN] = filter;

    main_win->update_view();
}

static void on_filter_hide_regular_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_REGULAR] = filter;

    main_win->update_view();
}

static void on_filter_hide_directory_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_DIR] = filter;

    main_win->update_view();
}

static void on_filter_hide_symlink_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMLINK);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_SYMLINK] = filter;

    main_win->update_view();
}

static void on_filter_hide_special_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SPECIAL);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_SPECIAL] = filter;

    main_win->update_view();
}

static void on_filter_hide_shortcut_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SHORTCUT);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_SHORTCUT] = filter;

    main_win->update_view();
}

static void on_filter_hide_mountable_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_MOUNTABLE);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_MOUNTABLE] = filter;

    main_win->update_view();
}

static void on_filter_hide_hidden_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_HIDDEN);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_HIDDEN] = filter;

    main_win->update_view();
}

static void on_filter_hide_backupfiles_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BACKUPS);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_BACKUP] = filter;

    main_win->update_view();
}

static void on_filter_hide_virtual_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_VIRTUAL);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_VIRTUAL] = filter;

    main_win->update_view();
}

static void on_filter_hide_volatile_changed (GnomeCmdMainWin *main_win)
{
    gboolean filter;

    filter = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_VOLATILE);
    gnome_cmd_data.options.filter.file_types[GnomeCmdData::GcmdFileType::G_FILE_IS_VOLATILE] = filter;

    main_win->update_view();
}

static void on_backup_pattern_changed (GnomeCmdMainWin *main_win)
{
    char *backup_pattern;

    backup_pattern = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN);
    gnome_cmd_data.options.set_backup_pattern(backup_pattern);
    main_win->update_view();
    g_free(backup_pattern);
}

static void on_symbolic_links_as_regular_files_changed (GnomeCmdMainWin *main_win)
{
    gboolean symbolic_links_as_regular_files;

    symbolic_links_as_regular_files = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SYMBOLIC_LINKS_AS_REG_FILES);
    gnome_cmd_data.options.symbolic_links_as_regular_files = symbolic_links_as_regular_files;

    main_win->update_view();
}

static void on_list_font_changed (GnomeCmdMainWin *main_win)
{
    g_free(gnome_cmd_data.options.list_font);
    gnome_cmd_data.options.list_font = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);

    main_win->update_view();
}

static void on_ext_disp_mode_changed (GnomeCmdMainWin *main_win)
{
    gint ext_disp_mode;

    ext_disp_mode = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
    gnome_cmd_data.options.ext_disp_mode = (GnomeCmdExtDispMode) ext_disp_mode;

    main_win->update_view();
}

static void on_icon_size_changed (GnomeCmdMainWin *main_win)
{
    guint icon_size;

    icon_size = g_settings_get_uint (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
    gnome_cmd_data.options.icon_size = icon_size;

    main_win->update_view();
}

static void on_show_devbuttons_changed (GnomeCmdMainWin *main_win)
{
    gboolean show_devbuttons;

    show_devbuttons = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS);
    gnome_cmd_data.show_devbuttons = show_devbuttons;
    main_win->fs(ACTIVE)->update_show_devbuttons();
    main_win->fs(INACTIVE)->update_show_devbuttons();
}

static void on_show_devlist_changed (GnomeCmdMainWin *main_win)
{
    gboolean show_devlist;

    show_devlist = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST);
    gnome_cmd_data.show_devlist = show_devlist;

    main_win->fs(ACTIVE)->update_show_devlist();
    main_win->fs(INACTIVE)->update_show_devlist();
}

static void on_show_cmdline_changed (GnomeCmdMainWin *main_win)
{
    gboolean show_cmdline;

    show_cmdline = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE);
    gnome_cmd_data.cmdline_visibility = show_cmdline;
    main_win->update_cmdline_visibility();
}

static void on_show_toolbar_changed (GnomeCmdMainWin *main_win)
{
    if (gnome_cmd_data.show_toolbar != g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR))
    {
        gnome_cmd_data.show_toolbar = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR);
        main_win->update_show_toolbar();
    }
}

static void on_show_buttonbar_changed (GnomeCmdMainWin *main_win)
{
    if (gnome_cmd_data.buttonbar_visibility != g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR))
    {
        gnome_cmd_data.buttonbar_visibility = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR);
        main_win->update_buttonbar_visibility();
    }
}

static void on_horizontal_orientation_changed (GnomeCmdMainWin *main_win)
{
    gboolean horizontal_orientation;

    horizontal_orientation = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION);
    gnome_cmd_data.horizontal_orientation = horizontal_orientation;

    main_win->update_horizontal_orientation();
    main_win->focus_file_lists();
}

static void on_always_show_tabs_changed (GnomeCmdMainWin *main_win)
{
    gboolean always_show_tabs;

    always_show_tabs = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS);
    gnome_cmd_data.options.always_show_tabs = always_show_tabs;

    main_win->update_style();
}

static void on_tab_lock_indicator_changed (GnomeCmdMainWin *main_win)
{
    gint tab_lock_indicator;

    tab_lock_indicator = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR);
    gnome_cmd_data.options.tab_lock_indicator = tab_lock_indicator;

    main_win->update_style();
}

static void on_use_trash_changed (GnomeCmdMainWin *main_win)
{
    gboolean use_trash;

    use_trash = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_USE_TRASH);
    gnome_cmd_data.options.deleteToTrash = use_trash;
}

static void on_confirm_delete_changed (GnomeCmdMainWin *main_win)
{
    gboolean confirm_delete;

    confirm_delete = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE);
    gnome_cmd_data.options.confirm_delete = confirm_delete;
}

static void on_confirm_delete_default_changed (GnomeCmdMainWin *main_win)
{
    gint confirm_delete_default;

    confirm_delete_default = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT);
    gnome_cmd_data.options.confirm_delete_default = (GtkButtonsType) confirm_delete_default;
}

static void on_confirm_copy_overwrite_changed (GnomeCmdMainWin *main_win)
{
    gint confirm_copy_overwrite;

    confirm_copy_overwrite = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE);
    gnome_cmd_data.options.confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) confirm_copy_overwrite;
}

static void on_confirm_move_overwrite_changed (GnomeCmdMainWin *main_win)
{
    gint confirm_move_overwrite;

    confirm_move_overwrite = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE);
    gnome_cmd_data.options.confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) confirm_move_overwrite;
}

static void on_mouse_drag_and_drop_changed (GnomeCmdMainWin *main_win)
{
    gint mouse_dnd_default;

    mouse_dnd_default = g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP);
    gnome_cmd_data.options.mouse_dnd_default = (GnomeCmdDefaultDndMode) mouse_dnd_default;
}

static void on_select_dirs_changed (GnomeCmdMainWin *main_win)
{
    gboolean select_dirs;

    select_dirs = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
    gnome_cmd_data.options.select_dirs = select_dirs;
}

static void on_case_sensitive_changed (GnomeCmdMainWin *main_win)
{
    gboolean case_sensitive;

    case_sensitive = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);
    gnome_cmd_data.options.case_sens_sort = case_sensitive;
}

static void on_symlink_string_changed (GnomeCmdMainWin *main_win)
{
    g_free(gnome_cmd_data.options.symlink_prefix);
    gnome_cmd_data.options.symlink_prefix = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SYMLINK_PREFIX);
}

static void on_use_ls_colors_changed (GnomeCmdMainWin *main_win)
{
    gboolean use_ls_colors;

    use_ls_colors = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);
    gnome_cmd_data.options.use_ls_colors = use_ls_colors;

    main_win->update_view();
}

static void on_always_download_changed (GnomeCmdMainWin *main_win)
{
    gboolean always_download;

    always_download = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD);
    gnome_cmd_data.options.honor_expect_uris = always_download;
}

static void on_multiple_instances_changed (GnomeCmdMainWin *main_win)
{
    gboolean allow_multiple_instances;

    allow_multiple_instances = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    gnome_cmd_data.options.allow_multiple_instances = allow_multiple_instances;
}

static void on_use_internal_viewer_changed (GnomeCmdMainWin *main_win)
{
    gboolean use_internal_viewer;
    use_internal_viewer = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER);
    gnome_cmd_data.options.use_internal_viewer = use_internal_viewer;
}

static void on_use_internal_search_changed (GnomeCmdMainWin *main_win)
{
    gboolean use_internal_search;
    use_internal_search = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_SEARCH);
    gnome_cmd_data.options.use_internal_search = use_internal_search;
}

static void on_quick_search_shortcut_changed (GnomeCmdMainWin *main_win)
{
    GnomeCmdQuickSearchShortcut quick_search;
    quick_search = (GnomeCmdQuickSearchShortcut) g_settings_get_enum (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
    gnome_cmd_data.options.quick_search = quick_search;
}

static void on_quick_search_exact_match_begin_changed (GnomeCmdMainWin *main_win)
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
    gnome_cmd_data.options.quick_search_exact_match_begin = quick_search_exact_match;
}

static void on_quick_search_exact_match_end_changed (GnomeCmdMainWin *main_win)
{
    gboolean quick_search_exact_match;

    quick_search_exact_match = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);
    gnome_cmd_data.options.quick_search_exact_match_end = quick_search_exact_match;
}

static void on_dev_only_icon_changed (GnomeCmdMainWin *main_win)
{
    gboolean dev_only_icon;

    dev_only_icon = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON);
    gnome_cmd_data.options.device_only_icon = dev_only_icon;
}

static void on_samba_device_icon_changed (GnomeCmdMainWin *main_win)
{
    gboolean show_samba_workgroups_button;

    show_samba_workgroups_button = g_settings_get_boolean (gnome_cmd_data.options.gcmd_settings->general, GCMD_SETTINGS_SHOW_SAMBA_WORKGROUP_BUTTON);
    gnome_cmd_data.options.show_samba_workgroups_button = show_samba_workgroups_button;
}

static void on_mainmenu_visibility_changed (GnomeCmdMainWin *main_win)
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

static void on_search_cmd_changed()
{
    gchar *search_cmd;
    g_free(gnome_cmd_data.options.search);
    search_cmd = g_settings_get_string (gnome_cmd_data.options.gcmd_settings->programs, GCMD_SETTINGS_SEARCH_CMD);
    gnome_cmd_data.options.search = search_cmd;
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


static void gcmd_connect_gsettings_signals(GcmdSettings *gs, GnomeCmdMainWin *main_win)
{
    g_signal_connect_swapped (gs->general,
                      "changed::bookmarks",
                      G_CALLBACK (on_bookmarks_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::size-display-mode",
                      G_CALLBACK (on_size_display_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::perm-display-mode",
                      G_CALLBACK (on_perm_display_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::graphical-layout-mode",
                      G_CALLBACK (on_graphical_layout_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::list-row-height",
                      G_CALLBACK (on_list_row_height_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::date-disp-format",
                      G_CALLBACK (on_date_disp_format_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::list-font",
                      G_CALLBACK (on_list_font_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-unknown",
                      G_CALLBACK (on_filter_hide_unknown_changed),
                      main_win);

     g_signal_connect_swapped (gs->filter,
                      "changed::hide-regular",
                      G_CALLBACK (on_filter_hide_regular_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-directory",
                      G_CALLBACK (on_filter_hide_directory_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-symlink",
                      G_CALLBACK (on_filter_hide_symlink_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-special",
                      G_CALLBACK (on_filter_hide_special_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-shortcut",
                      G_CALLBACK (on_filter_hide_shortcut_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-mountable",
                      G_CALLBACK (on_filter_hide_mountable_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-virtual",
                      G_CALLBACK (on_filter_hide_virtual_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-volatile",
                      G_CALLBACK (on_filter_hide_volatile_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-dotfile",
                      G_CALLBACK (on_filter_hide_hidden_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::hide-backupfiles",
                      G_CALLBACK (on_filter_hide_backupfiles_changed),
                      main_win);

    g_signal_connect_swapped (gs->filter,
                      "changed::backup-pattern",
                      G_CALLBACK (on_backup_pattern_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::symbolic-links-as-regular-files",
                      G_CALLBACK (on_symbolic_links_as_regular_files_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::extension-display-mode",
                      G_CALLBACK (on_ext_disp_mode_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::icon-size",
                      G_CALLBACK (on_icon_size_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::show-devbuttons",
                      G_CALLBACK (on_show_devbuttons_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::show-devlist",
                      G_CALLBACK (on_show_devlist_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::show-cmdline",
                      G_CALLBACK (on_show_cmdline_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::show-toolbar",
                      G_CALLBACK (on_show_toolbar_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::show-buttonbar",
                      G_CALLBACK (on_show_buttonbar_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::horizontal-orientation",
                      G_CALLBACK (on_horizontal_orientation_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::symlink-string",
                      G_CALLBACK (on_symlink_string_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::always-show-tabs",
                      G_CALLBACK (on_always_show_tabs_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::tab-lock-indicator",
                      G_CALLBACK (on_tab_lock_indicator_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::delete-to-trash",
                      G_CALLBACK (on_use_trash_changed),
                      main_win);

    g_signal_connect_swapped (gs->confirm,
                      "changed::delete",
                      G_CALLBACK (on_confirm_delete_changed),
                      main_win);

    g_signal_connect_swapped (gs->confirm,
                      "changed::delete-default",
                      G_CALLBACK (on_confirm_delete_default_changed),
                      main_win);

    g_signal_connect_swapped (gs->confirm,
                      "changed::copy-overwrite",
                      G_CALLBACK (on_confirm_copy_overwrite_changed),
                      main_win);

    g_signal_connect_swapped (gs->confirm,
                      "changed::move-overwrite",
                      G_CALLBACK (on_confirm_move_overwrite_changed),
                      main_win);

    g_signal_connect_swapped (gs->confirm,
                      "changed::mouse-drag-and-drop",
                      G_CALLBACK (on_mouse_drag_and_drop_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::select-dirs",
                      G_CALLBACK (on_select_dirs_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::case-sensitive",
                      G_CALLBACK (on_case_sensitive_changed),
                      main_win);

    g_signal_connect_swapped (gs->colors,
                      "changed::use-ls-colors",
                      G_CALLBACK (on_use_ls_colors_changed),
                      main_win);

    g_signal_connect_swapped (gs->programs,
                      "changed::dont-download",
                      G_CALLBACK (on_always_download_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::allow-multiple-instances",
                      G_CALLBACK (on_multiple_instances_changed),
                      main_win);

    g_signal_connect_swapped (gs->programs,
                      "changed::use-internal-viewer",
                      G_CALLBACK (on_use_internal_viewer_changed),
                      main_win);

    g_signal_connect_swapped (gs->programs,
                      "changed::use-internal-search",
                      G_CALLBACK (on_use_internal_search_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::quick-search",
                      G_CALLBACK (on_quick_search_shortcut_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::quick-search-exact-match-begin",
                      G_CALLBACK (on_quick_search_exact_match_begin_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::quick-search-exact-match-end",
                      G_CALLBACK (on_quick_search_exact_match_end_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::dev-only-icon",
                      G_CALLBACK (on_dev_only_icon_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::show-samba-workgroup-button",
                      G_CALLBACK (on_samba_device_icon_changed),
                      main_win);

    g_signal_connect_swapped (gs->general,
                      "changed::mainmenu-visibility",
                      G_CALLBACK (on_mainmenu_visibility_changed),
                      main_win);

    g_signal_connect (gs->general,
                      "changed::opts-dialog-width",
                      G_CALLBACK (on_opts_dialog_width_changed),
                      nullptr);

    g_signal_connect (gs->general,
                      "changed::opts-dialog-height",
                      G_CALLBACK (on_opts_dialog_height_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::viewer-cmd",
                      G_CALLBACK (on_viewer_cmd_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::editor-cmd",
                      G_CALLBACK (on_editor_cmd_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::differ-cmd",
                      G_CALLBACK (on_differ_cmd_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::search-cmd",
                      G_CALLBACK (on_search_cmd_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::sendto-cmd",
                      G_CALLBACK (on_sendto_cmd_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::terminal-cmd",
                      G_CALLBACK (on_terminal_cmd_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::terminal-exec-cmd",
                      G_CALLBACK (on_terminal_exec_cmd_changed),
                      nullptr);

    g_signal_connect (gs->programs,
                      "changed::use-gcmd-block",
                      G_CALLBACK (on_use_gcmd_block_changed),
                      nullptr);
}


static void gcmd_settings_init (GcmdSettings *gs)
{
    GSettingsSchemaSource   *global_schema_source;
    GSettingsSchema         *global_schema;

    global_schema_source = GnomeCmdData::GetGlobalSchemaSource();

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_GENERAL, FALSE);
    gs->general = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_FILTER, FALSE);
    gs->filter = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_CONFIRM, FALSE);
    gs->confirm = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_COLORS, FALSE);
    gs->colors = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_PROGRAMS, FALSE);
    gs->programs = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_NETWORK, FALSE);
    gs->network = g_settings_new_full (global_schema, nullptr, nullptr);
}


GcmdSettings *gcmd_settings_new ()
{
    auto gs = (GcmdSettings *) g_object_new (GCMD_TYPE_SETTINGS, nullptr);
    return gs;
}


GnomeCmdData::Options::Options(const Options &cfg)
{
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
    size_disp_mode = cfg.size_disp_mode;
    perm_disp_mode = cfg.perm_disp_mode;
    date_format = g_strdup (cfg.date_format);
    list_font = g_strdup (cfg.list_font);
    list_row_height = cfg.list_row_height;
    ext_disp_mode = cfg.ext_disp_mode;
    layout = cfg.layout;
    use_ls_colors = cfg.use_ls_colors;
    icon_size = cfg.icon_size;
    icon_scale_quality = cfg.icon_scale_quality;
    theme_icon_dir = cfg.theme_icon_dir;
    always_show_tabs = cfg.always_show_tabs;
    tab_lock_indicator = cfg.tab_lock_indicator;
    confirm_delete = cfg.confirm_delete;
    confirm_delete_default = cfg.confirm_delete_default;
    confirm_copy_overwrite = cfg.confirm_copy_overwrite;
    confirm_move_overwrite = cfg.confirm_move_overwrite;
    mouse_dnd_default = cfg.mouse_dnd_default;
    filter = cfg.filter;
    backup_pattern = g_strdup (cfg.backup_pattern);
    backup_pattern_list = patlist_new (cfg.backup_pattern);
    honor_expect_uris = cfg.honor_expect_uris;
    viewer = g_strdup (cfg.viewer);
    use_internal_viewer = cfg.use_internal_viewer;
    editor = g_strdup (cfg.editor);
    differ = g_strdup (cfg.differ);
    use_internal_search = cfg.use_internal_search;
    search = g_strdup (cfg.search);
    sendto = g_strdup (cfg.sendto);
    termopen = g_strdup (cfg.termopen);
    termexec = g_strdup (cfg.termexec);
    device_only_icon = cfg.device_only_icon;
    deleteToTrash = cfg.deleteToTrash;
    show_samba_workgroups_button = cfg.show_samba_workgroups_button;
    gcmd_settings = nullptr;
}


GnomeCmdData::Options &GnomeCmdData::Options::operator = (const Options &cfg)
{
    if (this != &cfg)
    {
        this->~Options();       //  free allocated data

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
        size_disp_mode = cfg.size_disp_mode;
        perm_disp_mode = cfg.perm_disp_mode;
        date_format = g_strdup (cfg.date_format);
        list_font = g_strdup (cfg.list_font);
        list_row_height = cfg.list_row_height;
        ext_disp_mode = cfg.ext_disp_mode;
        layout = cfg.layout;
        use_ls_colors = cfg.use_ls_colors;
        icon_size = cfg.icon_size;
        icon_scale_quality = cfg.icon_scale_quality;
        theme_icon_dir = cfg.theme_icon_dir;
        always_show_tabs = cfg.always_show_tabs;
        tab_lock_indicator = cfg.tab_lock_indicator;
        confirm_delete = cfg.confirm_delete;
        confirm_copy_overwrite = cfg.confirm_copy_overwrite;
        confirm_move_overwrite = cfg.confirm_move_overwrite;
        mouse_dnd_default = cfg.mouse_dnd_default;
        filter = cfg.filter;
        backup_pattern = g_strdup (cfg.backup_pattern);
        backup_pattern_list = patlist_new (cfg.backup_pattern);
        honor_expect_uris = cfg.honor_expect_uris;
        viewer = g_strdup (cfg.viewer);
        use_internal_viewer = cfg.use_internal_viewer;
        editor = g_strdup (cfg.editor);
        differ = g_strdup (cfg.differ);
        use_internal_search = cfg.use_internal_search;
        search = g_strdup (cfg.search);
        sendto = g_strdup (cfg.sendto);
        termopen = g_strdup (cfg.termopen);
        termexec = g_strdup (cfg.termexec);
        device_only_icon = cfg.device_only_icon;
        show_samba_workgroups_button = cfg.show_samba_workgroups_button;
        gcmd_settings = nullptr;
    }

    return *this;
}


GnomeCmdColorMode GnomeCmdData::Options::color_mode()
{
    return (GnomeCmdColorMode) g_settings_get_enum (gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME);
}


void GnomeCmdData::Options::set_color_mode(GnomeCmdColorMode color_mode)
{
    g_settings_set_enum (gcmd_settings->colors, GCMD_SETTINGS_COLORS_THEME, color_mode);
}


struct SearchProfiles
{
    std::vector<GnomeCmdData::SearchProfile> profiles;
};


extern "C" SearchProfiles *gnome_cmd_search_config_new_profiles (GnomeCmdData::SearchConfig *cfg, gboolean with_default)
{
    auto profiles = new SearchProfiles();
    profiles->profiles = cfg->profiles;
    if (with_default)
        profiles->profiles.push_back(cfg->default_profile);
    return profiles;
}

extern "C" void gnome_cmd_search_config_take_profiles (GnomeCmdData::SearchConfig *cfg, SearchProfiles *profiles)
{
    cfg->profiles = profiles->profiles;
    delete profiles;
}


extern "C" SearchProfiles *gnome_cmd_search_profiles_dup (SearchProfiles *profiles)
{
    auto profiles_copy = new SearchProfiles();
    profiles_copy->profiles = profiles->profiles;
    return profiles_copy;
}

extern "C" uint gnome_cmd_search_profiles_len (SearchProfiles *profiles)
{
    return profiles->profiles.size();
}

extern "C" GnomeCmdData::SearchProfile *gnome_cmd_search_profiles_get (SearchProfiles *profiles, guint profile_index)
{
    return &profiles->profiles[profile_index];
}

extern "C" const gchar *gnome_cmd_search_profiles_get_name (SearchProfiles *profiles, guint profile_index)
{
    return profiles->profiles[profile_index].name.c_str();
}

extern "C" void gnome_cmd_search_profiles_set_name (SearchProfiles *profiles, guint profile_index, const gchar *name)
{
    profiles->profiles[profile_index].name = name;
}

extern "C" gchar *gnome_cmd_search_profiles_get_description (SearchProfiles *profiles, guint profile_index)
{
    return g_strdup (profiles->profiles[profile_index].description().c_str());
}

extern "C" void gnome_cmd_search_profiles_reset (SearchProfiles *profiles, guint profile_index)
{
    profiles->profiles[profile_index].reset();
}

extern "C" guint gnome_cmd_search_profiles_duplicate (SearchProfiles *profiles, guint profile_index)
{
    profiles->profiles.push_back(profiles->profiles[profile_index]);
    return profiles->profiles.size() - 1;
}

extern "C" void gnome_cmd_search_profiles_pick (SearchProfiles *profiles, guint *indexes, guint size)
{
    std::vector<GnomeCmdData::SearchProfile> picked_profiles;
    for (guint i = 0; i < size; ++i)
        picked_profiles.push_back(profiles->profiles[indexes[i]]);
    profiles->profiles = picked_profiles;
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


extern "C" GList *gnome_cmd_search_config_get_name_patterns(GnomeCmdData::SearchConfig *search_config)
{
    return search_config->name_patterns.ents;
}

extern "C" void gnome_cmd_search_config_add_name_pattern(GnomeCmdData::SearchConfig *search_config, const gchar *p)
{
    search_config->name_patterns.add(p);
}

extern "C" gint gnome_cmd_search_config_get_default_profile_syntax(GnomeCmdData::SearchConfig *search_config)
{
    return search_config->default_profile.syntax;
}

extern "C" void gnome_cmd_search_config_set_default_profile_syntax(GnomeCmdData::SearchConfig *search_config, gint syntax)
{
    search_config->default_profile.syntax = (Filter::Type) syntax;
}


struct AdvrenameProfiles
{
    std::vector<GnomeCmdData::AdvrenameConfig::Profile> profiles;
};


extern "C" AdvrenameProfiles *gnome_cmd_advrename_config_new_profiles (GnomeCmdData::AdvrenameConfig *cfg, gboolean with_default)
{
    auto profiles = new AdvrenameProfiles();
    profiles->profiles = cfg->profiles;
    if (with_default)
        profiles->profiles.push_back(cfg->default_profile);
    return profiles;
}

extern "C" void gnome_cmd_advrename_config_take_profiles (GnomeCmdData::AdvrenameConfig *cfg, AdvrenameProfiles *profiles)
{
    cfg->profiles = profiles->profiles;
    delete profiles;
}


extern "C" AdvrenameProfiles *gnome_cmd_advrename_profiles_dup (AdvrenameProfiles *profiles)
{
    auto profiles_copy = new AdvrenameProfiles();
    profiles_copy->profiles = profiles->profiles;
    return profiles_copy;
}

extern "C" uint gnome_cmd_advrename_profiles_len (AdvrenameProfiles *profiles)
{
    return profiles->profiles.size();
}

extern "C" GnomeCmdData::AdvrenameConfig::Profile *gnome_cmd_advrename_profiles_get (AdvrenameProfiles *profiles, guint profile_index)
{
    return &profiles->profiles[profile_index];
}

extern "C" const gchar *gnome_cmd_advrename_profiles_get_name (AdvrenameProfiles *profiles, guint profile_index)
{
    return profiles->profiles[profile_index].name.c_str();
}

extern "C" void gnome_cmd_advrename_profiles_set_name (AdvrenameProfiles *profiles, guint profile_index, const gchar *name)
{
    profiles->profiles[profile_index].name = name;
}

extern "C" gchar *gnome_cmd_advrename_profiles_get_description (AdvrenameProfiles *profiles, guint profile_index)
{
    return g_strdup (profiles->profiles[profile_index].description().c_str());
}

extern "C" void gnome_cmd_advrename_profiles_reset (AdvrenameProfiles *profiles, guint profile_index)
{
    profiles->profiles[profile_index].reset();
}

extern "C" guint gnome_cmd_advrename_profiles_duplicate (AdvrenameProfiles *profiles, guint profile_index)
{
    profiles->profiles.push_back(profiles->profiles[profile_index]);
    return profiles->profiles.size() - 1;
}

extern "C" void gnome_cmd_advrename_profiles_pick (AdvrenameProfiles *profiles, guint *indexes, guint size)
{
    std::vector<GnomeCmdData::AdvrenameConfig::Profile> picked_profiles;
    for (guint i = 0; i < size; ++i)
        picked_profiles.push_back(profiles->profiles[indexes[i]]);
    profiles->profiles = picked_profiles;
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


void GnomeCmdData::save_bookmarks()
{
    GVariant *bookmarksToStore = gnome_cmd_con_list_save_bookmarks (priv->con_list);
    if (bookmarksToStore == nullptr)
        bookmarksToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_BOOKMARKS);

    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_BOOKMARKS, bookmarksToStore);
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


void GnomeCmdData::add_search_profile_to_gvariant_builder(GVariantBuilder *builder, SearchProfile searchProfile)
{
    g_variant_builder_add(builder, GCMD_SETTINGS_SEARCH_PROFILE_FORMAT_STRING,
        searchProfile.name.c_str(),
        searchProfile.max_depth,
        (gint) searchProfile.syntax,
        searchProfile.filename_pattern.c_str(),
        searchProfile.content_search,
        searchProfile.match_case,
        searchProfile.text_pattern.c_str()
    );
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
        profile.name.c_str(),
        profile.template_string.empty() ? "$N" : profile.template_string.c_str(),
        profile.counter_start,
        profile.counter_step,
        profile.counter_width,
        profile.case_conversion,
        profile.trim_blanks,
        gVariantBuilderAdvrenameFromArray,
        gVariantBuilderAdvrenameToList,
        gVariantBuilderAdvrenameMatchCaseList
    );
}


/**
 * Save devices in gSettings
 */
void GnomeCmdData::save_devices()
{
    GVariant* devicesToStore = gnome_cmd_con_list_save_devices (priv->con_list);
    if (!devicesToStore)
        devicesToStore = g_settings_get_default_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICE_LIST);
    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICE_LIST, devicesToStore);
    g_variant_unref(devicesToStore);
}


/**
 * Save tabs in given gSettings and in given key
 */
static void save_tabs (GSettings *gSettings, const char *gSettingsKey, GnomeCmdMainWin *main_win)
{
    if (main_win == nullptr)
        return;

    GVariant* fileListTabs;
    GVariantBuilder gVariantBuilder;
    g_variant_builder_init (&gVariantBuilder, G_VARIANT_TYPE_ARRAY);

    for (int fileSelectorIdInt = LEFT; fileSelectorIdInt <= RIGHT; fileSelectorIdInt++)
    {
        FileSelectorID fileSelectorId = static_cast<FileSelectorID>(fileSelectorIdInt);
        GnomeCmdFileSelector gnomeCmdFileSelector = *main_win->fs(fileSelectorId);
        GListModel *tabs = gnomeCmdFileSelector.GetTabs();

        guint tabs_count = g_list_model_get_n_items (tabs);
        for (guint i = 0; i < tabs_count; ++i)
        {
            auto page = GTK_NOTEBOOK_PAGE (g_list_model_get_item (tabs, i));
            auto fl = GNOME_CMD_FILE_LIST (gtk_notebook_page_get_child (page));
            if (!fl)
                continue;

            if (gnome_cmd_data.options.save_tabs_on_exit || (gnome_cmd_data.options.save_dirs_on_exit && fl == gnomeCmdFileSelector.file_list()) || fl->locked)
            {
                if (!fl->cwd)
                    continue;
                gchar* uriString = GNOME_CMD_FILE (fl->cwd)->get_uri_str();
                if (!uriString)
                    continue;
                g_variant_builder_add (&gVariantBuilder, GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING,
                                        uriString,
                                        (guchar) fileSelectorId,
                                        fl->get_sort_column(),
                                        fl->get_sort_order(),
                                        fl->locked);
                g_free(uriString);
            }
        }
        g_object_unref (tabs);
    }
    fileListTabs = g_variant_builder_end (&gVariantBuilder);

    if (!fileListTabs)
    {
        return;
    }

    g_settings_set_value(gSettings, gSettingsKey, fileListTabs);
}


/**
 * Save connections in gSettings
 */
void GnomeCmdData::save_connections()
{
    GVariant* connectionsToStore = gnome_cmd_con_list_save_connections (priv->con_list);
    if (!connectionsToStore)
        connectionsToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_CONNECTIONS);
    g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_CONNECTIONS, connectionsToStore);
    g_variant_unref(connectionsToStore);
}


void GnomeCmdData::load_bookmarks()
{
    auto *gVariantBookmarks = g_settings_get_value (options.gcmd_settings->general, GCMD_SETTINGS_BOOKMARKS);
    gnome_cmd_con_list_load_bookmarks (priv->con_list, gVariantBookmarks);
}


/**
 * This method reads the gsettings section of the advance rename tool
 */
void GnomeCmdData::load_advrename_profiles ()
{
    GVariant *gVariantProfiles = g_settings_get_value (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_PROFILES);

    g_autoptr(GVariantIter) iter1 = nullptr;
    g_autoptr(GVariantIter) iter2 = nullptr;
    g_autoptr(GVariantIter) iter3 = nullptr;
    g_autoptr(GVariantIter) iter4 = nullptr;

    g_variant_get (gVariantProfiles, GCMD_SETTINGS_ADVRENAME_PROFILES_FORMAT_STRING, &iter1);

    gchar *name {nullptr};
    gchar *templateString {nullptr};
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
            &templateString,
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

        AdvrenameConfig::Profile profile;

        profile.reset();

        profile.name            = name;
        profile.template_string = templateString;
        profile.counter_start   = counter_start;
        profile.counter_step    = counter_step;
        profile.counter_width   = counter_width;
        profile.case_conversion = case_conversion;
        profile.trim_blanks     = trim_blanks;

        auto regexes_from       = new vector<string>;
        auto regexes_to         = new vector<string>;
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
            profile.regexes.push_back(GnomeCmd::ReplacePattern(regexes_from->at(ii),
                                                               regexes_to->at(ii),
                                                               regexes_match_case->at(ii)));
        }

        if (profileNumber == 0)
            advrename_defaults.default_profile = profile;
        else
            advrename_defaults.profiles.push_back(profile);

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
    auto *gVariantProfiles = g_settings_get_value (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PROFILES);

    g_autoptr(GVariantIter) iter1 {nullptr};

    g_variant_get (gVariantProfiles, GCMD_SETTINGS_SEARCH_PROFILES_FORMAT_STRING, &iter1);

    gchar *name {nullptr};
    gchar *filenamePattern {nullptr};
    gchar *textPattern {nullptr};
    gint maxDepth {0};
    gint syntax {0};
    guint profileNumber {0};
    gboolean contentSearch {false};
    gboolean matchCase {false};

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


void GnomeCmdData::save_cmdline_history(GnomeCmdMainWin *main_win)
{
    if (options.save_cmdline_history_on_exit)
    {
        if (main_win != nullptr)
        {
            cmdline_history = gnome_cmd_cmdline_get_history (main_win->get_cmdline());
            g_settings_set_strv (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, cmdline_history);
        }
    }
    else
    {
        set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY, nullptr);
    }
}


void GnomeCmdData::save_directory_history()
{
    if (options.save_dir_history_on_exit)
    {
        set_gsettings_string_array_from_glist(
            options.gcmd_settings->general,
            GCMD_SETTINGS_DIRECTORY_HISTORY,
            gnome_cmd_con_get_dir_history (gnome_cmd_con_list_get_home (priv->con_list))->ents);
    }
    else
    {
        GVariant* dirHistoryToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_DIRECTORY_HISTORY, dirHistoryToStore);
    }
}


void GnomeCmdData::save_search_history()
{
    if (options.save_search_history_on_exit)
    {
        set_gsettings_string_array_from_glist(
            options.gcmd_settings->general,
            GCMD_SETTINGS_SEARCH_PATTERN_HISTORY,
            search_defaults.name_patterns.ents);

        set_gsettings_string_array_from_glist(
            options.gcmd_settings->general,
            GCMD_SETTINGS_SEARCH_TEXT_HISTORY,
            search_defaults.content_patterns.ents);
    }
    else
    {
        GVariant* searchHistoryToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PATTERN_HISTORY);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PATTERN_HISTORY, searchHistoryToStore);

        searchHistoryToStore = g_settings_get_default_value (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_TEXT_HISTORY);
        g_settings_set_value(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_TEXT_HISTORY, searchHistoryToStore);
    }
}


/**
 * Returns a GList with newly allocated char strings
 */
inline GList* GnomeCmdData::get_list_from_gsettings_string_array (GSettings *settings_given, const gchar *key)
{
    GList *list = nullptr;
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
    g_strfreev(cmdline_history);

    cmdline_history = g_settings_get_strv (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY);
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


GnomeCmdData::GnomeCmdData(): search_defaults(profiles)
{
    quick_connect = nullptr;

    //TODO: Include into GnomeCmdData::Options
    memset(fs_col_width, 0, sizeof(fs_col_width));
    gui_update_rate = DEFAULT_GUI_UPDATE_RATE;

    cmdline_history = nullptr;
    cmdline_history_length = 0;

    use_gcmd_block = TRUE;

    main_win_width = 600;
    main_win_height = 400;

    main_win_maximized = TRUE;

    umask = ::umask(0);
    ::umask(umask);
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
            // gnome_cmd_con_close (GNOME_CMD_CON (quick_connect));
            // gtk_object_destroy (GTK_OBJECT (quick_connect));
        }

        g_free (priv);
    }
}

void GnomeCmdData::gsettings_init()
{
    options.gcmd_settings = gcmd_settings_new();
}


void GnomeCmdData::connect_signals(GnomeCmdMainWin *main_win)
{
    gcmd_connect_gsettings_signals (options.gcmd_settings, main_win);
}


/**
 * This function checks if the given GSettings key enholds a valid color string. If not,
 * the key's value is reset to the default value.
 * @returns TRUE if the current value is reset by the default value, otherwise FALSE
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
                  colorstring, key, g_variant_get_string(variant, nullptr));
        g_settings_set_string (settings_given, key, g_variant_get_string(variant, nullptr));
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
void GnomeCmdData::load_tabs()
{
    GVariant *gvTabs, *tab;
    GVariantIter iter;

    gvTabs = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_FILE_LIST_TABS);

    g_variant_iter_init (&iter, gvTabs);

    while ((tab = g_variant_iter_next_value (&iter)) != nullptr)
    {
        gchar *uriCharString;
        gboolean sort_order, locked;
        guchar fileSelectorId, sort_column;

        g_assert (g_variant_is_of_type (tab, G_VARIANT_TYPE (GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING)));
        g_variant_get(tab, GCMD_SETTINGS_FILE_LIST_TAB_FORMAT_STRING, &uriCharString, &fileSelectorId, &sort_column, &sort_order, &locked);
        string uriString(uriCharString);
        if (!uriString.empty() && sort_column < GnomeCmdFileList::NUM_COLUMNS)
        {
            this->tabs[(FileSelectorID) fileSelectorId].push_back(make_pair(uriString, make_tuple((GnomeCmdFileList::ColumnID) sort_column, (GtkSortType) sort_order, locked)));
        }
        g_variant_unref(tab);
        g_free(uriCharString);
    }
    g_variant_unref(gvTabs);
}

/**
 * Loads devices from gSettings into gcmd options
 */
void GnomeCmdData::load_devices()
{
    GVariant *gvDevices = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_DEVICE_LIST);
    gnome_cmd_con_list_load_devices (priv->con_list, gvDevices);
}

/**
 * Loads connections from gSettings into gcmd options
 */
void GnomeCmdData::load_connections()
{
    GVariant *gvConnections = g_settings_get_value(options.gcmd_settings->general, GCMD_SETTINGS_CONNECTIONS);
    gnome_cmd_con_list_load_connections (priv->con_list, gvConnections);
}


void GnomeCmdData::load()
{
    if (!priv)
        priv = g_new0 (Private, 1);

    options.use_ls_colors = g_settings_get_boolean (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS);

    options.size_disp_mode = (GnomeCmdSizeDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE);
    options.perm_disp_mode = (GnomeCmdPermDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE);

    gchar *utf8_date_format = g_settings_get_string (options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT);
    options.date_format = g_locale_from_utf8 (utf8_date_format, -1, nullptr, nullptr, nullptr);
    g_free (utf8_date_format);

    options.layout = (GnomeCmdLayout) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE);

    options.list_row_height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT);

    options.confirm_delete = g_settings_get_boolean (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE);
    options.confirm_delete_default = (GtkButtonsType) g_settings_get_enum (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT);
    options.confirm_copy_overwrite = (GnomeCmdConfirmOverwriteMode) g_settings_get_enum (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE);
    options.confirm_move_overwrite = (GnomeCmdConfirmOverwriteMode) g_settings_get_enum (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE);
    options.mouse_dnd_default      = (GnomeCmdDefaultDndMode) g_settings_get_enum (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP);

    options.filter.file_types[G_FILE_IS_UNKNOWN] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN);
    options.filter.file_types[G_FILE_IS_REGULAR] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR);
    options.filter.file_types[G_FILE_IS_DIR] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY);
    options.filter.file_types[G_FILE_IS_SYMLINK] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMLINK);
    options.filter.file_types[G_FILE_TYPE_SPECIAL] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SPECIAL);
    options.filter.file_types[G_FILE_TYPE_SHORTCUT] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SHORTCUT);
    options.filter.file_types[G_FILE_TYPE_MOUNTABLE] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_MOUNTABLE);
    options.filter.file_types[G_FILE_IS_VIRTUAL] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_VIRTUAL);
    options.filter.file_types[G_FILE_IS_VOLATILE] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_VOLATILE);
    options.filter.file_types[G_FILE_IS_HIDDEN] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_HIDDEN);
    options.filter.file_types[G_FILE_IS_BACKUP] = g_settings_get_boolean (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BACKUPS);

    options.select_dirs = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS);
    options.case_sens_sort = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE);
    options.symbolic_links_as_regular_files = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SYMBOLIC_LINKS_AS_REG_FILES);

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

    options.list_font = g_settings_get_string (options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT);

    options.ext_disp_mode = (GnomeCmdExtDispMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE);
    options.left_mouse_button_mode = (LeftMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM);
    options.left_mouse_button_unselects = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS);
    options.middle_mouse_button_mode = (MiddleMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE);
    options.right_mouse_button_mode = (RightMouseButtonMode) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE);
    options.deleteToTrash = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_USE_TRASH);
    options.icon_size = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE);
    options.icon_scale_quality = (GdkInterpType) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SCALE_QUALITY);
    options.theme_icon_dir = g_settings_get_string(options.gcmd_settings->general, GCMD_SETTINGS_MIME_ICON_DIR);
    cmdline_history_length = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH);
    horizontal_orientation = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION);
    gui_update_rate = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE);

    show_toolbar = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR);
    show_devbuttons = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS);
    show_devlist = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST);
    cmdline_visibility = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE);
    buttonbar_visibility = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR);
    mainmenu_visibility = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY);

    options.honor_expect_uris = g_settings_get_boolean (options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD);
    options.allow_multiple_instances = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    options.use_internal_viewer = g_settings_get_boolean (options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER);
    options.use_internal_search = g_settings_get_boolean (options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_SEARCH);
    options.quick_search = (GnomeCmdQuickSearchShortcut) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT);
    options.quick_search_exact_match_begin = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN);
    options.quick_search_exact_match_end = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END);

    options.device_only_icon = g_settings_get_boolean(options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON);
    options.show_samba_workgroups_button = g_settings_get_boolean(options.gcmd_settings->general, GCMD_SETTINGS_SHOW_SAMBA_WORKGROUP_BUTTON);
    options.symlink_prefix = g_settings_get_string(options.gcmd_settings->general, GCMD_SETTINGS_SYMLINK_PREFIX);
    if (!*options.symlink_prefix || strcmp(options.symlink_prefix, _("link to %s"))==0)
    {
        g_free (options.symlink_prefix);
        options.symlink_prefix = nullptr;
    }

    options.viewer = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_VIEWER_CMD);
    options.editor = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_EDITOR_CMD);
    options.differ = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_DIFFER_CMD);
    options.search = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_SEARCH_CMD);
    options.sendto = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_SENDTO_CMD);
    options.termopen = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_CMD);
    options.termexec = g_settings_get_string(options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_EXEC_CMD);
    use_gcmd_block = g_settings_get_boolean(options.gcmd_settings->programs, GCMD_SETTINGS_USE_GCMD_BLOCK);

    options.save_dirs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIRS_ON_EXIT);
    options.save_tabs_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_TABS_ON_EXIT);
    options.save_dir_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_DIR_HISTORY_ON_EXIT);
    options.save_cmdline_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_CMDLINE_HISTORY_ON_EXIT);
    options.save_search_history_on_exit = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_SAVE_SEARCH_HISTORY_ON_EXIT);
    options.search_window_is_transient = g_settings_get_boolean(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_IS_TRANSIENT);
    search_defaults.height = g_settings_get_uint(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_HEIGHT);
    search_defaults.width = g_settings_get_uint(options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_WIDTH);
    search_defaults.content_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_TEXT_HISTORY);
    search_defaults.name_patterns.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_PATTERN_HISTORY);

    options.always_show_tabs = g_settings_get_boolean (options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS);
    options.tab_lock_indicator = (TabLockIndicator) g_settings_get_enum (options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR);

    options.backup_pattern = g_settings_get_string (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN);
    options.backup_pattern_list = patlist_new (options.backup_pattern);

    main_win_maximized = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_STATE) == 4;

    advrename_defaults.width = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_WIDTH);
    advrename_defaults.height = g_settings_get_uint (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_HEIGHT);
    advrename_defaults.templates.ents = get_list_from_gsettings_string_array (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_TEMPLATE_HISTORY);

    load_tabs();

    load_cmdline_history();

    if (!priv->con_list)
        priv->con_list = gnome_cmd_con_list_new (options.show_samba_workgroups_button);
    else
        advrename_defaults.profiles.clear();

    gnome_cmd_con_list_lock (priv->con_list);
    load_devices();
    load_advrename_profiles ();
    load_search_profiles    ();
    load_connections        ();
    load_bookmarks          ();
    load_directory_history  ();
    gnome_cmd_con_list_unlock (priv->con_list);

    gchar *quick_connect_uri = g_settings_get_string(options.gcmd_settings->network, GCMD_SETTINGS_QUICK_CONNECT_URI);
    quick_connect = gnome_cmd_con_remote_new (nullptr, quick_connect_uri);
    g_free (quick_connect_uri);

    gnome_cmd_con_list_set_volume_monitor (priv->con_list);
}


void GnomeCmdData::save(GnomeCmdMainWin *main_win)
{
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_SIZE_DISP_MODE, options.size_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_PERM_DISP_MODE, options.perm_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_GRAPHICAL_LAYOUT_MODE, options.layout);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LIST_ROW_HEIGHT, &(options.list_row_height));

    gchar *utf8_date_format = g_locale_to_utf8 (options.date_format, -1, nullptr, nullptr, nullptr);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_DATE_DISP_FORMAT, utf8_date_format);
    g_free (utf8_date_format);

    set_gsettings_when_changed      (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE, &(options.confirm_delete));
    set_gsettings_enum_when_changed (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_DELETE_DEFAULT, options.confirm_delete_default);
    set_gsettings_enum_when_changed (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_COPY_OVERWRITE, options.confirm_copy_overwrite);
    set_gsettings_enum_when_changed (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOVE_OVERWRITE, options.confirm_move_overwrite);
    set_gsettings_enum_when_changed (options.gcmd_settings->confirm, GCMD_SETTINGS_CONFIRM_MOUSE_DRAG_AND_DROP, options.mouse_dnd_default);

    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_UNKNOWN, &(options.filter.file_types[G_FILE_IS_UNKNOWN]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_REGULAR, &(options.filter.file_types[G_FILE_IS_REGULAR]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_DIRECTORY, &(options.filter.file_types[G_FILE_IS_DIR]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SYMLINK, &(options.filter.file_types[G_FILE_IS_SYMLINK]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SPECIAL, &(options.filter.file_types[G_FILE_IS_SPECIAL]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_SHORTCUT, &(options.filter.file_types[G_FILE_IS_SHORTCUT]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_MOUNTABLE, &(options.filter.file_types[G_FILE_IS_MOUNTABLE]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_VIRTUAL, &(options.filter.file_types[G_FILE_IS_VIRTUAL]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_VOLATILE, &(options.filter.file_types[G_FILE_IS_VOLATILE]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_HIDDEN, &(options.filter.file_types[G_FILE_IS_HIDDEN]));
    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_HIDE_BACKUPS, &(options.filter.file_types[G_FILE_IS_BACKUP]));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SELECT_DIRS, &(options.select_dirs));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_CASE_SENSITIVE, &(options.case_sens_sort));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SYMBOLIC_LINKS_AS_REG_FILES, &(options.symbolic_links_as_regular_files));

    set_gsettings_when_changed      (options.gcmd_settings->colors, GCMD_SETTINGS_COLORS_USE_LS_COLORS, &(options.use_ls_colors));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LIST_FONT, options.list_font);

    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_EXT_DISP_MODE, options.ext_disp_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_CLICKS_TO_OPEN_ITEM, options.left_mouse_button_mode);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_LEFT_MOUSE_BUTTON_UNSELECTS, &(options.left_mouse_button_unselects));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_MIDDLE_MOUSE_BUTTON_MODE, options.middle_mouse_button_mode);
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_RIGHT_MOUSE_BUTTON_MODE, options.right_mouse_button_mode);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SIZE, &(options.icon_size));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_USE_TRASH, &(options.deleteToTrash));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_ICON_SCALE_QUALITY, options.icon_scale_quality);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MIME_ICON_DIR, options.theme_icon_dir);
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_CMDLINE_HISTORY_LENGTH, &(cmdline_history_length));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION, &(horizontal_orientation));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_GUI_UPDATE_RATE, &(gui_update_rate));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MULTIPLE_INSTANCES, &(options.allow_multiple_instances));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_SHORTCUT, options.quick_search);

    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_DONT_DOWNLOAD, &(options.honor_expect_uris));
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_VIEWER, &(options.use_internal_viewer));
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_USE_INTERNAL_SEARCH, &(options.use_internal_search));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_BEGIN, &(options.quick_search_exact_match_begin));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_QUICK_SEARCH_EXACT_MATCH_END, &(options.quick_search_exact_match_end));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_DEV_ONLY_ICON, &(options.device_only_icon));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_SAMBA_WORKGROUP_BUTTON, &(options.show_samba_workgroups_button));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_TOOLBAR, &(show_toolbar));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS, &(show_devbuttons));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_DEVLIST, &(show_devlist));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_CMDLINE, &(cmdline_visibility));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR, &(buttonbar_visibility));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY, &(mainmenu_visibility));

    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_VIEWER_CMD, options.viewer);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_EDITOR_CMD, options.editor);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_DIFFER_CMD, options.differ);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_SEARCH_CMD, options.search);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_SENDTO_CMD, options.sendto);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_CMD, options.termopen);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_TERMINAL_EXEC_CMD, options.termexec);
    set_gsettings_when_changed      (options.gcmd_settings->programs, GCMD_SETTINGS_USE_GCMD_BLOCK, &(use_gcmd_block));

    gchar *quick_connect_uri = gnome_cmd_con_get_uri_string (GNOME_CMD_CON (quick_connect));

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
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_SEARCH_WIN_IS_TRANSIENT , &(options.search_window_is_transient));

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ALWAYS_SHOW_TABS, &(options.always_show_tabs));
    set_gsettings_enum_when_changed (options.gcmd_settings->general, GCMD_SETTINGS_TAB_LOCK_INDICATOR, options.tab_lock_indicator);

    set_gsettings_when_changed      (options.gcmd_settings->filter, GCMD_SETTINGS_FILTER_BACKUP_PATTERN, options.backup_pattern);

    int main_win_state = main_win_maximized ? 4 : 0;
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_MAIN_WIN_STATE, &main_win_state);

    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_WIDTH, &(advrename_defaults.width));
    set_gsettings_when_changed      (options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_HEIGHT, &(advrename_defaults.height));
    set_gsettings_string_array_from_glist(options.gcmd_settings->general, GCMD_SETTINGS_ADVRENAME_TOOL_TEMPLATE_HISTORY, advrename_defaults.templates.ents);

    save_tabs                       (options.gcmd_settings->general, GCMD_SETTINGS_FILE_LIST_TABS, main_win);
    save_devices                    ();
    save_cmdline_history            (main_win);
    save_directory_history          ();
    save_search_history             ();
    save_search_profiles            ();
    save_connections                ();
    save_bookmarks                  ();
    save_advrename_profiles         ();

    g_settings_sync ();
}


/**
 * This function tests if the given colorstring enholds a valid color-describing string.
 * See documentation of gdk_color_parse() for valid strings.
 * @returns TRUE if the string is a valid color representation, else FALSE.
 */
gboolean GnomeCmdData::is_valid_color_string(const char *colorstring)
{
    g_return_val_if_fail(colorstring, FALSE);

    GdkRGBA test_color;
    return gdk_rgba_parse (&test_color, colorstring);
}

/**
 * This function loads a color specification, stored at the char pointer spec,
 * into *color if it is a valid color specification.
 * @returns the return value of gdk_color_parse function.
 */
gboolean GnomeCmdData::gnome_cmd_data_parse_color (const gchar *spec, GdkRGBA *color)
{
    g_return_val_if_fail(spec,FALSE);
    g_return_val_if_fail(color,FALSE);

    return gdk_rgba_parse (color, spec);
}

/**
 * The task of this function is to store red, green and blue color
 * values in the GdkRGBA variable to which color is pointing to, based
 * on the GSettings value of key. First, it is tested if this value is a
 * valid GdkRGBA string. If yes, color is updated; if not, the current
 * string representing color is used to set back the string in the
 * GSettings key.
 */
gboolean GnomeCmdData::set_color_if_valid_key_value(GdkRGBA *color, GSettings *settings_given, const char *key)
{
    gboolean return_value;
    gchar *colorstring_new;

    colorstring_new = g_settings_get_string (settings_given, key);
    if (!is_valid_color_string(colorstring_new))
    {
        gchar *colorstring_old;

        colorstring_old = gdk_rgba_to_string (color);
        g_settings_set_string (settings_given, key, colorstring_old);
        g_warning("Illegal color string \'%s\'. Resetting to old value \'%s\'", colorstring_new, colorstring_old);
        g_free(colorstring_old);
        return_value = TRUE;
    }
    else
    {
        gnome_cmd_data_parse_color(colorstring_new, color);
        return_value = FALSE;
    }
    g_free(colorstring_new);
    return return_value;
}

/**
 * As GSettings enum-type is of GVARIANT_CLASS String, we need a separate function for
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

gboolean GnomeCmdData::set_gsettings_color_when_changed (GSettings *settings_given, const char *key, GdkRGBA *color)
{
    gboolean return_value;
    gchar *colorstring;

    colorstring = gdk_rgba_to_string (color);
    return_value = set_gsettings_when_changed (settings_given, key, colorstring);
    g_free(colorstring);

    return return_value;
}


GnomeCmdData::SearchProfile::~SearchProfile(){};
GnomeCmdData::AdvrenameConfig::Profile::~Profile(){};


gpointer gnome_cmd_data_get_con_list ()
{
    return gnome_cmd_data.priv->con_list;
}
