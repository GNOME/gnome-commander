/**
 * @file gnome-cmd-rename-dialog.cc
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
#include "gnome-cmd-file.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"
#include "dialogs/gnome-cmd-rename-dialog.h"

using namespace std;


struct GnomeCmdRenameDialogPrivate
{
    GnomeCmdFile *f;
    GtkEntry *textbox;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdRenameDialog, gnome_cmd_rename_dialog, GTK_TYPE_POPOVER)


static gboolean on_dialog_keypressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    GnomeCmdRenameDialog *dialog = GNOME_CMD_RENAME_DIALOG (user_data);
    auto priv = static_cast<GnomeCmdRenameDialogPrivate *> (gnome_cmd_rename_dialog_get_instance_private (dialog));

    switch (keyval)
    {
        case GDK_KEY_Escape:
            {
                gtk_popover_popdown (GTK_POPOVER (dialog));
                gtk_widget_grab_focus (GTK_WIDGET (main_win->fs(ACTIVE)->file_list()));
                priv->f->unref();
                return TRUE;
            }
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            {
                GError *error = nullptr;
                gchar *new_fname = g_strdup (gtk_editable_get_text (GTK_EDITABLE (priv->textbox)));
                gboolean result = priv->f->rename(new_fname, &error);

                if (result)
                {
                    GnomeCmdFileList *fl = main_win->fs(ACTIVE)->file_list();
                    fl->focus_file(new_fname, TRUE);
                    gtk_widget_grab_focus (GTK_WIDGET (fl));
                }

                priv->f->unref();
                gtk_popover_popdown (GTK_POPOVER (dialog));

                if (!result)
                {
                    gnome_cmd_error_message (*main_win, new_fname, error);
                }

                g_free (new_fname);
            }
            return TRUE;

        case GDK_KEY_F2:
        case GDK_KEY_F5:
        case GDK_KEY_F6:
            gnome_cmd_toggle_file_name_selection (GTK_WIDGET (priv->textbox));
            return TRUE;

        default:
            return FALSE;
    }
}


static void gnome_cmd_rename_dialog_class_init (GnomeCmdRenameDialogClass *klass)
{
}


static void gnome_cmd_rename_dialog_init (GnomeCmdRenameDialog *dialog)
{
    auto priv = static_cast<GnomeCmdRenameDialogPrivate *> (gnome_cmd_rename_dialog_get_instance_private (dialog));

    priv->textbox = GTK_ENTRY (gtk_entry_new ());
    gtk_popover_set_child (GTK_POPOVER (dialog), GTK_WIDGET (priv->textbox));

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (priv->textbox), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_dialog_keypressed), dialog);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_rename_dialog_new (GnomeCmdFile *f, GtkWidget *parent, gint x, gint y, gint width, gint height)
{
    g_return_val_if_fail (f != NULL, NULL);

    GnomeCmdRenameDialog *dialog = (GnomeCmdRenameDialog *) g_object_new (GNOME_CMD_TYPE_RENAME_DIALOG, NULL);
    auto priv = static_cast<GnomeCmdRenameDialogPrivate *> (gnome_cmd_rename_dialog_get_instance_private (dialog));

    priv->f = f->ref();

    gtk_widget_set_parent (GTK_WIDGET (dialog), parent);
    GdkRectangle rect = { x, y, width, height };
    gtk_popover_set_pointing_to (GTK_POPOVER (dialog), &rect);

    gtk_widget_set_size_request (GTK_WIDGET (priv->textbox), width, height);
    gtk_editable_set_text (GTK_EDITABLE (priv->textbox), f->get_name());

    gtk_widget_grab_focus (GTK_WIDGET (priv->textbox));
    gtk_editable_select_region (GTK_EDITABLE (priv->textbox), 0, -1);
    gtk_widget_show (GTK_WIDGET (priv->textbox));

    return GTK_WIDGET (dialog);
}
