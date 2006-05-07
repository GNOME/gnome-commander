/*
  GNOME Commander - A GNOME based file manager
  Copyright (C) 2001-2006 Marcus Bjurman

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <config.h>
#include <gtk/gtkclipboard.h>
#include "gnome-cmd-includes.h"
#include "useractions.h"
#include "gnome-cmd-main-win.h"
#include "plugin_manager.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "cap.h"
#include "gnome-cmd-prepare-copy-dialog.h"
#include "gnome-cmd-prepare-move-dialog.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-search-dialog.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-options-dialog.h"
#include "gnome-cmd-bookmark-dialog.h"
#include "gnome-cmd-ftp-dialog.h"
#include "utils.h"


static GnomeCmdFileList *get_active_fl ()
{
    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_active_fs (main_win);

    if (fs) return fs->list;
    return NULL;
}


static GnomeCmdFileSelector *get_active_fs ()
{
    return gnome_cmd_main_win_get_active_fs (main_win);
}


static GnomeCmdFileSelector *get_inactive_fs ()
{
    return gnome_cmd_main_win_get_inactive_fs (main_win);
}


/**
 * The file returned from this function is not to be unrefed
 */
static GnomeCmdFile *
get_selected_file ()
{
    GnomeCmdFile *finfo = gnome_cmd_file_list_get_selected_file (get_active_fl ());

    if (!finfo)
        create_error_dialog ("No file selected");
    return finfo;
}



/************** File Menu **************/
void
file_cap_cut                        (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_cap_cut (get_active_fl ());
}


void
file_cap_copy                       (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_cap_copy (get_active_fl ());
}


void
file_cap_paste                      (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_selector_cap_paste (get_active_fs ());
}


void
file_copy                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GnomeCmdFileSelector *src_fs;
    GnomeCmdFileSelector *dest_fs;

    if (main_win) {
        src_fs = gnome_cmd_main_win_get_active_fs (main_win);
        dest_fs = gnome_cmd_main_win_get_inactive_fs (main_win);

        if (src_fs && dest_fs)
            gnome_cmd_prepare_copy_dialog_show (src_fs, dest_fs);
    }
}


void
file_move                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GnomeCmdFileSelector *src_fs;
    GnomeCmdFileSelector *dest_fs;

    if (main_win) {
        src_fs = gnome_cmd_main_win_get_active_fs (main_win);
        dest_fs = gnome_cmd_main_win_get_inactive_fs (main_win);

        if (src_fs && dest_fs)
            gnome_cmd_prepare_move_dialog_show (src_fs, dest_fs);
    }
}


void
file_delete                         (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_delete_dialog (get_active_fl ());
}


void
file_view                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_view (get_active_fl (), -1);
}


void
file_internal_view                  (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_view (get_active_fl (), TRUE);
}


void
file_external_view                  (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_view (get_active_fl (), FALSE);
}


void
file_edit                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GdkModifierType mask;

    gdk_window_get_pointer (NULL, NULL, NULL, &mask);
    
    if (mask & GDK_SHIFT_MASK)
        gnome_cmd_file_selector_start_editor (get_active_fs ());
    else
        gnome_cmd_file_list_edit (get_active_fl ());
}


void
file_chmod                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_chmod_dialog (get_active_fl ());
}


void
file_chown                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_chown_dialog (get_active_fl ());
}


void
file_mkdir                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_selector_show_mkdir_dialog (get_active_fs ());
}


void
file_create_symlink                 (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GnomeCmdFile *finfo = gnome_cmd_file_list_get_focused_file (get_active_fl ());
    gnome_cmd_file_selector_create_symlink (
        gnome_cmd_main_win_get_inactive_fs (main_win), finfo);
}


void
file_rename                         (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_rename_dialog (get_active_fl ());
}


void
file_advrename                      (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_advrename_dialog (get_active_fl ());
}


void
file_properties                     (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_properties_dialog (get_active_fl ());
}


void
file_diff                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GnomeCmdFile *finfo = get_selected_file ();

    if (finfo)
    {
        GList *tmp;
        gchar *cmd, *p1, *p2 = NULL;
        GnomeCmdFile *finfo2 = NULL;
        gboolean found = FALSE;
        GList *all_files = gnome_cmd_file_list_get_all_files (GNOME_CMD_FILE_LIST (gnome_cmd_main_win_get_inactive_fs (main_win)->list));
        

        /**
         * Go through all the files in the other list until we find one with the same name
         */
        for (tmp = all_files; tmp; tmp = tmp->next)
        {
            finfo2 = (GnomeCmdFile*)tmp->data;

            if (strcmp (gnome_cmd_file_get_name (finfo), gnome_cmd_file_get_name (finfo2)) == 0)
            {
                found = TRUE;
                break;
            }
        }

        p1 = g_shell_quote (gnome_cmd_file_get_real_path (finfo));

        if (found)
            p2 = g_shell_quote (gnome_cmd_file_get_real_path (finfo2));

        cmd = g_strdup_printf (gnome_cmd_data_get_differ (), p1, p2?:"");

        g_free (p1);
        if (p2)
            g_free (p2);

        run_command (cmd, FALSE);
        g_print (_("running \"%s\"\n"), cmd);
        g_free (cmd);
    }
}


void
file_exit                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gint x, y;

    gdk_window_get_root_origin (GTK_WIDGET (main_win)->window, &x, &y);
    gnome_cmd_data_set_main_win_pos (x, y);

    gtk_widget_destroy (GTK_WIDGET (main_win));
}


/************** Edit Menu **************/
void
edit_search                         (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GnomeCmdFileSelector *fs = get_active_fs ();
    GtkWidget *dialog =
        gnome_cmd_search_dialog_new (
            gnome_cmd_file_selector_get_directory (fs));
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


void
edit_quick_search                   (GtkMenuItem     *menuitem,
                                      gpointer        not_used)
{
    gnome_cmd_file_list_show_quicksearch (get_active_fl (), 0);
}


void
edit_filter                         (GtkMenuItem     *menuitem,
                                      gpointer        not_used)
{
    gnome_cmd_file_selector_show_filter (get_active_fs (), 0);
}


void
edit_copy_fnames                    (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GdkModifierType mask;

    static gchar sep[] = " ";

    gdk_window_get_pointer (NULL, NULL, NULL, &mask);
    
    GnomeCmdFileList *fl = get_active_fl ();
    GList *sfl = gnome_cmd_file_list_get_selected_files (fl);
    GList *i;
    gchar **fnames = g_new (char*, g_list_length (sfl) + 1);
    gchar **f = fnames;
    gchar *s;

    sfl = gnome_cmd_file_list_sort_selection (sfl, fl);

    for (i = sfl; i; i = i->next) 
	{
        GnomeCmdFile *finfo = GNOME_CMD_FILE (i->data);

        if (finfo)
          *f++ = (mask & GDK_SHIFT_MASK) ? (char*)gnome_cmd_file_get_real_path (finfo) :
                                           (char*)gnome_cmd_file_get_name (finfo);
    }

    *f = NULL;

    s = g_strjoinv(sep,fnames);

    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), s, -1);

    g_free (s);
    g_list_free (sfl);
    g_free (fnames);
}


/************** Mark Menu **************/
void
mark_toggle                         (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_toggle (get_active_fl ());
}


void
mark_toggle_and_step                (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_toggle_and_step (get_active_fl ());
}


void
mark_select_all                     (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_select_all (get_active_fl ());
}


void
mark_unselect_all                   (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_unselect_all (get_active_fl ());
}


void
mark_select_with_pattern            (GtkMenuItem    *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_selpat_dialog (get_active_fl (), TRUE);
}


void
mark_unselect_with_pattern            (GtkMenuItem    *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_show_selpat_dialog (get_active_fl (), FALSE);
}


void
mark_invert_selection                 (GtkMenuItem    *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_invert_selection (get_active_fl ());
}


void
mark_select_all_with_same_extension   (GtkMenuItem    *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_select_all_with_same_extension (get_active_fl ());
}


void
mark_unselect_all_with_same_extension   (GtkMenuItem    *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_unselect_all_with_same_extension (get_active_fl ());
}


void
mark_restore_selection                 (GtkMenuItem    *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_restore_selection (get_active_fl ());
}


void
mark_compare_directories               (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_file_list_compare_directories ();
}


/************** View Menu **************/

void
view_conbuttons                        (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;
    gnome_cmd_data_set_conbuttons_visibility (menuitem->active);
    gnome_cmd_file_selector_update_conbuttons_visibility (get_active_fs ());
    gnome_cmd_file_selector_update_conbuttons_visibility (get_inactive_fs ());
}


void
view_toolbar                           (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;
    gnome_cmd_data_set_toolbar_visibility (menuitem->active);
    gnome_cmd_main_win_update_toolbar_visibility (main_win);
}


void
view_buttonbar                         (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;
    gnome_cmd_data_set_buttonbar_visibility (menuitem->active);
    gnome_cmd_main_win_update_buttonbar_visibility (main_win);
}


void
view_cmdline                           (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;
    gnome_cmd_data_set_cmdline_visibility (menuitem->active);
    gnome_cmd_main_win_update_cmdline_visibility (main_win);
}


void
view_hidden_files                      (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;
    gnome_cmd_data_get_filter_settings()->hidden = !menuitem->active;
    gnome_cmd_file_selector_reload (get_active_fs ());
    gnome_cmd_file_selector_reload (get_inactive_fs ());
}


void
view_backup_files                      (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;
    gnome_cmd_data_get_filter_settings()->backup = !menuitem->active;
    gnome_cmd_file_selector_reload (get_active_fs ());
    gnome_cmd_file_selector_reload (get_inactive_fs ());
}


void
view_up                                 (GtkMenuItem     *menuitem,
                                        gpointer        not_used)
{
    gnome_cmd_file_selector_goto_directory (get_active_fs (), "..");
}


void
view_first                             (GtkMenuItem     *menuitem,
                                        gpointer        not_used)
{
    gnome_cmd_file_selector_first (get_active_fs ());
}


void
view_back                              (GtkMenuItem     *menuitem,
                                        gpointer        not_used)
{
    gnome_cmd_file_selector_back (get_active_fs ());
}


void
view_forward                           (GtkMenuItem     *menuitem,
                                        gpointer        not_used)
{
    gnome_cmd_file_selector_forward (get_active_fs ());
}


void
view_last                              (GtkMenuItem     *menuitem,
                                        gpointer        not_used)
{
    gnome_cmd_file_selector_last (get_active_fs ());
}


void
view_refresh                           (GtkMenuItem     *menuitem,
                                        gpointer        not_used)
{
    gnome_cmd_file_selector_reload (get_active_fs ());
}


/************** Options Menu **************/
void
options_edit                        (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GtkWidget *dialog = gnome_cmd_options_dialog_new ();
    gtk_widget_ref (dialog);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_win));
    gtk_widget_show (dialog);
}


void
options_edit_mime_types             (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    edit_mimetypes (NULL, FALSE);
}


/************** Connections Menu **************/
void
connections_ftp_connect             (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GtkWidget *dialog = gnome_cmd_ftp_dialog_new ();
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


void
connections_ftp_quick_connect       (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    show_ftp_quick_connect_dialog ();
}


void
connections_change                  (GtkMenuItem *menuitem,
                                     GnomeCmdCon *con)
{
    gnome_cmd_file_selector_set_connection (get_active_fs (), con, NULL);
}


void
connections_close           (GtkMenuItem     *menuitem,
                             GnomeCmdCon *con)
{
    GnomeCmdCon *c1, *c2, *home;
    GnomeCmdFileSelector *active, *inactive;

    active = gnome_cmd_main_win_get_active_fs (main_win);
    inactive = gnome_cmd_main_win_get_inactive_fs (main_win);

    c1 = gnome_cmd_file_selector_get_connection (active);
    c2 = gnome_cmd_file_selector_get_connection (inactive);
    home = gnome_cmd_con_list_get_home (gnome_cmd_data_get_con_list ());

    if (con == c1)
        gnome_cmd_file_selector_set_connection (active, home, NULL);
    if (con == c2)
        gnome_cmd_file_selector_set_connection (inactive, home, NULL);

    gnome_cmd_con_close (con);
}


void
connections_close_current           (GtkMenuItem     *menuitem,
                                     gpointer         not_used)
{
    GnomeCmdCon *con = gnome_cmd_file_selector_get_connection (get_active_fs ());

    connections_close (menuitem, con);
}


/************** Bookmarks Menu **************/

void
bookmarks_add_current               (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_bookmark_add_current ();
}


void
bookmarks_edit                      (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    GtkWidget *dialog = gnome_cmd_bookmark_dialog_new ();
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}



/************** Plugins Menu **************/

void
plugins_configure                   (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    plugin_manager_show ();
}



/************** Help Menu **************/

void
help_help                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_help_display("gnome-commander.xml",NULL);
}


void
help_keyboard                       (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gnome_cmd_help_display("gnome-commander.xml","gnome-commander-keyboard");
}


void
help_about                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used)
{
    gchar *authors[] = {
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        "Assaf Gordon <agordon88@gmail.com>",
        NULL
    };

    gchar *docers[] = {
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        NULL
    };

    GtkWidget *dialog = gnome_about_new (
        "GNOME Commander",
        VERSION,
        "Copyright (C) 2001-2006 Marcus Bjurman",
        _("A fast and powerful file manager for the GNOME desktop"),
        (const gchar**)authors,
        (const gchar**)docers,
        /* xgettext:
         * Replace this with your name when translating  */
        _("No translation is currently in use."),
        NULL);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}
