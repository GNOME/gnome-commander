/**
 * @file gnome-cmd-xfer.h
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

#pragma once

#include "gnome-cmd-types.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-xfer-progress-win.h"

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

struct XferData
{
    GFileCopyFlags copyFlags;
    GnomeCmdConfirmOverwriteMode overwriteMode;
    GnomeVFSXferOptions xferOptions;
    GnomeVFSAsyncHandle *handle;
    GnomeCmdTransferType transferType{COPY};

    // Source and target GFile's. The first srcGFile should be transfered to the first destGFile and so on...
    GList *srcGFileList;
    GList *destGFileList;

    GnomeCmdDir *destGnomeCmdDir;
    GnomeCmdFileList *src_fl;
    GList *src_files;

    // Used for showing the progress
    GnomeCmdXferProgressWin *win;
    gulong curFileNumber;
    gchar *curSrcFileName;
    gulong filesTotal;
    gboolean first_time;
    gchar *cur_file_name;

    guint64 fileSize;
    guint64 bytesCopiedFile;
    guint64 bytesTotal;
    guint64 bytesTotalTransferred{0};

    GFunc on_completed_func;
    gpointer on_completed_data;

    gboolean done;
    gboolean aborted;

    gboolean problem{FALSE};                 // signals to the main thread that the work thread is waiting for an answer on what to do
    COPY_ERROR_ACTION problem_action;        // action to take when an error occurs
    GFile *problemSrcGFile;
    GFile *problemDestGFile;
    GThread *thread{nullptr};                // the work thread
    GMutex mutex{nullptr};                   // used to sync the main and worker thread
    GError *error{nullptr};                  // the cause that the file cant be deleted
    GType currentSrcFileType;                // the file type of the file which is currently copied
};

void
gnome_cmd_copy_start (GList *src_files,
                      GnomeCmdDir *to,
                      GnomeCmdFileList *src_fl,
                      gchar *dest_fn,
                      GFileCopyFlags copyFlags,
                      GnomeCmdConfirmOverwriteMode overwriteMode,
                      GtkSignalFunc on_completed_func,
                      gpointer on_completed_data);

void
gnome_cmd_move_start (GList *src_files,
                      GnomeCmdDir *to,
                      GnomeCmdFileList *src_fl,
                      gchar *dest_fn,
                      GFileCopyFlags copyFlags,
                      GnomeCmdConfirmOverwriteMode overwriteMode,
                      GtkSignalFunc on_completed_func,
                      gpointer on_completed_data);

void
gnome_cmd_copy_gfiles_start (GList *src_uri_list,
                           GnomeCmdDir *to,
                           GnomeCmdFileList *src_fl,
                           GList *src_files,
                           gchar *dest_fn,
                           GFileCopyFlags copyFlags,
                           GnomeCmdConfirmOverwriteMode overwriteMode,
                           GtkSignalFunc on_completed_func,
                           gpointer on_completed_data);

void
gnome_cmd_move_gfiles_start (GList *src_uri_list,
                           GnomeCmdDir *to,
                           GnomeCmdFileList *src_fl,
                           GList *src_files,
                           gchar *dest_fn,
                           GFileCopyFlags copyFlags,
                           GnomeCmdConfirmOverwriteMode overwriteMode,
                           GtkSignalFunc on_completed_func,
                           gpointer on_completed_data);

void
gnome_cmd_link_gfiles_start (GList *src_uri_list,
                           GnomeCmdDir *to,
                           GnomeCmdFileList *src_fl,
                           GList *src_files,
                           gchar *dest_fn,
                           GFileCopyFlags copyFlags,
                           GnomeCmdConfirmOverwriteMode overwriteMode,
                           GtkSignalFunc on_completed_func,
                           gpointer on_completed_data);

void
gnome_cmd_tmp_download (GFile *srcGFile,
                             GFile *destGFile,
                             GFileCopyFlags copyFlags,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data);

void
gnome_cmd_transfer_gfiles (XferData *xferData);

gboolean
gnome_cmd_copy_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer on_completed_data);

gboolean
gnome_cmd_move_gfile_recursive (GFile *srcGFile,
                                GFile *destGFile,
                                GFileCopyFlags copyFlags,
                                gpointer on_completed_data);
