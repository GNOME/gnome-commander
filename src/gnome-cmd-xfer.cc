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
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "utils.h"
#include "src/dialogs/gnome-cmd-delete-dialog.h"

using namespace std;

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
                  GnomeCmdConfirmOverwriteMode overwriteMode,
                  GFunc on_completed_func, gpointer on_completed_data)
{
    XferData *xferData = g_new0 (XferData, 1);

    xferData->copyFlags = copyFlags;
    xferData->srcGFileList = srcGFileList;
    xferData->destGFileList = destGFileList;
    xferData->destGnomeCmdDir = to_dir;
    xferData->src_fl = src_fl;
    xferData->src_files = src_files;
    xferData->win = nullptr;
    xferData->cur_file_name = nullptr;
    xferData->curFileNumber = 0;
    xferData->filesTotal = 0;
    xferData->first_time = TRUE;
    xferData->on_completed_func = on_completed_func;
    xferData->on_completed_data = on_completed_data;
    xferData->done = FALSE;
    xferData->aborted = FALSE;
    xferData->problem = FALSE;
    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
    xferData->thread = nullptr;
    xferData->error = nullptr;
    xferData->overwriteMode = overwriteMode;

    return xferData;
}


inline gchar *get_file_details_string(GFile *gFile)
{
    auto gFileInfoTmp = g_file_query_info(gFile,
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


static void run_simple_error_dialog(const char *msg, XferData *xferData)
{
    gdk_threads_enter ();
    gint guiResponse = run_simple_dialog (
        *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Transfer problem"),
        -1, _("Abort"), _("Retry"), _("Skip"), NULL);
    gdk_threads_leave ();

    switch (guiResponse)
    {
        case 0:
            xferData->problem_action = COPY_ERROR_ACTION_ABORT;
            break;
        case 1:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
        case 2:
            xferData->problem_action = COPY_ERROR_ACTION_SKIP;
            break;
        default:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
    }
}


static void run_directory_copy_overwrite_dialog(const char *msg, XferData *xferData)
{
    gint guiResponse = -1;
    g_list_length(xferData->srcGFileList) > 1
        ? guiResponse = run_simple_dialog (
            *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Copy problem"),
            -1, _("Abort"), _("Retry"), _("Copy into"), _("Rename"), _("Rename all"), _("Skip"), _("Skip all"), NULL)
        : guiResponse = run_simple_dialog (
            *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Copy problem"),
            -1, _("Abort"), _("Retry"), _("Copy into"), _("Rename"), NULL);

    switch (guiResponse)
    {
        case 0:
            xferData->problem_action = COPY_ERROR_ACTION_ABORT;
            break;
        case 1:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
        case 2:
            xferData->problem_action = COPY_ERROR_ACTION_COPY_INTO;
            break;
        case 3:
            xferData->problem_action = COPY_ERROR_ACTION_RENAME;
            break;
        case 4:
            xferData->problem_action = COPY_ERROR_ACTION_RENAME_ALL;
            break;
        case 5:
            xferData->problem_action = COPY_ERROR_ACTION_SKIP;
            break;
        case 6:
            xferData->problem_action = COPY_ERROR_ACTION_SKIP_ALL;
            break;
        default:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
    }
}


static void run_file_copy_overwrite_dialog(XferData *xferData)
{
    gint guiResponse = -1;
    auto problemSrcBasename = get_gfile_attribute_string(xferData->problemSrcGFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    auto problemDestBasename = get_gfile_attribute_string(xferData->problemDestGFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    auto sourceDetails = get_file_details_string (xferData->problemSrcGFile);
    auto targetDetails = get_file_details_string (xferData->problemDestGFile);

    auto msg = g_strdup_printf (_("Overwrite file:\n\n<b>%s</b>\n<span color='dimgray' size='smaller'>%s</span>\n\nWith:\n\n<b>%s</b>\n<span color='dimgray' size='smaller'>%s</span>"),
                                problemDestBasename,
                                targetDetails,
                                problemSrcBasename,
                                sourceDetails);

    g_free (problemSrcBasename);
    g_free (problemDestBasename);
    g_free (sourceDetails);
    g_free (targetDetails);

    gdk_threads_enter ();
    xferData->filesTotal > 1
    ? guiResponse = run_simple_dialog (
        *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Copy problem"),
        -1, _("Abort"), _("Retry"), _("Replace"), _("Rename"), _("Skip"), _("Replace all"), _("Rename all"), _("Skip all"), NULL)
    : guiResponse = run_simple_dialog (
        *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Copy problem"),
        -1, _("Abort"), _("Retry"), _("Replace"), _("Rename"), NULL);
    gdk_threads_leave ();

    switch (guiResponse)
    {
        case 0:
            xferData->problem_action = COPY_ERROR_ACTION_ABORT;
            break;
        case 1:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
        case 2:
            xferData->problem_action = COPY_ERROR_ACTION_REPLACE;
            break;
        case 3:
            xferData->problem_action = COPY_ERROR_ACTION_RENAME;
            break;
        case 4:
            xferData->problem_action = COPY_ERROR_ACTION_SKIP;
            break;
        case 5:
            xferData->problem_action = COPY_ERROR_ACTION_REPLACE_ALL;
            break;
        case 6:
            xferData->problem_action = COPY_ERROR_ACTION_RENAME_ALL;
            break;
        case 7:
            xferData->problem_action = COPY_ERROR_ACTION_SKIP_ALL;
            break;
        default:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
    }
    g_free (msg);
}

static void update_transfer_gui_error_copy (XferData *xferData)
{
    gchar *msg = g_strdup_printf (_("Error while transferring “%s”\n\n%s"),
                                    g_file_peek_path(xferData->problemSrcGFile),
                                    xferData->error->message);

    if(!g_error_matches(xferData->error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        run_simple_error_dialog(msg, xferData);
    }
    else
    {
        if (xferData->currentSrcFileType == G_FILE_TYPE_DIRECTORY)
        {
            gdk_threads_enter ();
            switch(xferData->overwriteMode)
            {
                case GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL:
                    xferData->problem_action = COPY_ERROR_ACTION_SKIP_ALL;
                    break;
                case GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL:
                    xferData->problem_action = COPY_ERROR_ACTION_RENAME_ALL;
                    break;
                case GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY:
                    xferData->problem_action = COPY_ERROR_ACTION_COPY_INTO;
                    break;
                case GNOME_CMD_CONFIRM_OVERWRITE_QUERY:
                default:
                    run_directory_copy_overwrite_dialog(msg, xferData);
                    break;
            }
            gdk_threads_leave ();
        }
        if (xferData->currentSrcFileType == G_FILE_TYPE_REGULAR)
        {
            switch(xferData->overwriteMode)
            {
                case GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL:
                    xferData->problem_action = COPY_ERROR_ACTION_SKIP_ALL;
                    break;
                case GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL:
                    xferData->problem_action = COPY_ERROR_ACTION_RENAME_ALL;
                    break;
                case GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY:
                    xferData->problem_action = COPY_ERROR_ACTION_REPLACE_ALL;
                    break;
                case GNOME_CMD_CONFIRM_OVERWRITE_QUERY:
                default:
                    run_file_copy_overwrite_dialog(xferData);
                    break;
            }
        }
    }
    g_free (msg);
}


static void run_move_overwrite_dialog(XferData *xferData)
{
    gint guiResponse = -1;
    auto problemSrcBasename = get_gfile_attribute_string(xferData->problemSrcGFile,
                                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    auto problemDestBasename = get_gfile_attribute_string(xferData->problemDestGFile,
                                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    auto sourceDetails = get_file_details_string (xferData->problemSrcGFile);
    auto targetDetails = get_file_details_string (xferData->problemDestGFile);
    // ToDo: Fix wording for files/folders
    auto msg = g_strdup_printf (_("Overwrite file:\n\n<b>%s</b>\n<span color='dimgray' size='smaller'>%s</span>\n\nWith:\n\n<b>%s</b>\n<span color='dimgray' size='smaller'>%s</span>"),
            problemDestBasename,
            targetDetails,
            problemSrcBasename,
            sourceDetails);
    g_free (problemSrcBasename);
    g_free (problemDestBasename);
    g_free (sourceDetails);
    g_free (targetDetails);
    xferData->filesTotal > 1
        ? guiResponse = run_simple_dialog (
            *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Move problem"),
            -1, _("Abort"), _("Retry"), _("Replace"), _("Rename"), _("Skip"), _("Replace all"), _("Rename all"), _("Skip all"), NULL)
        : guiResponse = run_simple_dialog (
            *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Move problem"),
            -1, _("Abort"), _("Retry"), _("Replace"), _("Rename"), NULL);
    switch (guiResponse)
    {
        case 0:
            xferData->problem_action = COPY_ERROR_ACTION_ABORT;
            break;
        case 1:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
        case 2:
            xferData->problem_action = COPY_ERROR_ACTION_REPLACE;
            break;
        case 3:
            xferData->problem_action = COPY_ERROR_ACTION_RENAME;
            break;
        case 4:
            xferData->problem_action = COPY_ERROR_ACTION_SKIP;
            break;
        case 5:
            xferData->problem_action = COPY_ERROR_ACTION_REPLACE_ALL;
            break;
        case 6:
            xferData->problem_action = COPY_ERROR_ACTION_RENAME_ALL;
            break;
        case 7:
            xferData->problem_action = COPY_ERROR_ACTION_SKIP_ALL;
            break;
        default:
            xferData->problem_action = COPY_ERROR_ACTION_RETRY;
            break;
    }
}

//ToDo: Merge with 'update_transfer_gui_error_copy'
static void update_transfer_gui_error_move (XferData *xferData)
{
    gchar *msg = g_strdup_printf (_("Error while transferring “%s”\n\n%s"),
                                    g_file_peek_path(xferData->problemSrcGFile),
                                    xferData->error->message);

    if(!g_error_matches(xferData->error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        run_simple_error_dialog(msg, xferData);
    }
    else
    {
        gdk_threads_enter ();
        switch(xferData->overwriteMode)
        {
            case GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL:
                xferData->problem_action = COPY_ERROR_ACTION_SKIP_ALL;
                break;
            case GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL:
                xferData->problem_action = COPY_ERROR_ACTION_RENAME_ALL;
                break;
            case GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY:
                // Note: When doing a native move operation, there is no option to
                // move a folder into another folder like it is available for the copy
                // process. The reason is that in the former case, we don't step recursively
                // through the subdirectories. That's why we only offer a 'replace', but no
                // 'copy into' when moving folders.
                xferData->problem_action = COPY_ERROR_ACTION_REPLACE_ALL;
                break;
            case GNOME_CMD_CONFIRM_OVERWRITE_QUERY:
            default:
                run_move_overwrite_dialog(xferData);
                break;
        }
        gdk_threads_leave ();
    }
    g_free (msg);
}

static gboolean update_transfer_gui (XferData *xferData)
{
    g_mutex_lock (&xferData->mutex);

    if (xferData->problem && xferData->error)
    {
        switch (xferData->transferType)
        {
            case COPY:
                update_transfer_gui_error_copy(xferData);
                break;
            case MOVE:
                update_transfer_gui_error_move(xferData);
                break;
            case LINK:
                break;
        }

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
        if (xferData->destGnomeCmdDir)
        {
            gnome_cmd_dir_relist_files (xferData->destGnomeCmdDir, FALSE);
            main_win->focus_file_lists();
            gnome_cmd_dir_unref (xferData->destGnomeCmdDir);
            xferData->destGnomeCmdDir = nullptr;
        }

        if (xferData->win)
        {
            gtk_widget_destroy (GTK_WIDGET (xferData->win));
            xferData->win = nullptr;
        }

        // ToDo: If more than one file should be transferred and one file could not be
        // transferred successsfully, we have to check for this error here
        if (xferData->problem_action == COPY_ERROR_ACTION_NO_ACTION_YET
            && xferData->on_completed_func)
        {
            xferData->on_completed_func (xferData->on_completed_data, nullptr);
        }

        free_xfer_data (xferData);

        return FALSE;
    }

    if (xferData->bytesTotalTransferred == 0)
        gnome_cmd_xfer_progress_win_set_action (xferData->win, _("copying…"));

    if (xferData->curFileNumber)
    {
        gchar *msg = g_strdup_printf (_("[file %ld of %ld] “%s”"), xferData->curFileNumber,
                                        xferData->filesTotal, xferData->curSrcFileName);

        gnome_cmd_xfer_progress_win_set_msg (xferData->win, msg);

        g_free (msg);
    }

    if (xferData->bytesTotal > 0)
    {
        gfloat totalProgress =
            (gfloat)((gdouble)(xferData->bytesTotalTransferred + xferData->bytesCopiedFile)
                / (gdouble)xferData->bytesTotal);

        if ((totalProgress >= 0.0 && totalProgress <= 1.0) || xferData->first_time)
        {
            xferData->first_time = FALSE;
            gnome_cmd_xfer_progress_win_set_total_progress (xferData->win, xferData->bytesCopiedFile,
                                                            xferData->fileSize,
                                                            xferData->bytesTotalTransferred + xferData->bytesCopiedFile,
                                                            xferData->bytesTotal);
            while (gtk_events_pending ())
                gtk_main_iteration_do (FALSE);
        }
    }
    g_mutex_unlock (&xferData->mutex);

    return TRUE;
}

static gboolean update_tmp_download_gui (XferData *xferData)
{
    g_mutex_lock (&xferData->mutex);

    if (xferData->problem && xferData->error)
    {
        gchar *msg = g_strdup_printf (_("Error while transferring “%s”\n\n%s"),
                                        g_file_peek_path(xferData->problemSrcGFile),
                                        xferData->error->message);

        run_simple_error_dialog(msg, xferData);

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
        if (xferData->destGnomeCmdDir)
        {
            gnome_cmd_dir_relist_files (xferData->destGnomeCmdDir, FALSE);
            main_win->focus_file_lists();
            gnome_cmd_dir_unref (xferData->destGnomeCmdDir);
            xferData->destGnomeCmdDir = nullptr;
        }

        if (xferData->win)
        {
            gtk_widget_destroy (GTK_WIDGET (xferData->win));
            xferData->win = nullptr;
        }

        if (xferData->problem_action == COPY_ERROR_ACTION_NO_ACTION_YET
            && xferData->on_completed_func)
        {
            xferData->on_completed_func (xferData->on_completed_data, nullptr);
        }

        free_xfer_data (xferData);

        return FALSE;
    }

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


inline gchar *remove_basename (const gchar *in)
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


inline gboolean file_is_already_in_dir (GFile *gFile, GnomeCmdDir *dir)
{
    gchar *uri_str = remove_basename (g_file_get_uri(gFile));

    if (!uri_str)
        return false;

    gchar *dir_uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    gboolean ret = (strcmp (uri_str, dir_uri_str) == 0);

    g_free (uri_str);
    g_free (dir_uri_str);

    return ret;
}

static void
set_files_total(XferData *xferData)
{
    for(auto gFileSrcListItem = xferData->srcGFileList; gFileSrcListItem; gFileSrcListItem = gFileSrcListItem->next)
    {
        guint64 diskUsage = 0;
        guint64 numFiles = 0;
        auto gFile = (GFile *) gFileSrcListItem->data;
        g_return_if_fail(G_IS_FILE(gFile));

        g_file_measure_disk_usage (gFile,
           G_FILE_MEASURE_NONE,
           nullptr, nullptr, nullptr,
           &diskUsage,
           nullptr,
           &numFiles,
           nullptr);
        xferData->bytesTotal = diskUsage;
        xferData->filesTotal += numFiles;
    }
}


void
gnome_cmd_copy_gfiles_start (GList *srcGFileGList,
                             GnomeCmdDir *destGnomeCmdDir,
                             GnomeCmdFileList *srcGnomeCmdFileList,
                             GList *srcFilesGList,
                             gchar *destFileName,
                             GFileCopyFlags copyFlags,
                             GnomeCmdConfirmOverwriteMode overwriteMode,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data)
{
    g_return_if_fail (srcGFileGList != nullptr);
    g_return_if_fail (GNOME_CMD_IS_DIR (destGnomeCmdDir));

    GFile *srcGFile, *destGFile;

    // Sanity check
    for (GList *i = srcGFileGList; i; i = i->next)
    {
        srcGFile = (GFile *) i->data;
        if (gfile_is_parent_to_dir_or_equal (srcGFile, destGnomeCmdDir))
        {
            gnome_cmd_show_message (*main_win, _("Copying a directory into itself is a bad idea."), _("The whole operation was cancelled."));
            return;
        }
        if (file_is_already_in_dir (srcGFile, destGnomeCmdDir))
        {
            auto srcFilename = g_file_get_basename(srcGFile);
            if (destFileName && strcmp (destFileName, srcFilename) == 0)
            {
                g_free(srcFilename);
                DEBUG ('x', "Copying a file to the same directory as it's already in, is not permitted\n");
                return;
            }
            g_free(srcFilename);
        }
    }

    XferData *xferData = create_xfer_data (copyFlags, srcGFileGList, nullptr,
                            destGnomeCmdDir, srcGnomeCmdFileList, srcFilesGList, overwriteMode,
                            (GFunc) on_completed_func, on_completed_data);
    xferData->transferType = COPY;

    if (g_list_length (srcGFileGList) == 1 && destFileName != nullptr)
    {
        destGFile = gnome_cmd_dir_get_child_gfile (destGnomeCmdDir, destFileName);

        xferData->destGFileList = g_list_append (xferData->destGFileList, destGFile);
    }
    else
    {
        for (auto srcGFileGListItem = srcGFileGList; srcGFileGListItem; srcGFileGListItem = srcGFileGListItem->next)
        {
            srcGFile = (GFile *) srcGFileGListItem->data;
            auto basename = g_file_get_basename(srcGFile);

            destGFile = gnome_cmd_dir_get_child_gfile (destGnomeCmdDir, basename);
            g_free (basename);

            xferData->destGFileList = g_list_append (xferData->destGFileList, destGFile);
        }
    }

    g_free (destFileName);

    set_files_total(xferData);

    xferData->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new (xferData->filesTotal));
    gtk_widget_ref (GTK_WIDGET (xferData->win));
    gtk_window_set_title (GTK_WINDOW (xferData->win), _("preparing…"));
    gtk_widget_show (GTK_WIDGET (xferData->win));

    g_mutex_init(&xferData->mutex);

    //  start the transfer
    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_transfer_gfiles, xferData);
    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_transfer_gui, xferData);
    g_mutex_clear(&xferData->mutex);
}

void
gnome_cmd_move_gfiles_start (GList *srcGFileGList,
                             GnomeCmdDir *destGnomeCmdDir,
                             GnomeCmdFileList *srcGnomeCmdFileList,
                             GList *srcGnomeCmdFileGList,
                             gchar *destFileName,
                             GFileCopyFlags copyFlags,
                             GnomeCmdConfirmOverwriteMode overwriteMode,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data)
{
    g_return_if_fail (srcGFileGList != nullptr);
    g_return_if_fail (GNOME_CMD_IS_DIR (destGnomeCmdDir));

    GFile *srcGFile, *destGFile;

    // Sanity check
    for (GList *i = srcGFileGList; i; i = i->next)
    {
        srcGFile = (GFile *) i->data;
        if (gfile_is_parent_to_dir_or_equal (srcGFile, destGnomeCmdDir))
        {
            gnome_cmd_show_message (*main_win, _("Moving a directory into itself is a bad idea."), _("The whole operation was cancelled."));
            return;
        }
        if (file_is_already_in_dir (srcGFile, destGnomeCmdDir))
        {
            auto srcFilename = g_file_get_basename(srcGFile);
            if (destFileName && strcmp (destFileName, srcFilename) == 0)
            {
                g_free(srcFilename);
                DEBUG ('x', "Moving a file to the same directory as it's already in, is not permitted\n");
                return;
            }
            g_free(srcFilename);
        }
    }

    XferData *xferData = create_xfer_data (copyFlags, srcGFileGList, nullptr,
                            destGnomeCmdDir, srcGnomeCmdFileList, srcGnomeCmdFileGList,
                            overwriteMode, (GFunc) on_completed_func, on_completed_data);
    xferData->transferType = MOVE;

    if (g_list_length (srcGFileGList) == 1 && destFileName != nullptr)
    {
        destGFile = gnome_cmd_dir_get_child_gfile (destGnomeCmdDir, destFileName);

        xferData->destGFileList = g_list_append (xferData->destGFileList, destGFile);
    }
    else
    {
        for (auto srcGFileGListItem = srcGFileGList; srcGFileGListItem; srcGFileGListItem = srcGFileGListItem->next)
        {
            srcGFile = (GFile *) srcGFileGListItem->data;
            auto basename = g_file_get_basename(srcGFile);

            destGFile = gnome_cmd_dir_get_child_gfile (destGnomeCmdDir, basename);
            g_free (basename);

            xferData->destGFileList = g_list_append (xferData->destGFileList, destGFile);
        }
    }

    g_free (destFileName);

    set_files_total(xferData);

    xferData->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new (xferData->filesTotal));
    gtk_widget_ref (GTK_WIDGET (xferData->win));
    gtk_window_set_title (GTK_WINDOW (xferData->win), _("preparing…"));
    gtk_widget_show (GTK_WIDGET (xferData->win));

    g_mutex_init(&xferData->mutex);

    //  start the transfer
    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_transfer_gfiles, xferData);
    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_transfer_gui, xferData);
    g_mutex_clear(&xferData->mutex);
}


void
gnome_cmd_link_gfiles_start (GList *srcGFileGList,
                             GnomeCmdDir *destGnomeCmdDir,
                             GnomeCmdFileList *srcGnomeCmdFileList,
                             GList *srcGnomeCmdFileGList,
                             gchar *destFileName,
                             GFileCopyFlags copyFlags,
                             GnomeCmdConfirmOverwriteMode overwriteMode,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data)
{
    g_return_if_fail (srcGFileGList != nullptr);
    g_return_if_fail (GNOME_CMD_IS_DIR (destGnomeCmdDir));

    GFile *srcGFile, *destGFile;

    // Sanity check
    for (GList *i = srcGFileGList; i; i = i->next)
    {
        srcGFile = (GFile *) i->data;
        if (gfile_is_parent_to_dir_or_equal (srcGFile, destGnomeCmdDir))
        {
            gnome_cmd_show_message (*main_win, _("Moving a directory into itself is a bad idea."), _("The whole operation was cancelled."));
            return;
        }
        if (file_is_already_in_dir (srcGFile, destGnomeCmdDir))
        {
            auto srcFilename = g_file_get_basename(srcGFile);
            if (destFileName && strcmp (destFileName, srcFilename) == 0)
            {
                g_free(srcFilename);
                DEBUG ('x', "Moving a file to the same directory as it's already in, is not permitted\n");
                return;
            }
            g_free(srcFilename);
        }
    }

    XferData *xferData = create_xfer_data (copyFlags, srcGFileGList, nullptr,
                            destGnomeCmdDir, srcGnomeCmdFileList, srcGnomeCmdFileGList,
                            overwriteMode, (GFunc) on_completed_func, on_completed_data);
    xferData->transferType = LINK;

    if (g_list_length (srcGFileGList) == 1 && destFileName != nullptr)
    {
        destGFile = gnome_cmd_dir_get_child_gfile (destGnomeCmdDir, destFileName);

        xferData->destGFileList = g_list_append (xferData->destGFileList, destGFile);
    }
    else
    {
        for (auto srcGFileGListItem = srcGFileGList; srcGFileGListItem; srcGFileGListItem = srcGFileGListItem->next)
        {
            srcGFile = (GFile *) srcGFileGListItem->data;
            auto basename = g_file_get_basename(srcGFile);

            destGFile = gnome_cmd_dir_get_child_gfile (destGnomeCmdDir, basename);
            g_free (basename);

            xferData->destGFileList = g_list_append (xferData->destGFileList, destGFile);
        }
    }

    g_free (destFileName);

    set_files_total(xferData);

    gnome_cmd_transfer_gfiles(xferData);
}


void
gnome_cmd_tmp_download (GFile *srcGFile,
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
                             nullptr, nullptr, nullptr, GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                             (GFunc) on_completed_func, on_completed_data);
    xferData->transferType = COPY;

    g_mutex_init(&xferData->mutex);

    xferData->win = GNOME_CMD_XFER_PROGRESS_WIN (gnome_cmd_xfer_progress_win_new (g_list_length (xferData->srcGFileList)));
    gtk_window_set_title (GTK_WINDOW (xferData->win), _("downloading to /tmp"));
    gtk_widget_ref (GTK_WIDGET (xferData->win));
    gtk_widget_show (GTK_WIDGET (xferData->win));

    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_transfer_gfiles, xferData);
    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_tmp_download_gui, xferData);
    g_mutex_clear(&xferData->mutex);
}


void
gnome_cmd_transfer_gfiles (XferData *xferData)
{
    g_return_if_fail (xferData->srcGFileList != nullptr);
    g_return_if_fail (xferData->destGFileList != nullptr);

    if (g_list_length(xferData->srcGFileList) != g_list_length(xferData->destGFileList))
        return;

    GError *error = nullptr;

    auto gFileDestListItem = xferData->destGFileList;

    for (auto gFileSrcListItem = xferData->srcGFileList; gFileSrcListItem; gFileSrcListItem = gFileSrcListItem->next,
                                                                           gFileDestListItem = gFileDestListItem->next)
    {
        auto srcGFile = (GFile *) gFileSrcListItem->data;
        auto destGFile = (GFile *) gFileDestListItem->data;

         switch (xferData->transferType)
        {
            case COPY:
                gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, xferData->copyFlags, xferData);
                break;
            case MOVE:
                gnome_cmd_move_gfile_recursive(srcGFile, destGFile, xferData->copyFlags, xferData);
                break;
            case LINK:
                auto symlinkSrcPath = g_file_get_path(srcGFile);

                g_file_make_symbolic_link (destGFile,
                                   symlinkSrcPath,
                                   nullptr,
                                   &error);

                g_free(symlinkSrcPath);

                if (error)
                {
                    gchar *msg = g_strdup_printf (_("Error while creating symlink “%s”\n\n%s"),
                                                    g_file_peek_path(destGFile),
                                                    error->message);

                    run_simple_dialog (
                        *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Symlink creation problem"),
                        -1, _("Ignore"), NULL);
                    g_free(msg);
                    g_error_free(error);
                    error = nullptr;
                }
                break;
        }
    }

    main_win->focus_file_lists();
    xferData->done = TRUE;
}


static void
update_transferred_data (goffset currentNumBytes,
                 goffset totalNumBytes,
                 gpointer xferDataPointer)
{
    auto xferData = (XferData *) xferDataPointer;
    xferData->bytesCopiedFile = currentNumBytes;
    xferData->fileSize = totalNumBytes;
}


/**
 * This function forces the thread for copying files and folders to wait until
 * the user clicked something on the popup gui which makes problem_action becomes != -1.
 */
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
        g_object_unref(xferData->problemSrcGFile);
        g_object_unref(xferData->problemDestGFile);
        g_clear_error (&(xferData->error));
    }

    g_mutex_unlock (&xferData->mutex);
}


static void
do_the_copy(GFile *srcGFile, GFile *destGFile, GFileCopyFlags copyFlags, gpointer xferDataPointer)
{
    g_return_if_fail(xferDataPointer != nullptr);
    g_return_if_fail(srcGFile != nullptr);
    g_return_if_fail(destGFile != nullptr);

    GError *tmpError = nullptr;
    auto xferData = (XferData *) xferDataPointer;

    g_free(xferData->curSrcFileName);
    xferData->curSrcFileName = get_gfile_attribute_string(srcGFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    if(!g_file_copy(srcGFile, destGFile, copyFlags, nullptr, update_transferred_data, xferDataPointer, &tmpError))
    {
        g_propagate_error(&(xferData->error), tmpError);
        xferData->problemSrcGFile = g_file_dup(srcGFile);
        xferData->problemDestGFile = g_file_dup(destGFile);
        xfer_progress_update(xferData);
    }
    else
    {
        xferData->curFileNumber++;
        xferData->bytesTotalTransferred += xferData->fileSize;
        if (xferData->problem_action == COPY_ERROR_ACTION_RETRY)
            xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
    }
}

static GFile*
get_new_dest_gfile(GFile *srcGFile, GFile *destGFile, const char *copyString, guint increment)
{
    auto destGFileParent = g_file_get_parent(destGFile);
    auto srcBasename = g_file_get_basename(srcGFile);
    auto newDestGFilePath = g_strdup_printf("%s%s%s (%s %d)",
                                            g_file_peek_path(destGFileParent),
                                            G_DIR_SEPARATOR_S,
                                            srcBasename,
                                            copyString,
                                            increment);
    auto newDestGFile = g_file_new_for_path(newDestGFilePath);
    g_free(newDestGFilePath);
    g_free(srcBasename);
    g_object_unref(destGFileParent);

    return newDestGFile;
}

static void
set_new_nonexisting_dest_gfile(GFile *srcGFile, GFile **destGFile, XferData *xferData)
{
    // Translators: Translate 'Copy' as a noun here
    auto copyString = C_("Filename suffix", "Copy");
    guint increment = 1;

    auto newDestGFile = get_new_dest_gfile(srcGFile, *destGFile, copyString, increment);

    while (g_file_query_exists(newDestGFile, nullptr))
    {
        g_object_unref(newDestGFile);
        increment++;
        newDestGFile = get_new_dest_gfile(srcGFile, *destGFile, copyString, increment);
    }

    // If destGFile is an item of the user-provided list of items
    // to be transferred, we have to update this list-item also
    auto oldDestItem = g_list_find(xferData->destGFileList, *destGFile);
    if (oldDestItem)
    {
        oldDestItem->data = newDestGFile;
    }

    g_object_unref(*destGFile);

    *destGFile = newDestGFile;
}

/**
 * This function moves directories and files from srcGFile to destGFile.
 * ...
 */
gboolean
gnome_cmd_move_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer xferDataPointer)
{
    auto xferData = (XferData *) xferDataPointer;
    if (xferData->error)
    {
        return false;
    }

    g_return_val_if_fail(G_IS_FILE(srcGFile), false);
    g_return_val_if_fail(G_IS_FILE(destGFile), false);

    GError *tmpError = nullptr;

    g_free(xferData->curSrcFileName);
    xferData->curSrcFileName = get_gfile_attribute_string(srcGFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    xferData->currentSrcFileType = g_file_query_file_type(srcGFile, G_FILE_QUERY_INFO_NONE, nullptr);

    if(!g_file_move(srcGFile, destGFile, copyFlags, nullptr, update_transferred_data, xferDataPointer, &tmpError))
    {
        if(g_file_query_file_type(srcGFile, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY
           && g_file_query_exists(destGFile, nullptr)
           && g_error_matches(tmpError, G_IO_ERROR, G_IO_ERROR_WOULD_RECURSE))
        {
            g_error_free(tmpError);
            tmpError = nullptr;
            if (gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, xferData->copyFlags, xferData))
            {
                // Folder was copied, now delete the source for completing the move operation
                auto gFileInfo = g_file_query_info(srcGFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &tmpError);
                if (tmpError)
                {
                    auto srcFileName = g_file_get_basename(srcGFile);
                    auto msg = g_strdup_printf(_("Source “%s” could not be deleted. Aborting!\n\n%s"), srcFileName, tmpError->message);
                    run_simple_dialog ( *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Transfer problem"), -1, _("Abort"), NULL);
                    g_free (srcFileName);
                    g_error_free(tmpError);
                    return false;
                }

                auto srcGFileParent = g_file_get_parent(srcGFile);
                if (!srcGFileParent)
                {
                    auto srcFileName = g_file_get_basename(srcGFile);
                    auto msg = g_strdup_printf(_("Source “%s” could not be deleted. Aborting!\n\n%s"), srcFileName, tmpError->message);
                    run_simple_dialog ( *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Transfer problem"), -1, _("Abort"), NULL);
                    g_free (srcFileName);
                    g_error_free(tmpError);
                    g_object_unref(gFileInfo);
                    return false;
                }
                auto gFileParentPath = g_file_get_path(srcGFileParent);
                auto gnomeCmdDirParent = gnome_cmd_dir_new (get_home_con(), new GnomeCmdPlainPath(gFileParentPath));
                auto gnomeCmdDir = gnome_cmd_dir_new_from_gfileinfo(gFileInfo, gnomeCmdDirParent);

                auto deleteData = g_new0 (DeleteData, 1);
                deleteData->gnomeCmdFiles = g_list_append(nullptr, gnomeCmdDir);
                do_delete (deleteData, false); // false -> do not show progress window

                g_free(gFileParentPath);
                g_object_unref(srcGFileParent);
            }
            else
            {
                auto srcFileName = g_file_get_basename(srcGFile);
                auto msg = g_strdup_printf(_("Source “%s” could not be copied. Aborting!"), srcFileName);
                run_simple_dialog ( *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Transfer problem"), -1, _("Abort"), NULL);
                g_free (srcFileName);
                g_error_free(tmpError);
                return false;
            }
        }
        else
        {
            // If G_FILE_COPY_OVERWRITE is not specified (i.e. when not 'Overwrite silently' is selected)
            // and the target exists and is a file, then the error G_IO_ERROR_EXISTS is returned.
            if(g_error_matches(tmpError, G_IO_ERROR, G_IO_ERROR_EXISTS))
            {
                g_propagate_error(&(xferData->error), tmpError);
                xferData->problemSrcGFile = g_file_dup(srcGFile);
                xferData->problemDestGFile = g_file_dup(destGFile);
                xfer_progress_update(xferData);

                guint copyFlagsTemp = copyFlags;

                switch (xferData->problem_action)
                {
                    case COPY_ERROR_ACTION_RETRY:
                        xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                        return gnome_cmd_move_gfile_recursive(srcGFile, destGFile, copyFlags, xferData);
                    case COPY_ERROR_ACTION_REPLACE:
                        xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                        copyFlagsTemp = copyFlags | G_FILE_COPY_OVERWRITE;
                        gnome_cmd_move_gfile_recursive(srcGFile, destGFile, (GFileCopyFlags) copyFlagsTemp, xferData);
                        break;
                    case COPY_ERROR_ACTION_REPLACE_ALL:
                        copyFlagsTemp = copyFlags | G_FILE_COPY_OVERWRITE;
                        gnome_cmd_move_gfile_recursive(srcGFile, destGFile, (GFileCopyFlags) copyFlagsTemp, xferData);
                        break;
                    case COPY_ERROR_ACTION_RENAME:
                        xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                        set_new_nonexisting_dest_gfile(srcGFile, &destGFile, xferData);
                        // ToDo: Handle tmpError, also on other occurences!
                        g_file_move(srcGFile, destGFile, copyFlags, nullptr, update_transferred_data, xferDataPointer, &tmpError);
                        break;
                    case COPY_ERROR_ACTION_RENAME_ALL:
                        set_new_nonexisting_dest_gfile(srcGFile, &destGFile, xferData);
                        g_file_move(srcGFile, destGFile, copyFlags, nullptr, update_transferred_data, xferDataPointer, &tmpError);
                        break;
                    case COPY_ERROR_ACTION_SKIP:
                        xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                        break;
                    case COPY_ERROR_ACTION_SKIP_ALL:
                        return true;
                    // ToDo: Copy into can only be selected in case of copying a directory into an
                    // already exising directory, so we are ignoring this here because we deal with
                    // moving files in this if branch.
                    case COPY_ERROR_ACTION_COPY_INTO:
                    case COPY_ERROR_ACTION_ABORT:
                        return false;
                    case COPY_ERROR_ACTION_NO_ACTION_YET:
                    default:
                        break;
                }
            }
            else
            {
                g_propagate_error(&(xferData->error), tmpError);
                xferData->problemSrcGFile = g_file_dup(srcGFile);
                xferData->problemDestGFile = g_file_dup(destGFile);
                xfer_progress_update(xferData);
            }
        }
    }
    else
    {
        xferData->curFileNumber++;
        xferData->bytesTotalTransferred += xferData->fileSize;
        if (xferData->problem_action == COPY_ERROR_ACTION_RETRY)
            xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
    }

    if(xferData->problem_action == COPY_ERROR_ACTION_RETRY)
    {
        xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
        gnome_cmd_move_gfile_recursive(srcGFile, destGFile, copyFlags, xferData);
    }

    return true;
}

/**
 * This function recursively copies directories and files from srcGFile to destGFile.
 * As the function can call itself multiple times, some logic is build into it to handle the
 * case when the destination is already existing and the user decides up on that.
 *
 * The function does return when it copied a regular file or when something went wrong
 * unexpectedly. It calls itself if a directory should be copied or if a rename should happen
 * as the original destination exists already.
 */
gboolean
gnome_cmd_copy_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer xferDataPointer)
{
    auto xferData = (XferData *) xferDataPointer;
    if (xferData->error)
    {
        return false;
    }

    g_return_val_if_fail(G_IS_FILE(srcGFile), false);
    g_return_val_if_fail(G_IS_FILE(destGFile), false);

    GError *tmpError = nullptr;

    switch (g_file_query_file_type(srcGFile, G_FILE_QUERY_INFO_NONE, nullptr))
    {
        case G_FILE_TYPE_DIRECTORY:
        {
            xferData->currentSrcFileType = G_FILE_TYPE_DIRECTORY;

            switch (xferData->problem_action)
            {
                case COPY_ERROR_ACTION_NO_ACTION_YET:
                    // Try to create the dest directory
                    if (!g_file_make_directory (destGFile, nullptr, &tmpError))
                    {
                        g_warning("g_file_make_directory error: %s\n", tmpError->message);
                        g_propagate_error(&(xferData->error), tmpError);
                        xferData->problemSrcGFile = g_file_dup(srcGFile);
                        xferData->problemDestGFile = g_file_dup(destGFile);
                        xfer_progress_update(xferData);
                        return gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, copyFlags, xferData);
                    }
                    break;
                case COPY_ERROR_ACTION_RENAME:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    set_new_nonexisting_dest_gfile(srcGFile, &destGFile, xferData);
                    if (!g_file_make_directory (destGFile, nullptr, &tmpError))
                    {
                        g_warning("g_file_make_directory error: %s\n", tmpError->message);
                        g_propagate_error(&(xferData->error), tmpError);
                        xferData->problemSrcGFile = g_file_dup(srcGFile);
                        xferData->problemDestGFile = g_file_dup(destGFile);
                        xfer_progress_update(xferData);
                        return false;
                    }
                    break;
                case COPY_ERROR_ACTION_COPY_INTO:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    break;
                case COPY_ERROR_ACTION_RENAME_ALL:
                    set_new_nonexisting_dest_gfile(srcGFile, &destGFile, xferData);
                    if (!g_file_make_directory (destGFile, nullptr, &tmpError))
                    {
                        g_warning("g_file_make_directory error: %s\n", tmpError->message);
                        g_propagate_error(&(xferData->error), tmpError);
                        xferData->problemSrcGFile = g_file_dup(srcGFile);
                        xferData->problemDestGFile = g_file_dup(destGFile);
                        xfer_progress_update(xferData);
                        return false;
                    }
                    break;
                case COPY_ERROR_ACTION_SKIP:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    return true;
                case COPY_ERROR_ACTION_SKIP_ALL:
                    return true;
                case COPY_ERROR_ACTION_RETRY:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    return gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, copyFlags, xferData);
                case COPY_ERROR_ACTION_ABORT:
                    return false;
                case COPY_ERROR_ACTION_REPLACE:     // This is not handled for directories when copying
                case COPY_ERROR_ACTION_REPLACE_ALL: // This is not handled for directories when copying
                default:
                    return false;
            }

            // After processing the user response above, we are looping through the directory now.
            auto enumerator = g_file_enumerate_children(srcGFile, "*", G_FILE_QUERY_INFO_NONE, NULL, &tmpError);
            if (tmpError)
            {
                g_warning("g_file_enumerate_children error: %s\n", tmpError->message);
                g_propagate_error(&(xferData->error), tmpError);
                xferData->problemSrcGFile = g_file_dup(srcGFile);
                xferData->problemDestGFile = g_file_dup(destGFile);
                xfer_progress_update(xferData);
                return false;
            }

            auto gFileInfoChildFile = g_file_enumerator_next_file(enumerator, nullptr, &tmpError);
            while(gFileInfoChildFile != nullptr && !tmpError)
            {
                if(g_file_info_get_file_type(gFileInfoChildFile) == G_FILE_TYPE_DIRECTORY)
                {
                    auto dirNameChildGFile = g_file_info_get_name(gFileInfoChildFile);
                    auto sourceChildGFile = g_file_get_child(g_file_enumerator_get_container(enumerator), dirNameChildGFile);
                    auto targetPath = g_strdup_printf("%s%s%s", g_file_peek_path(destGFile), G_DIR_SEPARATOR_S, dirNameChildGFile);
                    auto targetGFile = g_file_new_for_path(targetPath);

                    gnome_cmd_copy_gfile_recursive(sourceChildGFile, targetGFile, copyFlags, xferData);

                    g_object_unref(sourceChildGFile);
                    g_object_unref(targetGFile);
                    g_free(targetPath);
                }
                else if(g_file_info_get_file_type(gFileInfoChildFile) == G_FILE_TYPE_REGULAR)
                {
                    xferData->currentSrcFileType = G_FILE_TYPE_REGULAR;
                    auto fileNameChildGFile = g_file_info_get_name(gFileInfoChildFile);
                    auto srcChildGFile = g_file_get_child(g_file_enumerator_get_container(enumerator), fileNameChildGFile);
                    auto targetPath = g_strdup_printf("%s%s%s", g_file_peek_path(destGFile), G_DIR_SEPARATOR_S, fileNameChildGFile);
                    auto targetGFile = g_file_new_for_path(targetPath);
                    do_the_copy(srcChildGFile, targetGFile, copyFlags, xferDataPointer);
                    guint copyFlagsTemp = copyFlags;
                    switch (xferData->problem_action)
                    {
                        case COPY_ERROR_ACTION_RETRY:
                            xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                            return gnome_cmd_copy_gfile_recursive(srcChildGFile, targetGFile, copyFlags, xferData);
                        case COPY_ERROR_ACTION_REPLACE:
                            xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                            copyFlagsTemp = copyFlags | G_FILE_COPY_OVERWRITE;
                            gnome_cmd_copy_gfile_recursive(srcChildGFile, targetGFile, (GFileCopyFlags) copyFlagsTemp, xferData);
                            break;
                        case COPY_ERROR_ACTION_RENAME:
                            xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                            set_new_nonexisting_dest_gfile(srcChildGFile, &targetGFile, xferData);
                            gnome_cmd_copy_gfile_recursive(srcChildGFile, targetGFile, copyFlags, xferData);
                            break;
                        case COPY_ERROR_ACTION_SKIP:
                            xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                            break;
                        case COPY_ERROR_ACTION_REPLACE_ALL:
                            copyFlagsTemp = copyFlags | G_FILE_COPY_OVERWRITE;
                            gnome_cmd_copy_gfile_recursive(srcChildGFile, targetGFile, (GFileCopyFlags) copyFlagsTemp, xferData);
                            break;
                        case COPY_ERROR_ACTION_SKIP_ALL:
                            g_object_unref(srcChildGFile);
                            g_object_unref(targetGFile);
                            g_free(targetPath);
                            g_object_unref(gFileInfoChildFile);
                            g_file_enumerator_close (enumerator, nullptr, nullptr);
                            return true;
                        case COPY_ERROR_ACTION_RENAME_ALL:
                            set_new_nonexisting_dest_gfile(srcChildGFile, &targetGFile, xferData);
                            gnome_cmd_copy_gfile_recursive(srcChildGFile, targetGFile, copyFlags, xferData);
                            break;
                        case COPY_ERROR_ACTION_ABORT:
                        case COPY_ERROR_ACTION_COPY_INTO:
                            g_object_unref(srcChildGFile);
                            g_object_unref(targetGFile);
                            g_free(targetPath);
                            g_object_unref(gFileInfoChildFile);
                            g_file_enumerator_close (enumerator, nullptr, nullptr);
                            return false;
                        case COPY_ERROR_ACTION_NO_ACTION_YET:
                        default:
                            break;
                    }
                    g_object_unref(srcChildGFile);
                    g_object_unref(targetGFile);
                    g_free(targetPath);
                }
                g_object_unref(gFileInfoChildFile);
                gFileInfoChildFile = g_file_enumerator_next_file(enumerator, NULL, &tmpError);
            }
            if (tmpError)
            {
                printf("g_file_enumerator_next_file: %s\n", tmpError->message);
                g_propagate_error(&(xferData->error), tmpError);
                xfer_progress_update(xferData);
            }
            g_file_enumerator_close (enumerator, nullptr, nullptr);
            break;
        }
        case G_FILE_TYPE_REGULAR:
        {
            xferData->currentSrcFileType = G_FILE_TYPE_REGULAR;
            do_the_copy(srcGFile, destGFile, copyFlags, xferDataPointer);
            switch (xferData->problem_action)
            {
                case COPY_ERROR_ACTION_RETRY:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    return gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, copyFlags, xferData);
                case COPY_ERROR_ACTION_REPLACE:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, G_FILE_COPY_OVERWRITE, xferData);
                    break;
                case COPY_ERROR_ACTION_RENAME:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    set_new_nonexisting_dest_gfile(srcGFile, &destGFile, xferData);
                    do_the_copy(srcGFile, destGFile, copyFlags, xferDataPointer);
                    break;
                case COPY_ERROR_ACTION_SKIP:
                    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                    break;
                case COPY_ERROR_ACTION_REPLACE_ALL:
                    gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, G_FILE_COPY_OVERWRITE, xferData);
                    break;
                case COPY_ERROR_ACTION_SKIP_ALL:
                    return true;
                case COPY_ERROR_ACTION_RENAME_ALL:
                    set_new_nonexisting_dest_gfile(srcGFile, &destGFile, xferData);
                    do_the_copy(srcGFile, destGFile, copyFlags, xferDataPointer);
                    break;
                case COPY_ERROR_ACTION_COPY_INTO: // This is not handled for files when copying
                case COPY_ERROR_ACTION_ABORT:
                    return false;
                case COPY_ERROR_ACTION_NO_ACTION_YET:
                default:
                    break;
            }
        }

        case G_FILE_TYPE_MOUNTABLE:
        case G_FILE_TYPE_UNKNOWN:
        case G_FILE_TYPE_SHORTCUT:
        case G_FILE_TYPE_SPECIAL:
        case G_FILE_TYPE_SYMBOLIC_LINK:
        default:
            break;
    }

    if(xferData->problem_action == COPY_ERROR_ACTION_RETRY)
    {
        xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
        gnome_cmd_copy_gfile_recursive(srcGFile, destGFile, copyFlags, xferData);
    }

    return true;
}

void
gnome_cmd_copy_start (GList *srcGnomeCmdFileGList,
                      GnomeCmdDir *toGnomeCmdDir,
                      GnomeCmdFileList *srcGnomeCmdFileList,
                      gchar *destFileName,
                      GFileCopyFlags copyFlags,
                      GnomeCmdConfirmOverwriteMode overwriteMode,
                      GtkSignalFunc on_completed_func,
                      gpointer on_completed_data)
{
    g_return_if_fail (srcGnomeCmdFileGList != nullptr);
    g_return_if_fail (GNOME_CMD_IS_DIR (toGnomeCmdDir));

    GList *srcGFileList = gnome_cmd_file_list_to_gfile_list (srcGnomeCmdFileGList);

    gnome_cmd_copy_gfiles_start (srcGFileList,
                               toGnomeCmdDir,
                               srcGnomeCmdFileList,
                               srcGnomeCmdFileGList,
                               destFileName,
                               copyFlags,
                               overwriteMode,
                               on_completed_func,
                               on_completed_data);
}

void
gnome_cmd_move_start (GList *srcGnomeCmdFileGList,
                      GnomeCmdDir *toGnomeCmdDir,
                      GnomeCmdFileList *srcGnomeCmdFileList,
                      gchar *destFileName,
                      GFileCopyFlags copyFlags,
                      GnomeCmdConfirmOverwriteMode overwriteMode,
                      GtkSignalFunc on_completed_func,
                      gpointer on_completed_data)
{
    g_return_if_fail (srcGnomeCmdFileGList != nullptr);
    g_return_if_fail (GNOME_CMD_IS_DIR (toGnomeCmdDir));

    GList *srcGFileList = gnome_cmd_file_list_to_gfile_list (srcGnomeCmdFileGList);

    gnome_cmd_move_gfiles_start (srcGFileList,
                                 toGnomeCmdDir,
                                 srcGnomeCmdFileList,
                                 srcGnomeCmdFileGList,
                                 destFileName,
                                 copyFlags,
                                 overwriteMode,
                                 on_completed_func,
                                 on_completed_data);
}
