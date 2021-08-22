/** 
 * @file gnome-cmd-delete-dialog.h
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

struct DeleteData
{
    GtkWidget *progbar;
    GtkWidget *proglabel;
    GtkWidget *progwin;

    gboolean problem{FALSE};              // signals to the main thread that the work thread is waiting for an answer on what to do
    gint problem_action;                  // where the answer is delivered
    const gchar *problemFileName;         // the filename of the file that can't be deleted
    GError *error{nullptr};               // the cause that the file cant be deleted
    GThread *thread{nullptr};             // the work thread
    GList *gnomeCmdFiles{nullptr};        // the GnomeCmdFiles that should be deleted (can be folders, too)
    GList *deletedGnomeCmdFiles{nullptr}; // this is the real list of deleted files (can be different from the list above)
    gboolean stop{FALSE};                 // tells the work thread to stop working
    gboolean deleteDone{FALSE};           // tells the main thread that the work thread is done
    gchar *msg{nullptr};                  // a message descriping the current status of the delete operation
    gfloat progress{0};                   // a float values between 0 and 1 representing the progress of the whole operation
    GMutex mutex{nullptr};                // used to sync the main and worker thread
    guint64 itemsDeleted{0};              // items deleted in the current run
    guint64 itemsTotal{0};                // total number of items which should be deleted
};

void do_delete (DeleteData *deleteData);

void gnome_cmd_delete_dialog_show (GList *files);
