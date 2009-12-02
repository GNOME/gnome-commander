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
#include "libgcmd-deps.h"
#include "gnome-cmd-dialog.h"
#include "gnome-cmd-string-dialog.h"
#include "libgcmd-widget-factory.h"

struct _GnomeCmdStringDialogPrivate
{
    GnomeCmdStringDialogCallback ok_cb;
    GFunc cancel_cb;
    gpointer data;
    GtkWidget *dialog;
    gchar *error_desc;
};


static GnomeCmdDialogClass *parent_class = NULL;


static void
on_ok (GtkButton *button, GnomeCmdStringDialog *dialog)
{
    gboolean valid = TRUE;

    if (dialog->priv->ok_cb) {
        gint i;
        gchar **values = (gchar**)g_new (gpointer, dialog->rows);

        for (i=0; i<dialog->rows; i++)
            values[i] = (gchar*)gtk_entry_get_text (GTK_ENTRY (dialog->entries[i]));

        valid = dialog->priv->ok_cb (dialog, (const gchar**)values, dialog->priv->data);
        if (!valid)
            create_error_dialog (dialog->priv->error_desc);
        g_free (values);
    }

    if (valid)
        gtk_widget_hide (GTK_WIDGET (dialog));
}


static void
on_cancel (GtkButton *button, GnomeCmdStringDialog *dialog)
{
    if (dialog->priv->cancel_cb)
        dialog->priv->cancel_cb (button, dialog->priv->data);

    gtk_widget_hide (GTK_WIDGET (dialog));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdStringDialog *dialog = GNOME_CMD_STRING_DIALOG (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);

    if (dialog->priv)
        g_free (dialog->priv->error_desc);
 
    g_free (dialog->priv);
    dialog->priv = NULL;
}


static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdStringDialogClass *klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = GTK_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = gtk_type_class (gnome_cmd_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = map;
}

static void
init (GnomeCmdStringDialog *string_dialog)
{
    string_dialog->priv = g_new0 (GnomeCmdStringDialogPrivate, 1);
    string_dialog->rows = -1;
}


static void
setup_widget (GnomeCmdStringDialog *string_dialog, gint rows)
{
    gint i;
    GtkWidget *dialog = GTK_WIDGET (string_dialog);
    GtkWidget *table;
    GtkWidget *btn;

    string_dialog->rows = rows;
    string_dialog->labels = (GtkWidget**)g_new (gpointer, rows);
    string_dialog->entries = (GtkWidget**)g_new (gpointer, rows);
    string_dialog->priv->error_desc = g_strdup (_("No error description available"));

    table = create_table (dialog, rows, 2);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), table);

    for (i=0; i<rows; i++) {
        string_dialog->labels[i] = create_label (dialog, "");
        table_add (table, string_dialog->labels[i], 0, i, GTK_FILL);

        string_dialog->entries[i] = create_entry (dialog, "entry", "");
        gtk_entry_set_activates_default (GTK_ENTRY (string_dialog->entries[i]), TRUE);
        table_add (table, string_dialog->entries[i], 1, i, GTK_FILL|GTK_EXPAND);
    }

    gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog),
        GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_cancel), string_dialog);
    btn = gnome_cmd_dialog_add_button (
        GNOME_CMD_DIALOG (dialog),
        GTK_STOCK_OK, GTK_SIGNAL_FUNC (on_ok), string_dialog);

    gtk_widget_grab_focus (string_dialog->entries[0]);
    gtk_widget_grab_default (btn);
}


/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_string_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdStringDialog",
            sizeof (GnomeCmdStringDialog),
            sizeof (GnomeCmdStringDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}


GtkWidget*
gnome_cmd_string_dialog_new_with_cancel (const gchar *title,
                                         const gchar **labels,
                                         gint rows,
                                         GnomeCmdStringDialogCallback ok_cb,
                                         GtkSignalFunc cancel_cb,
                                         gpointer user_data)
{
    GnomeCmdStringDialog *dialog = gtk_type_new (gnome_cmd_string_dialog_get_type ());

    gnome_cmd_string_dialog_setup_with_cancel (dialog, title, labels, rows, ok_cb, cancel_cb, user_data);

    return GTK_WIDGET (dialog);
}


GtkWidget*
gnome_cmd_string_dialog_new (const gchar *title,
                             const gchar **labels,
                             gint rows,
                             GnomeCmdStringDialogCallback ok_cb,
                             gpointer user_data)
{
    return gnome_cmd_string_dialog_new_with_cancel (title, labels, rows, ok_cb, NULL, user_data);
}


void
gnome_cmd_string_dialog_setup_with_cancel (GnomeCmdStringDialog *dialog,
                                           const gchar *title,
                                           const gchar **labels,
                                           gint rows,
                                           GnomeCmdStringDialogCallback ok_cb,
                                           GtkSignalFunc cancel_cb,
                                           gpointer user_data)
{
    gint i;

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
        for (i=0; i<rows; i++)
            gnome_cmd_string_dialog_set_label (dialog, i, labels[i]);
}


void
gnome_cmd_string_dialog_setup (GnomeCmdStringDialog *dialog,
                               const gchar *title,
                               const gchar **labels,
                               gint rows,
                               GnomeCmdStringDialogCallback ok_cb,
                               gpointer user_data)
{
    gnome_cmd_string_dialog_setup_with_cancel (dialog, title, labels, rows, ok_cb, NULL, user_data);
}


void
gnome_cmd_string_dialog_set_hidden (GnomeCmdStringDialog *dialog, gint row, gboolean hidden)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (row >= 0 && row < dialog->rows);

    gtk_entry_set_visibility (GTK_ENTRY (dialog->entries[row]), !hidden);
}


void
gnome_cmd_string_dialog_set_title (GnomeCmdStringDialog *dialog, const gchar *title)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    gtk_window_set_title (GTK_WINDOW (dialog), title);
}


void
gnome_cmd_string_dialog_set_label (GnomeCmdStringDialog *dialog, gint row, const gchar *label)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (row >= 0 && row < dialog->rows);
    g_return_if_fail (label != NULL);

    gtk_label_set_text (GTK_LABEL (dialog->labels[row]), label);
}


void
gnome_cmd_string_dialog_set_userdata (GnomeCmdStringDialog *dialog, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    dialog->priv->data = user_data;
}


void
gnome_cmd_string_dialog_set_ok_cb (GnomeCmdStringDialog *dialog, GnomeCmdStringDialogCallback ok_cb)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (ok_cb != NULL);

    dialog->priv->ok_cb = ok_cb;
}


void
gnome_cmd_string_dialog_set_cancel_cb (GnomeCmdStringDialog *dialog, GtkSignalFunc cancel_cb)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));

    dialog->priv->cancel_cb = (GFunc)cancel_cb;
}


void
gnome_cmd_string_dialog_set_value (GnomeCmdStringDialog *dialog, gint row, const gchar *value)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (row >= 0 && row < dialog->rows);

    gtk_entry_set_text (GTK_ENTRY (dialog->entries[row]), value?value:"");
}


void
gnome_cmd_string_dialog_set_error_desc (GnomeCmdStringDialog *dialog, gchar *msg)
{
    g_return_if_fail (GNOME_CMD_IS_STRING_DIALOG (dialog));
    g_return_if_fail (msg != NULL);

    g_free (dialog->priv->error_desc);

    dialog->priv->error_desc = g_strdup (msg);
}
