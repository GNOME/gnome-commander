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
#include "gnome-cmd-mkdir-dialog.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"


struct _GnomeCmdMkdirDialogPrivate
{
    GnomeCmdDir *dir;
    GnomeCmdMainWin *mw;
};


static GnomeCmdStringDialogClass *parent_class = NULL;


static gboolean
on_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdMkdirDialog *dialog)
{
    const gchar *filename = values[0];

    // dont create any directory if no name was passed or cancel was selected
    if (filename == NULL || *filename == 0)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("A directory name must be entered")));
        return FALSE;
    }

    GnomeVFSURI *uri = gnome_cmd_dir_get_child_uri (dialog->priv->dir, filename);

    GnomeVFSResult result = gnome_vfs_make_directory_for_uri (uri,
                                                              GNOME_VFS_PERM_USER_READ|GNOME_VFS_PERM_USER_WRITE|GNOME_VFS_PERM_USER_EXEC|
                                                              GNOME_VFS_PERM_GROUP_READ|GNOME_VFS_PERM_GROUP_EXEC|
                                                              GNOME_VFS_PERM_OTHER_READ|GNOME_VFS_PERM_OTHER_EXEC);
    if (result == GNOME_VFS_OK)
    {
        gchar *uri_str = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
        gnome_cmd_dir_file_created (dialog->priv->dir, uri_str);
        g_free (uri_str);

        gnome_cmd_file_list_focus_file (gnome_cmd_main_win_get_active_fs (main_win)->list, filename, TRUE);
        gnome_cmd_dir_unref (dialog->priv->dir);
        gnome_vfs_uri_unref (uri);
        return TRUE;
    }

    gnome_vfs_uri_unref (uri);
    gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (gnome_vfs_result_to_string (result)));
    return FALSE;
}


static void
on_cancel (GtkWidget *widget, GnomeCmdMkdirDialog *dialog)
{
    gnome_cmd_dir_unref (dialog->priv->dir);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdMkdirDialog *dialog = GNOME_CMD_MKDIR_DIALOG (object);

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
class_init (GnomeCmdMkdirDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdStringDialogClass *) gtk_type_class (gnome_cmd_string_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = map;
}


static void
init (GnomeCmdMkdirDialog *dialog)
{
    dialog->priv = g_new (GnomeCmdMkdirDialogPrivate, 1);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_mkdir_dialog_new (GnomeCmdDir *dir)
{
    const gchar *labels[] = {_("Directory name:"), NULL};
    GnomeCmdMkdirDialog *dialog = (GnomeCmdMkdirDialog *) gtk_type_new (gnome_cmd_mkdir_dialog_get_type ());

    dialog->priv->dir = dir;
    gnome_cmd_dir_ref (dir);

    gnome_cmd_string_dialog_setup_with_cancel (
        GNOME_CMD_STRING_DIALOG (dialog),
        _("Make Directory"),
        labels,
        1,
        (GnomeCmdStringDialogCallback)on_ok,
        (GtkSignalFunc)on_cancel,
        dialog);

    return GTK_WIDGET (dialog);
}


GtkType
gnome_cmd_mkdir_dialog_get_type         (void)
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdMkdirDialog",
            sizeof (GnomeCmdMkdirDialog),
            sizeof (GnomeCmdMkdirDialogClass),
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
