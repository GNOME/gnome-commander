/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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


struct _GnomeCmdRenameDialogPrivate
{
    GnomeCmdFile *finfo;
    GnomeCmdMainWin *mw;
    GtkEntry *textbox;
};


static GtkWindowClass *parent_class = NULL;


static gboolean
on_dialog_keypressed (GtkWidget *widget,
                      GdkEventKey *event,
                      gpointer user_data)
{
    GnomeCmdRenameDialog *dialog = GNOME_CMD_RENAME_DIALOG(widget);

    switch (event->keyval)
    {
        case GDK_Escape:
            gnome_cmd_file_unref (dialog->priv->finfo);
            gtk_widget_destroy(widget);
            return TRUE;

        case GDK_Return:
        case GDK_KP_Enter:
            {
                const gchar *new_fname = gtk_entry_get_text (dialog->priv->textbox);
                GnomeVFSResult ret = gnome_cmd_file_rename (dialog->priv->finfo, new_fname);

                gnome_cmd_file_unref (dialog->priv->finfo);
                gtk_widget_destroy (widget);
                // TODO: if (ret != GNOME_VFS_OK)
            }
            return TRUE;

        default:
            return FALSE;
    }
}


static gboolean
on_focus_out (GtkWidget *widget,
                      GdkEventKey *event)
{
    gnome_cmd_file_unref (GNOME_CMD_RENAME_DIALOG(widget)->priv->finfo);
    gtk_widget_destroy (widget);
    return TRUE;
}


/*******************************
 * Gtk class implementation
 *******************************/
static void
destroy (GtkObject *object)
{
    GnomeCmdRenameDialog *dialog = GNOME_CMD_RENAME_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdRenameDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkWindowClass *) gtk_type_class (gtk_window_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void
init (GnomeCmdRenameDialog *dialog)
{
    dialog->priv = g_new (GnomeCmdRenameDialogPrivate, 1);
    gtk_signal_connect (GTK_OBJECT (dialog), "key-press-event", (GtkSignalFunc)on_dialog_keypressed, NULL);
    gtk_signal_connect (GTK_OBJECT (dialog), "focus-out-event", (GtkSignalFunc)on_focus_out, NULL);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_rename_dialog_new (GnomeCmdFile *finfo, gint x, gint y, gint width, gint height)
{
    g_return_val_if_fail (finfo != NULL, NULL);

    GnomeCmdRenameDialog *dialog = (GnomeCmdRenameDialog *) gtk_type_new (gnome_cmd_rename_dialog_get_type ());

    dialog->priv->finfo = finfo;
    gnome_cmd_file_ref (finfo);

    gchar *fname = get_utf8 (gnome_cmd_file_get_name (finfo));

    gtk_window_set_has_frame (GTK_WINDOW(dialog), 0);
    gtk_window_set_decorated (GTK_WINDOW(dialog), 0);
    gtk_widget_set_uposition (GTK_WIDGET(dialog), x, y);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW(dialog), TRUE);

    gtk_widget_set_size_request (GTK_WIDGET(dialog), width + 1, height + 1);
    dialog->priv->textbox = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_size_request (GTK_WIDGET(dialog->priv->textbox), width, height);
    gtk_container_add (GTK_CONTAINER(dialog), GTK_WIDGET(dialog->priv->textbox));
    gtk_widget_set_style (GTK_WIDGET(dialog->priv->textbox), list_style);

    gtk_entry_set_text(dialog->priv->textbox, fname);
    gtk_entry_select_region (dialog->priv->textbox, 0, -1);

    gtk_widget_show(GTK_WIDGET(dialog->priv->textbox));

    return GTK_WIDGET (dialog);
}


GtkType
gnome_cmd_rename_dialog_get_type         (void)
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

