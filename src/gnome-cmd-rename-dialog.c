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
#include "utils.h"


struct _GnomeCmdRenameDialogPrivate
{
    GnomeCmdFile *finfo;
    GnomeCmdMainWin *mw;
};


static GnomeCmdStringDialogClass *parent_class = NULL;



static gboolean
on_ok (GnomeCmdStringDialog *string_dialog,
       const gchar **values,
       GnomeCmdRenameDialog *dialog)
{
    GnomeVFSResult ret;
    const gchar *filename = values[0];

    g_return_val_if_fail (dialog, TRUE);
    g_return_val_if_fail (dialog->priv, TRUE);
    g_return_val_if_fail (dialog->priv->finfo, TRUE);

    if (!filename) {
        gnome_cmd_string_dialog_set_error_desc (
            string_dialog, g_strdup (_("No filename entered")));
        return FALSE;
    }

    ret = gnome_cmd_file_rename (dialog->priv->finfo, filename);
    if (ret == GNOME_VFS_OK) {
        gnome_cmd_file_list_focus_file (gnome_cmd_main_win_get_active_fs (main_win)->list,
                                        filename, TRUE);
        gnome_cmd_file_unref (dialog->priv->finfo);
        return TRUE;
    }

    gnome_cmd_string_dialog_set_error_desc (GNOME_CMD_STRING_DIALOG (dialog),
                                            g_strdup (gnome_vfs_result_to_string (ret)));

    return FALSE;
}


static void
on_cancel (GtkWidget *widget, GnomeCmdRenameDialog *dialog)
{
    gnome_cmd_file_unref (dialog->priv->finfo);
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
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = GTK_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = gtk_type_class (gnome_cmd_string_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = map;
}


static void
init (GnomeCmdRenameDialog *dialog)
{
    dialog->priv = g_new (GnomeCmdRenameDialogPrivate, 1);
}



/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_rename_dialog_new (GnomeCmdFile *finfo)
{
    const gchar *labels[] = {""};
    gchar *msg;
    GtkWidget *msg_label;
    gchar *fname;
    GnomeCmdRenameDialog *dialog;

    g_return_val_if_fail (finfo != NULL, NULL);

    dialog = gtk_type_new (gnome_cmd_rename_dialog_get_type ());

    dialog->priv->finfo = finfo;
    gnome_cmd_file_ref (finfo);

    fname = get_utf8 (gnome_cmd_file_get_name (finfo));
    msg = g_strdup_printf (_("Rename \"%s\" to"), fname);
    msg_label = create_label (GTK_WIDGET (dialog), msg);
    g_free (msg);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), msg_label);

    gnome_cmd_string_dialog_setup_with_cancel (
        GNOME_CMD_STRING_DIALOG (dialog),
        _("Rename File"),
        labels,
        1,
        (GnomeCmdStringDialogCallback)on_ok,
        GTK_SIGNAL_FUNC (on_cancel),
        dialog);

    gnome_cmd_string_dialog_set_value (
        GNOME_CMD_STRING_DIALOG (dialog), 0, fname);
    g_free (fname);

    gtk_entry_select_region (
        GTK_ENTRY (GNOME_CMD_STRING_DIALOG (dialog)->entries[0]), 0, -1);

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

        dlg_type = gtk_type_unique (gnome_cmd_string_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}

