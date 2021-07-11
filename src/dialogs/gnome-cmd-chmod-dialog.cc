/**
 * @file gnome-cmd-chmod-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-chmod-component.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-user-actions.h"
#include "utils.h"
#include "dialogs/gnome-cmd-chmod-dialog.h"

using namespace std;


enum ChmodRecursiveMode
{
    CHMOD_ALL_FILES,
    CHMOD_DIRS_ONLY,
    CHMOD_MAX
};

static const gchar *recurse_opts[CHMOD_MAX] = {
    N_("All files"),
    N_("Directories only")
};

struct GnomeCmdChmodDialogPrivate
{
    GList *files;
    GnomeCmdFile *f;
    guint32 permissions;

    GtkWidget *chmod_component;
    GtkWidget *recurse_check;
    GtkWidget *recurse_combo;
};


G_DEFINE_TYPE (GnomeCmdChmodDialog, gnome_cmd_chmod_dialog, GNOME_CMD_TYPE_DIALOG)


static void do_chmod (GnomeCmdFile *in, guint32 permissions, gboolean recursive, ChmodRecursiveMode mode)
{
    g_return_if_fail (in != NULL);
    GError *error = nullptr;

    if (!(recursive
          && mode == CHMOD_DIRS_ONLY
          && in->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY))
    {
        if (!in->chmod(permissions, &error))
        {
            auto message = g_strdup_printf (_("Could not chmod %s"), in->get_name());
            gnome_cmd_show_message (nullptr, message, error->message);
            g_error_free(error);
            g_free(message);
            return;
        }

        if (!recursive)
        {
            return;
        }
    }

    if (in->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        GnomeCmdDir *dir = gnome_cmd_dir_ref (GNOME_CMD_DIR (in));

        gnome_cmd_dir_list_files (dir, FALSE);

        for (GList *i = gnome_cmd_dir_get_files (dir); i; i = i->next)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) i->data;
            auto filename = get_gfile_attribute_string(f->gFile, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
            if (!f->is_dotdot && strcmp (filename, ".") != 0
                && !g_file_info_get_is_symlink(f->gFileInfo))
            {
                do_chmod (f, permissions, TRUE, mode);
            }
            g_free(filename);
        }
        gnome_cmd_dir_unref (dir);
    }
}


inline void do_chmod_files (GnomeCmdChmodDialog *dialog)
{
    for (GList *i = dialog->priv->files; i; i = i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;
        gboolean recursive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->recurse_check));
        const gchar *mode_text = get_combo_text (dialog->priv->recurse_combo);
        //ToDo: This needs a fix. It does not work with Gcmd working in non-english language.
        ChmodRecursiveMode mode = strcmp (mode_text, recurse_opts[CHMOD_ALL_FILES]) == 0 ? CHMOD_ALL_FILES :
                                                                                           CHMOD_DIRS_ONLY;

        do_chmod (f, dialog->priv->permissions, recursive, mode);
        view_refresh (NULL, NULL);
    }
}


inline void show_perms (GnomeCmdChmodDialog *dialog)
{
    gnome_cmd_chmod_component_set_perms (GNOME_CMD_CHMOD_COMPONENT (dialog->priv->chmod_component), dialog->priv->permissions);
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
    dialog->priv->permissions =
        gnome_cmd_chmod_component_get_perms (GNOME_CMD_CHMOD_COMPONENT (dialog->priv->chmod_component));
}


static void gnome_cmd_chmod_dialog_finalize (GObject *object)
{
    GnomeCmdChmodDialog *dialog = GNOME_CMD_CHMOD_DIALOG (object);

    g_free (dialog->priv);

    G_OBJECT_CLASS (gnome_cmd_chmod_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_chmod_dialog_class_init (GnomeCmdChmodDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_chmod_dialog_finalize;
}


static void gnome_cmd_chmod_dialog_init (GnomeCmdChmodDialog *dialog)
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
    g_object_ref (dialog->priv->chmod_component);
    g_object_set_data_full (G_OBJECT (chmod_dialog), "chmod_component", dialog->priv->chmod_component, g_object_unref);
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

    g_signal_connect (dialog->priv->recurse_check, "toggled", G_CALLBACK (on_toggle_recurse), chmod_dialog);
    g_signal_connect (dialog->priv->chmod_component, "perms-changed", G_CALLBACK (on_perms_changed), chmod_dialog);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_chmod_dialog_new (GList *files)
{
    g_return_val_if_fail (files != NULL, NULL);

    GnomeCmdChmodDialog *dialog = (GnomeCmdChmodDialog *) g_object_new (GNOME_CMD_TYPE_CHMOD_DIALOG, NULL);
    dialog->priv->files = gnome_cmd_file_list_copy (files);

    dialog->priv->f = (GnomeCmdFile *) dialog->priv->files->data;
    g_return_val_if_fail (dialog->priv->f != NULL, NULL);

    dialog->priv->permissions = get_gfile_attribute_uint32(dialog->priv->f->gFile, G_FILE_ATTRIBUTE_UNIX_MODE) & 0xFFF;

    show_perms (dialog);

    GList *strings = NULL;

    strings = g_list_append (strings, _(recurse_opts[CHMOD_ALL_FILES]));
    strings = g_list_append (strings, _(recurse_opts[CHMOD_DIRS_ONLY]));

    gtk_combo_set_popdown_strings (GTK_COMBO (dialog->priv->recurse_combo), strings);

    return GTK_WIDGET (dialog);
}
