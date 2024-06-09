/**
 * @file dirlist.cc
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

#include "gnome-cmd-includes.h"
#include "dirlist.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-data.h"
#include "utils.h"

using namespace std;


#define FILES_PER_NOTIFICATION 50
#define LIST_PRIORITY 0
#define FILES_PER_UPDATE 50
#define DIR_PBAR_MAX 50


struct GnomeCmdDirList
{
    GnomeCmdDir *dir;

    GList *gFileInfoList;
    gint list_counter;

    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *pbar;

    GnomeCmdDirListDone list_done;
};


static void async_list (GnomeCmdDirList *dirlist);
static void sync_list  (GnomeCmdDirList *dirlist);
static void enumerate_children_callback(GObject *direnum, GAsyncResult *result, gpointer user_data);
static void dirlist_cancel (GnomeCmdDirList *dirlist);


static void on_dir_list_cancel (GtkButton *btn, GnomeCmdDirList *dirlist)
{
    DEBUG('l', "on_dir_list_cancel\n");
    dirlist_cancel (dirlist);

    gtk_window_destroy (GTK_WINDOW (dirlist->dialog));
    dirlist->dialog = nullptr;
}


static void create_list_progress_dialog (GtkWindow *parent_window, GnomeCmdDirList *dir)
{
    dir->dialog = gnome_cmd_dialog_new (parent_window, nullptr);
    g_object_ref (dir->dialog);

    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dir->dialog),
        _("_Cancel"),
        G_CALLBACK (on_dir_list_cancel), dir);

    GtkWidget *vbox = create_vbox (dir->dialog, FALSE, 12);

    dir->label = create_label (dir->dialog, _("Waiting for file list"));

    dir->pbar = create_progress_bar (dir->dialog);
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (dir->pbar), FALSE);
    gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (dir->pbar), 1.0 / (gdouble) DIR_PBAR_MAX);

    gtk_box_append (GTK_BOX (vbox), dir->label);
    gtk_box_append (GTK_BOX (vbox), dir->pbar);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dir->dialog), vbox);

    gtk_widget_show_all (dir->dialog);
}


static gboolean update_list_progress (GnomeCmdDirList *dirlist)
{
    DEBUG ('l', "Checking list progress...\n");

    if (dirlist->dialog != nullptr)
    {
        gchar *msg = g_strdup_printf (ngettext ("%d file listed", "%d files listed", dirlist->list_counter), dirlist->list_counter);
        gtk_label_set_text (GTK_LABEL (dirlist->label), msg);
        gtk_progress_bar_pulse (GTK_PROGRESS_BAR (dirlist->pbar));
        DEBUG('l', "%s\n", msg);
        g_free (msg);
        return TRUE;
    }

    g_free (dirlist);

    return FALSE;
}


static void get_gfileenumerator_async_callback (GObject *gFileObject, GAsyncResult *result, gpointer user_data)
{
    auto dirlist = static_cast<GnomeCmdDirList*>(user_data);
    auto gFile = (GFile*) gFileObject;
    GError *error = nullptr;

    auto gFileEnumerator = g_file_enumerate_children_finish (gFile, result, &error);
    if (error)
    {
        g_warning("g_file_enumerate_children_finish error: %s\n", error->message);
        g_error_free(error);
        return;
    }

    g_file_enumerator_next_files_async(gFileEnumerator,
                    FILES_PER_UPDATE,
                    G_PRIORITY_LOW,
                    nullptr,
                    enumerate_children_callback,
                    dirlist);

    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_list_progress, dirlist);
}


static void async_list (GnomeCmdDirList *dirlist)
{
    dirlist->gFileInfoList = nullptr;

    auto gFile = GNOME_CMD_FILE (dirlist->dir)->get_file();
    gchar *uri_str = g_file_get_uri(gFile);
    DEBUG('l', "async_list: %s\n", uri_str);
    g_free (uri_str);

    if (gnome_cmd_dir_get_connection(dirlist->dir)->is_local)
    {
        GError *error = nullptr;
        GFileEnumerator *enumerator = g_file_enumerate_children (gFile,
                                                                 "*",
                                                                 G_FILE_QUERY_INFO_NONE,
                                                                 nullptr,
                                                                 &error);
        if (error)
        {
            g_critical("Unable to enumerate children, error: %s", error->message);
            return;
        }

        g_file_enumerator_next_files_async (enumerator,
                                            FILES_PER_UPDATE,
                                            G_PRIORITY_LOW,
                                            nullptr,
                                            enumerate_children_callback,
                                            dirlist);

        g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_list_progress, dirlist);
    }
    else
    {
        g_file_enumerate_children_async (gFile,
                                         "*",
                                         G_FILE_QUERY_INFO_NONE,
                                         G_PRIORITY_DEFAULT,
                                         nullptr,
                                         get_gfileenumerator_async_callback,
                                         dirlist);
    }
}

static void sync_list (GnomeCmdDirList *dirlist)
{
    g_return_if_fail(dirlist != nullptr);

    GError *error = nullptr;

    gchar *uri_str = GNOME_CMD_FILE (dirlist->dir)->get_uri_str();
    DEBUG('l', "sync_list: %s\n", uri_str);
    g_free (uri_str);

    dirlist->gFileInfoList = nullptr;

    auto gFile = GNOME_CMD_FILE (dirlist->dir)->get_file();

    auto gFileEnumerator = g_file_enumerate_children (gFile,
                            "*",
                            G_FILE_QUERY_INFO_NONE,
                            nullptr,
                            &error);
    if (error)
    {
        g_critical("sync_list: Unable to enumerate children, error: %s", error->message);
        dirlist->list_done (dirlist->dir, false, nullptr, error);
        g_error_free (error);
        return;
    }

    GFileInfo *gFileInfoTmp = nullptr;
    do
    {
        gFileInfoTmp = g_file_enumerator_next_file(gFileEnumerator, nullptr, &error);
        if(error)
        {
            g_critical("sync_list: Unable to enumerate next file, error: %s", error->message);
            break;
        }
        if (gFileInfoTmp)
        {
            dirlist->gFileInfoList = g_list_append(dirlist->gFileInfoList, gFileInfoTmp);
        }
    }
    while (gFileInfoTmp && !error);

    g_file_enumerator_close (gFileEnumerator, nullptr, nullptr);
    g_object_unref(gFileEnumerator);

    dirlist->list_done (dirlist->dir, error == nullptr, dirlist->gFileInfoList, error);
    if (error)
        g_error_free(error);
    g_free (dirlist);
}

static void enumerate_children_callback(GObject *direnum, GAsyncResult *result, gpointer user_data)
{
    auto gFileEnumerator = G_FILE_ENUMERATOR(direnum);
    auto dirlist = static_cast<GnomeCmdDirList*>(user_data);
    GError *error = nullptr;

    GList *gFileInfosList = g_file_enumerator_next_files_finish(gFileEnumerator, result, &error);

    if (error)
    {
        g_critical("Unable to iterate the g_file_enumerator, error: %s", error->message);
        dirlist->list_done (dirlist->dir, false, dirlist->gFileInfoList, error);
        g_file_enumerator_close(gFileEnumerator, nullptr, nullptr);
        g_object_unref(direnum);
        g_error_free(error);
        if (dirlist->dialog)
        {
            gtk_window_destroy (GTK_WINDOW (dirlist->dialog));
            dirlist->dialog = nullptr;
        }
        return;
    }
    else if(gFileInfosList == nullptr)
    {
        /* DONE */
        DEBUG('l', "All files listed, calling list_done func\n");
        dirlist->list_done (dirlist->dir, true, dirlist->gFileInfoList, nullptr);
        g_file_enumerator_close(gFileEnumerator, nullptr, nullptr);
        g_object_unref(direnum);
        if (dirlist->dialog)
        {
            gtk_window_destroy (GTK_WINDOW (dirlist->dialog));
            dirlist->dialog = nullptr;
        }
        return;
    }
    else
    {
        dirlist->gFileInfoList = g_list_concat (dirlist->gFileInfoList, g_list_copy (gFileInfosList));
        dirlist->list_counter += FILES_PER_UPDATE;
        g_file_enumerator_next_files_async(gFileEnumerator,
                        FILES_PER_UPDATE,
                        G_PRIORITY_LOW,
                        nullptr,
                        enumerate_children_callback,
                        dirlist);
    }
    g_list_free(gFileInfosList);
}


void dirlist_list (GtkWindow *parent_window, GnomeCmdDir *dir, gboolean visualProgress, GnomeCmdDirListDone list_done)
{
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    GnomeCmdDirList *dirlist = g_new0 (GnomeCmdDirList, 1);

    dirlist->dir = dir;
    dirlist->gFileInfoList = nullptr;
    dirlist->list_counter = 0;
    dirlist->list_done = list_done;

    if (visualProgress)
    {
        create_list_progress_dialog (parent_window, dirlist);
        async_list (dirlist);
    }
    else
    {
        sync_list (dirlist);
    }
}


static void dirlist_cancel (GnomeCmdDirList *dirlist)
{
    // ToDo: Add a cancel-trigger for the async dir listing
    DEBUG('l', "Cancel dir-listing not implemented yet...\n");
}


GList* sync_dir_list (const gchar *absDirPath)
{
    g_return_val_if_fail (absDirPath != nullptr, nullptr);

    auto gFile = g_file_new_for_path(absDirPath);

    GList *gFileInfoList = sync_dir_list (gFile, nullptr);

    g_object_unref(gFile);

    return gFileInfoList;
}

GList* sync_dir_list (GFile *gFile, GCancellable* cancellable)
{
    g_return_val_if_fail (gFile != nullptr, nullptr);

    gchar *pathOrUri = g_file_get_path (gFile);
    if (!pathOrUri)
        pathOrUri = g_file_get_uri (gFile);

    DEBUG('l', "sync_dir_list: %s\n", pathOrUri);

    if (!g_file_query_exists(gFile, cancellable))
    {
        g_warning("sync_dir_list error: \"%s\" does not exist", pathOrUri);
        g_free (pathOrUri);
        return nullptr;
    }

    g_free (pathOrUri);

    GError *error = nullptr;
    GList *gFileInfoList = nullptr;

    auto gFileEnumerator = g_file_enumerate_children (gFile,
                            "*",
                            G_FILE_QUERY_INFO_NONE,
                            cancellable,
                            &error);
    if(error)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_critical("sync_dir_list: Unable to enumerate children, error: %s", error->message);
        g_error_free(error);
        return nullptr;
    }

    GFileInfo *gFileInfoTmp = nullptr;
    do
    {
        gFileInfoTmp = g_file_enumerator_next_file(gFileEnumerator, cancellable, &error);
        if(error)
        {
            if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_critical("sync_dir_list: Unable to enumerate next file, error: %s", error->message);
            break;
        }
        if (gFileInfoTmp)
        {
            gFileInfoList = g_list_append(gFileInfoList, gFileInfoTmp);
        }
    }
    while (gFileInfoTmp && !error);

    if (error)
        g_error_free(error);

    g_file_enumerator_close (gFileEnumerator, nullptr, nullptr);
    g_object_unref(gFileEnumerator);
    return gFileInfoList;
}
