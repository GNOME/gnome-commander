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
#include "gnome-cmd-make-copy-dialog.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-xfer.h"

using namespace std;


struct GnomeCmdMakeCopyDialogPrivate
{
    GnomeCmdFile *f;
    GnomeCmdDir *dir;
    GnomeCmdMainWin *mw;
};


static GnomeCmdStringDialogClass *parent_class = NULL;


inline void copy_file (GnomeCmdFile *f, GnomeCmdDir *dir, const gchar *filename)
{
    GList *src_files = g_list_append (NULL, f);

    gnome_cmd_xfer_start (src_files,
                          dir,
                          NULL,
                          g_strdup (filename),
                          GNOME_VFS_XFER_RECURSIVE,
                          GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
                          NULL,
                          NULL);
}


static gboolean on_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdMakeCopyDialog *dialog)
{
    g_return_val_if_fail (dialog, TRUE);
    g_return_val_if_fail (dialog->priv, TRUE);
    g_return_val_if_fail (dialog->priv->f, TRUE);

    const gchar *filename = values[0];

    if (!filename)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("No file name entered")));
        return FALSE;
    }

    copy_file (dialog->priv->f, dialog->priv->dir, filename);

    return TRUE;
}


static void on_cancel (GtkWidget *widget, GnomeCmdMakeCopyDialog *dialog)
{
    gnome_cmd_file_unref (dialog->priv->f);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdMakeCopyDialog *dialog = GNOME_CMD_MAKE_COPY_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdMakeCopyDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdStringDialogClass *) gtk_type_class (gnome_cmd_string_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdMakeCopyDialog *dialog)
{
    dialog->priv = g_new0 (GnomeCmdMakeCopyDialogPrivate, 1);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_make_copy_dialog_new (GnomeCmdFile *f, GnomeCmdDir *dir)
{
    g_return_val_if_fail (f != NULL, NULL);

    const gchar *labels[] = {""};

    GnomeCmdMakeCopyDialog *dialog = (GnomeCmdMakeCopyDialog *) gtk_type_new (gnome_cmd_make_copy_dialog_get_type ());

    dialog->priv->f = f;
    dialog->priv->dir = dir;
    gnome_cmd_file_ref (f);
    gnome_cmd_file_ref (GNOME_CMD_FILE (dir));

    gchar *msg = g_strdup_printf (_("Copy \"%s\" to"), gnome_cmd_file_get_name (f));
    GtkWidget *msg_label = create_label (GTK_WIDGET (dialog), msg);
    g_free (msg);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), msg_label);

    gnome_cmd_string_dialog_setup_with_cancel (GNOME_CMD_STRING_DIALOG (dialog),
                                               _("Copy File"),
                                               labels,
                                               1,
                                               (GnomeCmdStringDialogCallback) on_ok,
                                               GTK_SIGNAL_FUNC (on_cancel),
                                               dialog);

    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, f->info->name);

    return GTK_WIDGET (dialog);
}


GtkType gnome_cmd_make_copy_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdMakeCopyDialog",
            sizeof (GnomeCmdMakeCopyDialog),
            sizeof (GnomeCmdMakeCopyDialogClass),
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
