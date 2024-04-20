/**
 * @file gnome-cmd-string-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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

struct GnomeCmdStringDialog::Private
{
    GnomeCmdStringDialogCallback ok_cb;
    GnomeCmdCallback<GtkButton*> cancel_cb;
    gpointer data;
    gchar *error_desc;
};


G_DEFINE_TYPE (GnomeCmdStringDialog, gnome_cmd_string_dialog, GNOME_CMD_TYPE_DIALOG)


static void on_ok (GtkButton *button, GnomeCmdStringDialog *dialog)
{
    gboolean valid = TRUE;

    if (dialog->priv->ok_cb)
    {
        gchar **values = (gchar**) g_new (gpointer, dialog->rows);

        for (gint i=0; i<dialog->rows; i++)
            values[i] = (gchar*)gtk_entry_get_text (GTK_ENTRY (dialog->entries[i]));

        valid = dialog->priv->ok_cb (dialog, (const gchar**)values, dialog->priv->data);
        if (!valid)
            create_error_dialog ("%s", dialog->priv->error_desc);
        g_free (values);
    }

    if (valid)
        gtk_widget_hide (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdStringDialog *dialog)
{
    if (dialog->priv->cancel_cb)
        dialog->priv->cancel_cb (button, dialog->priv->data);

    gtk_widget_hide (GTK_WIDGET (dialog));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkWidget *object)
{
    GnomeCmdStringDialog *dialog = GNOME_CMD_STRING_DIALOG (object);

    GTK_WIDGET_CLASS (gnome_cmd_string_dialog_parent_class)->destroy (object);

    if (dialog->priv)
        g_free (dialog->priv->error_desc);

    g_free (dialog->priv);
    dialog->priv = NULL;
}


static void map (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (gnome_cmd_string_dialog_parent_class)->map (widget);
}


static void gnome_cmd_string_dialog_class_init (GnomeCmdStringDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    widget_class->destroy = destroy;
    widget_class->map = map;
}


static void gnome_cmd_string_dialog_init (GnomeCmdStringDialog *string_dialog)
{
    string_dialog->priv = g_new0 (GnomeCmdStringDialog::Private, 1);
    string_dialog->rows = -1;
}


inline void setup_widget (GnomeCmdStringDialog *string_dialog, gint rows)
{
    GtkWidget *dialog = GTK_WIDGET (string_dialog);
    GtkWidget *grid;
    GtkWidget *btn;

    string_dialog->rows = rows;
    string_dialog->labels = (GtkWidget**) g_new (gpointer, rows);
    string_dialog->entries = (GtkWidget**) g_new (gpointer, rows);
    string_dialog->priv->error_desc = g_strdup (_("No error description available"));

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

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL, G_CALLBACK (on_cancel), string_dialog);
    btn = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK, G_CALLBACK (on_ok), string_dialog);

    gtk_widget_grab_focus (string_dialog->entries[0]);
    gtk_widget_grab_default (btn);
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

    dialog->priv->data = user_data;
}


void gnome_cmd_string_dialog_set_ok_cb (GnomeCmdStringDialog *dialog, GnomeCmdStringDialogCallback ok_cb)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (ok_cb != NULL);

    dialog->priv->ok_cb = ok_cb;
}


void gnome_cmd_string_dialog_set_cancel_cb (GnomeCmdStringDialog *dialog, GnomeCmdCallback<GtkButton*> cancel_cb)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    dialog->priv->cancel_cb = cancel_cb;
}


void gnome_cmd_string_dialog_set_value (GnomeCmdStringDialog *dialog, gint row, const gchar *value)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (row >= 0 && row < dialog->rows);

    gtk_entry_set_text (GTK_ENTRY (dialog->entries[row]), value?value:"");
}


void gnome_cmd_string_dialog_set_error_desc (GnomeCmdStringDialog *dialog, gchar *msg)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (msg != NULL);

    g_free (dialog->priv->error_desc);

    dialog->priv->error_desc = g_strdup (msg);
}
