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

#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-xfer-progress-win.h"

struct XferData
{
    GFileCopyFlags copyFlags;
    GnomeVFSXferOptions xferOptions;
    GnomeVFSAsyncHandle *handle;

    // Source and target GFile's. The first srcGFile should be transfered to the first destGFile and so on...
    GList *srcGFileList;
    GList *destGFileList;

    GnomeCmdDir *to_dir;
    GnomeCmdFileList *src_fl;
    GList *src_files;

    // Used for showing the progress
    GnomeCmdXferProgressWin *win;
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

    gboolean problem{FALSE};                 // signals to the main thread that the work thread is waiting for an answer on what to do
    gint problem_action{-1};                 // where the answer is delivered
    const gchar *problem_file_name{nullptr}; // the filename of the file that can't be deleted
    GThread *thread{nullptr};                // the work thread
    GMutex mutex{nullptr};                   // used to sync the main and worker thread
    GError *error{nullptr};                  // the cause that the file cant be deleted
};

void
gnome_cmd_xfer_start (GList *src_files,
                      GnomeCmdDir *to,
                      GnomeCmdFileList *src_fl,
                      gchar *dest_fn,
                      GnomeVFSXferOptions xferOptions,
                      GnomeVFSXferOverwriteMode xferOverwriteMode,
                      GtkSignalFunc on_completed_func,
                      gpointer on_completed_data);


void
gnome_cmd_xfer_gfiles_start (GList *src_uri_list,
                           GnomeCmdDir *to,
                           GnomeCmdFileList *src_fl,
                           GList *src_files,
                           gchar *dest_fn,
                           GnomeVFSXferOptions xferOptions,
                           GnomeVFSXferOverwriteMode xferOverwriteMode,
                           GtkSignalFunc on_completed_func,
                           gpointer on_completed_data);

void
gnome_cmd_xfer_tmp_download (GFile *srcGFile,
                             GFile *destGFile,
                             GFileCopyFlags copyFlags,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data);

void
gnome_cmd_xfer_tmp_download_multiple (XferData *xferData);

void
gnome_cmd_xfer_tmp_download (GFile *srcGFile,
                               GFile *destGFile,
                               GFileCopyFlags copyFlags,
                               gpointer on_completed_data);
