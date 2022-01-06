/**
 * @file gnome-cmd-prepare-xfer-dialog.cc
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
#include "gnome-cmd-dir.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"
#include "dialogs/gnome-cmd-prepare-xfer-dialog.h"

using namespace std;


static GnomeCmdDialogClass *parent_class = NULL;


inline gboolean con_device_has_path (FileSelectorID fsID, GnomeCmdCon *&dev, const gchar *user_path)
{
    dev = main_win->fs(fsID)->get_connection();

    return GNOME_CMD_IS_CON_DEVICE (dev) &&
           g_str_has_prefix (user_path, gnome_cmd_con_device_get_mountp_string (GNOME_CMD_CON_DEVICE (dev)));
}


static void on_ok (GtkButton *button, GnomeCmdPrepareXferDialog *dialog)
{
    GnomeCmdCon *con = gnome_cmd_dir_get_connection (dialog->default_dest_dir);
    gchar *user_path = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->dest_dir_entry))));

    if (!user_path)
    {
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return;
    }

    gchar *dest_path = NULL;
    gchar *dest_fn = NULL;
    gint user_path_len;

    user_path_len = strlen (user_path);
    dest_path = user_path;
    GnomeCmdDir *dest_dir;

    // Make whatever the user entered into a valid path if possible
    if (user_path_len > 2 && user_path[user_path_len-1] == '/')
        user_path[user_path_len-1] = '\0';

    if (user_path[0] == '/')
    {
        if (main_win->fs(INACTIVE)->is_local())   // hack to avoiding 'root' dir for mounted devices
        {
            GnomeCmdCon *dev;

            // if LEFT or RIGHT device (connection) points to user_path than adjust user_path and set con to the found device
            if (con_device_has_path (INACTIVE, dev, user_path) || con_device_has_path (ACTIVE, dev, user_path))
            {
                dest_path = g_strdup (user_path + strlen (gnome_cmd_con_device_get_mountp_string (GNOME_CMD_CON_DEVICE (dev))));
                con = dev;
            }
            else    // otherwise connection not present in any pane, use home connection instead
                con = get_home_con ();
        }
    }
    else
    {
        if (!gnome_cmd_dir_is_local (dialog->default_dest_dir))
        {
            const gchar *t = gnome_cmd_dir_get_path (dialog->default_dest_dir)->get_path();
            dest_path = g_build_filename (t, user_path, NULL);
        }
        else
        {
            gchar *t = GNOME_CMD_FILE (dialog->src_fs->get_directory())->GetPathStringThroughParent();
            dest_path = g_build_filename (t, user_path, NULL);
            g_free (t);
        }
        g_free (user_path);
    }

    // Check if something exists at the given path and find out what it is
    GFileType gFileType;
    gboolean  fileExists;

    fileExists = gnome_cmd_con_get_path_target_type (con, dest_path, &gFileType);

    if (g_list_length (dialog->src_files) == 1)
    {
        GnomeCmdFile *gFile = GNOME_CMD_FILE (dialog->src_files->data);

        if (fileExists && gFileType == G_FILE_TYPE_DIRECTORY)
        {
            // There exists a directory, copy into it using the original filename
            dest_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dest_path));
            dest_fn = g_strdup (gFile->get_name());
        }
        else
            if (fileExists)
            {
                // There exists something else, asume that the user wants to overwrite it for now
                gchar *t = g_path_get_dirname (dest_path);
                dest_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, t));
                g_free (t);
                dest_fn = g_path_get_basename (dest_path);
            }
            else
            {
                // Nothing found, check if the parent dir exists
                gchar *parent_dir = g_path_get_dirname (dest_path);
                fileExists = gnome_cmd_con_get_path_target_type (con, parent_dir, &gFileType);
                if (fileExists && gFileType == G_FILE_TYPE_DIRECTORY)
                {
                    // yup, xfer to it
                    dest_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, parent_dir));
                    g_free (parent_dir);
                    dest_fn = g_path_get_basename (dest_path);
                }
                else
                    if (fileExists)
                    {
                        // the parent dir was a file, abort!
                        g_free (parent_dir);
                        g_free (dest_path);
                        gtk_widget_destroy (GTK_WIDGET (dialog));
                        return;
                    }
                    else
                    {
                        // Nothing exists, ask the user if a new directory might be suitable in the path that he specified
                        gchar *msg = g_strdup_printf (_("The directory “%s” doesn’t exist, do you want to create it?"),
                                                      g_path_get_basename (parent_dir));
                        gint choice = run_simple_dialog (GTK_WIDGET (dialog), TRUE, GTK_MESSAGE_QUESTION, msg, "",
                                                         -1, _("No"), _("Yes"), NULL);
                        g_free (msg);

                        if (choice == 1)
                        {
                            GError *error = nullptr;
                            if (!gnome_cmd_con_mkdir (con, parent_dir, error))
                            {
                                gnome_cmd_show_message (*main_win, error ? error->message : _("Unknown error"));
                                g_error_free(error);
                                g_free (dest_path);
                                gtk_widget_destroy (GTK_WIDGET (dialog));
                                return;
                            }
                        }
                        else
                        {
                            g_free (dest_path);
                            gtk_widget_destroy (GTK_WIDGET (dialog));
                            return;
                        }

                        dest_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, parent_dir));
                        g_free (parent_dir);
                        dest_fn = g_path_get_basename (dest_path);
                    }
            }
    }
    else
    {
        if (fileExists && gFileType == G_FILE_TYPE_DIRECTORY)
        {
            // There exists a directory, copy to it
            dest_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dest_path));
        }
        else
            if (fileExists)
            {
                // There exists something which is not a directory, abort!
                g_free (dest_path);
                gtk_widget_destroy (GTK_WIDGET (dialog));
                return;
            }
            else
            {
                // Nothing exists, ask the user if a new directory might be suitable in the path that he specified
                gchar *msg = g_strdup_printf (_("The directory “%s” doesn’t exist, do you want to create it?"),
                                              g_path_get_basename (dest_path));
                GtkWidget *dir_dialog = gtk_message_dialog_new (*main_win,
                                                            (GtkDialogFlags) 0,
                                                            GTK_MESSAGE_QUESTION,
                                                            GTK_BUTTONS_OK_CANCEL,
                                                            "%s",
                                                            msg);
                gint choice = gtk_dialog_run (GTK_DIALOG (dir_dialog));
                gtk_widget_destroy (dir_dialog);
                g_free (msg);

                if (choice == GTK_RESPONSE_OK)
                {
                    GError *error = nullptr;
                    if (!gnome_cmd_con_mkdir (con, dest_path, error))
                    {
                        gnome_cmd_show_message (*main_win, error->message);
                        g_error_free(error);
                        g_free (dest_path);
                        gtk_widget_destroy (GTK_WIDGET (dialog));
                        return;
                    }
                }
                else
                {
                    g_free (dest_path);
                    gtk_widget_destroy (GTK_WIDGET (dialog));
                    return;
                }
                dest_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dest_path));
            }
    }

    if (!GNOME_CMD_IS_DIR (dest_dir))
    {
        g_free (dest_path);
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return;
    }

    if (g_list_length (dialog->src_files) == 1)
        DEBUG ('x', "Starting xfer the file '%s' to '%s'\n", dest_fn, GNOME_CMD_FILE (dest_dir)->get_uri_str());
    else
        DEBUG ('x', "Starting xfer %d files to '%s'\n", g_list_length (dialog->src_files), GNOME_CMD_FILE (dest_dir)->get_uri_str());


    gnome_cmd_dir_ref (dest_dir);

    switch (dialog->gnomeCmdTransferType)
    {
        case COPY:
            gnome_cmd_copy_start (dialog->src_files,
                                  dest_dir,
                                  dialog->src_fs->file_list(),
                                  dest_fn,
                                  dialog->gFileCopyFlags,
                                  dialog->overwriteMode,
                                  NULL, NULL);
            break;
        case MOVE:
            gnome_cmd_move_start (dialog->src_files,
                      dest_dir,
                      dialog->src_fs->file_list(),
                      dest_fn,
                      dialog->gFileCopyFlags,
                      dialog->overwriteMode,
                      NULL, NULL);
            break;
        case LINK:
        default:
            break;
    }

    g_free (dest_path);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, gpointer user_data)
{
    GnomeCmdPrepareXferDialog *dialog = GNOME_CMD_PREPARE_XFER_DIALOG (user_data);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static gboolean on_dest_dir_entry_keypressed (GtkEntry *entry, GdkEventKey *event, GnomeCmdPrepareXferDialog *dialog)
{
    switch (event->keyval)
    {
        case GDK_Return:
        case GDK_KP_Enter:
            gtk_signal_emit_by_name (GTK_OBJECT (dialog->ok_button), "clicked", dialog, NULL);
            return TRUE;

        case GDK_F5:
        case GDK_F6:
            gnome_cmd_toggle_file_name_selection (dialog->dest_dir_entry);
            return TRUE;

        default:
            return FALSE;
    }
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdPrepareXferDialog *dialog = GNOME_CMD_PREPARE_XFER_DIALOG (object);

    gnome_cmd_file_list_unref (dialog->src_files);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdPrepareXferDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (GNOME_CMD_TYPE_DIALOG);

    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdPrepareXferDialog *dialog)
{
    GtkWidget *dest_dir_vbox, *dir_entry;

    // dest dir
    dest_dir_vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 0);

    dialog->dest_dir_frame = create_category (GTK_WIDGET (dialog), dest_dir_vbox, "");
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), dialog->dest_dir_frame);

    dir_entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (dest_dir_vbox), dir_entry, TRUE, TRUE, 0);
    dialog->dest_dir_entry = dir_entry;

    gtk_widget_show_all (dialog->dest_dir_entry);

    g_signal_connect (dialog->dest_dir_entry, "key-press-event", G_CALLBACK (on_dest_dir_entry_keypressed), dialog);

    // options
    GtkWidget *options_hbox = create_hbox (GTK_WIDGET (dialog), TRUE, 6);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), options_hbox);

    dialog->left_vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 0);
    dialog->right_vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 0);

    dialog->left_vbox_frame = create_category (GTK_WIDGET (dialog), dialog->left_vbox, "");
    gtk_container_add (GTK_CONTAINER (options_hbox), dialog->left_vbox_frame);

    dialog->right_vbox_frame = create_category (GTK_WIDGET (dialog), dialog->right_vbox, "");
    gtk_container_add (GTK_CONTAINER (options_hbox), dialog->right_vbox_frame);

    // buttons
    dialog->cancel_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL, NULL, NULL);
    dialog->ok_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK, NULL, NULL);

    g_signal_connect_after (dialog->cancel_button, "clicked", G_CALLBACK (on_cancel), dialog);
    g_signal_connect_after (dialog->ok_button, "clicked", G_CALLBACK (on_ok), dialog);

    gtk_widget_set_size_request (GTK_WIDGET (dialog), 500, -1);
}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_prepare_xfer_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            (gchar*) "GnomeCmdPrepareXferDialog",
            sizeof (GnomeCmdPrepareXferDialog),
            sizeof (GnomeCmdPrepareXferDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (GNOME_CMD_TYPE_DIALOG, &dlg_info);
    }
    return dlg_type;
}


inline gboolean path_points_at_directory (GnomeCmdFileSelector *to, const gchar *dest_path)
{
    GFileType type;

    gboolean fileExists = gnome_cmd_con_get_path_target_type (to->get_connection(), dest_path, &type);

    return fileExists && type == G_FILE_TYPE_DIRECTORY;
}


GtkWidget *gnome_cmd_prepare_xfer_dialog_new (GnomeCmdFileSelector *from, GnomeCmdFileSelector *to)
{
    g_return_val_if_fail (from!=NULL, NULL);
    g_return_val_if_fail (to!=NULL, NULL);

    gchar *destString = NULL;
    GnomeCmdPrepareXferDialog *dialog = (GnomeCmdPrepareXferDialog *) g_object_new (GNOME_CMD_TYPE_PREPARE_XFER_DIALOG, NULL);

    dialog->src_files = from->file_list()->get_selected_files();
    gnome_cmd_file_list_ref (dialog->src_files);
    dialog->default_dest_dir = to->get_directory();
    dialog->src_fs = from;

    guint num_files = g_list_length (dialog->src_files);

    if (!to->is_local())
    {
        if (num_files == 1)
        {
            auto gFile = static_cast<GnomeCmdFile*> (dialog->src_files->data);
            destString = g_strdup (gFile->get_name());
        }
    }
    else
    {
        if (num_files == 1)
        {
            auto gFile = static_cast<GnomeCmdFile*> (dialog->src_files->data);

            auto tempPathString = GNOME_CMD_FILE (dialog->default_dest_dir)->get_real_path();
            auto path = get_utf8 (tempPathString);
            g_free (tempPathString);

            destString = g_build_filename (path, gFile->get_name(), NULL);
            if (path_points_at_directory (to, destString))
            {
                g_free (destString);
                destString = g_strdup (path);
            }

            g_free (path);
        }
        else
        {
            auto tempPathString = GNOME_CMD_FILE (dialog->default_dest_dir)->get_real_path();
            destString = get_utf8 (tempPathString);
            g_free (tempPathString);
        }
    }
    if (destString)
    {
        gtk_entry_set_text (GTK_ENTRY (dialog->dest_dir_entry), destString);
        g_free (destString);
    }

    gtk_widget_grab_focus (GTK_WIDGET (dialog->dest_dir_entry));

    return GTK_WIDGET (dialog);
}
