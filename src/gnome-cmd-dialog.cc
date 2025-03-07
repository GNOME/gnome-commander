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
#include "gnome-cmd-includes.h"
#include "text-utils.h"
#include "gnome-cmd-dialog.h"
#include "widget-factory.h"

struct GnomeCmdDialogPrivate
{
    GList *buttons;
    GtkWidget *content;
    GtkWidget *buttonbox;
    GtkSizeGroup *buttonbox_size_group;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdDialog, gnome_cmd_dialog, GTK_TYPE_WINDOW)


static gboolean on_dialog_keypressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    if (keyval == GDK_KEY_Escape)
    {
        gtk_window_destroy (GTK_WINDOW (user_data));
        return TRUE;
    }

    return FALSE;
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
    gtk_window_set_title (GTK_WINDOW (dialog), " ");

    GtkWidget *vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 0);
    gtk_widget_set_margin_top (GTK_WIDGET (vbox), 12);
    gtk_widget_set_margin_bottom (GTK_WIDGET (vbox), 12);
    gtk_widget_set_margin_start (GTK_WIDGET (vbox), 12);
    gtk_widget_set_margin_end (GTK_WIDGET (vbox), 12);
    gtk_box_set_spacing (GTK_BOX (vbox), 12);
    gtk_window_set_child (GTK_WINDOW (dialog), vbox);

    priv->content = create_vbox (GTK_WIDGET (dialog), FALSE, 18);
    gtk_widget_set_vexpand (priv->content, TRUE);
    gtk_box_append (GTK_BOX (vbox), priv->content);

    GtkWidget *button_bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    priv->buttonbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    priv->buttonbox_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    gtk_widget_set_hexpand (priv->buttonbox, TRUE);
    gtk_widget_set_halign (priv->buttonbox, GTK_ALIGN_END);
    gtk_box_append (GTK_BOX (button_bar), priv->buttonbox);
    gtk_box_append (GTK_BOX (vbox), button_bar);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (dialog), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_dialog_keypressed), dialog);
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_dialog_new (GtkWindow *parent_window, const gchar *title)
{
    GnomeCmdDialog *dialog = (GnomeCmdDialog *) g_object_new (GNOME_CMD_TYPE_DIALOG,
        "transient-for", parent_window,
        NULL);

    if (title)
        gtk_window_set_title (GTK_WINDOW (dialog), title);

    return GTK_WIDGET (dialog);
}


GtkWidget *gnome_cmd_dialog_add_button (GnomeCmdDialog *dialog, const gchar *label, GCallback on_click, gpointer data)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIALOG (dialog), NULL);
    auto priv = static_cast<GnomeCmdDialogPrivate*>(gnome_cmd_dialog_get_instance_private (dialog));

    GtkWidget *btn = create_button_with_data (GTK_WIDGET (dialog), label, on_click, data);

    gtk_size_group_add_widget (priv->buttonbox_size_group, GTK_WIDGET (btn));
    gtk_box_append (GTK_BOX (priv->buttonbox), btn);
    gtk_window_set_default_widget (GTK_WINDOW (dialog), btn);
    gtk_widget_grab_focus (btn);

    priv->buttons = g_list_append (priv->buttons, btn);

    return btn;
}


void gnome_cmd_dialog_add_category (GnomeCmdDialog *dialog, GtkWidget *category)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (GTK_IS_WIDGET (category));
    auto priv = static_cast<GnomeCmdDialogPrivate*>(gnome_cmd_dialog_get_instance_private (dialog));

    gtk_box_append (GTK_BOX (priv->content), category);
}


void gnome_cmd_dialog_add_expanding_category (GnomeCmdDialog *dialog, GtkWidget *category)
{
    g_return_if_fail (GNOME_CMD_IS_DIALOG (dialog));
    g_return_if_fail (GTK_IS_WIDGET (category));
    auto priv = static_cast<GnomeCmdDialogPrivate*>(gnome_cmd_dialog_get_instance_private (dialog));

    gtk_widget_set_hexpand (category, TRUE);
    gtk_widget_set_vexpand (category, TRUE);
    gtk_box_append (GTK_BOX (priv->content), category);
}
