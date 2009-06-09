/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include <sys/types.h>
#include <regex.h>
#include <errno.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-con-smb.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-patternsel-dialog.h"
#include "gnome-cmd-mkdir-dialog.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-list-popmenu.h"
#include "gnome-cmd-user-actions.h"
#include "history.h"
#include "cap.h"
#include "utils.h"
#include "gnome-cmd-make-copy-dialog.h"

using namespace std;


struct GnomeCmdFileSelectorClass
{
    GtkVBoxClass parent_class;

    void (* changed_dir) (GnomeCmdFileSelector *fs, GnomeCmdDir *dir);
};


#define FS_PBAR_MAX 50


GtkTargetEntry drop_types [] = {
    { TARGET_URI_LIST_TYPE, 0, TARGET_URI_LIST },
    { TARGET_URL_TYPE, 0, TARGET_URL }
};

static GtkVBoxClass *parent_class = NULL;

class GnomeCmdFileSelector::Private
{
  public:

    GList *old_btns;
    GtkWidget *filter_box;

    History *dir_history;
    gboolean active;
    gboolean realized;

    GnomeCmdFile *sym_file;

    Private();
    ~Private();

    //////////////////////////////////////////////////////////////////  ->> GnomeCmdFileList

    gboolean sel_first_file;

    GtkWidget *con_open_dialog;
    GtkWidget *con_open_dialog_label;
    GtkWidget *con_open_dialog_pbar;
    GnomeCmdCon *con_opening;

    gboolean autoscroll_dir;
    guint autoscroll_timeout;
    gint autoscroll_y;
};


inline GnomeCmdFileSelector::Private::Private()
{
    old_btns = NULL;
    filter_box = NULL;
    active = FALSE;
    realized = FALSE;
    sel_first_file = TRUE;
    dir_history = NULL;
    sym_file = NULL;

    con_open_dialog = NULL;
    con_open_dialog_label = NULL;
    con_open_dialog_pbar = NULL;
    con_opening = NULL;

    autoscroll_dir = FALSE;
    autoscroll_timeout = 0;
    autoscroll_y = 0;
}


inline GnomeCmdFileSelector::Private::~Private()
{
}


enum {CHANGED_DIR, LAST_SIGNAL};

static guint file_selector_signals[LAST_SIGNAL] = { 0 };

/*******************************
 * Utility functions
 *******************************/


inline void show_list_popup (GnomeCmdFileSelector *fs)
{
    // create the popup menu
    GtkWidget *menu = gnome_cmd_list_popmenu_new (fs);
    gtk_widget_ref (menu);

    gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, fs, 3, GDK_CURRENT_TIME);
}


inline void GnomeCmdFileSelector::update_selected_files_label()
{
    GList *all_files = file_list()->get_visible_files();

    if (!all_files)
        return;

    GnomeVFSFileSize sel_bytes = 0;
    GnomeVFSFileSize total_bytes = 0;
    gint num_files = 0;
    gint num_dirs = 0;
    gint num_sel_files = 0;
    gint num_sel_dirs = 0;

    GnomeCmdSizeDispMode size_mode = gnome_cmd_data.size_disp_mode;
    if (size_mode==GNOME_CMD_SIZE_DISP_MODE_POWERED)
        size_mode = GNOME_CMD_SIZE_DISP_MODE_GROUPED;

    for (GList *i = all_files; i; i = i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        switch (f->info->type)
        {
            case GNOME_VFS_FILE_TYPE_DIRECTORY:
                if (strcmp(f->info->name, "..") != 0)
                {
                    num_dirs++;
                    if (gnome_cmd_file_has_tree_size (f))
                        total_bytes += gnome_cmd_file_get_tree_size (f);
                }
                break;

            case GNOME_VFS_FILE_TYPE_REGULAR:
                num_files++;
                total_bytes += f->info->size;
                break;

            default:
                break;
        }
    }

    for (GList *i = file_list()->get_marked_files(); i; i = i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        switch (f->info->type)
        {
            case GNOME_VFS_FILE_TYPE_DIRECTORY:
                num_sel_dirs++;
                if (gnome_cmd_file_has_tree_size (f))
                    sel_bytes += gnome_cmd_file_get_tree_size (f);
                break;

            case GNOME_VFS_FILE_TYPE_REGULAR:
                num_sel_files++;
                sel_bytes += f->info->size;
                break;

            default:
                break;
        }
    }

    gchar *sel_str = g_strdup (size2string (sel_bytes/1024, size_mode));
    gchar *total_str = g_strdup (size2string (total_bytes/1024, size_mode));

    gchar *file_str = g_strdup_printf (ngettext("%s of %s kB in %d of %d file",
                                                "%s of %s kB in %d of %d files",
                                                num_files),
                                       sel_str, total_str, num_sel_files, num_files);
    gchar *info_str = g_strdup_printf (ngettext("%s, %d of %d dir selected",
                                                "%s, %d of %d dirs selected",
                                                num_dirs),
                                       file_str, num_sel_dirs, num_dirs);

    gtk_label_set_text (GTK_LABEL (info_label), info_str);

    g_free (sel_str);
    g_free (total_str);
    g_free (file_str);
    g_free (info_str);
}


inline void show_dir_tree_sizes (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    fs->file_list()->invalidate_tree_size();

    for (GList *files = fs->file_list()->get_visible_files(); files; files = files->next)
        fs->file_list()->show_dir_tree_size((GnomeCmdFile *) files->data);

    fs->update_selected_files_label();
}


inline void set_connection (GnomeCmdFileSelector *fs, GnomeCmdCon *con, GnomeCmdDir *dir=NULL)
{
    gboolean con_change_needed = fs->get_connection()==con;

    if (!con_change_needed)
    {
        fs->file_list()->con = con;
        fs->priv->dir_history = gnome_cmd_con_get_dir_history (con);

        if (fs->file_list()->lwd)
        {
            gnome_cmd_dir_cancel_monitoring (fs->file_list()->lwd);
            gnome_cmd_dir_unref (fs->file_list()->lwd);
            fs->file_list()->lwd = NULL;
        }
        if (fs->file_list()->cwd)
        {
            gnome_cmd_dir_cancel_monitoring (fs->file_list()->cwd);
            gnome_cmd_dir_unref (fs->file_list()->cwd);
            fs->file_list()->cwd = NULL;
        }
    }

    if (!dir)
        dir = gnome_cmd_con_should_remember_dir (con) ? gnome_cmd_con_get_cwd (con) :
                                                        gnome_cmd_con_get_default_dir (con);

    fs->set_directory(dir);

    if (!con_change_needed)
        gnome_cmd_combo_select_data (GNOME_CMD_COMBO (fs->con_combo), con);
}


inline void GnomeCmdFileSelector::update_files()
{
    GnomeCmdDir *dir = get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    file_list()->show_files(dir);
    gnome_cmd_clist_set_voffset (GNOME_CMD_CLIST (file_list()), file_list()->cwd->voffset);

    if (priv->realized)
        update_selected_files_label();
    if (priv->active)
        file_list()->select_row(0);
}


inline void GnomeCmdFileSelector::update_direntry()
{
    gchar *tmp = gnome_cmd_dir_get_display_path (file_list()->cwd);

    g_return_if_fail (tmp != NULL);

    gnome_cmd_dir_indicator_set_dir (GNOME_CMD_DIR_INDICATOR (dir_indicator), tmp);

    g_free (tmp);
}


void gnome_cmd_file_list_show_make_copy_dialog (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdFile *f = fs->file_list()->get_selected_file();

    if (GNOME_CMD_IS_FILE (f))
    {
        GtkWidget *dialog = gnome_cmd_make_copy_dialog_new (f, fs->file_list()->cwd);

        gtk_widget_ref (dialog);
        gtk_widget_show (dialog);
    }
}


/******************************************************
 * DnD functions
 **/

inline void restore_drag_indicator (GnomeCmdFileSelector *fs)
{
    gnome_cmd_clist_set_drag_row (GNOME_CMD_CLIST (fs->file_list()), -1);
}


static void unref_uri_list (GList *list)
{
    g_return_if_fail (list != NULL);

    g_list_foreach (list, (GFunc) gnome_vfs_uri_unref, NULL);
}


static void
drag_data_received (GtkWidget          *widget,
                    GdkDragContext     *context,
                    gint                x,
                    gint                y,
                    GtkSelectionData   *selection_data,
                    guint               info,
                    guint32             time,
                    GnomeCmdFileSelector *fs)
{
    GtkCList *clist = GTK_CLIST (fs->file_list());
    GnomeCmdFile *f;
    GnomeCmdDir *to, *cwd;
    GList *uri_list = NULL;
    gchar *to_fn = NULL;
    GnomeVFSXferOptions xferOptions;

    // Find out what operation to perform
    switch (context->action)
    {
        case GDK_ACTION_MOVE:
            xferOptions = GNOME_VFS_XFER_REMOVESOURCE;
            break;

        case GDK_ACTION_COPY:
            xferOptions = GNOME_VFS_XFER_RECURSIVE;
            break;

        case GDK_ACTION_LINK:
            xferOptions = GNOME_VFS_XFER_LINK_ITEMS;
            break;

        default:
            warn_print ("Unknown context->action in drag_data_received\n");
            return;
    }


    // Find the row that the file was dropped on
    y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);
    if (y < 0) return;

    int row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->file_list()), x, y);

    // Transform the drag data to a list with uris
    uri_list = strings_to_uris ((gchar *) selection_data->data);

    if (g_list_length (uri_list) == 1)
    {
        GnomeVFSURI *uri = (GnomeVFSURI *) uri_list->data;
        to_fn = gnome_vfs_unescape_string (gnome_vfs_uri_extract_short_name (uri), 0);
    }

    f = fs->file_list()->get_file_at_row(row);
    cwd = fs->get_directory();

    if (f && f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
        /* The drop was over a directory in the list, which means that the
         * xfer should be done to that directory instead of the current one in the list
         */
        if (strcmp (f->info->name, "..") == 0)
            to = gnome_cmd_dir_get_parent (cwd);
        else
            to = gnome_cmd_dir_get_child (cwd, f->info->name);
    }
    else
        to = cwd;

    g_return_if_fail (GNOME_CMD_IS_DIR (to));

    gnome_cmd_dir_ref (to);

    // Start the xfer
    gnome_cmd_xfer_uris_start (uri_list,
                               to,
                               NULL,
                               NULL,
                               to_fn,
                               xferOptions,
                               GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
                               GTK_SIGNAL_FUNC (unref_uri_list),
                               uri_list);
}


static void drag_begin (GtkWidget *widget, GdkDragContext *context, GnomeCmdFileSelector *fs)
{
}


static void drag_end (GtkWidget *widget, GdkDragContext *context, GnomeCmdFileSelector *fs)
{
    restore_drag_indicator (fs);
}


static void drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, GnomeCmdFileSelector *fs)
{
    if (fs->priv->autoscroll_timeout)
    {
        g_source_remove (fs->priv->autoscroll_timeout);
        fs->priv->autoscroll_timeout = 0;
    }

    restore_drag_indicator (fs);
}


static void drag_data_delete (GtkWidget *widget, GdkDragContext *drag_context, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdDir *dir = fs->get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    GList *files = fs->file_list()->get_selected_files();
    fs->file_list()->remove_files(files);
    g_list_free (files);
}


static gboolean do_scroll (GnomeCmdFileSelector *fs)
{
    gint w, h;
    gint focus_row, top_row, bottom_row;
    gint row_count;
    guint offset;
    gint row_height;
    GtkCList *clist = GTK_CLIST (fs->file_list());

    gdk_window_get_size (GTK_WIDGET (clist)->window, &w, &h);

    offset = (0-clist->voffset);
    row_height = gnome_cmd_data.list_row_height;
    row_count = clist->rows;
    focus_row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->file_list()), 1, fs->priv->autoscroll_y);
    top_row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->file_list()), 1, 0);
    bottom_row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->file_list()), 1, h);

    if (!fs->priv->autoscroll_dir)
    {
        if (focus_row > 0)
            gtk_clist_moveto (clist, top_row-1, 0, 0, 0);
        else
            return FALSE;
    }
    else
    {
        if (focus_row < row_count)
            gtk_clist_moveto (clist, top_row+1, 0, 0, 0);
        else
            return FALSE;
    }

    return TRUE;
}


static void autoscroll_if_appropriate (GnomeCmdFileSelector *fs, gint x, gint y)
{
    if (y < 0) return;

    GtkCList *clist = GTK_CLIST (fs->file_list());
    // guint offset = (0-clist->voffset);
    gint w, h;

    gdk_window_get_size (GTK_WIDGET (clist)->window, &w, &h);

    gint smin = h/8;
    gint smax = h-smin;

    if (y < smin)
    {
        if (fs->priv->autoscroll_timeout) return;
        fs->priv->autoscroll_dir = FALSE;
        fs->priv->autoscroll_y = y;
        fs->priv->autoscroll_timeout = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) do_scroll, fs);
    }
    else if (y > smax)
    {
        if (fs->priv->autoscroll_timeout) return;
        fs->priv->autoscroll_dir = TRUE;
        fs->priv->autoscroll_y = y;
        fs->priv->autoscroll_timeout =
            g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) do_scroll, fs);
    }
    else
    {
        if (fs->priv->autoscroll_timeout)
        {
            g_source_remove (fs->priv->autoscroll_timeout);
            fs->priv->autoscroll_timeout = 0;
        }
    }
}


static gboolean drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, GnomeCmdFileSelector *fs)
{
    gdk_drag_status (context, context->suggested_action, time);

    GtkCList *clist = GTK_CLIST (fs->file_list());

    y -= (clist->column_title_area.height - GTK_CONTAINER (clist)->border_width);

    gint row = gnome_cmd_clist_get_row (GNOME_CMD_CLIST (fs->file_list()), x, y);

    if (row > -1)
    {
        GnomeCmdFile *f = fs->file_list()->get_file_at_row(row);

        if (f->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
            row = -1;

        gnome_cmd_clist_set_drag_row (GNOME_CMD_CLIST (clist), row);
    }

    autoscroll_if_appropriate (fs, x, y);

    return FALSE;
}


inline void init_dnd (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    // Set up drag source
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "drag-begin", GTK_SIGNAL_FUNC (drag_begin), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "drag-end", GTK_SIGNAL_FUNC (drag_end), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "drag-leave", GTK_SIGNAL_FUNC (drag_leave), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "drag-data-delete", GTK_SIGNAL_FUNC (drag_data_delete), fs);

    // Set up drag destination
    gtk_drag_dest_set (GTK_WIDGET (fs->file_list()),
                       GTK_DEST_DEFAULT_DROP,
                       drop_types, G_N_ELEMENTS (drop_types),
                       (GdkDragAction) (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK));

    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "drag-motion", GTK_SIGNAL_FUNC (drag_motion), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "drag-leave", GTK_SIGNAL_FUNC (drag_leave), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "drag-data-received", GTK_SIGNAL_FUNC (drag_data_received), fs);
}


static void update_dir_combo (GnomeCmdFileSelector *fs)
{/*

    if (!fs->priv->dir_history)  return;

    GList *tmp = fs->priv->dir_history->ents;

    gnome_cmd_combo_clear (GNOME_CMD_COMBO (fs->dir_combo));

    for (; tmp; tmp = tmp->next)
    {
        gchar *text[2];

        text[0] = (gchar *) tmp->data;
        text[1] = NULL;

        gnome_cmd_combo_append (GNOME_CMD_COMBO (fs->dir_combo), text, tmp->data);
    }

    if (fs->priv->dir_history->ents && fs->priv->dir_history->pos)
        gtk_clist_select_row (
            GTK_CLIST (GNOME_CMD_COMBO (fs->dir_combo)->list),
            g_list_index (fs->priv->dir_history->ents, fs->priv->dir_history->pos->data),
            0);*/
}


static void update_vol_label (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (GNOME_CMD_IS_CON (fs->get_connection()));

    gchar *s;

    if (gnome_cmd_con_can_show_free_space (fs->get_connection()))
    {
        GnomeVFSFileSize free_space;
        GnomeVFSURI *uri = gnome_cmd_file_get_uri (GNOME_CMD_FILE (fs->file_list()->cwd));
        GnomeVFSResult res = gnome_vfs_get_volume_free_space (uri, &free_space);
        gnome_vfs_uri_unref (uri);

        if (res == GNOME_VFS_OK)
        {
            gchar *sfree = gnome_vfs_format_file_size_for_display (free_space);
            s = g_strdup_printf (_("%s free"), sfree);
            g_free (sfree);
        }
        else
            s = g_strdup (_("Unknown disk usage"));
    }
    else
        s = g_strdup ("");

    gtk_label_set_text (GTK_LABEL (fs->vol_label), s);

    g_free (s);
}


void GnomeCmdFileSelector::goto_directory(const gchar *in_dir)
{
    g_return_if_fail (in_dir != NULL);

    GnomeCmdDir *cur_dir = get_directory();

    g_return_if_fail (GNOME_CMD_IS_DIR (cur_dir));

    GnomeCmdDir *new_dir = NULL;
    const gchar *focus_dir = NULL;
    gchar *dir;

    if (g_str_has_prefix (in_dir, "~"))
        dir = gnome_vfs_expand_initial_tilde (in_dir);
    else
        dir = unquote_if_needed (in_dir);

    if (strcmp (dir, "..") == 0)
    {
        // lets get the parent directory
        new_dir = gnome_cmd_dir_get_parent (cur_dir);
        if (!new_dir)
        {
            g_free (dir);
            return;
        }
        focus_dir = gnome_cmd_file_get_name (GNOME_CMD_FILE (cur_dir));
    }
    else
    {
        // check if it's an absolute address or not
        if (dir[0] == '/')
            new_dir = gnome_cmd_dir_new (get_connection(), gnome_cmd_con_create_path (get_connection(), dir));
        else
            if (g_str_has_prefix (dir, "\\\\") )
            {
                GnomeCmdPath *path = gnome_cmd_con_create_path (get_smb_con (), dir);
                if (path)
                    new_dir = gnome_cmd_dir_new (get_smb_con (), path);
            }
            else
                new_dir = gnome_cmd_dir_get_child (cur_dir, dir);
    }

    if (new_dir)
        set_directory(new_dir);

    // focus the current dir when going back to the parent dir
    if (focus_dir)
        file_list()->focus_file(focus_dir, FALSE);

    g_free (dir);
}


static void do_file_specific_action (GnomeCmdFileSelector *fs, GnomeCmdFile *f)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (f!=NULL);
    g_return_if_fail (f->info!=NULL);

    if (f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
        fs->file_list()->invalidate_tree_size();

        if (strcmp (f->info->name, "..") == 0)
            fs->goto_directory("..");
        else
            fs->set_directory(GNOME_CMD_DIR (f));
    }
}


inline void add_file_to_cmdline (GnomeCmdFileSelector *fs, gboolean fullpath)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdFile *f = fs->file_list()->get_selected_file();

    if (f && gnome_cmd_data.cmdline_visibility)
    {
        gchar *text = fullpath ? gnome_cmd_file_get_quoted_real_path (f) :
                                 gnome_cmd_file_get_quoted_name (f);

        gnome_cmd_cmdline_append_text (gnome_cmd_main_win_get_cmdline (main_win), text);
        gnome_cmd_cmdline_focus (gnome_cmd_main_win_get_cmdline (main_win));
        g_free (text);
    }
}


inline void add_cwd_to_cmdline (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (gnome_cmd_data.cmdline_visibility)
    {
        gchar *dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (fs->file_list()->cwd));
        gnome_cmd_cmdline_append_text (gnome_cmd_main_win_get_cmdline (main_win), dpath);
        g_free (dpath);

        gnome_cmd_cmdline_focus (gnome_cmd_main_win_get_cmdline (main_win));
    }
}



/*******************************
 * Callbacks
 *******************************/

static void on_con_list_list_changed (GnomeCmdConList *con_list, GnomeCmdFileSelector *fs)
{
    fs->update_connections();
}


static void on_dir_file_created (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()->insert_file(f))
        fs->update_selected_files_label();
}


static void on_dir_file_deleted (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()->cwd == dir)
        if (fs->file_list()->remove_file(f))
            fs->update_selected_files_label();
}


static void on_dir_file_changed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()->has_file(f))
    {
        fs->file_list()->update_file(f);
        fs->update_selected_files_label();
    }
}


static void on_dir_file_renamed (GnomeCmdDir *dir, GnomeCmdFile *f, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (f));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->file_list()->has_file(f))
    {
        // gnome_cmd_file_invalidate_metadata (f, TAG_FILE);    // FIXME: should be handled in GnomeCmdDir, not here
        fs->file_list()->update_file(f);

        GnomeCmdFileList::ColumnID sort_col = GNOME_CMD_FILE_LIST (fs->file_list())->get_sort_column();

        if (sort_col==GnomeCmdFileList::COLUMN_NAME || sort_col==GnomeCmdFileList::COLUMN_EXT)
            fs->file_list()->sort();
    }
}


static void on_con_combo_item_selected (GnomeCmdCombo *con_combo, GnomeCmdCon *con, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gnome_cmd_main_win_switch_fs (main_win, fs);
    fs->set_connection(con);
}


static void on_combo_popwin_hidden (GnomeCmdCombo *combo, GnomeCmdFileSelector *fs)
{
    gnome_cmd_main_win_refocus (main_win);
}


static void on_con_btn_clicked (GtkButton *button, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdCon *con = (GnomeCmdCon *) gtk_object_get_data (GTK_OBJECT (button), "con");

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gnome_cmd_main_win_switch_fs (main_win, fs);
    fs->set_connection(con);
}


static void create_con_buttons (GnomeCmdFileSelector *fs)
{
    static GtkTooltips *tooltips = NULL;

    if (!gnome_cmd_data.conbuttons_visibility)
        return;

    for (GList *l = fs->priv->old_btns; l; l=l->next)
        gtk_object_destroy (GTK_OBJECT (l->data));

    g_list_free (fs->priv->old_btns);
    fs->priv->old_btns = NULL;

    if (!tooltips)
        tooltips = gtk_tooltips_new ();

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l=l->next)
    {
        GtkWidget *btn, *label;
        GtkWidget *hbox;
        GnomeCmdPixmap *pm;
        GnomeCmdCon *con = GNOME_CMD_CON (l->data);

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con) &&
            !GNOME_CMD_IS_CON_SMB (con))  continue;

        pm = gnome_cmd_con_get_go_pixmap (con);

        btn = create_styled_button (NULL);
        gtk_object_set_data (GTK_OBJECT (btn), "con", con);
        gtk_signal_connect (GTK_OBJECT (btn), "clicked", (GtkSignalFunc) on_con_btn_clicked, fs);
        gtk_box_pack_start (GTK_BOX (fs->con_btns_hbox), btn, FALSE, FALSE, 0);
        GTK_WIDGET_UNSET_FLAGS (btn, GTK_CAN_FOCUS);
        fs->priv->old_btns = g_list_append (fs->priv->old_btns, btn);
        gtk_tooltips_set_tip (tooltips, btn, gnome_cmd_con_get_go_text (con), NULL);

        hbox = gtk_hbox_new (FALSE, 1);
        gtk_widget_ref (hbox);
        gtk_object_set_data_full (GTK_OBJECT (fs), "con-hbox", hbox, (GtkDestroyNotify) gtk_widget_unref);
        gtk_widget_show (hbox);

        if (pm)
        {
            GtkWidget *pixmap = gtk_pixmap_new (pm->pixmap, pm->mask);
            if (pixmap)
            {
                gtk_widget_ref (pixmap);
                gtk_object_set_data_full (GTK_OBJECT (fs), "con-pixmap", pixmap,
                                          (GtkDestroyNotify) gtk_widget_unref);
                gtk_widget_show (pixmap);
                gtk_box_pack_start (GTK_BOX (hbox), pixmap, TRUE, TRUE, 0);
            }
        }

        if (!gnome_cmd_data.device_only_icon || !pm)
        {
            label = gtk_label_new (gnome_cmd_con_get_alias (con));
            gtk_widget_ref (label);
            gtk_object_set_data_full (GTK_OBJECT (fs), "con-label", label,
                                      (GtkDestroyNotify) gtk_widget_unref);
            gtk_widget_show (label);
            gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
        }

        gtk_container_add (GTK_CONTAINER (btn), hbox);
    }
}


static void on_realize (GnomeCmdFileSelector *fs, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    fs->priv->realized = TRUE;

    create_con_buttons (fs);
    fs->update_connections();
}


static void on_list_file_clicked (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1 && gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_DOUBLE_CLICK)
        do_file_specific_action (fs, f);
}


static void on_list_file_released (GnomeCmdFileList *fl, GnomeCmdFile *f, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_BUTTON_RELEASE && event->button == 1 && !fl->modifier_click && gnome_cmd_data.left_mouse_button_mode == GnomeCmdData::LEFT_BUTTON_OPENS_WITH_SINGLE_CLICK)
        do_file_specific_action (fs, f);
}


static void on_list_list_clicked (GnomeCmdFileList *fl, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_BUTTON_PRESS)
    {
        if (event->button == 1 || event->button == 3)
            gnome_cmd_main_win_switch_fs (main_win, fs);
        else
            if (event->button == 2)
                fs->goto_directory("..");
    }
}


static void on_list_empty_space_clicked (GnomeCmdFileList *fl, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    if (event->type == GDK_BUTTON_PRESS)
        if (event->button == 3)
            show_list_popup (fs);
}


static void on_list_files_changed (GnomeCmdFileList *fl, GnomeCmdFileSelector *fs)
{
    fs->update_selected_files_label();
}


static void on_list_dir_changed (GnomeCmdFileList *fl, GnomeCmdDir *dir, GnomeCmdFileSelector *fs)
{
}


static void on_dir_list_ok (GnomeCmdDir *dir, GList *files, GnomeCmdFileSelector *fs)
{
    DEBUG('l', "on_dir_list_ok\n");

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    if (fs->priv->realized)
    {
        gtk_widget_set_sensitive (*fs, TRUE);
        set_cursor_default_for_widget (*fs);
        gtk_widget_grab_focus (GTK_WIDGET (fs->file_list()));
    }

    if (fs->file_list()->connected_dir != dir)
    {
        if (fs->file_list()->connected_dir != NULL)
        {
            gtk_signal_disconnect_by_func (GTK_OBJECT (fs->file_list()->connected_dir), GTK_SIGNAL_FUNC (on_dir_file_created), fs);
            gtk_signal_disconnect_by_func (GTK_OBJECT (fs->file_list()->connected_dir), GTK_SIGNAL_FUNC (on_dir_file_deleted), fs);
            gtk_signal_disconnect_by_func (GTK_OBJECT (fs->file_list()->connected_dir), GTK_SIGNAL_FUNC (on_dir_file_changed), fs);
            gtk_signal_disconnect_by_func (GTK_OBJECT (fs->file_list()->connected_dir), GTK_SIGNAL_FUNC (on_dir_file_renamed), fs);
        }

        gtk_signal_connect (GTK_OBJECT (dir), "file-created", GTK_SIGNAL_FUNC (on_dir_file_created), fs);
        gtk_signal_connect (GTK_OBJECT (dir), "file-deleted", GTK_SIGNAL_FUNC (on_dir_file_deleted), fs);
        gtk_signal_connect (GTK_OBJECT (dir), "file-changed", GTK_SIGNAL_FUNC (on_dir_file_changed), fs);
        gtk_signal_connect (GTK_OBJECT (dir), "file-renamed", GTK_SIGNAL_FUNC (on_dir_file_renamed), fs);
        fs->file_list()->connected_dir = dir;
    }

    gnome_cmd_con_set_cwd (fs->get_connection(), dir);

    if (fs->priv->dir_history && !fs->priv->dir_history->is_locked)
    {
        gchar *fpath = gnome_cmd_file_get_path (GNOME_CMD_FILE (dir));
        fs->priv->dir_history->add(fpath);
        g_free (fpath);
        update_dir_combo (fs);
    }

    gtk_signal_emit (GTK_OBJECT (fs), file_selector_signals[CHANGED_DIR], dir);

    fs->update_direntry();
    update_vol_label (fs);

    if (fs->file_list()->cwd != dir) return;

    fs->priv->sel_first_file = FALSE;
    fs->update_files();
    fs->priv->sel_first_file = TRUE;

    if (!fs->priv->active)
    {
        GTK_CLIST (fs->file_list())->focus_row = -1;
        gtk_clist_unselect_all (GTK_CLIST (fs->file_list()));
    }

    if (fs->priv->sel_first_file && fs->priv->active)
        gtk_clist_select_row (GTK_CLIST (fs->file_list()), 0, 0);

    fs->update_selected_files_label();

    DEBUG('l', "returning from on_dir_list_ok\n");
}


static gboolean set_home_connection (GnomeCmdFileSelector *fs)
{
    g_printerr ("Setting home connection\n");
    fs->set_connection(get_home_con ());

    return FALSE;
}


static void on_dir_list_failed (GnomeCmdDir *dir, GnomeVFSResult result, GnomeCmdFileSelector *fs)
{
    DEBUG('l', "on_dir_list_failed\n");

    if (result != GNOME_VFS_OK)
    {
        gchar *msg = g_strdup_printf (_("Listing failed: %s\n"), gnome_vfs_result_to_string (result));
        create_error_dialog (msg);
        g_free (msg);
    }

    gtk_signal_disconnect_by_data (GTK_OBJECT (fs->file_list()->cwd), fs);
    fs->file_list()->connected_dir = NULL;
    gnome_cmd_dir_unref (fs->file_list()->cwd);
    set_cursor_default_for_widget (GTK_WIDGET (fs));
    gtk_widget_set_sensitive (*fs, TRUE);

    if (fs->file_list()->lwd && fs->get_connection() == gnome_cmd_dir_get_connection (fs->file_list()->lwd))
    {
        fs->file_list()->cwd = fs->file_list()->lwd;
        gtk_signal_connect (GTK_OBJECT (fs->file_list()->cwd), "list-ok", GTK_SIGNAL_FUNC (on_dir_list_ok), fs);
        gtk_signal_connect (GTK_OBJECT (fs->file_list()->cwd), "list-failed", GTK_SIGNAL_FUNC (on_dir_list_failed), fs);
        fs->file_list()->lwd = NULL;
    }
    else
        g_timeout_add (1, (GtkFunction) set_home_connection, fs);
}


// This function should only be called for input made when the file-selector was focused
static gboolean on_list_key_pressed (GtkCList *clist, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    gboolean ret = FALSE;

    if (fs->file_list()->key_pressed(event))
        ret = TRUE;
    else if (fs->key_pressed(event))
        ret = TRUE;
    else if (gnome_cmd_main_win_keypressed (main_win, event))
        ret = TRUE;
    else if (gcmd_user_actions.handle_key_event(main_win, fs->file_list(), event))
        ret = TRUE;

    if (ret)
        stop_kp (GTK_OBJECT (clist));

    return ret;
}


static gboolean on_list_key_pressed_private (GtkCList *clist, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (state_is_blank (event->state) || state_is_shift (event->state))
    {
        if ((event->keyval >= GDK_A && event->keyval <= GDK_Z) ||
            (event->keyval >= GDK_a && event->keyval <= GDK_z) ||
            event->keyval == GDK_period)
        {
            static gchar text[2];

            if (!gnome_cmd_data.cmdline_visibility)
                gnome_cmd_file_list_show_quicksearch (fs->file_list(), (gchar) event->keyval);
            else
            {
                text[0] = event->keyval;
                text[1] = '\0';
                gnome_cmd_cmdline_append_text (gnome_cmd_main_win_get_cmdline (main_win), text);
                gnome_cmd_cmdline_focus (gnome_cmd_main_win_get_cmdline (main_win));
            }
            return TRUE;
        }
    }

    return FALSE;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (object);

    delete fs->priv;

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdFileSelectorClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);;
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkVBoxClass *) gtk_type_class (gtk_vbox_get_type ());

    file_selector_signals[CHANGED_DIR] =
        gtk_signal_new ("changed-dir",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdFileSelectorClass, changed_dir),
            gtk_marshal_NONE__POINTER,
            GTK_TYPE_NONE,
            1, GTK_TYPE_POINTER);

    object_class->destroy = destroy;
    widget_class->map = ::map;
    klass->changed_dir = NULL;
}


static void init (GnomeCmdFileSelector *fs)
{
    GtkWidget *padding;

    fs->priv = new GnomeCmdFileSelector::Private;

    GtkVBox *vbox = GTK_VBOX (fs);

    // create the box used for packing the dir_combo and buttons
    fs->update_conbuttons_visibility();

    // create the box used for packing the con_combo and information
    fs->con_hbox = create_hbox (*fs, FALSE, 2);

    // create the list
    fs->file_list() = new GnomeCmdFileList;             // FIXME: file_list() = ...
    fs->list_widget = GTK_WIDGET (fs->file_list());
    gtk_widget_ref (fs->list_widget);
    gtk_object_set_data_full (GTK_OBJECT (fs), "list_widget", fs->list_widget, (GtkDestroyNotify) gtk_widget_unref);
    fs->file_list()->show_column(GnomeCmdFileList::COLUMN_DIR, FALSE);

    // create the connection combo
    fs->con_combo = gnome_cmd_combo_new (2, 1, NULL);
    gtk_widget_ref (fs->con_combo);
    gtk_object_set_data_full (GTK_OBJECT (fs), "con_combo", fs->con_combo, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_set_size_request (fs->con_combo, 150, -1);
    gtk_clist_set_row_height (GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 20);
    gtk_entry_set_editable (GTK_ENTRY (GNOME_CMD_COMBO (fs->con_combo)->entry), FALSE);
    gtk_clist_set_column_width (GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 0, 20);
    gtk_clist_set_column_width (GTK_CLIST (GNOME_CMD_COMBO (fs->con_combo)->list), 1, 60);
    GTK_WIDGET_UNSET_FLAGS (GNOME_CMD_COMBO (fs->con_combo)->button, GTK_CAN_FOCUS);

    // create the free space on volume label
    fs->vol_label = gtk_label_new ("");
    gtk_widget_ref (fs->vol_label);
    gtk_object_set_data_full (GTK_OBJECT (fs), "vol_label", fs->vol_label, (GtkDestroyNotify) gtk_widget_unref);
    gtk_misc_set_alignment (GTK_MISC (fs->vol_label), 1, 0.5);

    // create the directory indicator
    fs->dir_indicator = gnome_cmd_dir_indicator_new (fs);
    gtk_widget_ref (fs->dir_indicator);
    gtk_object_set_data_full (GTK_OBJECT (fs),
                              "dir_indicator", fs->dir_indicator,
                              (GtkDestroyNotify) gtk_widget_unref);

    // create the scrollwindow that we'll place the list in
    fs->scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (fs->scrolledwindow);
    gtk_object_set_data_full (GTK_OBJECT (fs),
                              "scrolledwindow", fs->scrolledwindow,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (fs->scrolledwindow),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    // create the info label
    fs->info_label = gtk_label_new ("not initialized");
    gtk_widget_ref (fs->info_label);
    gtk_object_set_data_full (GTK_OBJECT (fs), "infolabel", fs->info_label,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_misc_set_alignment (GTK_MISC (fs->info_label), 0.0f, 0.5f);

    // pack the widgets
    gtk_box_pack_start (GTK_BOX (fs), fs->con_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), fs->dir_indicator, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (fs->scrolledwindow), fs->list_widget);
    gtk_box_pack_start (GTK_BOX (vbox), fs->scrolledwindow, TRUE, TRUE, 0);
    padding = create_hbox (*fs, FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), padding, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (padding), fs->info_label, FALSE, TRUE, 6);
    gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->con_combo, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (fs->con_hbox), fs->vol_label, TRUE, TRUE, 6);

    // initialize dnd
    init_dnd (fs);

    // connect signals
    gtk_signal_connect (GTK_OBJECT (fs), "realize", GTK_SIGNAL_FUNC (on_realize), fs);

    gtk_signal_connect (GTK_OBJECT (fs->con_combo), "item-selected", GTK_SIGNAL_FUNC (on_con_combo_item_selected), fs);
    gtk_signal_connect (GTK_OBJECT (fs->con_combo), "popwin-hidden", GTK_SIGNAL_FUNC (on_combo_popwin_hidden), fs);

    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "file-clicked", GTK_SIGNAL_FUNC (on_list_file_clicked), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "file-released", GTK_SIGNAL_FUNC (on_list_file_released), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "list-clicked", GTK_SIGNAL_FUNC (on_list_list_clicked), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "empty-space-clicked", GTK_SIGNAL_FUNC (on_list_empty_space_clicked), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "files-changed", GTK_SIGNAL_FUNC (on_list_files_changed), fs);

    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "key-press-event", GTK_SIGNAL_FUNC (on_list_key_pressed), fs);
    gtk_signal_connect (GTK_OBJECT (fs->file_list()), "key-press-event", GTK_SIGNAL_FUNC (on_list_key_pressed_private), fs);

    gtk_signal_connect (GTK_OBJECT (gnome_cmd_con_list_get ()), "list-changed", GTK_SIGNAL_FUNC (on_con_list_list_changed), fs);

    // show the widgets
    gtk_widget_show (GTK_WIDGET (vbox));
    fs->update_concombo_visibility();
    gtk_widget_show (fs->con_combo);
    gtk_widget_show (fs->vol_label);
    gtk_widget_show (fs->dir_indicator);
    gtk_widget_show (fs->scrolledwindow);
    gtk_widget_show (fs->list_widget);
    gtk_widget_show (fs->info_label);

    fs->update_style();
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_file_selector_get_type ()
{
    static GtkType fs_type = 0;

    if (fs_type == 0)
    {
        GtkTypeInfo fs_info =
        {
            "GnomeCmdFileSelector",
            sizeof (GnomeCmdFileSelector),
            sizeof (GnomeCmdFileSelectorClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        fs_type = gtk_type_unique (gtk_vbox_get_type (), &fs_info);
    }
    return fs_type;
}


GtkWidget *gnome_cmd_file_selector_new ()
{
    GnomeCmdFileSelector *fs = (GnomeCmdFileSelector *) gtk_type_new (gnome_cmd_file_selector_get_type ());

    return *fs;
}


void GnomeCmdFileSelector::reload()
{
    file_list()->unselect_all();

    GnomeCmdDir *dir = get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gnome_cmd_dir_relist_files (dir, gnome_cmd_con_needs_list_visprog (get_connection()));
}


void gnome_cmd_file_selector_start_editor (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (GNOME_CMD_IS_DIR (fs->file_list()->cwd));

    if (!fs->is_local())
        return;

    // create a command with an empty argument to the editor
    gchar *cmd = g_strdup_printf (gnome_cmd_data.get_editor(), "");
    gchar *dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (fs->file_list()->cwd));

    run_command_indir (cmd, dpath, FALSE);

    g_free (dpath);
    g_free (cmd);
}


void GnomeCmdFileSelector::first()
{
    if (!priv->dir_history->can_back())
        return;

    priv->dir_history->lock();
    goto_directory(priv->dir_history->first());
    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::back()
{
    if (!priv->dir_history->can_back())
        return;

    priv->dir_history->lock();
    goto_directory(priv->dir_history->back());
    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::forward()
{
    if (!priv->dir_history->can_forward())
        return;

    priv->dir_history->lock();
    goto_directory(priv->dir_history->forward());
    priv->dir_history->unlock();
}


void GnomeCmdFileSelector::last()
{
    if (!priv->dir_history->can_forward())
        return;

    priv->dir_history->lock();
    goto_directory(priv->dir_history->last());
    priv->dir_history->unlock();
}


gboolean GnomeCmdFileSelector::can_back()
{
    return priv->dir_history && priv->dir_history->can_back();
}


gboolean GnomeCmdFileSelector::can_forward()
{
    return priv->dir_history && priv->dir_history->can_forward();
}


void GnomeCmdFileSelector::set_directory(GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    if (file_list()->cwd == dir)
        return;

    gnome_cmd_dir_ref (dir);

    if (file_list()->lwd && file_list()->lwd != dir)
        gnome_cmd_dir_unref (file_list()->lwd);

    if (file_list()->cwd)
    {
        gnome_cmd_dir_cancel_monitoring (file_list()->cwd);
        file_list()->lwd = file_list()->cwd;
        gtk_signal_disconnect_by_data (GTK_OBJECT (file_list()->lwd), this);
        file_list()->connected_dir = NULL;
        file_list()->lwd->voffset = gnome_cmd_clist_get_voffset (GNOME_CMD_CLIST (file_list()));
    }

    file_list()->cwd = dir;

    if (priv->realized)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (this), FALSE);
        set_cursor_busy_for_widget (GTK_WIDGET (this));
    }

    gtk_signal_connect (GTK_OBJECT (dir), "list-ok", GTK_SIGNAL_FUNC (on_dir_list_ok), this);
    gtk_signal_connect (GTK_OBJECT (dir), "list-failed", GTK_SIGNAL_FUNC (on_dir_list_failed), this);

    gnome_cmd_dir_list_files (dir, gnome_cmd_con_needs_list_visprog (get_connection()));
    gnome_cmd_dir_start_monitoring (dir);
}


void GnomeCmdFileSelector::set_active(gboolean value)
{
    priv->active = value;

    if (value)
    {
        gtk_widget_grab_focus (list_widget);
        list->select_row(GTK_CLIST (list)->focus_row);
    }
    else
        gtk_clist_unselect_all (GTK_CLIST (list));

    gnome_cmd_dir_indicator_set_active (GNOME_CMD_DIR_INDICATOR (dir_indicator), value);
}


static void on_con_open_cancel (GtkButton *button, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (fs->priv->con_opening != NULL);
    g_return_if_fail (fs->priv->con_opening->state == CON_STATE_OPENING);

    DEBUG('m', "on_con_open_cancel\n");
    gnome_cmd_con_cancel_open (fs->priv->con_opening);

    gtk_widget_destroy (fs->priv->con_open_dialog);
    fs->priv->con_open_dialog = NULL;
    fs->priv->con_opening = NULL;
}


static gboolean update_con_open_progress (GnomeCmdFileSelector *fs)
{
    if (!fs->priv->con_open_dialog)
        return FALSE;

    const gchar *msg = gnome_cmd_con_get_open_msg (fs->priv->con_opening);
    gtk_label_set_text (GTK_LABEL (fs->priv->con_open_dialog_label), msg);
    progress_bar_update (fs->priv->con_open_dialog_pbar, FS_PBAR_MAX);

    return TRUE;
}


static void on_con_open_done (GnomeCmdCon *con, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (fs->priv->con_opening != NULL);
    g_return_if_fail (fs->priv->con_opening == con);
    g_return_if_fail (fs->priv->con_open_dialog != NULL);

    DEBUG('m', "on_con_open_done\n");
    gtk_signal_disconnect_by_data (GTK_OBJECT (con), fs);

    set_connection (fs, con);

    gtk_widget_destroy (fs->priv->con_open_dialog);
    fs->priv->con_open_dialog = NULL;
    fs->priv->con_opening = NULL;
}


static void on_con_open_failed (GnomeCmdCon *con, const gchar *emsg, GnomeVFSResult result, GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (fs->priv->con_opening != NULL);
    g_return_if_fail (fs->priv->con_opening == con);
    g_return_if_fail (fs->priv->con_open_dialog != NULL);

    gchar *s;

    DEBUG('m', "on_con_open_failed\n");
    gtk_signal_disconnect_by_data (GTK_OBJECT (con), fs);

    if (emsg)
        s = g_strdup (emsg);
    else
        s = g_strdup_printf (_("Failed to open connection: %s\n"), gnome_vfs_result_to_string (result));

    if (result != GNOME_VFS_OK || emsg != NULL)
        create_error_dialog (s);

    g_free (s);

    gtk_widget_destroy (fs->priv->con_open_dialog);
    fs->priv->con_open_dialog = NULL;
    fs->priv->con_opening = NULL;
}


static void create_con_open_progress_dialog (GnomeCmdFileSelector *fs)
{
    GtkWidget *vbox;

    fs->priv->con_open_dialog = gnome_cmd_dialog_new (NULL);
    gtk_widget_ref (fs->priv->con_open_dialog);

    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (fs->priv->con_open_dialog),
        GTK_STOCK_CANCEL,
        GTK_SIGNAL_FUNC (on_con_open_cancel), fs);

    vbox = create_vbox (fs->priv->con_open_dialog, FALSE, 0);

    fs->priv->con_open_dialog_label = create_label (fs->priv->con_open_dialog, "");

    fs->priv->con_open_dialog_pbar = create_progress_bar (fs->priv->con_open_dialog);
    gtk_progress_set_show_text (GTK_PROGRESS (fs->priv->con_open_dialog_pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (fs->priv->con_open_dialog_pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (fs->priv->con_open_dialog_pbar), 0, 0, FS_PBAR_MAX);

    gtk_box_pack_start (GTK_BOX (vbox), fs->priv->con_open_dialog_label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), fs->priv->con_open_dialog_pbar, FALSE, TRUE, 0);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (fs->priv->con_open_dialog), vbox);

    gnome_cmd_dialog_set_transient_for (GNOME_CMD_DIALOG (fs->priv->con_open_dialog), GTK_WINDOW (main_win));

    gtk_widget_show_all (fs->priv->con_open_dialog);
}


void gnome_cmd_file_selector_set_directory_to_opposite (GnomeCmdMainWin *mw, FileSelectorID fsID)
{
    g_return_if_fail (mw!=NULL);

    GnomeCmdFileSelector *fs = gnome_cmd_main_win_get_fs (mw, fsID);
    GnomeCmdFileSelector *other = gnome_cmd_main_win_get_fs (mw, !fsID);

    GnomeCmdDir *dir = other->get_directory();
    gboolean fs_is_active = fs->priv->active;

    if (!fs_is_active)
    {
        GnomeCmdFile *file = other->file_list()->get_selected_file();

        if (file && file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
            dir = gnome_cmd_dir_new_from_info (file->info, dir);
    }

    set_connection (fs, other->get_connection(), dir);

    other->set_active(!fs_is_active);
    fs->set_active(fs_is_active);
}


void GnomeCmdFileSelector::update_connections()
{
    if (!priv->realized)
        return;

    gboolean found_my_con = FALSE;

    gnome_cmd_combo_clear (GNOME_CMD_COMBO (con_combo));
    GNOME_CMD_COMBO (con_combo)->highest_pixmap = 20;
    GNOME_CMD_COMBO (con_combo)->widest_pixmap = 20;
    gtk_clist_set_row_height (GTK_CLIST (GNOME_CMD_COMBO (con_combo)->list), 20);
    gtk_clist_set_column_width (GTK_CLIST (GNOME_CMD_COMBO (con_combo)->list), 0, 20);

    for (GList *l=gnome_cmd_con_list_get_all (gnome_cmd_con_list_get ()); l; l = l->next)
    {
        gchar *text[3];
        GnomeCmdCon *con = (GnomeCmdCon *) l->data;

        if (!gnome_cmd_con_is_open (con) && !GNOME_CMD_IS_CON_DEVICE (con)
            && !GNOME_CMD_IS_CON_SMB (con))  continue;

        if (con == get_connection())
            found_my_con = TRUE;

        text[0] = NULL;
        text[1] = (gchar *) gnome_cmd_con_get_alias (con);
        text[2] = NULL;

        GnomeCmdPixmap *pixmap = gnome_cmd_con_get_go_pixmap (con);

        if (pixmap)
        {
            gint row = gnome_cmd_combo_append (GNOME_CMD_COMBO (con_combo), text, con);

            gnome_cmd_combo_set_pixmap (GNOME_CMD_COMBO (con_combo), row, 0, pixmap);
        }
    }

    // If the connection is no longer available use the home connection
    if (!found_my_con)
        set_connection(get_home_con ());
    else
        gnome_cmd_combo_select_data (GNOME_CMD_COMBO (con_combo), get_connection());

    create_con_buttons (this);
}


void GnomeCmdFileSelector::set_connection (GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (get_connection() == con)
    {
        if (!gnome_cmd_con_should_remember_dir (con))
            set_directory (gnome_cmd_con_get_default_dir (con));
        return;
    }

    if (!gnome_cmd_con_is_open (con))
    {
        gtk_signal_connect (GTK_OBJECT (con), "open-done", GTK_SIGNAL_FUNC (on_con_open_done), this);
        gtk_signal_connect (GTK_OBJECT (con), "open-failed", GTK_SIGNAL_FUNC (on_con_open_failed), this);
        priv->con_opening = con;

        create_con_open_progress_dialog (this);
        g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_con_open_progress, this);

        gnome_cmd_con_open (con);
    }
    else
        ::set_connection (this, con, start_dir);
}


gboolean gnome_cmd_file_selector_is_local (FileSelectorID fsID)
{
    return gnome_cmd_main_win_get_fs (main_win, fsID)->is_local();
}


void GnomeCmdFileSelector::update_style()
{
    gnome_cmd_combo_update_style (GNOME_CMD_COMBO (con_combo));
    file_list()->update_style();

    if (priv->realized)
        update_files();

    create_con_buttons (this);
    update_connections();
}


static gboolean on_new_textfile_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdFileSelector *fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), TRUE);

    const gchar *filename = values[0];
    gchar *escaped_filepath;
    gchar *cmd;

    if (!filename)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("No file name entered")));
        return FALSE;
    }

    GnomeCmdDir *dir = fs->get_directory();
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), TRUE);

    gchar *dpath = gnome_cmd_file_get_real_path (GNOME_CMD_FILE (dir));
    gchar *filepath = g_build_path (G_DIR_SEPARATOR_S, dpath, filename, NULL);
    g_free (dpath);
    g_return_val_if_fail (filepath, TRUE);

    escaped_filepath = g_strdup_printf ("\"%s\"", filepath);
    cmd = g_strdup_printf (gnome_cmd_data.get_editor(), escaped_filepath);
    g_free (filepath);
    g_free (escaped_filepath);

    if (cmd)
        run_command (cmd, FALSE);

    return TRUE;
}


static gboolean on_create_symlink_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdFileSelector *fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), TRUE);
    g_return_val_if_fail (fs->priv->sym_file != NULL, TRUE);

    const gchar *name = values[0];

    // dont create any symlink if no name was passed or cancel was selected
    if (name == NULL || strcmp (name, "") == 0)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("No file name given")));
        return FALSE;
    }

    GnomeVFSURI *uri = gnome_cmd_dir_get_child_uri (fs->file_list()->cwd, name);
    GnomeVFSResult result = gnome_vfs_create_symbolic_link (uri, gnome_cmd_file_get_uri_str (fs->priv->sym_file));

    if (result == GNOME_VFS_OK)
    {
        gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
        gnome_cmd_dir_file_created (fs->file_list()->cwd, uri_str);
        g_free (uri_str);
        gnome_vfs_uri_unref (uri);
        return TRUE;
    }

    gnome_vfs_uri_unref (uri);

    gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (gnome_vfs_result_to_string (result)));

    return FALSE;
}


void gnome_cmd_file_selector_show_new_textfile_dialog (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    const gchar *labels[] = {_("File name:")};
    GtkWidget *dialog;

    dialog = gnome_cmd_string_dialog_new (_("New Text File"), labels, 1,
                                          (GnomeCmdStringDialogCallback) on_new_textfile_ok, fs);
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));

    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


void gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    GnomeCmdDir *dir = fs->get_directory();
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    cap_paste_files (dir);
}


gboolean GnomeCmdFileSelector::key_pressed(GdkEventKey *event)
{
    g_return_val_if_fail (event != NULL, FALSE);

    GnomeCmdFile *f;

    if (state_is_ctrl_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
            case GDK_KP_Enter:
                add_file_to_cmdline (this, TRUE);
                return TRUE;
        }
    }
    else if (state_is_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_F5:
                gnome_cmd_file_list_show_make_copy_dialog (this);
                return TRUE;
        }
    }
    else if (state_is_alt (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Down:
                gnome_cmd_dir_indicator_show_history (GNOME_CMD_DIR_INDICATOR (dir_indicator));
                return TRUE;

            case GDK_Left:
                back();
                stop_kp (GTK_OBJECT (file_list()));
                return TRUE;

            case GDK_Right:
                forward();
                stop_kp (GTK_OBJECT (file_list()));
                return TRUE;
        }
    }
    else if (state_is_alt_shift (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Return:
            case GDK_KP_Enter:
                show_dir_tree_sizes (this);
                return TRUE;
        }
    }
    else if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_V:
            case GDK_v:
                gnome_cmd_file_selector_cap_paste (this);
                return TRUE;

            case GDK_P:
            case GDK_p:
                add_cwd_to_cmdline (this);
                return TRUE;

            case GDK_Page_Up:
                goto_directory("..");
                return TRUE;

            case GDK_Page_Down:
                f = file_list()->get_selected_file();
                if (f && f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
                    do_file_specific_action (this, f);
                return TRUE;

            case GDK_Return:
            case GDK_KP_Enter:
                add_file_to_cmdline (this, FALSE);
                return TRUE;
        }
    }
    else if (state_is_blank (event->state))
    {
        switch (event->keyval)
        {
            case GDK_space:
                set_cursor_busy ();
                file_list()->toggle();
                f = file_list()->get_selected_file();
                file_list()->show_dir_tree_size(f);
                stop_kp (GTK_OBJECT (file_list()));
                update_selected_files_label();
                set_cursor_default ();
                return TRUE;

            case GDK_Left:
            case GDK_KP_Left:
            case GDK_BackSpace:
                goto_directory("..");
                return TRUE;

            case GDK_Right:
            case GDK_KP_Right:
                f = file_list()->get_selected_file();
                if (f && f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
                    do_file_specific_action (this, f);
                stop_kp (GTK_OBJECT (file_list()));
                return TRUE;

            case GDK_Return:
            case GDK_KP_Enter:
                if (gnome_cmd_data.cmdline_visibility
                    && gnome_cmd_cmdline_is_empty (gnome_cmd_main_win_get_cmdline (main_win)))
                    gnome_cmd_cmdline_exec (gnome_cmd_main_win_get_cmdline (main_win));
                else
                    do_file_specific_action (this, file_list()->get_focused_file());
                return TRUE;

            case GDK_Escape:
                if (gnome_cmd_data.cmdline_visibility)
                    gnome_cmd_cmdline_set_text (gnome_cmd_main_win_get_cmdline (main_win), "");
                return TRUE;
        }
    }

    return FALSE;
}


void gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *f)
{
    const gchar *labels[] = {_("Symlink name:")};

    gchar *fname = get_utf8 (gnome_cmd_file_get_name (f));
    gchar *text = g_strdup_printf (gnome_cmd_data_get_symlink_prefix (), fname);
    g_free (fname);

    GtkWidget *dialog = gnome_cmd_string_dialog_new (_("Create Symbolic Link"),
                                                     labels,
                                                     1,
                                                     (GnomeCmdStringDialogCallback) on_create_symlink_ok,
                                                     fs);

    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, text);
    g_free (text);
    fs->priv->sym_file = f;
    gtk_widget_show (dialog);
}


void gnome_cmd_file_selector_create_symlinks (GnomeCmdFileSelector *fs, GList *files)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));

    gint choice = -1;

    for (; files; files=files->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) files->data;
        gchar *fname = get_utf8 (gnome_cmd_file_get_name (f));
        gchar *symlink_name = g_strdup_printf (gnome_cmd_data_get_symlink_prefix (), fname);

        GnomeVFSURI *uri = gnome_cmd_dir_get_child_uri (fs->file_list()->cwd, symlink_name);

        g_free (fname);
        g_free (symlink_name);

        GnomeVFSResult result;

        do
        {
            result = gnome_vfs_create_symbolic_link (uri, gnome_cmd_file_get_uri_str (f));

            if (result == GNOME_VFS_OK)
            {
                gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
                gnome_cmd_dir_file_created (fs->file_list()->cwd, uri_str);
                g_free (uri_str);
            }
            else
                if (choice != 1)  // choice != SKIP_ALL
                {
                    gchar *msg = g_strdup (gnome_vfs_result_to_string (result));
                    choice = run_simple_dialog (GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_QUESTION, msg, _("Create Symbolic Link"), 3, _("Skip"), _("Skip all"), _("Cancel"), _("Retry"), NULL);
                    g_free (msg);
                }
        }
        while (result != GNOME_VFS_OK && choice == 3);  // choice != RETRY

        gnome_vfs_uri_unref (uri);
    }
}


void GnomeCmdFileSelector::update_conbuttons_visibility()
{
    if (!gnome_cmd_data.conbuttons_visibility)
    {
        if (con_btns_hbox)
        {
            gtk_object_destroy (GTK_OBJECT (con_btns_hbox));
            con_btns_hbox = NULL;
        }
    }
    else
    {
        if (!con_btns_hbox)
        {
            con_btns_hbox = create_hbox (*this, FALSE, 2);
            gtk_box_pack_start (GTK_BOX (this), con_btns_hbox, FALSE, FALSE, 0);
            gtk_box_reorder_child (GTK_BOX (this), con_btns_hbox, 0);
            gtk_widget_show (con_btns_hbox);
            create_con_buttons (this);
        }
    }
}


void GnomeCmdFileSelector::update_concombo_visibility()
{
    if (gnome_cmd_data.concombo_visibility)
        gtk_widget_show (con_hbox);
    else
        gtk_widget_hide (con_hbox);
}


static void on_filter_box_close (GtkButton *btn, GnomeCmdFileSelector *fs)
{
    if (!fs->priv->filter_box) return;

    gtk_widget_destroy (fs->priv->filter_box);
    fs->priv->filter_box = NULL;
}


gboolean on_filter_box_keypressed (GtkEntry *entry, GdkEventKey *event, GnomeCmdFileSelector *fs)
{
    if (state_is_blank (event->state))
        if (event->keyval == GDK_Escape)
        {
            on_filter_box_close (NULL, fs);
            return TRUE;
        }

    return FALSE;
}


void GnomeCmdFileSelector::show_filter()
{
    if (priv->filter_box) return;

    priv->filter_box = create_hbox (*this, FALSE, 0);
    GtkWidget *label = create_label (*this, _("Filter:"));
    GtkWidget *entry = create_entry (*this, "entry", "");
    GtkWidget *close_btn = create_button_with_data (GTK_WIDGET (main_win), "x", GTK_SIGNAL_FUNC (on_filter_box_close), this);

    gtk_signal_connect (GTK_OBJECT (entry), "key-press-event", GTK_SIGNAL_FUNC (on_filter_box_keypressed), this);
    gtk_box_pack_start (GTK_BOX (priv->filter_box), label, FALSE, TRUE, 6);
    gtk_box_pack_start (GTK_BOX (priv->filter_box), entry, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (priv->filter_box), close_btn, FALSE, TRUE, 0);

    gtk_box_pack_start (*this, priv->filter_box, FALSE, TRUE, 0);

    gtk_widget_grab_focus (entry);
}


gboolean GnomeCmdFileSelector::is_active()
{
    return priv->active;
}
