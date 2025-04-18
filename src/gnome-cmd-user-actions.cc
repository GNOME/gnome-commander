/**
 * @file gnome-cmd-user-actions.cc
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

#include <gtk/gtk.h>
#include <set>
#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-home.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-dir-indicator.h"
#include "utils.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"
#include "dialogs/gnome-cmd-mkdir-dialog.h"
#include "dialogs/gnome-cmd-search-dialog.h"
#include "dialogs/gnome-cmd-options-dialog.h"

using namespace std;

/***********************************
 * Functions for using GSettings
 ***********************************/

struct _GcmdUserActionSettings
{
    GObject parent;
    GSettings *filter;
    GSettings *general;
    GSettings *programs;
};

G_DEFINE_TYPE (GcmdUserActionSettings, gcmd_user_action_settings, G_TYPE_OBJECT)

static void gcmd_user_action_settings_finalize (GObject *object)
{
    G_OBJECT_CLASS (gcmd_user_action_settings_parent_class)->finalize (object);
}

static void gcmd_user_action_settings_dispose (GObject *object)
{
    GcmdUserActionSettings *gs = GCMD_USER_ACTIONS (object);

    g_clear_object (&gs->general);
    g_clear_object (&gs->programs);

    G_OBJECT_CLASS (gcmd_user_action_settings_parent_class)->dispose (object);
}

static void gcmd_user_action_settings_class_init (GcmdUserActionSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcmd_user_action_settings_finalize;
    object_class->dispose = gcmd_user_action_settings_dispose;
}

GcmdUserActionSettings *gcmd_user_action_settings_new ()
{
    return (GcmdUserActionSettings *) g_object_new (USER_ACTION_SETTINGS, nullptr);
}

static void gcmd_user_action_settings_init (GcmdUserActionSettings *gs)
{
    GSettingsSchemaSource   *global_schema_source;
    GSettingsSchema         *global_schema;

    global_schema_source = GnomeCmdData::GetGlobalSchemaSource();

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_FILTER, FALSE);
    gs->filter = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_GENERAL, FALSE);
    gs->general = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_PROGRAMS, FALSE);
    gs->programs = g_settings_new_full (global_schema, nullptr, nullptr);
}

/***********************************
 * UserActions
 ***********************************/

#define GNOME_CMD_USER_ACTION(f)       extern "C" void f(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#define GNOME_CMD_USER_ACTION_TGL(f)   extern "C" void f(GSimpleAction *action, GVariant *parameter, gpointer user_data)

/************** File Menu **************/
GNOME_CMD_USER_ACTION(file_copy);
GNOME_CMD_USER_ACTION(file_copy_as);
GNOME_CMD_USER_ACTION(file_move);
GNOME_CMD_USER_ACTION(file_delete);
GNOME_CMD_USER_ACTION(file_view);
GNOME_CMD_USER_ACTION(file_internal_view);
GNOME_CMD_USER_ACTION(file_external_view);
GNOME_CMD_USER_ACTION(file_edit);
GNOME_CMD_USER_ACTION(file_edit_new_doc);
GNOME_CMD_USER_ACTION(file_search);
GNOME_CMD_USER_ACTION(file_quick_search);
GNOME_CMD_USER_ACTION(file_chmod);
GNOME_CMD_USER_ACTION(file_chown);
GNOME_CMD_USER_ACTION(file_mkdir);
GNOME_CMD_USER_ACTION(file_properties);
GNOME_CMD_USER_ACTION(file_diff);
GNOME_CMD_USER_ACTION(file_sync_dirs);
GNOME_CMD_USER_ACTION(file_rename);
GNOME_CMD_USER_ACTION(file_create_symlink);
GNOME_CMD_USER_ACTION(file_advrename);
GNOME_CMD_USER_ACTION(file_sendto);
GNOME_CMD_USER_ACTION(file_exit);

/************** Mark Menu **************/
GNOME_CMD_USER_ACTION(mark_toggle);
GNOME_CMD_USER_ACTION(mark_toggle_and_step);
GNOME_CMD_USER_ACTION(mark_select_all);
GNOME_CMD_USER_ACTION(mark_unselect_all);
GNOME_CMD_USER_ACTION(mark_select_all_files);
GNOME_CMD_USER_ACTION(mark_unselect_all_files);
GNOME_CMD_USER_ACTION(mark_select_with_pattern);
GNOME_CMD_USER_ACTION(mark_unselect_with_pattern);
GNOME_CMD_USER_ACTION(mark_invert_selection);
GNOME_CMD_USER_ACTION(mark_select_all_with_same_extension);
GNOME_CMD_USER_ACTION(mark_unselect_all_with_same_extension);
GNOME_CMD_USER_ACTION(mark_restore_selection);
GNOME_CMD_USER_ACTION(mark_compare_directories);

/************** Edit Menu **************/
GNOME_CMD_USER_ACTION(edit_cap_cut);
GNOME_CMD_USER_ACTION(edit_cap_copy);
GNOME_CMD_USER_ACTION(edit_cap_paste);
GNOME_CMD_USER_ACTION(edit_filter);
GNOME_CMD_USER_ACTION(edit_copy_fnames);

/************** View Menu **************/
GNOME_CMD_USER_ACTION_TGL(view_conbuttons);
GNOME_CMD_USER_ACTION_TGL(view_devlist);
GNOME_CMD_USER_ACTION_TGL(view_toolbar);
GNOME_CMD_USER_ACTION_TGL(view_buttonbar);
GNOME_CMD_USER_ACTION_TGL(view_cmdline);
GNOME_CMD_USER_ACTION(view_dir_history);
GNOME_CMD_USER_ACTION_TGL(view_hidden_files);
GNOME_CMD_USER_ACTION_TGL(view_backup_files);
GNOME_CMD_USER_ACTION(view_first);
GNOME_CMD_USER_ACTION(view_back);
GNOME_CMD_USER_ACTION(view_forward);
GNOME_CMD_USER_ACTION(view_last);
GNOME_CMD_USER_ACTION(view_equal_panes);
GNOME_CMD_USER_ACTION(view_maximize_pane);
GNOME_CMD_USER_ACTION(view_in_left_pane);
GNOME_CMD_USER_ACTION(view_in_right_pane);
GNOME_CMD_USER_ACTION(view_in_active_pane);
GNOME_CMD_USER_ACTION(view_in_inactive_pane);
GNOME_CMD_USER_ACTION(view_directory);
GNOME_CMD_USER_ACTION(view_close_tab);
GNOME_CMD_USER_ACTION(view_close_all_tabs);
GNOME_CMD_USER_ACTION(view_close_duplicate_tabs);
GNOME_CMD_USER_ACTION(view_prev_tab);
GNOME_CMD_USER_ACTION(view_next_tab);
GNOME_CMD_USER_ACTION(view_toggle_tab_lock);
GNOME_CMD_USER_ACTION_TGL(view_horizontal_orientation);
GNOME_CMD_USER_ACTION(view_main_menu);
GNOME_CMD_USER_ACTION(view_step_up);
GNOME_CMD_USER_ACTION(view_step_down);

/************** Bookmarks Menu **************/
GNOME_CMD_USER_ACTION(bookmarks_add_current);
GNOME_CMD_USER_ACTION(bookmarks_edit);
GNOME_CMD_USER_ACTION(bookmarks_goto);
GNOME_CMD_USER_ACTION(bookmarks_view);

/************** Options Menu **************/
GNOME_CMD_USER_ACTION(options_edit);
GNOME_CMD_USER_ACTION(options_edit_shortcuts);

/************** Plugins Menu ***********/
GNOME_CMD_USER_ACTION(plugins_configure);

static GnomeCmdFileList *get_fl (GnomeCmdMainWin *main_win, const FileSelectorID fsID)
{
    GnomeCmdFileSelector *fs = main_win->fs(fsID);

    return fs ? fs->file_list() : nullptr;
}


GcmdUserActionSettings *settings;


/************** File Menu **************/

void file_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_delete_dialog (get_fl (main_win, ACTIVE));
}


void file_search (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    if (gnome_cmd_data.options.use_internal_search)
    {
        auto dlg = main_win->get_or_create_search_dialog ();
        dlg->show_and_set_focus ();
    }
    else
    {
        GnomeCmdFileList *fl = get_fl (main_win, ACTIVE);
        GList *sfl = fl->get_selected_files();

        GError *error = nullptr;
        int result = spawn_async_r(nullptr, sfl, gnome_cmd_data.options.search, &error);
        switch (result)
        {
            case 0:
                break;
            case 1:
            case 2:
                DEBUG ('g', "Search command is empty.\n");
                gnome_cmd_show_message (*main_win, _("No search command given."), _("You can set a command for a search tool in the program options."));
                g_clear_error (&error);
                break;
            case 3:
            default:
                gnome_cmd_error_message (*main_win, _("Unable to execute command."), error);
                break;
        }
    }
}


void file_mkdir (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdDir *dir = fs->get_directory();

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gnome_cmd_dir_ref (dir);
    gnome_cmd_mkdir_dialog_new (dir, fs->file_list()->get_selected_file());
    gnome_cmd_dir_unref (dir);
}


void file_advrename (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GList *files = get_fl (main_win, ACTIVE)->get_selected_files();

    if (files)
    {
        auto dlg = main_win->get_or_create_advrename_dialog ();
        dlg->set(files);
        gtk_window_present (GTK_WINDOW (dlg));

        g_list_free (files);
    }
}


void file_exit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gtk_window_destroy (GTK_WINDOW (main_win));
}


/************** Edit Menu **************/
void edit_filter (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->show_filter();
}


/************** Mark Menu **************/
void mark_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->toggle();
}


void mark_toggle_and_step (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->toggle_and_step();
}


void mark_select_all (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->select_all();
}


void mark_select_all_files (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->select_all_files();
}


void mark_unselect_all_files (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->unselect_all_files();
}


void mark_unselect_all (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->unselect_all();
}


void mark_select_with_pattern (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_selpat_dialog (get_fl (main_win, ACTIVE), TRUE);
}


void mark_unselect_with_pattern (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_selpat_dialog (get_fl (main_win, ACTIVE), FALSE);
}

/* ***************************** View Menu ****************************** */
/* Changing of GSettings here will trigger functions in gnome-cmd-data.cc */
/* ********************************************************************** */

void view_conbuttons (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS, active);
}


void view_devlist (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->general, GCMD_SETTINGS_SHOW_DEVLIST, active);
}


void view_toolbar (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->general, GCMD_SETTINGS_SHOW_TOOLBAR, active);
}


void view_buttonbar (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR, active);
}


void view_cmdline (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->general, GCMD_SETTINGS_SHOW_CMDLINE, active);
}


void view_dir_history (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_dir_indicator_show_history (GNOME_CMD_DIR_INDICATOR (main_win->fs (ACTIVE)->dir_indicator));
}


void view_hidden_files (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->filter, GCMD_SETTINGS_FILTER_HIDE_HIDDEN, !active);
}


void view_backup_files (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->filter, GCMD_SETTINGS_FILTER_HIDE_BACKUPS, !active);
}


void view_horizontal_orientation (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION, active);
}

void view_step_up (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    fl->focus_prev();
}

void view_step_down (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    fl->focus_next();
}


void view_main_menu (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    if (!gtk_widget_get_realized ( GTK_WIDGET (main_win))) return;

    gboolean mainmenu_visibility;
    mainmenu_visibility = g_settings_get_boolean (settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY);
    g_settings_set_boolean (settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY, !mainmenu_visibility);
}

void view_first (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->first();
}


void view_back (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->back();
}


void view_forward (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->forward();
}


void view_last (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->last();
}


void view_equal_panes (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_equal_panes();
}


void view_maximize_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->maximize_pane();
}


void view_in_left_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(LEFT);
}


void view_in_right_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(RIGHT);
}


void view_in_active_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(ACTIVE);
}


void view_in_inactive_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(INACTIVE);
}


void view_directory (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    GnomeCmdFile *f = fl->get_selected_file();
    if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        fs->do_file_specific_action (fl, f);
}


void view_prev_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->prev_tab();
}


void view_next_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->next_tab();
}


void view_toggle_tab_lock (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    if (fl)
    {
        fl->locked = !fl->locked;
        fs->update_tab_label(fl);
    }
}


/************** Options Menu **************/
static void options_edit_done (gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->update_view();
    gnome_cmd_data.save(main_win);
}


void options_edit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GtkDialog *options_dialog = gnome_cmd_options_dialog (*main_win, gnome_cmd_data.options, options_edit_done, main_win);
    gtk_window_present (GTK_WINDOW (options_dialog));
}


/************** Bookmarks Menu **************/

void bookmarks_view (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_dir_indicator_show_bookmarks (GNOME_CMD_DIR_INDICATOR (main_win->fs (ACTIVE)->dir_indicator));
}
