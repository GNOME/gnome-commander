/**
 * @file gnome-cmd-string-dialog.cc
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
#include "libgcmd-deps.h"
#include "gnome-cmd-dialog.h"
#include "gnome-cmd-string-dialog.h"
#include "libgcmd-widget-factory.h"
#include "libgcmd-utils.h"

struct GnomeCmdStringDialogPrivate
{
    GnomeCmdStringDialogCallback ok_cb;
    GnomeCmdCallback<GtkButton*> cancel_cb;
    gpointer data;
    gchar *error_desc;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdStringDialog, gnome_cmd_string_dialog, GNOME_CMD_TYPE_DIALOG)


static void show_error_dialog (GtkWindow *parent_window, const gchar *msg)
{
    GtkWidget *dialog = gtk_message_dialog_new (parent_window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", msg);
    g_signal_connect (dialog, "response", G_CALLBACK (gtk_window_destroy), dialog);
    gtk_window_present (GTK_WINDOW (dialog));
}


static void on_ok (GtkButton *button, GnomeCmdStringDialog *dialog)
{
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (dialog));

    gboolean valid = TRUE;

    if (priv->ok_cb)
    {
        const gchar **values = (const gchar**) g_new (gpointer, dialog->rows);

        for (gint i=0; i<dialog->rows; i++)
            values[i] = gtk_editable_get_text (GTK_EDITABLE (dialog->entries[i]));

        valid = priv->ok_cb (dialog, (const gchar**)values, priv->data);
        if (!valid)
            show_error_dialog (GTK_WINDOW (dialog), priv->error_desc);
        g_free (values);
    }

    if (valid)
        gtk_widget_hide (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdStringDialog *dialog)
{
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (dialog));

    if (priv->cancel_cb)
        priv->cancel_cb (button, priv->data);

    gtk_widget_hide (GTK_WIDGET (dialog));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    GnomeCmdStringDialog *dialog = GNOME_CMD_STRING_DIALOG (object);
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (dialog));

    g_clear_pointer (&priv->error_desc, g_free);

    G_OBJECT_CLASS (gnome_cmd_string_dialog_parent_class)->dispose (object);
}


static void gnome_cmd_string_dialog_class_init (GnomeCmdStringDialogClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = dispose;
}


static void gnome_cmd_string_dialog_init (GnomeCmdStringDialog *string_dialog)
{
    string_dialog->rows = -1;
}


inline void setup_widget (GnomeCmdStringDialog *string_dialog, gint rows)
{
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (string_dialog));

    GtkWidget *dialog = GTK_WIDGET (string_dialog);
    GtkWidget *grid;
    GtkWidget *btn;

    string_dialog->rows = rows;
    string_dialog->labels = (GtkWidget**) g_new (gpointer, rows);
    string_dialog->entries = (GtkWidget**) g_new (gpointer, rows);
    priv->error_desc = g_strdup (_("No error description available"));

    grid = create_grid (dialog);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), grid);

    for (gint i=0; i<rows; i++)
    {
        string_dialog->labels[i] = create_label (dialog, "");
        gtk_grid_attach (GTK_GRID (grid), string_dialog->labels[i], 0, i, 1, 1);

        string_dialog->entries[i] = create_entry (dialog, "entry", "");
        gtk_entry_set_activates_default (GTK_ENTRY (string_dialog->entries[i]), TRUE);
        gtk_widget_set_hexpand (string_dialog->entries[i], TRUE);
        gtk_grid_attach (GTK_GRID (grid), string_dialog->entries[i], 1, i, 1, 1);
    }

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_Cancel"), G_CALLBACK (on_cancel), string_dialog);
    btn = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_OK"), G_CALLBACK (on_ok), string_dialog);

    gtk_widget_grab_focus (string_dialog->entries[0]);
    gtk_window_set_default_widget (GTK_WINDOW (dialog), btn);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_string_dialog_new_with_cancel (const gchar *title, const gchar **labels, gint rows, GnomeCmdStringDialogCallback ok_cb, GnomeCmdCallback<GtkButton*> cancel_cb, gpointer user_data)
{
    auto *dialog = static_cast< GnomeCmdStringDialog* >(g_object_new (GNOME_CMD_TYPE_STRING_DIALOG, NULL));

    gnome_cmd_string_dialog_setup_with_cancel (dialog, title, labels, rows, ok_cb, cancel_cb, user_data);

    return GTK_WIDGET (dialog);
}


void gnome_cmd_string_dialog_setup_with_cancel (GnomeCmdStringDialog *dialog, const gchar *title, const gchar **labels, gint rows, GnomeCmdStringDialogCallback ok_cb, GnomeCmdCallback<GtkButton*> cancel_cb, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (title != NULL);
    g_return_if_fail (labels != NULL);
    g_return_if_fail (rows > 0);
    g_return_if_fail (ok_cb != NULL);

    setup_widget (dialog, rows);
    gnome_cmd_string_dialog_set_title (dialog, title);
    gnome_cmd_string_dialog_set_userdata (dialog, user_data);
    gnome_cmd_string_dialog_set_ok_cb (dialog, ok_cb);
    gnome_cmd_string_dialog_set_cancel_cb (dialog, cancel_cb);

    if (labels)
        for (gint i=0; i<rows; i++)
            gnome_cmd_string_dialog_set_label (dialog, i, labels[i]);
}


void gnome_cmd_string_dialog_set_title (GnomeCmdStringDialog *dialog, const gchar *title)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    gtk_window_set_title (GTK_WINDOW (dialog), title);
}


void gnome_cmd_string_dialog_set_label (GnomeCmdStringDialog *dialog, gint row, const gchar *label)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (row >= 0 && row < dialog->rows);
    g_return_if_fail (label != NULL);

    gtk_label_set_text (GTK_LABEL (dialog->labels[row]), label);
}


void gnome_cmd_string_dialog_set_userdata (GnomeCmdStringDialog *dialog, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (dialog));

    priv->data = user_data;
}


void gnome_cmd_string_dialog_set_ok_cb (GnomeCmdStringDialog *dialog, GnomeCmdStringDialogCallback ok_cb)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (ok_cb != NULL);
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (dialog));

    priv->ok_cb = ok_cb;
}


void gnome_cmd_string_dialog_set_cancel_cb (GnomeCmdStringDialog *dialog, GnomeCmdCallback<GtkButton*> cancel_cb)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (dialog));

    priv->cancel_cb = cancel_cb;
}


void gnome_cmd_string_dialog_set_value (GnomeCmdStringDialog *dialog, gint row, const gchar *value)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (row >= 0 && row < dialog->rows);

    gtk_editable_set_text (GTK_EDITABLE (dialog->entries[row]), value ? value : "");
}


void gnome_cmd_string_dialog_set_error_desc (GnomeCmdStringDialog *dialog, gchar *msg)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (msg != NULL);
    auto priv = static_cast<GnomeCmdStringDialogPrivate*> (gnome_cmd_string_dialog_get_instance_private (dialog));

    g_free (priv->error_desc);

    priv->error_desc = g_strdup (msg);
}
