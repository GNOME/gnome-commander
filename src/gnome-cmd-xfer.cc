/** 
 * @file gnome-cmd-xfer.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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
#include <unistd.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-xfer-progress-win.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "utils.h"

using namespace std;


#define XFER_PRIORITY GNOME_VFS_PRIORITY_DEFAULT


struct XferData
{
    GnomeVFSXferOptions xferOptions;
    GnomeVFSAsyncHandle *handle;

    // Source and target uri's. The first src_uri should be transfered to the first dest_uri and so on...
    GList *src_uri_list;
    GList *dest_uri_list;

    GnomeCmdDir *to_dir;
    GnomeCmdFileList *src_fl;
    GList *src_files;

    // Used for showing the progress
    GnomeCmdXferProgressWin *win;
    GnomeVFSXferPhase cur_phase;
    GnomeVFSXferPhase prev_phase;
    GnomeVFSXferProgressStatus prev_status;
    gulong cur_file;
    gulong prev_file;
    gulong files_total;
    gfloat prev_totalprog;
    gboolean first_time;
    gchar *cur_file_name;

    GnomeVFSFileSize file_size;
    GnomeVFSFileSize bytes_copied;
    GnomeVFSFileSize bytes_total;
    GnomeVFSFileSize total_bytes_copied;

    GFunc on_completed_func;
    gpointer on_completed_data;

    gboolean done;
    gboolean aborted;

};


inline void free_xfer_data (XferData *data)
{
    if (data->on_completed_func)
        data->on_completed_func (data->on_completed_data, NULL);

    g_list_free (data->src_uri_list);

    //  free the list with target uris
    for (GList *i = data->dest_uri_list; i; i = i->next)
    {
        GnomeVFSURI *uri = (GnomeVFSURI *) i->data;
        gnome_vfs_uri_unref (uri);
    }

    g_list_free (data->dest_uri_list);
    g_free (data);
}


static XferData *
create_xfer_data (GnomeVFSXferOptions xferOptions, GList *src_uri_list, GList *dest_uri_list,
                  GnomeCmdDir *to_dir, GnomeCmdFileList *src_fl, GList *src_files,
                  GFunc on_completed_func, gpointer on_completed_data)
{
    XferData *data = g_new0 (XferData, 1);

    data->xferOptions = xferOptions;
    data->src_uri_list = src_uri_list;
    data->dest_uri_list = dest_uri_list;
    data->to_dir = to_dir;
    data->src_fl = src_fl;
    data->src_files = src_files;
    data->win = NULL;
    data->cur_file_name = NULL;
    data->prev_status = GNOME_VFS_XFER_PROGRESS_STATUS_OK;
    data->cur_phase = (GnomeVFSXferPhase) -1;
    data->prev_phase = (GnomeVFSXferPhase) -1;
    data->cur_file = -1;
    data->prev_file = -1;
    data->files_total = 0;
    data->prev_totalprog = (gfloat) 0.00;
    data->first_time = TRUE;
    data->on_completed_func = on_completed_func;
    data->on_completed_data = on_completed_data;
    data->done = FALSE;
    data->aborted = FALSE;

    // If this is a move-operation, determine totals
    // The async_xfer_callback-results for file and byte totals are not reliable
    if (xferOptions == GNOME_VFS_XFER_REMOVESOURCE) {
        GList *uris;
        GnomeVFSURI *uri;
        data->bytes_total = 0;
        data->files_total = 0;
        for (uris = data->src_uri_list; uris != NULL; uris = uris->next) {
            uri = (GnomeVFSURI*)uris->data;
            data->bytes_total += calc_tree_size(uri,&(data->files_total));
        }
    }

    return data;
}


inline gchar *file_details(const gchar *text_uri)
{
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
    GnomeVFSResult result = gnome_vfs_get_file_info (text_uri, info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
    gchar *size = create_nice_size_str (info->size);
    gchar *details = result==GNOME_VFS_OK ? g_strdup_printf ("%s, %s", size, time2string (info->mtime, gnome_cmd_data.options.date_format)) : g_strdup ("");
    gnome_vfs_file_info_unref (info);
    g_free (size);

    return details;
}


static gint async_xfer_callback (GnomeVFSAsyncHandle *handle, GnomeVFSXferProgressInfo *info, XferData *data)
{
    data->cur_phase = info->phase;
    data->cur_file = info->file_index;
    // only update totals if larger than current value
    if (data->files_total < info->files_total) data->files_total = info->files_total;
    if (data->bytes_total < info->bytes_total) data->bytes_total = info->bytes_total;
    data->file_size = info->file_size;
    data->bytes_copied = info->bytes_copied;
    data->total_bytes_copied = info->total_bytes_copied;

    if (data->aborted)
        return 0;

    if (info->source_name != NULL)
    {
        if (data->cur_file_name && strcmp (data->cur_file_name, info->source_name) != 0)
        {
            g_free (data->cur_file_name);
            data->cur_file_name = NULL;
        }

        if (!data->cur_file_name)
            data->cur_file_name = g_strdup (info->source_name);
    }

    if (info->status == GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE)
    {
    gchar *s = NULL;
    // Check if the src uri is from local ('file:///...'). If not, just use the base name.
    if ( !(s = gnome_vfs_get_local_path_from_uri (info->source_name) )) s = str_uri_basename (info->source_name);
        gchar *t = gnome_cmd_dir_is_local (data->to_dir) ? gnome_vfs_get_local_path_from_uri (info->target_name) : str_uri_basename (info->target_name);

        gchar *source_filename = get_utf8 (s);
        gchar *target_filename = get_utf8 (t);

        g_free (s);
        g_free (t);

        gchar *source_details = file_details (info->source_name);
        gchar *target_details = file_details (info->target_name);

        GtkWidget *msg;

        gchar *text = g_strdup_printf (_("Overwrite file:\n\n<b>%s</b>\n<span color='dimgray' size='smaller'>%s</span>\n\nWith:\n\n<b>%s</b>\n<span color='dimgray' size='smaller'>%s</span>"), target_filename, target_details, source_filename, source_details);

        g_free (source_filename);
        g_free (target_filename);
        g_free (source_details);
        g_free (target_details);

        gdk_threads_enter ();

        gint ret = run_simple_dialog (*main_win, FALSE, GTK_MESSAGE_QUESTION, text, " ",
                         1, _("Abort"), _("Replace"), _("Replace All"), _("Skip"), _("Skip All"), NULL);
        g_free(text);

        data->prev_status = GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE;
        gdk_threads_leave ();
        return ret==-1 ? 0 : ret;
    }

    if (info->status == GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR
        && data->prev_status != GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE)
    {
        const gchar *error = gnome_vfs_result_to_string (info->vfs_status);
        gchar *t = gnome_cmd_dir_is_local (data->to_dir) ? gnome_vfs_get_local_path_from_uri (info->target_name) :
                                                           str_uri_basename (info->target_name);
        gchar *fn = get_utf8 (t);
        gchar *msg = g_strdup_printf (_("Error while copying to %s\n\n%s"), fn, error);

        gdk_threads_enter ();
        gint ret = run_simple_dialog (*main_win, FALSE, GTK_MESSAGE_ERROR, msg, _("Transfer problem"),
                                      -1, _("Abort"), _("Retry"), _("Skip"), NULL);
        g_free (msg);
        g_free (fn);
        g_free (t);
        data->prev_status = GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR;
        gdk_threads_leave ();
        return ret==-1 ? 0 : ret;
    }

    if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED)
    {
        main_win->focus_file_lists();
        data->done = TRUE;
    }

    data->prev_status = info->status;

    return 1;
}


static gboolean update_xfer_gui_func (XferData *data)
{
    if (data->win && data->win->cancel_pressed)
    {
        data->aborted = TRUE;

        if (data->on_completed_func)
            data->on_completed_func (data->on_completed_data, NULL);

        gtk_widget_destroy (GTK_WIDGET (data->win));
        return FALSE;
    }

    if (data->cur_phase == GNOME_VFS_XFER_PHASE_COPYING)
    {
        if (data->prev_phase != GNOME_VFS_XFER_PHASE_COPYING)
        {
            gnome_cmd_xfer_progress_win_set_action (data->win, _("copying..."));
            data->prev_file = -1;
        }

        if (data->prev_file != data->cur_file)
        {
            gchar *t = str_uri_basename (data->cur_file_name);
            gchar *fn = get_utf8 (t);
            gchar *msg = g_strdup_printf (_("[file %ld of %ld] \"%s\""), data->cur_file, data->files_total, fn);

            gnome_cmd_xfer_progress_win_set_msg (data->win, msg);

            data->prev_file = data->cur_file;

            g_free (msg);
            g_free (fn);
            g_free (t);
        }

        if (data->bytes_total > 0)
        {
            gfloat total_prog = (gfloat)((gdouble)data->total_bytes_copied / (gdouble)data->bytes_total);
            gfloat total_diff = total_prog - data->prev_totalprog;

            if ((total_diff > (gfloat)0.01 && total_prog >= 0.0 && total_prog <= 1.0) || data->first_time)
            {
                data->first_time = FALSE;
                gnome_cmd_xfer_progress_win_set_total_progress (data->win, data->bytes_copied, data->file_size, data->total_bytes_copied, data->bytes_total);
                while (gtk_events_pending ())
                    gtk_main_iteration_do (FALSE);
            }
        }
    }

    if (data->done)
    {
        // Remove files from the source file list when a move operation has finished
        if (data->xferOptions & GNOME_VFS_XFER_REMOVESOURCE)
            if (data->src_fl && data->src_files)
            {
                /**********************************************************************

                    previous function used here:
                    gnome_cmd_file_list_remove_files (data->src_fl, data->src_files);

                    After 'async_xfer_callback' has been called the file list, 'src_files' has
                    not been altered and source files that have moved might be write protected.
                    So we need to check if files that are to be removed from the file list
                    still exist.

                *************************************************************************/

                for (; data->src_files; data->src_files = data->src_files->next)
                {
                    GnomeCmdFile *f = (GnomeCmdFile *) data->src_files->data;
                    GnomeVFSURI *src_uri = f->get_uri();
                    if (!gnome_vfs_uri_exists (src_uri))
                        data->src_fl->remove_file(f);
                    g_free (src_uri);
                }
            }

        //  Only update the files if needed
        if (data->to_dir)
        {
            gnome_cmd_dir_relist_files (data->to_dir, FALSE);
            main_win->focus_file_lists();
            gnome_cmd_dir_unref (data->to_dir);
            data->to_dir = NULL;
        }

        if (data->win)
        {
            gtk_widget_destroy (GTK_WIDGET (data->win));
            data->win = NULL;
        }

        free_xfer_data (data);

        return FALSE;
    }

    data->prev_phase = data->cur_phase;

    return TRUE;
}


inline gboolean uri_is_parent_to_dir_or_equal (GnomeVFSURI *uri, GnomeCmdDir *dir)
{
    GnomeVFSURI *dir_uri = GNOME_CMD_FILE (dir)->get_uri ();

    gboolean is_parent = gnome_vfs_uri_is_parent (uri, dir_uri, TRUE);

    gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
    gchar *dir_uri_str = gnome_vfs_uri_to_string (dir_uri, GNOME_VFS_URI_HIDE_PASSWORD);
    gboolean is_equal = gnome_vfs_uris_match (uri_str, dir_uri_str);

    g_free (uri_str);
    g_free (dir_uri_str);
    gnome_vfs_uri_unref (dir_uri);

    return is_parent || is_equal;
}


inline gchar *remove_basename (gchar *in)
{
    gchar *out = g_strdup (in);

    for (gint i=strlen(out)-1; i>0; i--)
        if (out[i] == '/')
        {
            out[i] = '\0';
            return out;
        }

    return NULL;
}


inline gboolean file_is_already_in_dir (GnomeVFSURI *uri, GnomeCmdDir *dir)
{
    gchar *tmp = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
    gchar *uri_str = remove_basename (tmp);
    gchar *dir_uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    gboolean ret = (strcmp (uri_str, dir_uri_str) == 0);

    g_free (uri_str);
    g_free (dir_uri_str);
    g_free (tmp);

    return ret;
}


void
gnome_cmd_xfer_uris_start (GList *src_uri_list,
                           GnomeCmdDir *to_dir,
                           GnomeCmdFileList *src_fl,
                           GList *src_files,
                           gchar *dest_fn,
                           GnomeVFSXferOptions xferOptions,
                           GnomeVFSXferOverwriteMode xferOverwriteMode,
                           GtkSignalFunc on_completed_func,
                           gpointer on_completed_data)
{
    g_return_if_fail (src_uri_list != NULL);
    g_return_if_fail (GNOME_CMD_IS_DIR (to_dir));

    GnomeVFSURI *src_uri, *dest_uri;

    // Sanity check
    for (GList *i = src_uri_list; i; i = i->next)
    {
        src_uri = (GnomeVFSURI *) i->data;
        if (uri_is_parent_to_dir_or_equal (src_uri, to_dir))
        {
            gnome_cmd_show_message (*main_win, _("Copying a directory into itself is a bad idea."), _("The whole operation was cancelled."));
            return;
        }
        if (file_is_already_in_dir (src_uri, to_dir))
            if (dest_fn && strcmp (dest_fn, gnome_vfs_uri_extract_short_name (src_uri)) == 0)
            {
                DEBUG ('x', "Copying a file to the same directory as it's already in, is not permitted\n");
                return;
            }
    }

    XferData *data = create_xfer_data (xferOptions, src_uri_list, NULL,
                                       to_dir, src_fl, src_files,
                                       (GFunc) on_completed_func, on_completed_data);

    gint num_files = g_list_length (src_uri_list);

    if (num_files == 1 && dest_fn != NULL)
    {
        dest_uri = gnome_cmd_dir_get_child_uri (to_dir, dest_fn);

        data->dest_uri_list = g_list_append (data->dest_uri_list, dest_uri);
    }
    else
    {
        for (; src_uri_list; src_uri_list = src_uri_list->next)
        {
            src_uri = (GnomeVFSURI *) src_uri_list->data;
            gchar *basename = gnome_vfs_uri_extract_short_name (src_uri);

            dest_uri = gnome_cmd_dir_get_child_uri (to_dir, basename);
            g_free (basename);

            data->dest_uri_list = g_list_append (data->dest_uri_list, dest_uri);
        }
    }

    g_free (dest_fn);

    data->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new (num_files));
    gtk_widget_ref (GTK_WIDGET (data->win));
    gtk_window_set_title (GTK_WINDOW (data->win), _("preparing..."));
    gtk_widget_show (GTK_WIDGET (data->win));

    //  start the transfer
    gnome_vfs_async_xfer (&data->handle, data->src_uri_list, data->dest_uri_list,
                          xferOptions, GNOME_VFS_XFER_ERROR_MODE_QUERY, xferOverwriteMode,
                          XFER_PRIORITY,
                          (GnomeVFSAsyncXferProgressCallback) async_xfer_callback, data,
                          NULL, NULL);

    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_xfer_gui_func, data);
}


void
gnome_cmd_xfer_start (GList *src_files,
                      GnomeCmdDir *to_dir,
                      GnomeCmdFileList *src_fl,
                      gchar *dest_fn,
                      GnomeVFSXferOptions xferOptions,
                      GnomeVFSXferOverwriteMode xferOverwriteMode,
                      GtkSignalFunc on_completed_func,
                      gpointer on_completed_data)
{
    g_return_if_fail (src_files != NULL);
    g_return_if_fail (GNOME_CMD_IS_DIR (to_dir));

    GList *src_uri_list = file_list_to_uri_list (src_files);

    gnome_cmd_xfer_uris_start (src_uri_list,
                               to_dir,
                               src_fl,
                               src_files,
                               dest_fn,
                               xferOptions,
                               xferOverwriteMode,
                               on_completed_func,
                               on_completed_data);
}


void
gnome_cmd_xfer_tmp_download (GnomeVFSURI *src_uri,
                             GnomeVFSURI *dest_uri,
                             GnomeVFSXferOptions xferOptions,
                             GnomeVFSXferOverwriteMode xferOverwriteMode,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data)
{
    g_return_if_fail (src_uri != NULL);
    g_return_if_fail (dest_uri != NULL);

    gnome_cmd_xfer_tmp_download_multiple (g_list_append (NULL, src_uri),
                                          g_list_append (NULL, dest_uri),
                                          xferOptions,
                                          xferOverwriteMode,
                                          on_completed_func,
                                          on_completed_data);
}


void
gnome_cmd_xfer_tmp_download_multiple (GList *src_uri_list,
                                      GList *dest_uri_list,
                                      GnomeVFSXferOptions xferOptions,
                                      GnomeVFSXferOverwriteMode xferOverwriteMode,
                                      GtkSignalFunc on_completed_func,
                                      gpointer on_completed_data)
{
    g_return_if_fail (src_uri_list != NULL);
    g_return_if_fail (dest_uri_list != NULL);

    XferData *data;

    data = create_xfer_data (xferOptions, src_uri_list, dest_uri_list,
                             NULL, NULL, NULL,
                             (GFunc) on_completed_func, on_completed_data);

    data->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new (g_list_length (src_uri_list)));
    gtk_window_set_title (GTK_WINDOW (data->win), _("downloading to /tmp"));
    gtk_widget_show (GTK_WIDGET (data->win));

    //  start the transfer
    GnomeVFSResult result;
    result = gnome_vfs_async_xfer (&data->handle, data->src_uri_list, data->dest_uri_list,
                                   xferOptions, GNOME_VFS_XFER_ERROR_MODE_ABORT, xferOverwriteMode,
                                   XFER_PRIORITY,
                                   (GnomeVFSAsyncXferProgressCallback) async_xfer_callback, data,
                                   NULL, NULL);

    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_xfer_gui_func, data);
}
