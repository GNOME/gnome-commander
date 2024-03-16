/**
 * @file gnome-cmd-chown-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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
#include "gnome-cmd-chown-dialog.h"
#include "gnome-cmd-chown-component.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-owner.h"

using namespace std;


struct GnomeCmdChownDialogPrivate
{
    GList *files;
    GtkWidget *chown_component;
    GtkWidget *recurse_check;
};

G_DEFINE_TYPE (GnomeCmdChownDialog, gnome_cmd_chown_dialog, GNOME_CMD_TYPE_DIALOG)


static void do_chown (GnomeCmdFile *in, uid_t uid, gid_t gid, gboolean recurse)
{
    g_return_if_fail (in != nullptr);
    g_return_if_fail (in->get_file_info() != nullptr);
    GError *error = nullptr;

    if(!in->chown(uid, gid, &error))
    {
        auto message = g_strdup_printf (_("Could not chown %s"), in->get_name());
        gnome_cmd_show_message (nullptr, message, error->message);
        g_error_free(error);
        g_free(message);
        return;
    }

    if (!recurse)
        return;

    if (in->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
    {
        GnomeCmdDir *dir = gnome_cmd_dir_ref (GNOME_CMD_DIR (in));

        gnome_cmd_dir_list_files (dir, FALSE);

        for (GList *i = gnome_cmd_dir_get_files (dir); i; i = i->next)
        {
            GnomeCmdFile *f = (GnomeCmdFile *) i->data;

            auto filename = get_gfile_attribute_string(f->get_file(), G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
            if (!f->is_dotdot && strcmp (filename, ".") != 0
                && !g_file_info_get_is_symlink(f->get_file_info()))
            {
                do_chown (f, uid, gid, TRUE);
            }
            g_free(filename);
        }
        gnome_cmd_dir_unref (dir);
    }
}


static void on_ok (GtkButton *button, GnomeCmdChownDialog *dialog)
{
    uid_t uid = -1;
    gid_t gid = -1;
    gint gid_temp;

    if (gcmd_owner.is_root())
    {
        gint uid_temp;
        uid_temp = gnome_cmd_chown_component_get_owner (GNOME_CMD_CHOWN_COMPONENT (dialog->priv->chown_component));
        g_return_if_fail (uid_temp >= 0);
        uid = uid_temp;
    }

    gid_temp = gnome_cmd_chown_component_get_group (GNOME_CMD_CHOWN_COMPONENT (dialog->priv->chown_component));
    g_return_if_fail (gid_temp >= 0);
    gid = gid_temp;

    gboolean recurse = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->recurse_check));

    for (GList *i = dialog->priv->files; i; i = i->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        g_return_if_fail (f != NULL);

        //ToDo: Check if this works also on non-local filesystems
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


static void gnome_cmd_chown_dialog_finalize (GObject *object)
{
    GnomeCmdChownDialog *dialog = GNOME_CMD_CHOWN_DIALOG (object);

    g_free (dialog->priv);

    G_OBJECT_CLASS (gnome_cmd_chown_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_chown_dialog_class_init (GnomeCmdChownDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_chown_dialog_finalize;
}


static void gnome_cmd_chown_dialog_init (GnomeCmdChownDialog *dialog)
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
    g_object_ref (dialog->priv->chown_component);
    g_object_set_data_full (G_OBJECT (dialog), "chown_component", dialog->priv->chown_component, g_object_unref);
    gtk_widget_show (dialog->priv->chown_component);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->chown_component, FALSE, FALSE, 0);

    dialog->priv->recurse_check = create_check (GTK_WIDGET (dialog), _("Apply Recursively"), "check");
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->recurse_check, FALSE, FALSE, 0);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK, GTK_SIGNAL_FUNC (on_ok), dialog);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_chown_dialog_new (GList *files)
{
    g_return_val_if_fail (files != nullptr, nullptr);

    auto *dialog = static_cast<GnomeCmdChownDialog*> (g_object_new (GNOME_CMD_TYPE_CHOWN_DIALOG, nullptr));
    dialog->priv->files = gnome_cmd_file_list_copy (files);
    GnomeCmdFile *f = GNOME_CMD_FILE (dialog->priv->files->data);

    gnome_cmd_chown_component_set (GNOME_CMD_CHOWN_COMPONENT (dialog->priv->chown_component),
        get_gfile_attribute_uint32(f->get_file(), G_FILE_ATTRIBUTE_UNIX_UID),
        get_gfile_attribute_uint32(f->get_file(), G_FILE_ATTRIBUTE_UNIX_GID));

    return GTK_WIDGET (dialog);
}
