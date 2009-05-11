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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-chown-dialog.h"
#include "gnome-cmd-chown-component.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-user-actions.h"
#include "owner.h"

using namespace std;


struct GnomeCmdChownDialogPrivate
{
    GList *files;
    GtkWidget *chown_component;
    GtkWidget *recurse_check;
};

static GnomeCmdDialogClass *parent_class = NULL;


static void do_chown (GnomeCmdFile *in, uid_t uid, gid_t gid, gboolean recurse)
{
    GnomeVFSResult ret;

    g_return_if_fail (in != NULL);
    g_return_if_fail (in->info != NULL);

    ret = gnome_cmd_file_chown (in, uid, gid);

    if (ret != GNOME_VFS_OK)
    {
        gchar *fpath = gnome_cmd_file_get_real_path (in);
        gchar *msg = g_strdup_printf (_("Could not chown %s\n%s"), fpath, gnome_vfs_result_to_string (ret));
        create_error_dialog (msg);
        g_free (msg);
        g_free (fpath);
    }
    else
        if (!recurse)
            return;

    if (in->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
        GnomeCmdDir *dir = GNOME_CMD_DIR (in);
        GList *files, *tmp;

        gnome_cmd_dir_ref (dir);
        gnome_cmd_dir_list_files (dir, FALSE);
        gnome_cmd_dir_get_files (dir, &files);

        for (tmp = files; tmp; tmp = tmp->next)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;
            if (strcmp (f->info->name, ".") != 0
                && strcmp (f->info->name, "..") != 0
                && !GNOME_VFS_FILE_INFO_SYMLINK(f->info))
            {
                do_chown (f, uid, gid, TRUE);
            }
        }
        gnome_cmd_dir_unref (dir);
    }
}


static void on_ok (GtkButton *button, GnomeCmdChownDialog *dialog)
{
    uid_t uid = -1;
    gid_t gid = -1;

    if (gcmd_owner.is_root())
    {
        uid = gnome_cmd_chown_component_get_owner (GNOME_CMD_CHOWN_COMPONENT (dialog->priv->chown_component));
        g_return_if_fail (uid >= 0);
    }

    gid = gnome_cmd_chown_component_get_group (GNOME_CMD_CHOWN_COMPONENT (dialog->priv->chown_component));
    g_return_if_fail (gid >= 0);

    gboolean recurse = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->recurse_check));

    for (GList *tmp = dialog->priv->files; tmp; tmp = tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;

        g_return_if_fail (f != NULL);

        if (GNOME_VFS_FILE_INFO_LOCAL (f->info))
            do_chown (f, uid, gid, recurse);
    }

    view_refresh (NULL, NULL);

    gnome_cmd_file_list_free (dialog->priv->files);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdChownDialog *dialog)
{
    gnome_cmd_file_list_free (dialog->priv->files);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdChownDialog *dialog = GNOME_CMD_CHOWN_DIALOG (object);

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
class_init (GnomeCmdChownDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void
init (GnomeCmdChownDialog *dialog)
{
    GtkWidget *chown_dialog = GTK_WIDGET (dialog);
    GtkWidget *vbox;

    dialog->priv = g_new0 (GnomeCmdChownDialogPrivate, 1);

    gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Chown"));

    vbox = create_vbox (chown_dialog, FALSE, 12);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox);

    dialog->priv->chown_component = gnome_cmd_chown_component_new ();
    gtk_widget_ref (dialog->priv->chown_component);
    gtk_object_set_data_full (GTK_OBJECT (dialog),
                              "chown_component", dialog->priv->chown_component,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (dialog->priv->chown_component);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->chown_component, FALSE, FALSE, 0);

    dialog->priv->recurse_check = create_check (GTK_WIDGET (dialog), _("Apply Recursively"), "check");
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->recurse_check, FALSE, FALSE, 0);


    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL,
                                 GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK,
                                 GTK_SIGNAL_FUNC (on_ok), dialog);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_chown_dialog_new (GList *files)
{
    g_return_val_if_fail (files != NULL, NULL);

    GnomeCmdChownDialog *dialog = (GnomeCmdChownDialog *) gtk_type_new (gnome_cmd_chown_dialog_get_type ());
    dialog->priv->files = gnome_cmd_file_list_copy (files);
    GnomeCmdFile *f = GNOME_CMD_FILE (dialog->priv->files->data);

    gnome_cmd_chown_component_set (GNOME_CMD_CHOWN_COMPONENT (dialog->priv->chown_component), f->info->uid, f->info->gid);

    return GTK_WIDGET (dialog);
}


GtkType gnome_cmd_chown_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdChownDialog",
            sizeof (GnomeCmdChownDialog),
            sizeof (GnomeCmdChownDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}
