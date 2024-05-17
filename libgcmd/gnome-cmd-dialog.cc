/** 
 * @file gnome-cmd-dialog.cc
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
#include "libgcmd-widget-factory.h"

struct GnomeCmdDialogPrivate
{
    GList *buttons;
    GtkWidget *content;
    GtkWidget *buttonbox;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdDialog, gnome_cmd_dialog, GTK_TYPE_WINDOW)


extern GtkWidget *main_win;


static gboolean on_dialog_keypressed (GtkWidget *dialog, GdkEventKey *event, gpointer user_data)
{
    if (event->keyval == GDK_KEY_Escape)
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

static void gnome_cmd_dialog_class_init (GnomeCmdDialogClass *klass)
{
}


static void gnome_cmd_dialog_init (GnomeCmdDialog *dialog)
{
    auto priv = static_cast<GnomeCmdDialogPrivate*>(gnome_cmd_dialog_get_instance_private (dialog));

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (dialog), " ");
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_win_widget));
    gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

    GtkWidget *vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
    gtk_box_set_spacing (GTK_BOX (vbox), 12);
    gtk_container_add (GTK_CONTAINER (dialog), vbox);

    priv->content = create_vbox (GTK_WIDGET (dialog), FALSE, 18);
    gtk_box_pack_start (GTK_BOX (vbox), priv->content, TRUE, TRUE, 0);

    priv->buttonbox = create_hbuttonbox (GTK_WIDGET (dialog));
    gtk_button_box_set_layout (GTK_BUTTON_BOX (priv->buttonbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (vbox), priv->buttonbox, FALSE, TRUE, 0);

    g_signal_connect (dialog, "key-press-event", G_CALLBACK (on_dialog_keypressed), NULL);
    g_signal_connect (dialog, "show", G_CALLBACK (on_dialog_show), dialog);
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_dialog_new (const gchar *title)
{
    GnomeCmdDialog *dialog = (GnomeCmdDialog *) g_object_new (GNOME_CMD_TYPE_DIALOG, NULL);

    if (title)
        gtk_window_set_title (GTK_WINDOW (dialog), title);

    return GTK_WIDGET (dialog);
}


GtkWidget *gnome_cmd_dialog_add_button (GnomeCmdDialog *dialog, const gchar *label, GCallback on_click, gpointer data)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIALOG (dialog), NULL);
    auto priv = static_cast<GnomeCmdDialogPrivate*>(gnome_cmd_dialog_get_instance_private (dialog));

    GtkWidget *btn = create_button_with_data (GTK_WIDGET (dialog), label, on_click, data);

    gtk_box_pack_start (GTK_BOX (priv->buttonbox), btn, FALSE, TRUE, 0);
    g_object_set (G_OBJECT (btn), "can-default", TRUE, NULL);
    gtk_widget_grab_default (btn);
    gtk_widget_grab_focus (btn);

    priv->buttons = g_list_append (priv->buttons, btn);

    return btn;
}


void gnome_cmd_dialog_add_category (GnomeCmdDialog *dialog, GtkWidget *category)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (GTK_IS_WIDGET (category));
    auto priv = static_cast<GnomeCmdDialogPrivate*>(gnome_cmd_dialog_get_instance_private (dialog));

    gtk_box_pack_start (GTK_BOX (priv->content), category, FALSE, TRUE, 0);
}


void gnome_cmd_dialog_add_expanding_category (GnomeCmdDialog *dialog, GtkWidget *category)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (GTK_IS_WIDGET (category));
    auto priv = static_cast<GnomeCmdDialogPrivate*>(gnome_cmd_dialog_get_instance_private (dialog));

    gtk_box_pack_start (GTK_BOX (priv->content), category, TRUE, TRUE, 0);
}
