/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak

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
#include "gnome-cmd-prepare-copy-dialog.h"
#include "gnome-cmd-prepare-xfer-dialog.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"

using namespace std;


typedef struct
{
    GnomeCmdPrepareXferDialog *dialog;
    GtkWidget *silent;
    GtkWidget *query;
    GtkWidget *skip;

    GtkWidget *follow_links;

} PrepareCopyData;


static void on_ok (GtkButton *button, gpointer user_data)
{
    PrepareCopyData *data = (PrepareCopyData *) user_data;
    GnomeCmdPrepareXferDialog *dlg = data->dialog;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->silent)))
        dlg->xferOverwriteMode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
    else
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->query)))
            dlg->xferOverwriteMode = GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
        else
            dlg->xferOverwriteMode = GNOME_VFS_XFER_OVERWRITE_MODE_SKIP;

    guint xferOptions = GNOME_VFS_XFER_RECURSIVE;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->follow_links)))
        xferOptions |= GNOME_VFS_XFER_FOLLOW_LINKS;

    dlg->xferOptions = (GnomeVFSXferOptions) xferOptions;
}


void gnome_cmd_prepare_copy_dialog_show (GnomeCmdFileSelector *from, GnomeCmdFileSelector *to)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (from));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (to));

    GSList *group = NULL;
    PrepareCopyData *data = g_new0 (PrepareCopyData, 1);
    gchar *dest_dir_frame_msg, *text;
    GtkWidget *label;

    data->dialog = GNOME_CMD_PREPARE_XFER_DIALOG (gnome_cmd_prepare_xfer_dialog_new (from, to));

    g_return_if_fail (data->dialog->src_files != NULL);

    gtk_window_set_title (GTK_WINDOW (data->dialog), _("Copy"));
    gtk_widget_ref (GTK_WIDGET (data->dialog));


    // Create prepare copy specific widgets

    data->silent = gtk_radio_button_new_with_label (group, _("Silently"));
    group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (data->silent));
    gtk_widget_ref (data->silent);
    g_object_set_data_full (G_OBJECT (data->dialog), "silent", data->silent, g_object_unref);
    gtk_widget_show (data->silent);
    gtk_box_pack_start (GTK_BOX (data->dialog->left_vbox), data->silent, FALSE, FALSE, 0);

    data->query = gtk_radio_button_new_with_label (group, _("Query First"));
    group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (data->query));
    gtk_widget_ref (data->query);
    g_object_set_data_full (G_OBJECT (data->dialog), "query", data->query, g_object_unref);
    gtk_widget_show (data->query);
    gtk_box_pack_start (GTK_BOX (data->dialog->left_vbox), data->query, FALSE, FALSE, 0);

    data->skip = gtk_radio_button_new_with_label (group, _("Skip All"));
    group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (data->skip));
    gtk_widget_ref (data->skip);
    g_object_set_data_full (G_OBJECT (data->dialog), "skip", data->skip, g_object_unref);
    gtk_widget_show (data->skip);
    gtk_box_pack_start (GTK_BOX (data->dialog->left_vbox), data->skip, FALSE, FALSE, 0);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (g_slist_nth_data (group, gnome_cmd_data.confirm_copy_overwrite)), TRUE);


    data->follow_links = gtk_check_button_new_with_label (_("Follow Links"));
    gtk_widget_ref (data->follow_links);
    g_object_set_data_full (G_OBJECT (data->dialog), "follow_links", data->follow_links, g_object_unref);
    gtk_widget_show (data->follow_links);
    gtk_box_pack_start (GTK_BOX (data->dialog->right_vbox), data->follow_links, FALSE, FALSE, 0);


    // Customize prepare xfer widgets

    text = get_bold_text (_("Overwrite Files"));
    label = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (data->dialog->left_vbox_frame), "label");
    gtk_label_set_markup (GTK_LABEL (label), text);
    g_free (text);

    text = get_bold_text (_("Options"));
    label = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (data->dialog->right_vbox_frame), "label");
    gtk_label_set_markup (GTK_LABEL (label), text);
    g_free (text);

    gint num_files = g_list_length (data->dialog->src_files);

    if (num_files == 1)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) data->dialog->src_files->data;
        gchar *fname = get_utf8 (f->info->name);
        dest_dir_frame_msg = g_strdup_printf (_("Copy \"%s\" to"), fname);
        g_free (fname);
    }
    else
        dest_dir_frame_msg = g_strdup_printf (ngettext("copy %d file to","copy %d files to",num_files), num_files);

    text = get_bold_text (dest_dir_frame_msg);
    label = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (data->dialog->dest_dir_frame), "label");
    gtk_label_set_markup (GTK_LABEL (label), text);
    g_free (text);

    g_free (dest_dir_frame_msg);


    // Connect signals

    g_signal_connect (data->dialog->ok_button, "clicked", G_CALLBACK (on_ok), data);


    // Show the dialog

    gtk_widget_show (GTK_WIDGET (data->dialog));
}
