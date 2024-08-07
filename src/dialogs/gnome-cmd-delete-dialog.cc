/**
 * @file gnome-cmd-delete-dialog.cc
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
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
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

static gboolean perform_delete_operation_r(DeleteData *deleteData, GList *gnomeCmdFileGList);

inline void cleanup (DeleteData *deleteData)
{
    gnome_cmd_file_list_free (deleteData->gnomeCmdFiles);

    //Just set deletedGnomeCmdFiles back to null as it is a subset of gnomeCmdFiles
    g_list_free (deleteData->deletedGnomeCmdFiles);
    deleteData->deletedGnomeCmdFiles = nullptr;

    g_mutex_clear(&deleteData->mutex);

    if (deleteData->cancellable)
        g_object_unref (deleteData->cancellable);

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

    if (deleteData->itemsDeleted > 0 && deleteData->itemsTotal > 0)
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
    if (deleteData->cancellable)
        g_cancellable_cancel (deleteData->cancellable);
    gtk_widget_set_sensitive (GTK_WIDGET (deleteData->progwin), FALSE);
}


static gboolean on_progwin_destroy (GtkWidget *win, DeleteData *deleteData)
{
    deleteData->stop = TRUE;
    if (deleteData->cancellable)
        g_cancellable_cancel (deleteData->cancellable);

    return FALSE;
}


inline void create_delete_progress_win (DeleteData *deleteData)
{
    GtkWidget *vbox;
    GtkWidget *bbox;
    GtkWidget *button;

    deleteData->progwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (deleteData->progwin), _("Deleting…"));
    gtk_window_set_resizable (GTK_WINDOW (deleteData->progwin), FALSE);
    gtk_window_set_position (GTK_WINDOW (deleteData->progwin), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request (GTK_WIDGET (deleteData->progwin), 300, -1);
    g_signal_connect (deleteData->progwin, "destroy-event", G_CALLBACK (on_progwin_destroy), deleteData);

    vbox = create_vbox (deleteData->progwin, FALSE, 6);
    gtk_container_add (GTK_CONTAINER (deleteData->progwin), vbox);
    gtk_widget_set_margin_top (vbox, 5);
    gtk_widget_set_margin_bottom (vbox, 5);
    gtk_widget_set_margin_start (vbox, 5);
    gtk_widget_set_margin_end (vbox, 5);

    deleteData->proglabel = create_label (deleteData->progwin, "");
    gtk_box_append (GTK_BOX (vbox), deleteData->proglabel);

    deleteData->progbar = create_progress_bar (deleteData->progwin);
    gtk_box_append (GTK_BOX (vbox), deleteData->progbar);

    bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append (GTK_BOX (vbox), bbox);

    button = create_button_with_data (deleteData->progwin, _("_Cancel"), G_CALLBACK (on_cancel), deleteData);
    gtk_widget_set_hexpand (button, TRUE);
    gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_can_default (button, TRUE);
    gtk_box_append (GTK_BOX (bbox), button);

    gtk_widget_show_all (bbox);

    g_object_ref (deleteData->progwin);
    gtk_widget_show (deleteData->progwin);
}


/**
 * @brief This function is meant to be called from within the function perform_delete_operation_r().
 * It will dive through a non-empty directory and tries to recursively delete all files and
 * folders inside of it.
 *
 * @param gnomeCmdDir Directory to go through and which should be deleted
 * @param deleteData DeleteData object
 * @return TRUE if deletion was performed successfully
 */
static gboolean perform_delete_subdirs(GnomeCmdDir *gnomeCmdDir, DeleteData *deleteData)
{
    gboolean deleted = TRUE;
    for (GList *dirChildItem = gnome_cmd_dir_get_files (gnomeCmdDir); dirChildItem; dirChildItem = dirChildItem->next)
    {
        if (deleteData->problem_action == DELETE_ERROR_ACTION_ABORT)
        {
            deleteData->stop = TRUE;
            gnome_cmd_dir_unref (gnomeCmdDir);
            return FALSE;
        }
        if (deleteData->problem_action == DELETE_ERROR_ACTION_RETRY)
        {
            dirChildItem = dirChildItem->prev ? dirChildItem->prev : dirChildItem;
            deleteData->problem_action = -1;
        }
        if (deleteData->problem_action == DELETE_ERROR_ACTION_SKIP)
        {
            // just go on and set problem_action to the default value
            deleteData->problem_action = -1;
        }

        auto gnomeCmdFile = static_cast<GnomeCmdFile*>(dirChildItem->data);

        if (gnomeCmdFile->is_dotdot)
            continue;

        GList *childsList = nullptr;
        childsList = g_list_append(childsList, gnomeCmdFile);
        deleted &= perform_delete_operation_r (deleteData, childsList);
        g_list_free(childsList);
    }
    return deleted;
}

/**
 * This function recursively removes files of a given GnomeCmdFile list and stores
 * possible errors or the progress information in the deleteData object.
 */
static gboolean perform_delete_operation_r(DeleteData *deleteData, GList *gnomeCmdFileGList)
{
    for (GList *gCmdFileGListItem = gnomeCmdFileGList; gCmdFileGListItem; gCmdFileGListItem = gCmdFileGListItem->next)
    {
        GError *tmpError = nullptr;
        if (deleteData->stop)
        {
            return FALSE;
        }

        auto gnomeCmdFile = static_cast<GnomeCmdFile*>(gCmdFileGListItem->data);

        g_return_val_if_fail (GNOME_CMD_IS_FILE(gnomeCmdFile), FALSE);
        g_return_val_if_fail (G_IS_FILE_INFO(gnomeCmdFile->get_file_info()), FALSE);

        auto filenameTmp = gnomeCmdFile->get_name();

        if (gnomeCmdFile->is_dotdot || strcmp(filenameTmp, ".") == 0)
            continue;

        switch (deleteData->originAction)
        {
            case DeleteData::OriginAction::FORCE_DELETE:
            case DeleteData::OriginAction::MOVE:
                g_file_delete (gnomeCmdFile->get_file(), deleteData->cancellable, &tmpError);
                break;
            case DeleteData::OriginAction::DELETE:
            default:
                gnome_cmd_data.options.deleteToTrash
                    ? g_file_trash (gnomeCmdFile->get_file(), deleteData->cancellable, &tmpError)
                    : g_file_delete (gnomeCmdFile->get_file(), deleteData->cancellable, &tmpError);
                break;
        }

        if (tmpError && g_error_matches (tmpError, G_IO_ERROR, G_IO_ERROR_NOT_EMPTY))
        {
            g_error_free(tmpError);
            tmpError = nullptr;
            auto gnomeCmdDir = GNOME_CMD_DIR (gnomeCmdFile);
            gnome_cmd_dir_list_files (deleteData->parent_window, gnomeCmdDir, FALSE);

            if (perform_delete_subdirs(gnomeCmdDir, deleteData))
            {
                // Now remove the directory itself, as it is finally empty
                GList *directory = nullptr;
                directory = g_list_append(directory, gnomeCmdFile);
                perform_delete_operation_r (deleteData, directory);
                g_list_free(directory);
            }
            else if (deleteData->stop)
            {
                return FALSE;
            }
        }
        else if (tmpError)
        {
            if (g_error_matches (tmpError, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_error_free(tmpError);
            else
            {
                g_warning ("Failed to delete %s: %s", gnomeCmdFile->get_name(), tmpError->message);
                g_propagate_error (&(deleteData->error), tmpError);
                deleteData->problemFileName = gnomeCmdFile->get_name();
                delete_progress_update(deleteData);
            }
            return FALSE;
        }

        deleteData->deletedGnomeCmdFiles = g_list_append(deleteData->deletedGnomeCmdFiles, gnomeCmdFile);
        deleteData->itemsDeleted++;
        delete_progress_update(deleteData);
        if (deleteData->stop)
            return FALSE;
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

    if (deleteData->progwin)
    {
        gtk_label_set_text (GTK_LABEL (deleteData->proglabel), deleteData->msg);
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (deleteData->progbar), deleteData->progress);
    }

    if (deleteData->problem)
    {
        gchar *msg = g_strdup_printf (_("Error while deleting “%s”\n\n%s"),
                                        deleteData->problemFileName,
                                        deleteData->error->message);

        deleteData->problem_action = run_simple_dialog (
            deleteData->parent_window, TRUE, GTK_MESSAGE_ERROR, msg, _("Delete problem"),
            -1, _("Abort"), _("Retry"), _("Skip"), NULL);

        g_free (msg);

        deleteData->problem = FALSE;
    }

    g_mutex_unlock (&deleteData->mutex);

    if (deleteData->deleteDone)
    {
        g_thread_join (deleteData->thread);

        if (deleteData->error)
            gnome_cmd_show_message (deleteData->parent_window, deleteData->error->message);

        if (deleteData->deletedGnomeCmdFiles)
        {
            for (GList *i = deleteData->deletedGnomeCmdFiles; i; i = i->next)
            {
                auto gnomeCmdFile = GNOME_CMD_FILE (i->data);
                auto gFile = gnomeCmdFile->get_gfile();

                if (!g_file_query_exists (gFile, nullptr))
                    gnomeCmdFile->is_deleted();
            }
        }

        if (deleteData->progwin)
        {
            gtk_window_destroy (GTK_WINDOW (deleteData->progwin));
        }

        cleanup (deleteData);

        return FALSE;  // returning FALSE here stops the timeout callbacks
    }

    return TRUE;
}


void do_delete (DeleteData *deleteData, gboolean showProgress = true)
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
        auto gFile = GNOME_CMD_FILE(fileListItem->data)->get_file();
        g_return_if_fail(G_IS_FILE(gFile));

        if (showProgress)
        {
            guint64 num_files = 0;
            guint64 num_dirs = 0;
            GError *error = nullptr;

            g_file_measure_disk_usage (gFile,
               G_FILE_MEASURE_NONE,
               nullptr, nullptr, nullptr, nullptr,
               &num_dirs,
               &num_files,
               &error);

            // If we cannot determine if gFile is empty or not, don't show progress dialog
            if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
            {
                showProgress = FALSE;
                g_error_free (error);
                error = nullptr;
                break;
            }

            deleteData->itemsTotal += num_files + num_dirs;
        }
    }

    if (showProgress)
    {
        create_delete_progress_win (deleteData);
        deleteData->cancellable = g_cancellable_new();
    }
    else
        deleteData->itemsTotal = 0;

    deleteData->thread = g_thread_new (NULL, (GThreadFunc) perform_delete_operation, deleteData);
    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_delete_status_widgets, deleteData);
}


/**
 * Remove a directory from the list of files to be deleted.
 * This happens as of user interaction.
 * The returned list has to be free'ed by g_list_free.
 */
static GList *remove_items_from_list_to_be_deleted(GtkWindow *parent_window, GList *files)
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
        if (!gnomeCmdFile->GetGfileAttributeBoolean(G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK)
            && gnomeCmdFile->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        {
            GError *error;
            error = nullptr;
            guint64 num_dirs;
            guint64 num_files;
            gboolean canNotMeasure = FALSE;

            g_file_measure_disk_usage (gnomeCmdFile->get_file(),
                       G_FILE_MEASURE_NONE,
                       nullptr, nullptr, nullptr, nullptr,
                       &num_dirs,
                       &num_files,
                       &error);
            if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
            {
                // If we cannot determine if gFile is empty or not, we have to assume it is not empty
                canNotMeasure = TRUE;
                g_error_free (error);
                error = nullptr;
            }
            if (error)
            {
                g_message ("remove_items_from_list_to_be_deleted: g_file_measure_disk_usage failed: %s", error->message);

                gchar *msg = g_strdup_printf (_("Error while deleting: \n\n%s"), error->message);

                run_simple_dialog (parent_window, FALSE, GTK_MESSAGE_ERROR, msg, _("Delete problem"), -1, _("Abort"), NULL);
                g_free (msg);

                g_error_free (error);
                return 0;
            }
            if (num_dirs != 1 || num_files != 0 || canNotMeasure) // num_dirs = 1 -> this is the folder to be deleted
            {
                gchar *msg = NULL;

                msg = canNotMeasure
                    ? g_strdup_printf (_("Do you really want to delete “%s”?"), gnomeCmdFile->get_name())
                    : g_strdup_printf (_("The directory “%s” is not empty. Do you really want to delete it?"), gnomeCmdFile->get_name());

                guiResponse = run_simple_dialog (parent_window, FALSE, GTK_MESSAGE_WARNING, msg, _("Delete"),
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
void gnome_cmd_delete_dialog_show (GtkWindow *parent_window, GList *files, gboolean forceDelete)
{
    g_return_if_fail (files != nullptr);

    gint response = 1;

    if (gnome_cmd_data.options.confirm_delete)
    {
        gchar *msg = nullptr;

        gint n_files = g_list_length (files);

        if (n_files == 1)
        {
            auto gnomeCmdFile = static_cast<GnomeCmdFile*> (g_list_nth_data (files, 0));
            g_return_if_fail (GNOME_CMD_IS_FILE(gnomeCmdFile));

            if (gnomeCmdFile->is_dotdot)
                return;

            msg = forceDelete || !gnome_cmd_data.options.deleteToTrash
                ? g_strdup_printf (_("Do you want to permanently delete “%s”?"), gnomeCmdFile->get_name())
                : g_strdup_printf (_("Do you want to move “%s” to the trash can?"), gnomeCmdFile->get_name());
        }
        else
        {
            msg = forceDelete || !gnome_cmd_data.options.deleteToTrash
                ? g_strdup_printf (ngettext("Do you want to permanently delete the selected file?",
                                            "Do you want to permanently delete the %d selected files?",
                                            n_files),
                                   n_files)
                : g_strdup_printf (ngettext("Do you want to move the selected file to the trash can?",
                                            "Do you want to move the %d selected files to the trash can?",
                                            n_files),
                                   n_files);
        }

        response = run_simple_dialog (parent_window, FALSE,
                                      forceDelete || !gnome_cmd_data.options.deleteToTrash
                                        ? GTK_MESSAGE_WARNING
                                        : GTK_MESSAGE_QUESTION,
                                      msg, _("Delete"),
                                      gnome_cmd_data.options.confirm_delete_default==GTK_BUTTONS_CANCEL ? 0 : 1, _("Cancel"), _("Delete"), NULL);

        g_free (msg);
    }

    if (response != 1)
    {
        g_list_free (files);
        return;
    }

    // eventually remove non-empty dirs from list
    files = remove_items_from_list_to_be_deleted(parent_window, files);

    if (files == nullptr)
        return;

    DeleteData *deleteData = g_new0 (DeleteData, 1);

    deleteData->parent_window = parent_window;
    deleteData->gnomeCmdFiles = files;
    deleteData->originAction = forceDelete
        ? DeleteData::OriginAction::FORCE_DELETE
        : DeleteData::OriginAction::DELETE;

    // Refing files for the delete procedure
    gnome_cmd_file_list_ref (deleteData->gnomeCmdFiles);

    do_delete (deleteData);
}
