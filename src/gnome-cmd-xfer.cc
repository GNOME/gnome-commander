/**
 * @file gnome-cmd-xfer.cc
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
#include <unistd.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-path.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"

using namespace std;


enum COPY_ERROR_ACTION
{
    COPY_ERROR_ACTION_NO_ACTION_YET = -1, // No error yet, so no action to take
    COPY_ERROR_ACTION_ABORT,
    COPY_ERROR_ACTION_RETRY,
    COPY_ERROR_ACTION_COPY_INTO,
    COPY_ERROR_ACTION_SKIP,
    COPY_ERROR_ACTION_RENAME,
    COPY_ERROR_ACTION_SKIP_ALL,
    COPY_ERROR_ACTION_RENAME_ALL,
    COPY_ERROR_ACTION_REPLACE,
    COPY_ERROR_ACTION_REPLACE_ALL
};


struct TransferControl;


struct XferData
{
    GtkWindow *parent_window;

    GFileCopyFlags copyFlags;
    GnomeCmdConfirmOverwriteMode overwriteMode;
    GnomeCmdTransferType transferType{COPY};

    // Source and target GFile's. The first srcGFile should be transferred to the first destGFile and so on...
    GList *srcGFileList;
    GList *destGFileList;

    GnomeCmdDir *destGnomeCmdDir;

    gulong curFileNumber;
    gchar *curSrcFileName;
    gulong filesTotal;
    gchar *cur_file_name;

    guint64 fileSize;
    guint64 bytesCopiedFile;
    guint64 bytesTotal;
    guint64 bytesTotalTransferred{0};

    TransferControl *transfer_control;

    GnomeCmdXferCallback on_completed_func;
    gpointer on_completed_data;

    GCancellable* cancellable;

    COPY_ERROR_ACTION problem_action;        // action to take when an error occurs
    GFile *problemSrcGFile;
    GFile *problemDestGFile;
    gchar *problemMessage;
    GThread *thread{nullptr};                // the work thread
    GMutex mutex{nullptr};                   // used to sync the main and worker thread
    GError *error{nullptr};                  // the cause that the file can't be deleted
    GType currentSrcFileType;                // the file type of the file which is currently copied
};

extern "C" TransferControl *start_transfer_gui(
    XferData *xferData,
    GtkWindow *parent_window,
    const gchar *title,
    guint64 no_of_files,
    GnomeCmdTransferType transfer_type,
    GnomeCmdConfirmOverwriteMode overwrite_mode
);

extern "C" GCancellable *transfer_get_cancellable(TransferControl *tc_ptr);

extern "C" void transfer_progress(
    TransferControl *tc_ptr,
    guint64 curFileNumber,
    const gchar *curSrcFileName,
    guint64 filesTotal,
    guint64 fileSize,
    guint64 bytesCopiedFile,
    guint64 bytesTotal,
    guint64 bytesTotalTransferred
);

extern "C" COPY_ERROR_ACTION transfer_ask_handle_error(
    TransferControl *tc_ptr,
    GError *error_ptr,
    GFile *src,
    GFile *dst,
    const gchar *message
);

extern "C" void transfer_done(TransferControl *tc_ptr);


extern "C" void finish_xfer(XferData *xferData, gboolean threaded, gboolean success);


extern "C" void do_delete_files_for_move (GtkWindow *parent_window, GList *files, gboolean showProgress);


static void xfer_progress_update (XferData *xferData);


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
    g_free(xferData->curSrcFileName);
    g_free (xferData);
}


static XferData *
create_xfer_data (GtkWindow *parent_window,
                  GFileCopyFlags copyFlags, GList *srcGFileList, GList *destGFileList,
                  GnomeCmdDir *to_dir,
                  GnomeCmdConfirmOverwriteMode overwriteMode,
                  GnomeCmdXferCallback on_completed_func, gpointer on_completed_data)
{
    XferData *xferData = g_new0 (XferData, 1);

    xferData->parent_window = parent_window;
    xferData->copyFlags = copyFlags;
    xferData->srcGFileList = srcGFileList;
    xferData->destGFileList = destGFileList;
    xferData->destGnomeCmdDir = to_dir;
    xferData->cur_file_name = nullptr;
    xferData->curFileNumber = 0;
    xferData->filesTotal = 0;
    xferData->on_completed_func = on_completed_func;
    xferData->on_completed_data = on_completed_data;
    xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
    xferData->thread = nullptr;
    xferData->error = nullptr;
    xferData->overwriteMode = overwriteMode;

    return xferData;
}


static void
gnome_cmd_transfer_gfiles (XferData *xferData);

static gboolean
gnome_cmd_copy_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer on_completed_data);

static gboolean
gnome_cmd_move_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer on_completed_data);


void finish_xfer(XferData *xferData, gboolean threaded, gboolean success)
{
    if (threaded)
        g_thread_join (xferData->thread);

    if (xferData->on_completed_func)
    {
        xferData->on_completed_func (success, xferData->on_completed_data);
    }

    if (xferData->destGnomeCmdDir)
    {
        gnome_cmd_dir_unref (xferData->destGnomeCmdDir);
        xferData->destGnomeCmdDir = nullptr;
    }

    if (threaded)
        g_mutex_clear(&xferData->mutex);

    if (xferData->cancellable)
        g_object_unref (xferData->cancellable);

    free_xfer_data (xferData);
}


inline gboolean gfile_is_parent_to_dir_or_equal (GFile *gFile, GnomeCmdDir *dir)
{
    auto dir_gFile = GNOME_CMD_FILE (dir)->get_file ();

    gboolean is_parent = g_file_has_parent (dir_gFile, gFile);

    gboolean are_equal = g_file_equal (gFile, dir_gFile);

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
        xferData->bytesTotal += diskUsage;
        xferData->filesTotal += numFiles;
    }
}


void
gnome_cmd_copy_gfiles_start (GtkWindow *parent_window,
                             GList *srcGFileGList,
                             GnomeCmdDir *destGnomeCmdDir,
                             gchar *destFileName,
                             GFileCopyFlags copyFlags,
                             GnomeCmdConfirmOverwriteMode overwriteMode,
                             GnomeCmdXferCallback on_completed_func,
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
            gnome_cmd_show_message (parent_window, _("Copying a directory into itself is a bad idea."), _("The whole operation was cancelled."));
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

    XferData *xferData = create_xfer_data (parent_window, copyFlags, srcGFileGList, nullptr,
                            destGnomeCmdDir, overwriteMode,
                            on_completed_func, on_completed_data);
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

    xferData->transfer_control = start_transfer_gui(
        xferData,
        parent_window,
        _("preparing…"),
        xferData->filesTotal,
        COPY,
        overwriteMode);

    g_set_object(&xferData->cancellable, transfer_get_cancellable(xferData->transfer_control));

    g_mutex_init(&xferData->mutex);

    //  start the transfer
    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_transfer_gfiles, xferData);
}

void
gnome_cmd_move_gfiles_start (GtkWindow *parent_window,
                             GList *srcGFileGList,
                             GnomeCmdDir *destGnomeCmdDir,
                             gchar *destFileName,
                             GFileCopyFlags copyFlags,
                             GnomeCmdConfirmOverwriteMode overwriteMode,
                             GnomeCmdXferCallback on_completed_func,
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
            gnome_cmd_show_message (parent_window, _("Moving a directory into itself is a bad idea."), _("The whole operation was cancelled."));
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

    XferData *xferData = create_xfer_data (parent_window, copyFlags, srcGFileGList, nullptr,
                            destGnomeCmdDir,
                            overwriteMode, on_completed_func, on_completed_data);
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

    xferData->transfer_control = start_transfer_gui(
        xferData,
        parent_window,
        _("preparing…"),
        xferData->filesTotal,
        MOVE,
        overwriteMode);

    g_set_object(&xferData->cancellable, transfer_get_cancellable(xferData->transfer_control));

    g_mutex_init(&xferData->mutex);

    //  start the transfer
    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_transfer_gfiles, xferData);
}


void
gnome_cmd_link_gfiles_start (GtkWindow *parent_window,
                             GList *srcGFileGList,
                             GnomeCmdDir *destGnomeCmdDir,
                             gchar *destFileName,
                             GFileCopyFlags copyFlags,
                             GnomeCmdConfirmOverwriteMode overwriteMode,
                             GnomeCmdXferCallback on_completed_func,
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
            gnome_cmd_show_message (parent_window, _("Moving a directory into itself is a bad idea."), _("The whole operation was cancelled."));
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

    XferData *xferData = create_xfer_data (parent_window, copyFlags, srcGFileGList, nullptr,
                            destGnomeCmdDir,
                            overwriteMode, on_completed_func, on_completed_data);
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

    xferData->transfer_control = start_transfer_gui(
        xferData,
        parent_window,
        _("preparing…"),
        xferData->filesTotal,
        LINK,
        overwriteMode);

    g_set_object(&xferData->cancellable, transfer_get_cancellable(xferData->transfer_control));

    g_mutex_init(&xferData->mutex);

    //  start the transfer
    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_transfer_gfiles, xferData);
}


void
gnome_cmd_tmp_download (GtkWindow *parent_window,
                        GList *srcGFileList,
                        GList *destGFileList,
                        GFileCopyFlags copyFlags,
                        GnomeCmdXferCallback on_completed_func,
                        gpointer on_completed_data)
{
    g_return_if_fail (srcGFileList != nullptr && srcGFileList->data != nullptr);
    g_return_if_fail (destGFileList != nullptr && destGFileList->data != nullptr);

    auto xferData = create_xfer_data (parent_window, copyFlags, srcGFileList, destGFileList,
                             nullptr, GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                             on_completed_func, on_completed_data);
    xferData->transferType = COPY;

    xferData->transfer_control = start_transfer_gui(
        xferData,
        parent_window,
        _("downloading to /tmp"),
        xferData->filesTotal,
        COPY,
        GNOME_CMD_CONFIRM_OVERWRITE_QUERY);

    g_set_object(&xferData->cancellable, transfer_get_cancellable(xferData->transfer_control));

    g_mutex_init(&xferData->mutex);

    xferData->thread = g_thread_new (NULL, (GThreadFunc) gnome_cmd_transfer_gfiles, xferData);
}


static void
gnome_cmd_transfer_gfiles (XferData *xferData)
{
    g_return_if_fail (xferData->srcGFileList != nullptr);
    g_return_if_fail (xferData->destGFileList != nullptr);

    if (g_list_length(xferData->srcGFileList) != g_list_length(xferData->destGFileList))
    {
        transfer_done(xferData->transfer_control);
        xferData->transfer_control = nullptr;
        return;
    }

    GError *error = nullptr;

    auto gFileDestListItem = xferData->destGFileList;

    for (auto gFileSrcListItem = xferData->srcGFileList; gFileSrcListItem; gFileSrcListItem = gFileSrcListItem->next,
                                                                           gFileDestListItem = gFileDestListItem->next)
    {
        auto srcGFile = (GFile *) gFileSrcListItem->data;
        auto destGFile = (GFile *) gFileDestListItem->data;

        if (xferData->cancellable && g_cancellable_is_cancelled (xferData->cancellable))
            break;

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
                    g_propagate_error(&(xferData->error), error);
                    xferData->problemSrcGFile = g_file_dup(srcGFile);
                    xferData->problemDestGFile = g_file_dup(destGFile);
                    xfer_progress_update(xferData);
                }
                break;
        }
    }

    transfer_done(xferData->transfer_control);
    xferData->transfer_control = nullptr;
}


/**
 * This function forces the thread for copying files and folders to wait until
 * the user clicked something on the popup gui which makes problem_action becomes != -1.
 */
static void xfer_progress_update (XferData *xferData)
{
    if (xferData->error)
    {
        xferData->problem_action = transfer_ask_handle_error(
            xferData->transfer_control,
            xferData->error,
            xferData->problemSrcGFile,
            xferData->problemDestGFile,
            xferData->problemMessage
        );

        g_clear_object (&xferData->problemSrcGFile);
        g_clear_object (&xferData->problemDestGFile);
        g_clear_error (&xferData->error);
        g_clear_pointer (&xferData->problemMessage, g_free);
    }
    else
    {
        transfer_progress(
            xferData->transfer_control,
            xferData->curFileNumber,
            xferData->curSrcFileName,
            xferData->filesTotal,
            xferData->fileSize,
            xferData->bytesCopiedFile,
            xferData->bytesTotal,
            xferData->bytesTotalTransferred
        );
    }
}


static void
update_transferred_data (goffset currentNumBytes,
                 goffset totalNumBytes,
                 gpointer xferDataPointer)
{
    auto xferData = (XferData *) xferDataPointer;
    xferData->bytesCopiedFile = currentNumBytes;
    xferData->fileSize = totalNumBytes;

    xfer_progress_update(xferData);
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
    if(!g_file_copy(srcGFile, destGFile, copyFlags, xferData->cancellable, update_transferred_data, xferDataPointer, &tmpError))
    {
        //ToDo: Remove partially copied file ?
        if (g_error_matches (tmpError, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_error_free(tmpError);
        }
        else
        {
            g_propagate_error(&(xferData->error), tmpError);
            xferData->problemSrcGFile = g_file_dup(srcGFile);
            xferData->problemDestGFile = g_file_dup(destGFile);
            xfer_progress_update(xferData);
        }
    }
    else
    {
        xferData->bytesCopiedFile = 0;
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
static gboolean
gnome_cmd_move_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer xferDataPointer)
{
    auto xferData = (XferData *) xferDataPointer;
    if (xferData->error)
        return false;
    if (xferData->cancellable && g_cancellable_is_cancelled (xferData->cancellable))
        return false;

    g_return_val_if_fail(G_IS_FILE(srcGFile), false);
    g_return_val_if_fail(G_IS_FILE(destGFile), false);

    GError *tmpError = nullptr;

    g_free(xferData->curSrcFileName);
    xferData->curSrcFileName = get_gfile_attribute_string(srcGFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    xferData->currentSrcFileType = g_file_query_file_type(srcGFile, G_FILE_QUERY_INFO_NONE, nullptr);

    if(!g_file_move(srcGFile, destGFile, copyFlags, nullptr, update_transferred_data, xferDataPointer, &tmpError))
    {
        if (g_error_matches (tmpError, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_error_free(tmpError);
            return false;
        }

        if(g_file_query_file_type(srcGFile, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY
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
                    g_propagate_error(&(xferData->error), tmpError);
                    xferData->problemMessage = g_strdup_printf(_("Source “%s” could not be deleted. Aborting!"), srcFileName);
                    xfer_progress_update(xferData);
                    return false;
                }

                auto srcGFileParent = g_file_get_parent(srcGFile);
                if (!srcGFileParent)
                {
                    auto srcFileName = g_file_get_basename(srcGFile);
                    g_propagate_error(&(xferData->error), tmpError);
                    xferData->problemMessage = g_strdup_printf(_("Source “%s” could not be deleted. Aborting!"), srcFileName);
                    xfer_progress_update(xferData);
                    return false;
                }

                GnomeCmdDir *gnomeCmdDirParent = nullptr;
                GnomeCmdCon *gnomeCmdCon = nullptr;
                auto srcUriSchema = g_file_get_uri_scheme(srcGFileParent);
                if (strcmp(srcUriSchema, "file") && (strcmp(srcUriSchema, "smb")))
                {
                    gnomeCmdCon = get_remote_con_for_gfile(gnome_cmd_con_list_get(), srcGFileParent);
                    if (gnomeCmdCon)
                    {
                        auto gFileParentUri = g_file_get_uri(srcGFileParent);
                        auto gUriParent = g_uri_parse(gFileParentUri, G_URI_FLAGS_NONE, nullptr);
                        auto gFileParentPathFromUri = g_uri_get_path(gUriParent);
                        gnomeCmdDirParent = gnome_cmd_dir_new (gnomeCmdCon, gnome_cmd_plain_path_new (gFileParentPathFromUri));
                        g_free(gFileParentUri);
                    }
                }
                else if (strcmp(srcUriSchema, "file"))
                {
                    auto gFileParentPath = g_file_get_path(srcGFileParent);
                    gnomeCmdCon = get_home_con();
                    gnomeCmdDirParent = gnome_cmd_dir_new (gnomeCmdCon, gnome_cmd_plain_path_new (gFileParentPath));
                    g_free(gFileParentPath);
                }

                auto gnomeCmdDir = gnome_cmd_dir_new_from_gfileinfo(gFileInfo, gnomeCmdDirParent);

                do_delete_files_for_move (xferData->parent_window, g_list_append(nullptr, gnomeCmdDir), false); // false -> do not show progress window

                g_object_unref(srcGFileParent);
                g_free(srcUriSchema);
            }
            else
            {
                if (!(xferData->cancellable && g_cancellable_is_cancelled (xferData->cancellable)))
                {
                    auto srcFileName = g_file_get_basename(srcGFile);
                    g_propagate_error(&(xferData->error), tmpError);
                    xferData->problemMessage = g_strdup_printf(_("Source “%s” could not be copied. Aborting!"), srcFileName);
                    xfer_progress_update(xferData);
                }
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
                tmpError = nullptr;
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
                        if(g_file_query_file_type(srcGFile, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY)
                            gnome_cmd_move_gfile_recursive(srcGFile, destGFile, (GFileCopyFlags) copyFlagsTemp, xferData);
                        else
                            g_file_move(srcGFile, destGFile, copyFlags, xferData->cancellable, update_transferred_data, xferDataPointer, &tmpError);
                        if (tmpError)
                        {
                            if (g_error_matches (tmpError, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                                g_error_free(tmpError);
                            else
                            {
                                g_warning("g_file_move error: %s\n", tmpError->message);
                                g_propagate_error(&(xferData->error), tmpError);
                                xferData->problemSrcGFile = g_file_dup(srcGFile);
                                xferData->problemDestGFile = g_file_dup(destGFile);
                                xfer_progress_update(xferData);
                            }
                            return false;
                        }
                        break;
                    case COPY_ERROR_ACTION_RENAME_ALL:
                        set_new_nonexisting_dest_gfile(srcGFile, &destGFile, xferData);
                        if(g_file_query_file_type(srcGFile, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY)
                            gnome_cmd_move_gfile_recursive(srcGFile, destGFile, (GFileCopyFlags) copyFlagsTemp, xferData);
                        else
                            g_file_move(srcGFile, destGFile, copyFlags, xferData->cancellable, update_transferred_data, xferDataPointer, &tmpError);
                        if (tmpError)
                        {
                            if (g_error_matches (tmpError, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                                g_error_free(tmpError);
                            else
                            {
                                g_warning("g_file_move error: %s\n", tmpError->message);
                                g_propagate_error(&(xferData->error), tmpError);
                                xferData->problemSrcGFile = g_file_dup(srcGFile);
                                xferData->problemDestGFile = g_file_dup(destGFile);
                                xfer_progress_update(xferData);
                            }
                            return false;
                        }
                        break;
                    case COPY_ERROR_ACTION_SKIP:
                        xferData->problem_action = COPY_ERROR_ACTION_NO_ACTION_YET;
                        break;
                    case COPY_ERROR_ACTION_SKIP_ALL:
                        return true;
                    // Copy into can only be selected in case of copying a directory into an
                    // already existing directory, so we are ignoring this here because we deal with
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
        xferData->bytesCopiedFile = 0;
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
static gboolean
gnome_cmd_copy_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer xferDataPointer)
{
    auto xferData = (XferData *) xferDataPointer;
    if (xferData->error)
        return false;
    if (xferData->cancellable && g_cancellable_is_cancelled (xferData->cancellable))
        return false;

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
                if (xferData->cancellable && g_cancellable_is_cancelled (xferData->cancellable))
                {
                    g_object_unref(gFileInfoChildFile);
                    g_file_enumerator_close (enumerator, nullptr, nullptr);
                    return false;
                }

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
                    if (copyFlags != G_FILE_COPY_OVERWRITE)
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
