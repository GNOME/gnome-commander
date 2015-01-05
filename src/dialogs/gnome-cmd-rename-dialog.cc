/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2015 Uwe Scholz

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
#include "gnome-cmd-file.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "utils.h"
#include "dialogs/gnome-cmd-rename-dialog.h"

using namespace std;


struct GnomeCmdRenameDialogPrivate
{
    GnomeCmdFile *f;
    GnomeCmdMainWin *mw;
    GtkEntry *textbox;
};


G_DEFINE_TYPE (GnomeCmdRenameDialog, gnome_cmd_rename_dialog, GTK_TYPE_WINDOW)


static gboolean on_dialog_keypressed (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    GnomeCmdRenameDialog *dialog = GNOME_CMD_RENAME_DIALOG(widget);

    if (dialog->priv->textbox->editable && event->type == GDK_KEY_PRESS)
        if (gtk_im_context_filter_keypress (dialog->priv->textbox->im_context, event))
            return TRUE;

    switch (event->keyval)
    {
        case GDK_Escape:
            dialog->priv->f->unref();
            gtk_widget_destroy(widget);
            return TRUE;

        case GDK_Return:
        case GDK_KP_Enter:
            {
                gchar *new_fname = g_strdup (gtk_entry_get_text (dialog->priv->textbox));
                GnomeVFSResult result = dialog->priv->f->rename(new_fname);

                if (result==GNOME_VFS_OK)
                    main_win->fs(ACTIVE)->file_list()->focus_file(new_fname, TRUE);

                dialog->priv->f->unref();
                gtk_widget_destroy (widget);

                if (result!=GNOME_VFS_OK)
                    gnome_cmd_show_message (NULL, new_fname, gnome_vfs_result_to_string (result));

                g_free (new_fname);
            }
            return TRUE;

        case GDK_F2:
        case GDK_F5:
        case GDK_F6:
            gnome_cmd_toggle_file_name_selection (GTK_WIDGET (dialog->priv->textbox));
            return TRUE;

        default:
            return FALSE;
    }
}


static gboolean on_focus_out (GtkWidget *widget, GdkEventKey *event)
{
    GNOME_CMD_RENAME_DIALOG(widget)->priv->f->unref();
    gtk_widget_destroy (widget);
    return TRUE;
}


static void gnome_cmd_rename_dialog_finalize (GObject *object)
{
    GnomeCmdRenameDialog *dialog = GNOME_CMD_RENAME_DIALOG (object);

    g_free (dialog->priv);

    G_OBJECT_CLASS (gnome_cmd_rename_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_rename_dialog_class_init (GnomeCmdRenameDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_rename_dialog_finalize;
}


static void gnome_cmd_rename_dialog_init (GnomeCmdRenameDialog *dialog)
{
    dialog->priv = g_new0 (GnomeCmdRenameDialogPrivate, 1);
    g_signal_connect (dialog, "key-press-event", G_CALLBACK (on_dialog_keypressed), NULL);
    g_signal_connect (dialog, "focus-out-event", G_CALLBACK (on_focus_out), NULL);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_rename_dialog_new (GnomeCmdFile *f, gint x, gint y, gint width, gint height)
{
    g_return_val_if_fail (f != NULL, NULL);

    GnomeCmdRenameDialog *dialog = (GnomeCmdRenameDialog *) g_object_new (GNOME_CMD_TYPE_RENAME_DIALOG, NULL);
    dialog->priv->f = f->ref();

    gtk_window_set_has_frame (GTK_WINDOW (dialog), 0);
    gtk_window_set_decorated (GTK_WINDOW (dialog), 0);
    gtk_widget_set_uposition (GTK_WIDGET (dialog), x, y);
    gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);

    gtk_widget_set_size_request (GTK_WIDGET (dialog), width+1, height+1);
    dialog->priv->textbox = GTK_ENTRY (gtk_entry_new ());
    gtk_widget_set_size_request (GTK_WIDGET (dialog->priv->textbox), width, height);
    gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (dialog->priv->textbox));
    gtk_widget_set_style (GTK_WIDGET (dialog->priv->textbox), list_style);

    gchar *fname = get_utf8 (f->get_name());

    gtk_entry_set_text (dialog->priv->textbox, fname);

    gtk_widget_grab_focus (GTK_WIDGET (dialog->priv->textbox));
    gtk_editable_select_region (GTK_EDITABLE (dialog->priv->textbox), 0, -1);
    gtk_widget_show (GTK_WIDGET (dialog->priv->textbox));

    g_free (fname);

    return GTK_WIDGET (dialog);
}
