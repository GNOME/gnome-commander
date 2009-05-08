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
#include "gnome-cmd-rename-dialog.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-style.h"
#include "utils.h"

using namespace std;


struct GnomeCmdRenameDialogPrivate
{
    GnomeCmdFile *f;
    GnomeCmdMainWin *mw;
    GtkEntry *textbox;
};


static GtkWindowClass *parent_class = NULL;


static gboolean on_dialog_keypressed (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    GnomeCmdRenameDialog *dialog = GNOME_CMD_RENAME_DIALOG(widget);

    if (dialog->priv->textbox->editable && event->type == GDK_KEY_PRESS)
        if (gtk_im_context_filter_keypress (dialog->priv->textbox->im_context, event))
            return TRUE;

    switch (event->keyval)
    {
        case GDK_Escape:
            gnome_cmd_file_unref (dialog->priv->f);
            gtk_widget_destroy(widget);
            return TRUE;

        case GDK_Return:
        case GDK_KP_Enter:
            {
                gchar *new_fname = g_strdup (gtk_entry_get_text (dialog->priv->textbox));
                GnomeVFSResult result = gnome_cmd_file_rename (dialog->priv->f, new_fname);

                if (result==GNOME_VFS_OK)
                    gnome_cmd_main_win_get_fs (main_win, ACTIVE)->file_list()->focus_file(new_fname, TRUE);

                gnome_cmd_file_unref (dialog->priv->f);
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
    gnome_cmd_file_unref (GNOME_CMD_RENAME_DIALOG(widget)->priv->f);
    gtk_widget_destroy (widget);
    return TRUE;
}


/*******************************
 * Gtk class implementation
 *******************************/
static void destroy (GtkObject *object)
{
    GnomeCmdRenameDialog *dialog = GNOME_CMD_RENAME_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdRenameDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkWindowClass *) gtk_type_class (gtk_window_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdRenameDialog *dialog)
{
    dialog->priv = g_new0 (GnomeCmdRenameDialogPrivate, 1);
    gtk_signal_connect (GTK_OBJECT (dialog), "key-press-event", GTK_SIGNAL_FUNC (on_dialog_keypressed), NULL);
    gtk_signal_connect (GTK_OBJECT (dialog), "focus-out-event", GTK_SIGNAL_FUNC (on_focus_out), NULL);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_rename_dialog_new (GnomeCmdFile *f, gint x, gint y, gint width, gint height)
{
    g_return_val_if_fail (f != NULL, NULL);

    GnomeCmdRenameDialog *dialog = (GnomeCmdRenameDialog *) gtk_type_new (gnome_cmd_rename_dialog_get_type ());

    dialog->priv->f = f;
    gnome_cmd_file_ref (f);

    gtk_window_set_has_frame (GTK_WINDOW (dialog), 0);
    gtk_window_set_decorated (GTK_WINDOW (dialog), 0);
    gtk_widget_set_uposition (GTK_WIDGET (dialog), x, y);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);

    gtk_widget_set_size_request (GTK_WIDGET (dialog), width+1, height+1);
    dialog->priv->textbox = GTK_ENTRY (gtk_entry_new ());
    gtk_widget_set_size_request (GTK_WIDGET (dialog->priv->textbox), width, height);
    gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (dialog->priv->textbox));
    gtk_widget_set_style (GTK_WIDGET (dialog->priv->textbox), list_style);

    gchar *fname = get_utf8 (gnome_cmd_file_get_name (f));

    gtk_entry_set_text (dialog->priv->textbox, fname);

    gtk_widget_grab_focus (GTK_WIDGET (dialog->priv->textbox));
    gtk_entry_select_region (dialog->priv->textbox, 0, -1);
    gtk_widget_show (GTK_WIDGET (dialog->priv->textbox));

    g_free (fname);

    return GTK_WIDGET (dialog);
}


GtkType gnome_cmd_rename_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdRenameDialog",
            sizeof (GnomeCmdRenameDialog),
            sizeof (GnomeCmdRenameDialogClass),
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
