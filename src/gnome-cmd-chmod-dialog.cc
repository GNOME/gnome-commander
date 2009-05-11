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
#include "gnome-cmd-chmod-dialog.h"
#include "gnome-cmd-chmod-component.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"

using namespace std;


typedef enum
{
    CHMOD_ALL_FILES,
    CHMOD_DIRS_ONLY,
    CHMOD_MAX
} ChmodRecursiveMode;

static gchar *recurse_opts[CHMOD_MAX] = {
    N_("All files"),
    N_("Directories only")
};

struct GnomeCmdChmodDialogPrivate
{
    GList *files;
    GnomeCmdFile *f;
    GnomeVFSFilePermissions perms;

    GtkWidget *chmod_component;
    GtkWidget *recurse_check;
    GtkWidget *recurse_combo;
};


static GnomeCmdDialogClass *parent_class = NULL;


static void do_chmod (GnomeCmdFile *in, GnomeVFSFilePermissions perm, gboolean recursive, ChmodRecursiveMode mode)
{
    g_return_if_fail (in != NULL);
    g_return_if_fail (in->info != NULL);

    if (!(recursive && mode == CHMOD_DIRS_ONLY && in->info->type != GNOME_VFS_FILE_TYPE_DIRECTORY))
    {
        GnomeVFSResult ret = gnome_cmd_file_chmod (in, perm);

        if (ret != GNOME_VFS_OK)
            gnome_cmd_show_message (NULL, gnome_cmd_file_get_name (in), gnome_vfs_result_to_string (ret));
        else
            if (!recursive)
                return;
    }

    if (in->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
    {
        GnomeCmdDir *dir = GNOME_CMD_DIR (in);
        GList *files;

        gnome_cmd_dir_ref (dir);
        gnome_cmd_dir_list_files (dir, FALSE);
        gnome_cmd_dir_get_files (dir, &files);

        for (GList *tmp = files; tmp; tmp = tmp->next)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;
            if (strcmp (f->info->name, ".") != 0
                && strcmp (f->info->name, "..") != 0
                && !GNOME_VFS_FILE_INFO_SYMLINK(f->info))
            {
                do_chmod (f, perm, TRUE, mode);
            }
        }
        gnome_cmd_dir_unref (dir);
    }
}


inline void do_chmod_files (GnomeCmdChmodDialog *dialog)
{
    for (GList *tmp = dialog->priv->files; tmp; tmp = tmp->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) tmp->data;
        gboolean recursive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->recurse_check));
        const gchar *mode_text = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (dialog->priv->recurse_combo)->entry));
        ChmodRecursiveMode mode = strcmp (mode_text, recurse_opts[CHMOD_ALL_FILES]) == 0 ? CHMOD_ALL_FILES :
                                                                                           CHMOD_DIRS_ONLY;

        do_chmod (f, dialog->priv->perms, recursive, mode);
        view_refresh (NULL, NULL);
    }
}


inline void show_perms (GnomeCmdChmodDialog *dialog)
{
    gnome_cmd_chmod_component_set_perms (GNOME_CMD_CHMOD_COMPONENT (dialog->priv->chmod_component), dialog->priv->perms);
}


static void on_ok (GtkButton *button, GnomeCmdChmodDialog *dialog)
{
    do_chmod_files (dialog);
    gnome_cmd_file_list_free (dialog->priv->files);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdChmodDialog *dialog)
{
    gnome_cmd_file_list_free (dialog->priv->files);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_toggle_recurse (GtkToggleButton *togglebutton, GnomeCmdChmodDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->priv->recurse_combo, gtk_toggle_button_get_active (togglebutton));
}


static void on_perms_changed (GnomeCmdChmodComponent *component, GnomeCmdChmodDialog *dialog)
{
    dialog->priv->perms =
        gnome_cmd_chmod_component_get_perms (GNOME_CMD_CHMOD_COMPONENT (dialog->priv->chmod_component));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdChmodDialog *dialog = GNOME_CMD_CHMOD_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdChmodDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdChmodDialog *dialog)
{
    GtkWidget *chmod_dialog = GTK_WIDGET (dialog);
    GtkWidget *vbox;
    GtkWidget *hsep;

    dialog->priv = g_new (GnomeCmdChmodDialogPrivate, 1);

    gtk_window_set_policy (GTK_WINDOW (chmod_dialog), FALSE, FALSE, FALSE);
    gtk_window_set_position (GTK_WINDOW (chmod_dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (chmod_dialog), _("Access Permissions"));

    vbox = create_vbox (chmod_dialog, FALSE, 12);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox);


    dialog->priv->chmod_component = gnome_cmd_chmod_component_new ((GnomeVFSFilePermissions) 0);
    gtk_widget_ref (dialog->priv->chmod_component);
    gtk_object_set_data_full (GTK_OBJECT (chmod_dialog),
                              "chmod_component", dialog->priv->chmod_component,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (dialog->priv->chmod_component);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->chmod_component, TRUE, TRUE, 0);


    hsep = create_hsep (chmod_dialog);
    gtk_box_pack_start (GTK_BOX (vbox), hsep, TRUE, TRUE, 0);

    dialog->priv->recurse_check = create_check (chmod_dialog, _("Apply Recursively for"), "check");
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->recurse_check, TRUE, TRUE, 0);

    dialog->priv->recurse_combo = create_combo (chmod_dialog);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->recurse_combo, TRUE, TRUE, 0);
    gtk_widget_set_sensitive (dialog->priv->recurse_combo, FALSE);


    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL,
                                 GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK,
                                 GTK_SIGNAL_FUNC (on_ok), dialog);

    gtk_signal_connect (GTK_OBJECT (dialog->priv->recurse_check), "toggled",
                        GTK_SIGNAL_FUNC (on_toggle_recurse), chmod_dialog);
    gtk_signal_connect (GTK_OBJECT (dialog->priv->chmod_component), "perms-changed",
                        GTK_SIGNAL_FUNC (on_perms_changed), chmod_dialog);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_chmod_dialog_new (GList *files)
{
    g_return_val_if_fail (files != NULL, NULL);

    GnomeCmdChmodDialog *dialog = (GnomeCmdChmodDialog *) gtk_type_new (gnome_cmd_chmod_dialog_get_type ());
    dialog->priv->files = gnome_cmd_file_list_copy (files);

    dialog->priv->f = (GnomeCmdFile *) dialog->priv->files->data;
    g_return_val_if_fail (dialog->priv->f != NULL, NULL);

    dialog->priv->perms = dialog->priv->f->info->permissions;

    show_perms (dialog);

    GList *strings = NULL;

    strings = g_list_append (strings, _(recurse_opts[CHMOD_ALL_FILES]));
    strings = g_list_append (strings, _(recurse_opts[CHMOD_DIRS_ONLY]));

    gtk_combo_set_popdown_strings (GTK_COMBO (dialog->priv->recurse_combo), strings);

    return GTK_WIDGET (dialog);
}


GtkType gnome_cmd_chmod_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdChmodDialog",
            sizeof (GnomeCmdChmodDialog),
            sizeof (GnomeCmdChmodDialogClass),
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
