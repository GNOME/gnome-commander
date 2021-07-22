/**
 * @file gnome-cmd-xfer.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "utils.h"

using namespace std;


#define XFER_PRIORITY GNOME_VFS_PRIORITY_DEFAULT

#define XFER_ERROR_ACTION_DEFAULT -1
#define XFER_ERROR_ACTION_ABORT 0
#define XFER_ERROR_ACTION_RETRY 1
#define XFER_ERROR_ACTION_SKIP  2

inline void free_xfer_data (XferData *xferData)
{
    //  free the list with target uris
    for (GList *i = xferData->destGFileList; i; i = i->next)
    {
        auto gFile = (GFile *) i->data;
        g_object_unref (gFile);
    }

    //  free the list with target uris
    for (GList *i = xferData->srcGFileList; i; i = i->next)
    {
        auto gFile = (GFile *) i->data;
        g_object_unref (gFile);
    }

    g_list_free (xferData->destGFileList);
    g_list_free (xferData->srcGFileList);
    g_free (xferData);
}


static XferData *
create_xfer_data (GFileCopyFlags copyFlags, GList *srcGFileList, GList *destGFileList,
                  GnomeCmdDir *to_dir, GnomeCmdFileList *src_fl, GList *src_files,
                  GFunc on_completed_func, gpointer on_completed_data)
{
    XferData *xferData = g_new0 (XferData, 1);

    xferData->copyFlags = copyFlags;
    xferData->srcGFileList = srcGFileList;
    xferData->destGFileList = destGFileList;
    xferData->to_dir = to_dir;
    xferData->src_fl = src_fl;
    xferData->src_files = src_files;
    xferData->win = nullptr;
    xferData->cur_file_name = nullptr;
    xferData->cur_file = -1;
    xferData->prev_file = -1;
    xferData->files_total = 0;
    xferData->prev_totalprog = (gfloat) 0.00;
    xferData->first_time = TRUE;
    xferData->on_completed_func = on_completed_func;
    xferData->on_completed_data = on_completed_data;
    xferData->done = FALSE;
    xferData->aborted = FALSE;
    xferData->problem = FALSE;
    xferData->problem_action = -1;
    xferData->problem_file_name = nullptr;
    xferData->thread = nullptr;
    xferData->error = nullptr;

    //ToDo: Fix this to complete migration from gnome-vfs to gvfs
    // If this is a move-operation, determine totals
    // The async_xfer_callback-results for file and byte totals are not reliable
    //if (xferOptions == GNOME_VFS_XFER_REMOVESOURCE) {
    //    GList *uris;
    //    xferData->bytes_total = 0;
    //    xferData->files_total = 0;
    //    for (uris = xferData->srcGFileList; uris != nullptr; uris = uris->next) {
    //        GnomeVFSURI *uri;
    //        uri = (GnomeVFSURI*)uris->xferData;
    //        xferData->bytes_total += calc_tree_size(uri,&(xferData->files_total));
    //        g_object_unref(gFile);
    //    }
    //}

    return xferData;
}


inline gchar *file_details(const gchar *text_uri)
{
    auto gFileTmp = g_file_new_for_uri(text_uri);
    auto gFileInfoTmp = g_file_query_info(gFileTmp,
                            G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_TIME_MODIFIED,
                            G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    gchar *size = create_nice_size_str (g_file_info_get_attribute_uint64(gFileInfoTmp, G_FILE_ATTRIBUTE_STANDARD_SIZE));

    gchar *details =
        gFileInfoTmp != nullptr
            ? g_strdup_printf ("%s, %s", size,
                                         time2string (g_file_info_get_modification_date_time(gFileInfoTmp),
                                                      gnome_cmd_data.options.date_format))
            : g_strdup ("");
    g_free (size);
    g_object_unref(gFileInfoTmp);

    return details;
}


static gint async_xfer_callback (GnomeVFSAsyncHandle *handle, GnomeVFSXferProgressInfo *info, XferData *data)


static gboolean update_xfer_gui (XferData *xferData)
{
    g_mutex_lock (&xferData->mutex);

    if (xferData->problem && xferData->error)
    {
        gchar *msg = g_strdup_printf (_("Error while transfering “%s”\n\n%s"),
                                        xferData->problem_file_name,
                                        xferData->error->message);

        xferData->problem_action = run_simple_dialog (
            *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Transfer problem"),
            -1, _("Abort"), _("Retry"), _("Skip"), NULL);

        g_free (msg);

        xferData->problem = FALSE;
    }

    if (xferData->win && xferData->win->cancel_pressed)
    {
        xferData->aborted = TRUE;

        if (xferData->on_completed_func)
            xferData->on_completed_func (xferData->on_completed_data, nullptr);

        gtk_widget_destroy (GTK_WIDGET (xferData->win));
        free_xfer_data (xferData);
        return FALSE;
    }

    if (xferData->done)
    {
        //  Only update the files if needed
        if (xferData->to_dir)
        {
            gnome_cmd_dir_relist_files (xferData->to_dir, FALSE);
            main_win->focus_file_lists();
            gnome_cmd_dir_unref (xferData->to_dir);
            xferData->to_dir = nullptr;
        }

        if (xferData->win)
        {
            gtk_widget_destroy (GTK_WIDGET (xferData->win));
            xferData->win = nullptr;
        }

        // ToDo: If more than one file should be transfered and one file could not be
        // transfered successsfully, we have to check for this error here
        if (xferData->problem_action == XFER_ERROR_ACTION_DEFAULT
            && xferData->on_completed_func)
        {
            xferData->on_completed_func (xferData->on_completed_data, nullptr);
        }

        free_xfer_data (xferData);

        return FALSE;
    }

//    if (xferData->cur_phase == GNOME_VFS_XFER_PHASE_COPYING)
//    {
//        if (xferData->prev_phase != GNOME_VFS_XFER_PHASE_COPYING)
//        {
       gnome_cmd_xfer_progress_win_set_action (xferData->win, _("copying…"));
//            xferData->prev_file = -1;
//        }
//
//        if (xferData->prev_file != xferData->cur_file)
//        {
//            gchar *t = str_uri_basename (xferData->cur_file_name);
//            gchar *fn = get_utf8 (t);
//            gchar *msg = g_strdup_printf (_("[file %ld of %ld] “%s”"), xferData->cur_file, xferData->files_total, fn);
//
//            gnome_cmd_xfer_progress_win_set_msg (xferData->win, msg);
//
//            xferData->prev_file = xferData->cur_file;
//
//            g_free (msg);
//            g_free (fn);
//            g_free (t);
//        }
//
//        if (xferData->bytes_total > 0)
//        {
//            gfloat total_prog = (gfloat)((gdouble)xferData->total_bytes_copied / (gdouble)xferData->bytes_total);
//            gfloat total_diff = total_prog - xferData->prev_totalprog;
//
//            if ((total_diff > (gfloat)0.01 && total_prog >= 0.0 && total_prog <= 1.0) || xferData->first_time)
//            {
//                xferData->first_time = FALSE;
//                gnome_cmd_xfer_progress_win_set_total_progress (xferData->win, xferData->bytes_copied, xferData->file_size, xferData->total_bytes_copied, xferData->bytes_total);
//                while (gtk_events_pending ())
//                    gtk_main_iteration_do (FALSE);
//            }
//        }
//    }
    g_mutex_unlock (&xferData->mutex);

    return TRUE;
}


inline gboolean gfile_is_parent_to_dir_or_equal (GFile *gFile, GnomeCmdDir *dir)
{
    auto dir_gFile = GNOME_CMD_FILE (dir)->get_gfile ();

    gboolean is_parent = g_file_has_parent (dir_gFile, gFile);

    gboolean are_equal = g_file_equal (gFile, dir_gFile);

    g_object_unref (dir_gFile);

    return is_parent || are_equal;
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

    return nullptr;
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
    g_return_if_fail (src_uri_list != nullptr);
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
void
gnome_cmd_xfer_tmp_download (GFile *srcGFile,
                             GFile *destGFile,
                             GFileCopyFlags copyFlags,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data)
{
    g_return_if_fail (srcGFile != nullptr && G_IS_FILE(srcGFile));
    g_return_if_fail (destGFile != nullptr && G_IS_FILE(destGFile));

    auto srcGFileList = g_list_append (nullptr, srcGFile);
    auto destGFileList = g_list_append (nullptr, destGFile);

    auto xferData = create_xfer_data (copyFlags, srcGFileList, destGFileList,
                             nullptr, nullptr, nullptr,
                             (GFunc) on_completed_func, on_completed_data);

    g_mutex_init(&xferData->mutex);

    // ToDo: This must be extracted in an own method
    xferData->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new (g_list_length (xferData->srcGFileList)));
    gtk_window_set_title (GTK_WINDOW (xferData->win), _("downloading to /tmp"));
    g_object_ref(xferData->win);
    gtk_widget_show (GTK_WIDGET (xferData->win));

    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_xfer_tmp_download_multiple, xferData);
    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_xfer_gui, xferData);
    g_mutex_clear(&xferData->mutex);
}


void
gnome_cmd_xfer_tmp_download_multiple (XferData *xferData)
{
    g_return_if_fail (xferData->srcGFileList != nullptr);
    g_return_if_fail (xferData->destGFileList != nullptr);

    if (g_list_length(xferData->srcGFileList) != g_list_length(xferData->destGFileList))
        return;

    auto gFileDestListItem = xferData->destGFileList;

    for (auto gFileSrcListItem = xferData->srcGFileList; gFileSrcListItem; gFileSrcListItem = gFileSrcListItem->next)
    {
        auto srcGFile = (GFile *) gFileSrcListItem->data;
        auto destGFile = (GFile *) gFileDestListItem->data;

        gnome_cmd_xfer_tmp_download(srcGFile, destGFile, xferData->copyFlags, xferData);

        if (xferData->problem_action == XFER_ERROR_ACTION_ABORT)
            break;

        gFileDestListItem = xferData->destGFileList->next;
    }

    xferData->done = TRUE;
}

static void
update_copied_xfer_data (goffset current_num_bytes,
                 goffset total_num_bytes,
                 gpointer xferDataPointer)
{
    auto xferData = (XferData *) xferDataPointer;
    xferData->total_bytes_copied = current_num_bytes;
    xferData->bytes_total = total_num_bytes;
}


static void xfer_progress_update (XferData *xferData)
{
    g_mutex_lock (&xferData->mutex);

    if (xferData->error)
    {
        xferData->problem = TRUE;

        g_mutex_unlock (&xferData->mutex);
        while (xferData->problem_action == -1)
            g_thread_yield ();
        g_mutex_lock (&xferData->mutex);
        xferData->problem_file_name = nullptr;
        g_clear_error (&(xferData->error));
    }

    g_mutex_unlock (&xferData->mutex);
}


void
gnome_cmd_xfer_tmp_download (GFile *srcGFile,
                             GFile *destGFile,
                             GFileCopyFlags copyFlags,
                             gpointer xferDataPointer)
{
    GError *tmpError = nullptr;

    g_return_if_fail(G_IS_FILE(srcGFile));
    g_return_if_fail(G_IS_FILE(destGFile));
    auto xferData = (XferData *) xferDataPointer;

    // Start the transfer
    if (!g_file_copy (srcGFile, destGFile, copyFlags, nullptr, update_copied_xfer_data, xferDataPointer, &tmpError))
    {
        g_warning("g_file_copy: File could not be copied: %s\n", tmpError->message);
        g_propagate_error(&(xferData->error), tmpError);
        xferData->problem_file_name = g_file_peek_path(srcGFile);
        xfer_progress_update(xferData);
    }
    else
    {
        // Set back to default value because transfering the file might work only after the first try
        xferData->problem_action = XFER_ERROR_ACTION_DEFAULT;
    }

    if(xferData->problem_action == XFER_ERROR_ACTION_RETRY)
    {
        xferData->problem_action = XFER_ERROR_ACTION_DEFAULT;
        gnome_cmd_xfer_tmp_download(srcGFile, destGFile, copyFlags, xferData);
    }
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
    g_return_if_fail (src_files != nullptr);
    g_return_if_fail (GNOME_CMD_IS_DIR (to_dir));

    GList *srcGFileList = gnome_cmd_file_list_to_gfile_list (src_files);

    gnome_cmd_xfer_uris_start (srcGFileList,
                               to_dir,
                               src_fl,
                               src_files,
                               dest_fn,
                               xferOptions,
                               xferOverwriteMode,
                               on_completed_func,
                               on_completed_data);
}
