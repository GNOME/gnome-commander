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
#include "libgcmd-widget-factory.h"

struct _GnomeCmdDialogPrivate
{
    GtkWidget *content;
    GtkWidget *buttonbox;
};


static GtkWindowClass *parent_class = NULL;
extern GtkWidget *main_win;


static gboolean on_dialog_keypressed (GtkWidget *dialog, GdkEventKey *event, gpointer user_data)
{
    if (event->keyval == GDK_Escape)
    {
        gtk_widget_destroy (dialog);
        return TRUE;
    }

    return FALSE;
}


static void on_dialog_show (GtkWidget *w, GnomeCmdDialog *dialog)
{
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdDialog *dialog = GNOME_CMD_DIALOG (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);

    g_free (dialog->priv);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = gtk_type_class (gtk_window_get_type ());
    object_class->destroy = destroy;
    widget_class->map = map;
}


static void init (GnomeCmdDialog *dialog)
{
    GtkWidget *vbox;

    dialog->buttons = NULL;
    dialog->priv = g_new0 (GnomeCmdDialogPrivate, 1);

    gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, TRUE);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (dialog), " ");
    gnome_cmd_dialog_set_transient_for (GNOME_CMD_DIALOG (dialog), GTK_WINDOW (main_win_widget));
    gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

    vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
    gtk_box_set_spacing (GTK_BOX (vbox), 12);
    gtk_container_add (GTK_CONTAINER (dialog), vbox);

    dialog->priv->content = create_vbox (GTK_WIDGET (dialog), FALSE, 18);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->content, TRUE, TRUE, 0);

    dialog->priv->buttonbox = create_hbuttonbox (GTK_WIDGET (dialog));
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->priv->buttonbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->buttonbox, FALSE, TRUE, 0);

    gtk_signal_connect (GTK_OBJECT (dialog), "key-press-event", (GtkSignalFunc)on_dialog_keypressed, NULL);
    gtk_signal_connect (GTK_OBJECT (dialog), "show", (GtkSignalFunc)on_dialog_show, dialog);
}


/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdDialog",
            sizeof (GnomeCmdDialog),
            sizeof (GnomeCmdDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gtk_window_get_type (), &dlg_info);
    }
    return dlg_type;
}


GtkWidget *gnome_cmd_dialog_new (const gchar *title)
{
    GnomeCmdDialog *dialog = gtk_type_new (gnome_cmd_dialog_get_type ());

    if (title)
        gnome_cmd_dialog_setup (dialog, title);

    return GTK_WIDGET (dialog);
}


void gnome_cmd_dialog_setup (GnomeCmdDialog *dialog, const gchar *title)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (title != NULL);

    if (title)
        gtk_window_set_title (GTK_WINDOW (dialog), title);
}


GtkWidget *gnome_cmd_dialog_add_button (GnomeCmdDialog *dialog, const gchar *stock_id, GtkSignalFunc on_click, gpointer data)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIALOG (dialog), NULL);

    GtkWidget *btn = create_stock_button_with_data (GTK_WIDGET (dialog), (gpointer) stock_id, on_click, data);

    gtk_box_pack_start (GTK_BOX (dialog->priv->buttonbox), btn, FALSE, TRUE, 0);
    g_object_set (G_OBJECT (btn), "can-default", TRUE, NULL);
    gtk_widget_grab_default (btn);
    gtk_widget_grab_focus (btn);

    dialog->buttons = g_list_append (dialog->buttons, btn);

    return btn;
}


void gnome_cmd_dialog_add_category (GnomeCmdDialog *dialog, GtkWidget *category)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (GTK_IS_WIDGET (category));

    gtk_box_pack_start (GTK_BOX (dialog->priv->content), category, FALSE, TRUE, 0);
}


void gnome_cmd_dialog_add_expanding_category (GnomeCmdDialog *dialog, GtkWidget *category)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (GTK_IS_WIDGET (category));

    gtk_box_pack_start (GTK_BOX (dialog->priv->content), category, TRUE, TRUE, 0);
}


void gnome_cmd_dialog_editable_enters (GnomeCmdDialog *dialog, GtkEditable *editable)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (GTK_IS_EDITABLE (editable));

    g_signal_connect_swapped(editable, "activate", G_CALLBACK(gtk_window_activate_default), dialog);
}


void gnome_cmd_dialog_set_transient_for (GnomeCmdDialog *dialog, GtkWindow *win)
{
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (win));
}


void gnome_cmd_dialog_set_modal (GnomeCmdDialog *dialog)
{
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
}


void gnome_cmd_dialog_set_resizable (GnomeCmdDialog *dialog, gboolean value)
{
    gtk_window_set_resizable (GTK_WINDOW (dialog), value);
    gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, value, !value);
}
