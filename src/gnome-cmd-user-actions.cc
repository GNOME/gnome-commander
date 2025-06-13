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
#include "gnome-cmd-dir-indicator.h"
#include "utils.h"
#include "dialogs/gnome-cmd-mkdir-dialog.h"

using namespace std;


/***********************************
 * UserActions
 ***********************************/

#define GNOME_CMD_USER_ACTION(f)       extern "C" void f(GSimpleAction *action, GVariant *parameter, gpointer user_data)

/************** File Menu **************/
GNOME_CMD_USER_ACTION(file_delete);
GNOME_CMD_USER_ACTION(file_mkdir);
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
GNOME_CMD_USER_ACTION(view_dir_history);
GNOME_CMD_USER_ACTION(view_in_left_pane);
GNOME_CMD_USER_ACTION(view_in_right_pane);
GNOME_CMD_USER_ACTION(view_in_active_pane);
GNOME_CMD_USER_ACTION(view_in_inactive_pane);
GNOME_CMD_USER_ACTION(view_directory);
GNOME_CMD_USER_ACTION(view_prev_tab);
GNOME_CMD_USER_ACTION(view_next_tab);
GNOME_CMD_USER_ACTION(view_toggle_tab_lock);
GNOME_CMD_USER_ACTION(view_step_up);
GNOME_CMD_USER_ACTION(view_step_down);

/************** Bookmarks Menu **************/
GNOME_CMD_USER_ACTION(bookmarks_view);


static GnomeCmdFileList *get_fl (GnomeCmdMainWin *main_win, const FileSelectorID fsID)
{
    GnomeCmdFileSelector *fs = main_win->fs(fsID);

    return fs ? fs->file_list() : nullptr;
}

/************** File Menu **************/

void file_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_delete_dialog (get_fl (main_win, ACTIVE));
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

void view_dir_history (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_selector_show_history (main_win->fs (ACTIVE));
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
        gboolean locked = gnome_cmd_file_selector_is_tab_locked (fs, fl);
        gnome_cmd_file_selector_set_tab_locked (fs, fl, !locked);
        gnome_cmd_file_selector_update_tab_label (fs, fl);
    }
}


/************** Bookmarks Menu **************/

void bookmarks_view (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_selector_show_bookmarks (main_win->fs (ACTIVE));
}
