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
#include "gnome-cmd-prepare-move-dialog.h"
#include "gnome-cmd-prepare-xfer-dialog.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"

using namespace std;


typedef struct
{
    GnomeCmdPrepareXferDialog *dialog;
    GtkWidget *silent;
    GtkWidget *query;
    GtkWidget *skip;

} PrepareMoveData;


static void on_ok (GtkButton *button, gpointer user_data)
{
    PrepareMoveData *data = (PrepareMoveData *) user_data;
    GnomeCmdPrepareXferDialog *dlg = data->dialog;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->silent)))
        dlg->xferOverwriteMode = GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE;
    else
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->query)))
            dlg->xferOverwriteMode = GNOME_VFS_XFER_OVERWRITE_MODE_QUERY;
        else
            dlg->xferOverwriteMode = GNOME_VFS_XFER_OVERWRITE_MODE_SKIP;

    dlg->xferOptions = GNOME_VFS_XFER_REMOVESOURCE;
}


void gnome_cmd_prepare_move_dialog_show (GnomeCmdFileSelector *from, GnomeCmdFileSelector *to)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (from));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (to));

    GSList *group = NULL;
    PrepareMoveData *data = g_new0 (PrepareMoveData, 1);
    gchar *dest_dir_frame_msg, *text;
    GtkWidget *label;

    GList *tmp = from->file_list()->get_selected_files();

    if (!tmp)
    {
        g_list_free (tmp);
        return;
    }

    data->dialog = GNOME_CMD_PREPARE_XFER_DIALOG (gnome_cmd_prepare_xfer_dialog_new (from, to));
    gtk_window_set_title (GTK_WINDOW (data->dialog), _("Move"));
    gtk_widget_ref (GTK_WIDGET (data->dialog));


    // Create prepare copy specific widgets

    data->silent = gtk_radio_button_new_with_label (group, _("Silently"));
    group = gtk_radio_button_group (GTK_RADIO_BUTTON (data->silent));
    gtk_widget_ref (data->silent);
    gtk_object_set_data_full (GTK_OBJECT (data->dialog), "silent", data->silent, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (data->silent);
    gtk_box_pack_start (GTK_BOX (data->dialog->left_vbox), data->silent, FALSE, FALSE, 0);

    data->query = gtk_radio_button_new_with_label (group, _("Query First"));
    group = gtk_radio_button_group (GTK_RADIO_BUTTON (data->query));
    gtk_widget_ref (data->query);
    gtk_object_set_data_full (GTK_OBJECT (data->dialog), "query", data->query, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (data->query);
    gtk_box_pack_start (GTK_BOX (data->dialog->left_vbox), data->query, FALSE, FALSE, 0);

    data->skip = gtk_radio_button_new_with_label (group, _("Skip All"));
    group = gtk_radio_button_group (GTK_RADIO_BUTTON (data->skip));
    gtk_widget_ref (data->skip);
    gtk_object_set_data_full (GTK_OBJECT (data->dialog), "skip", data->skip, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (data->skip);
    gtk_box_pack_start (GTK_BOX (data->dialog->left_vbox), data->skip, FALSE, FALSE, 0);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (g_slist_nth_data (group, gnome_cmd_data.confirm_move_overwrite)), TRUE);


    // Customize prepare xfer widgets

    text = get_bold_text (_("Overwrite Files"));
    label = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (data->dialog->left_vbox_frame), "label");
    gtk_label_set_markup (GTK_LABEL (label), text);
    g_free (text);

    text = get_bold_text (_("Options"));
    label = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (data->dialog->right_vbox_frame), "label");
    gtk_label_set_markup (GTK_LABEL (label), text);
    g_free (text);

    g_return_if_fail (data->dialog->src_files != NULL);

    gint num_files = g_list_length (data->dialog->src_files);

    if (num_files == 1)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) data->dialog->src_files->data;
        gchar *fname = get_utf8 (f->info->name);
        dest_dir_frame_msg = g_strdup_printf (_("Move \"%s\" to"), fname);
        g_free (fname);
    }
    else
        dest_dir_frame_msg = g_strdup_printf (ngettext("move %d file to","move %d files to",num_files), num_files);

    text = get_bold_text (dest_dir_frame_msg);
    label = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (data->dialog->dest_dir_frame), "label");
    gtk_label_set_markup (GTK_LABEL (label), text);
    g_free (text);

    g_free  (dest_dir_frame_msg);


    // Connect signals

    gtk_signal_connect (GTK_OBJECT (data->dialog->ok_button), "clicked", GTK_SIGNAL_FUNC (on_ok), data);


    // Show the dialog

    gtk_widget_show (GTK_WIDGET (data->dialog));
}
