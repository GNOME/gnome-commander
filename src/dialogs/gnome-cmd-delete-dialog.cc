/**
 * @file gnome-cmd-delete-dialog.cc
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"
#include "dialogs/gnome-cmd-delete-dialog.h"

using namespace std;

#define DELETE_NONEMPTY_CANCEL    0
#define DELETE_NONEMPTY_SKIP      1
#define DELETE_NONEMPTY_DELETEALL 2
#define DELETE_NONEMPTY_DELETE    3

#define DELETE_ERROR_ACTION_ABORT 0
#define DELETE_ERROR_ACTION_RETRY 1
#define DELETE_ERROR_ACTION_SKIP  2

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


inline void cleanup (DeleteData *deleteData)
{
    gnome_cmd_file_list_free (deleteData->gnomeCmdFiles);
    gnome_cmd_file_list_free (deleteData->deletedGnomeCmdFiles);
    g_free (deleteData);
}


static void delete_progress_update (DeleteData *deleteData)
{
    g_mutex_lock (&deleteData->mutex);

    if (deleteData->error)
    {
        deleteData->problem = TRUE;

        g_mutex_unlock (&deleteData->mutex);
        while (deleteData->problem_action == -1)
            g_thread_yield ();
        g_mutex_lock (&deleteData->mutex);
        deleteData->problemFileName = nullptr;
        g_clear_error (&(deleteData->error));
    }

    if (deleteData->itemsDeleted > 0)
    {
        gfloat f = (gfloat)deleteData->itemsDeleted/(gfloat)deleteData->itemsTotal;
        g_free (deleteData->msg);
        deleteData->msg = g_strdup_printf (ngettext("Deleted %lu of %lu file",
                                              "Deleted %lu of %lu files",
                                              deleteData->itemsTotal),
                                     deleteData->itemsDeleted, deleteData->itemsTotal);
        if (f < 0.001f) f = 0.001f;
        if (f > 0.999f) f = 0.999f;
        deleteData->progress = f;
    }

    g_mutex_unlock (&deleteData->mutex);
}


static void on_cancel (GtkButton *btn, DeleteData *deleteData)
{
    deleteData->stop = TRUE;
    gtk_widget_set_sensitive (GTK_WIDGET (deleteData->progwin), FALSE);
}


static gboolean on_progwin_destroy (GtkWidget *win, DeleteData *deleteData)
{
    deleteData->stop = TRUE;

    return FALSE;
}


inline void create_delete_progress_win (DeleteData *deleteData)
{
    GtkWidget *vbox;
    GtkWidget *bbox;
    GtkWidget *button;

    deleteData->progwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (deleteData->progwin), _("Deleting…"));
    gtk_window_set_policy (GTK_WINDOW (deleteData->progwin), FALSE, FALSE, FALSE);
    gtk_window_set_position (GTK_WINDOW (deleteData->progwin), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request (GTK_WIDGET (deleteData->progwin), 300, -1);
    g_signal_connect (deleteData->progwin, "destroy-event", G_CALLBACK (on_progwin_destroy), deleteData);

    vbox = create_vbox (deleteData->progwin, FALSE, 6);
    gtk_container_add (GTK_CONTAINER (deleteData->progwin), vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    deleteData->proglabel = create_label (deleteData->progwin, "");
    gtk_container_add (GTK_CONTAINER (vbox), deleteData->proglabel);

    deleteData->progbar = create_progress_bar (deleteData->progwin);
    gtk_container_add (GTK_CONTAINER (vbox), deleteData->progbar);

    bbox = create_hbuttonbox (deleteData->progwin);
    gtk_container_add (GTK_CONTAINER (vbox), bbox);

    button = create_stock_button_with_data (deleteData->progwin, GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_cancel), deleteData);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (bbox), button);

    g_object_ref (deleteData->progwin);
    gtk_widget_show (deleteData->progwin);
}

/**
 * This function recursively removes files of a given GnomeCmdFile list and stores
 * possible errors or the progress information in the deleteData object.
 */
static gboolean perform_delete_operation_r(DeleteData *deleteData, GList *gnomeCmdFileList)
{
    for (GList *gCmdFileListItem = gnomeCmdFileList; gCmdFileListItem; gCmdFileListItem = gCmdFileListItem->next)
    {
        if (deleteData->stop)
        {
            return FALSE;
        }

        auto gnomeCmdFile = (GnomeCmdFile *) gCmdFileListItem->data;

        g_return_val_if_fail (GNOME_CMD_IS_FILE(gnomeCmdFile), FALSE);
        g_return_val_if_fail (G_IS_FILE_INFO(gnomeCmdFile->gFileInfo), FALSE);

        auto filenameTmp = g_file_info_get_display_name(gnomeCmdFile->gFileInfo);

        if (gnomeCmdFile->is_dotdot || strcmp(filenameTmp, ".") == 0)
            continue;

        GError *tmpError = nullptr;
        auto gFile = gnomeCmdFile->gFile;

        guint64 numFiles = 0;
        guint64 numDirs = 0;

        if (!g_file_measure_disk_usage (gFile,
                G_FILE_MEASURE_NONE,
                nullptr, nullptr, nullptr, nullptr,
                &numDirs,
                &numFiles,
                &tmpError))
        {
                g_warning ("Failed to measure disk usage of %s: %s", g_file_peek_path (gFile), tmpError->message);
                g_propagate_error (&(deleteData->error), tmpError);
                deleteData->problemFileName = g_file_peek_path(gFile);
                delete_progress_update(deleteData);
                return FALSE;
        }

        if ((numDirs == 1 && numFiles == 0) // Empty directory
            || (get_gfile_attribute_boolean(gFile, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK)) //We want delete symlinks!
            || (get_gfile_attribute_uint32(gFile, G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)) // Not a directory
        {
            // DELETE IT!
            if (!g_file_delete (gFile, nullptr, &tmpError) &&
                !g_error_matches (tmpError, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            {
                g_warning ("Failed to delete %s: %s", g_file_peek_path (gFile), tmpError->message);
                g_propagate_error (&(deleteData->error), tmpError);
                deleteData->problemFileName = g_file_peek_path(gFile);
                delete_progress_update(deleteData);
                if (deleteData->stop)
                    return FALSE;
            }
            else
            {
                deleteData->deletedGnomeCmdFiles = g_list_append(deleteData->deletedGnomeCmdFiles, gnomeCmdFile);
                deleteData->itemsDeleted++;
                delete_progress_update(deleteData);
                if (deleteData->stop)
                    return FALSE;
            }
        }
        else
        {
            gboolean deleted = TRUE;
            gboolean dirIsEmpty = TRUE;

            auto gnomeCmdDir = gnome_cmd_dir_ref (GNOME_CMD_DIR (gnomeCmdFile));
            gnome_cmd_dir_list_files (gnomeCmdDir, FALSE);
            for (GList *subFolderItem = gnome_cmd_dir_get_files (gnomeCmdDir); subFolderItem; subFolderItem = subFolderItem->next)
            {
                // retValue can be set below within this for-loop
                if (!deleted && deleteData->problem_action == DELETE_ERROR_ACTION_ABORT)
                {
                    gnome_cmd_dir_unref (gnomeCmdDir);
                    return FALSE;
                }
                if (!deleted && deleteData->problem_action == DELETE_ERROR_ACTION_RETRY)
                {
                    subFolderItem = subFolderItem->prev ? subFolderItem->prev : subFolderItem;
                    deleteData->problem_action = -1;
                }
                if (!deleted && deleteData->problem_action == DELETE_ERROR_ACTION_SKIP)
                {
                    // do nothing, just go on
                    deleteData->problem_action = -1;
                    dirIsEmpty = FALSE;
                }

                auto subGnomeCmdFile = (GnomeCmdFile *) subFolderItem->data;

                if (!subGnomeCmdFile->is_dotdot)
                {
                    GList *subGnomeCmdFileList = nullptr;

                    subGnomeCmdFileList = g_list_append(subGnomeCmdFileList, subGnomeCmdFile);
                    deleted = perform_delete_operation_r (deleteData, subGnomeCmdFileList);
                    g_list_free(subGnomeCmdFileList);
                }
            }
            gnome_cmd_dir_unref (gnomeCmdDir);

            if (dirIsEmpty)
            {
                // Now remove the directory itself, if it is finally empty
                GList *directory = nullptr;
                directory = g_list_append(directory, gnomeCmdFile);
                perform_delete_operation_r (deleteData, directory);
                g_list_free(directory);
            }
        }
    }
    return TRUE;
}


static void perform_delete_operation (DeleteData *deleteData)
{
    perform_delete_operation_r(deleteData, deleteData->gnomeCmdFiles);

    deleteData->deleteDone = TRUE;
}


static gboolean update_delete_status_widgets (DeleteData *deleteData)
{
    g_mutex_lock (&deleteData->mutex);

    gtk_label_set_text (GTK_LABEL (deleteData->proglabel), deleteData->msg);
    gtk_progress_set_percentage (GTK_PROGRESS (deleteData->progbar), deleteData->progress);

    if (deleteData->problem)
    {
        gchar *msg = g_strdup_printf (_("Error while deleting “%s”\n\n%s"),
                                        deleteData->problemFileName,
                                        deleteData->error->message);

        deleteData->problem_action = run_simple_dialog (
            *main_win, TRUE, GTK_MESSAGE_ERROR, msg, _("Delete problem"),
            -1, _("Abort"), _("Retry"), _("Skip"), NULL);

        g_free (msg);

        deleteData->problem = FALSE;
    }

    g_mutex_unlock (&deleteData->mutex);


    if (deleteData->deleteDone)
    {
        if (deleteData->error)
            gnome_cmd_show_message (*main_win, deleteData->error->message);

        if (deleteData->deletedGnomeCmdFiles)
        {
            for (GList *i = deleteData->deletedGnomeCmdFiles; i; i = i->next)
            {
                GnomeCmdFile *f = GNOME_CMD_FILE (i->data);
                auto *gFile = f->get_gfile();

                if (!g_file_query_exists (gFile, nullptr))
                    f->is_deleted();
            }
        }

        gtk_widget_destroy (deleteData->progwin);

        cleanup (deleteData);

        return FALSE;  // returning FALSE here stops the timeout callbacks
    }

    return TRUE;
}


inline void do_delete (DeleteData *deleteData)
{
    g_return_if_fail(GNOME_CMD_IS_FILE(deleteData->gnomeCmdFiles->data));

    g_mutex_init(&deleteData->mutex);
    deleteData->deleteDone = FALSE;
    deleteData->error = nullptr;
    deleteData->problem_action = -1;
    deleteData->itemsDeleted = 0;
    deleteData->deletedGnomeCmdFiles = nullptr;

    for(auto fileListItem = deleteData->gnomeCmdFiles; fileListItem; fileListItem = fileListItem->next)
    {
        guint64 num_files = 0;
        guint64 num_dirs = 0;
        auto gFile = GNOME_CMD_FILE(fileListItem->data)->gFile;
        g_return_if_fail(G_IS_FILE(gFile));

        g_file_measure_disk_usage (gFile,
           G_FILE_MEASURE_NONE,
           nullptr, nullptr, nullptr, nullptr,
           &num_dirs,
           &num_files,
           nullptr);
        deleteData->itemsTotal += num_files + num_dirs;
    }

    create_delete_progress_win (deleteData);

    deleteData->thread = g_thread_new (NULL, (GThreadFunc) perform_delete_operation, deleteData);
    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_delete_status_widgets, deleteData);
    g_mutex_clear(&deleteData->mutex);
}


/**
 * Remove a directory from the list of files to be deleted.
 * This happens as of user interaction.
 * The returned list has to be free'ed by g_list_free.
 */
static GList *remove_items_from_list_to_be_deleted(GList *files)
{
    if (!gnome_cmd_data.options.confirm_delete)
    {
        return files;
    };

    auto itemsToDelete = g_list_copy(files);

    gint dirCount = 0;
    gint guiResponse = -1;
    for (auto file = files; file; file = file->next)
    {
        auto gnomeCmdFile = (GnomeCmdFile*) file->data;
        if (gnomeCmdFile->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        {
            GError *error;
            error = nullptr;
            guint64 num_dirs;
            guint64 num_files;

            g_file_measure_disk_usage (gnomeCmdFile->gFile,
                       G_FILE_MEASURE_NONE,
                       nullptr, nullptr, nullptr, nullptr,
                       &num_dirs,
                       &num_files,
                       &error);

            if (error)
            {
                g_message ("remove_items_from_list_to_be_deleted: g_file_measure_disk_usage failed: %s", error->message);

                gchar *msg = g_strdup_printf (_("Error while deleting: \n\n%s"), error->message);

                run_simple_dialog (*main_win, FALSE, GTK_MESSAGE_ERROR, msg, _("Delete problem"), -1, _("Abort"), NULL);
                g_free (msg);

                g_error_free (error);
                return 0;
            }
            if (num_dirs != 1 || num_files != 0) // num_dirs = 1 -> this is the folder to be deleted
            {
                gchar *msg = NULL;

                msg = g_strdup_printf (_("The directory “%s” is not empty. Do you really want to delete it?"), gnomeCmdFile->get_name());
                guiResponse = run_simple_dialog (*main_win, FALSE, GTK_MESSAGE_WARNING, msg, _("Delete"),
                                  gnome_cmd_data.options.confirm_delete_default==GTK_BUTTONS_CANCEL ? 0 : 3,
                                  _("Cancel"), _("Skip"),
                                  dirCount++ == 0 ? _("Delete All") : _("Delete Remaining"),
                                  _("Delete"), nullptr);
                g_free(msg);

                if (guiResponse != DELETE_NONEMPTY_SKIP
                    && guiResponse != DELETE_NONEMPTY_DELETEALL
                    && guiResponse != DELETE_NONEMPTY_DELETE)
                {
                    guiResponse = DELETE_NONEMPTY_CANCEL; // Set to zero for the case the user presses ESCAPE in the warning dialog)
                }

                if (guiResponse == DELETE_NONEMPTY_CANCEL || guiResponse == DELETE_NONEMPTY_DELETEALL)
                {
                    break;
                }
                else if (guiResponse == DELETE_NONEMPTY_SKIP)
                {
                    itemsToDelete = g_list_remove(itemsToDelete, file->data);
                    continue;
                }
                else if (guiResponse == DELETE_NONEMPTY_DELETE)
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
        }
    }
    if (guiResponse == DELETE_NONEMPTY_CANCEL)
    {
        g_list_free(itemsToDelete);
        return nullptr;
    }
    if (g_list_length(itemsToDelete) == 0)
    {
        return nullptr; // file list is empty
    }
    return itemsToDelete;
}

/**
 * Creates a delete dialog for the given list of GnomeCmdFiles
 */
void gnome_cmd_delete_dialog_show (GList *files)
{
    g_return_if_fail (files != nullptr);

    gint response = 1;

    if (gnome_cmd_data.options.confirm_delete)
    {
        gchar *msg = nullptr;

        gint n_files = g_list_length (files);

        if (n_files == 1)
        {
            auto f = (GnomeCmdFile *) g_list_nth_data (files, 0);
            g_return_if_fail (GNOME_CMD_IS_FILE(f));

            if (f->is_dotdot)
                return;

            msg = g_strdup_printf (_("Do you want to delete “%s”?"), f->get_name());
        }
        else
            msg = g_strdup_printf (ngettext("Do you want to delete the selected file?",
                                            "Do you want to delete the %d selected files?",
                                            n_files),
                                   n_files);

        response = run_simple_dialog (*main_win, FALSE,
                                      GTK_MESSAGE_QUESTION, msg, _("Delete"),
                                      gnome_cmd_data.options.confirm_delete_default==GTK_BUTTONS_CANCEL ? 0 : 1, _("Cancel"), _("Delete"), NULL);

        g_free (msg);
    }

    if (response != 1)
        return;

    // eventually remove non-empty dirs from list
    files = remove_items_from_list_to_be_deleted(files);

    if (files == nullptr)
        return;

    DeleteData *deleteData = g_new0 (DeleteData, 1);

    deleteData->gnomeCmdFiles = files;

    do_delete (deleteData);
}
