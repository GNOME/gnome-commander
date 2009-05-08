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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-delete-dialog.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"

using namespace std;


struct DeleteData
{
    GtkWidget *progbar;
    GtkWidget *proglabel;
    GtkWidget *progwin;

    gboolean problem;             // signals to the main thread that the work thread is waiting for an answer on what to do
    gint problem_action;          // where the answer is delivered
    gchar *problem_file;          // the filename of the file that can't be deleted
    GnomeVFSResult vfs_status;    // the cause that the file cant be deleted
    GThread *thread;              // the work thread
    GList *files;                 // the files that should be deleted
    gboolean stop;                // tells the work thread to stop working
    gboolean delete_done;         // tells the main thread that the work thread is done
    gchar *msg;                   // a message descriping the current status of the delete operation
    gfloat progress;              // a float values between 0 and 1 representing the progress of the whole operation
    GMutex *mutex;                // used to sync the main and worker thread
};


inline void cleanup (DeleteData *data)
{
    gnome_cmd_file_list_free (data->files);
    g_free (data);
}


static gint delete_progress_callback (GnomeVFSXferProgressInfo *info, DeleteData *data)
{
    gint ret = 0;

    g_mutex_lock (data->mutex);

    if (info->status == GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR)
    {
        data->vfs_status = info->vfs_status;
        data->problem_file = str_uri_basename(info->source_name);
        data->problem = TRUE;

        g_mutex_unlock (data->mutex);
        while (data->problem_action == -1)
            g_thread_yield ();
        g_mutex_lock (data->mutex);
        ret = data->problem_action;
        data->problem_action = -1;
        g_free (data->problem_file);
        data->problem_file = NULL;
        data->vfs_status = GNOME_VFS_OK;
    }
    else
        if (info->status == GNOME_VFS_XFER_PROGRESS_STATUS_OK)
        {
            if (info->file_index >= 0 && info->files_total > 0)
            {
                gfloat f = (gfloat)info->file_index/(gfloat)info->files_total;
                g_free (data->msg);
                data->msg = g_strdup_printf (ngettext("Deleted %ld of %ld file",
                                                      "Deleted %ld of %ld files",
                                                      info->files_total),
                                             info->file_index, info->files_total);
                if (f < 0.001f) f = 0.001f;
                if (f > 0.999f) f = 0.999f;
                data->progress = f;
            }

            ret = !data->stop;
        }
        else
            data->vfs_status = info->vfs_status;

    g_mutex_unlock (data->mutex);

    return ret;
}


static void on_cancel (GtkButton *btn, DeleteData *data)
{
    data->stop = TRUE;
    gtk_widget_set_sensitive (GTK_WIDGET (data->progwin), FALSE);
}


static gboolean on_progwin_destroy (GtkWidget *win, DeleteData *data)
{
    data->stop = TRUE;

    return FALSE;
}


inline void create_delete_progress_win (DeleteData *data)
{
    GtkWidget *vbox;
    GtkWidget *bbox;
    GtkWidget *button;

    data->progwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (data->progwin), _("Deleting..."));
    gtk_window_set_policy (GTK_WINDOW (data->progwin), FALSE, FALSE, FALSE);
    gtk_window_set_position (GTK_WINDOW (data->progwin), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request (GTK_WIDGET (data->progwin), 300, -1);
    gtk_signal_connect (GTK_OBJECT (data->progwin), "destroy-event", (GtkSignalFunc) on_progwin_destroy, data);

    vbox = create_vbox (data->progwin, FALSE, 6);
    gtk_container_add (GTK_CONTAINER (data->progwin), vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    data->proglabel = create_label (data->progwin, "");
    gtk_container_add (GTK_CONTAINER (vbox), data->proglabel);

    data->progbar = create_progress_bar (data->progwin);
    gtk_container_add (GTK_CONTAINER (vbox), data->progbar);

    bbox = create_hbuttonbox (data->progwin);
    gtk_container_add (GTK_CONTAINER (vbox), bbox);

    button = create_stock_button_with_data (data->progwin, GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_cancel), data);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    gtk_widget_ref (data->progwin);
    gtk_widget_show (data->progwin);
}


static void perform_delete_operation (DeleteData *data)
{
    GList *uri_list = NULL;

    // go through all files and add the uri of the appropriate ones to a list
    for (GList *i=data->files; i; i=i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        if (strcmp(f->info->name, "..") == 0 || strcmp(f->info->name, ".") == 0)
            continue;

        GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);
        if (!uri) continue;

        uri_list = g_list_append (uri_list, gnome_vfs_uri_ref (uri));
    }

    if (uri_list)
    {
        gnome_vfs_xfer_delete_list (uri_list,
                                    GNOME_VFS_XFER_ERROR_MODE_QUERY,
                                    GNOME_VFS_XFER_DEFAULT,
                                    (GnomeVFSXferProgressCallback) delete_progress_callback,
                                    data);

        g_list_foreach (uri_list, (GFunc) gnome_vfs_uri_unref, NULL);
        g_list_free (uri_list);
    }

    data->delete_done = TRUE;
}


static gboolean update_delete_status_widgets (DeleteData *data)
{
    g_mutex_lock (data->mutex);

    gtk_label_set_text (GTK_LABEL (data->proglabel), data->msg);
    gtk_progress_set_percentage (GTK_PROGRESS (data->progbar), data->progress);

    if (data->problem)
    {
        const gchar *error = gnome_vfs_result_to_string (data->vfs_status);
        gchar *msg = g_strdup_printf (_("Error while deleting \"%s\"\n\n%s"), data->problem_file, error);

        data->problem_action = run_simple_dialog (
            GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_ERROR, msg, _("Delete problem"),
            -1, _("Abort"), _("Retry"), _("Skip"), NULL);
        g_free (msg);

        data->problem = FALSE;
    }

    g_mutex_unlock (data->mutex);


    if (data->delete_done)
    {
        if (data->vfs_status != GNOME_VFS_OK)
            create_error_dialog (gnome_vfs_result_to_string (data->vfs_status));

        if (data->files)
            for (GList *tmp = data->files; tmp; tmp = tmp->next)
            {
                GnomeCmdFile *f = GNOME_CMD_FILE (tmp->data);
                GnomeVFSURI *uri = gnome_cmd_file_get_uri (f);

                if (!gnome_vfs_uri_exists (uri))
                    gnome_cmd_file_is_deleted (f);
            }

        gtk_widget_destroy (data->progwin);

        cleanup (data);

        return FALSE;  // Returning FALSE here stops the timeout callbacks
    }

    return TRUE;
}


inline void do_delete (DeleteData *data)
{
    data->mutex = g_mutex_new ();
    data->delete_done = FALSE;
    data->vfs_status = GNOME_VFS_OK;
    data->problem_action = -1;
    create_delete_progress_win (data);

    data->thread = g_thread_create ((GThreadFunc) perform_delete_operation, data, FALSE, NULL);
    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_delete_status_widgets, data);
}


void gnome_cmd_delete_dialog_show (GList *files)
{
    g_return_if_fail (files != NULL);

    gint response = 1;

    if (gnome_cmd_data.confirm_delete)
    {
        gchar *msg = NULL;

        gint n_files = g_list_length (files);

        if (n_files == 1)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) g_list_nth_data (files, 0);

            if (strcmp (f->info->name, "..") == 0)
                return;

            gchar *fname = get_utf8 (f->info->name);
            msg = g_strdup_printf (_("Do you want to delete \"%s\"?"), fname);
            g_free (fname);
        }
        else
            msg = g_strdup_printf (ngettext("Do you want to delete the selected file?",
                                            "Do you want to delete the %d selected files?",
                                            n_files),
                                   n_files);

        response = run_simple_dialog (GTK_WIDGET (main_win), FALSE,
                                      GTK_MESSAGE_QUESTION, msg, _("Delete"),
                                      1, _("Cancel"), _("OK"), NULL);

        g_free (msg);
    }

    if (response != 1)
        return;

    DeleteData *data = g_new0 (DeleteData, 1);

    data->files = gnome_cmd_file_list_copy (files);
    // data->stop = FALSE;
    // data->problem = FALSE;
    // data->delete_done = FALSE;
    // data->mutex = NULL;
    // data->msg = NULL;

    do_delete (data);
}
