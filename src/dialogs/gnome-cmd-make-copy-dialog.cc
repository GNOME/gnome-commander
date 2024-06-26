/**
 * @file gnome-cmd-make-copy-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-xfer.h"
#include "dialogs/gnome-cmd-make-copy-dialog.h"

using namespace std;


struct GnomeCmdMakeCopyDialogPrivate
{
    GnomeCmdFile *f;
    GnomeCmdDir *dir;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdMakeCopyDialog, gnome_cmd_make_copy_dialog, GNOME_CMD_TYPE_STRING_DIALOG)


inline void copy_file (GnomeCmdFile *f, GnomeCmdDir *dir, const gchar *filename)
{
    GList *src_files = g_list_append (NULL, f);

    gnome_cmd_copy_start (src_files,
                          dir,
                          NULL,
                          g_strdup (filename),
                          G_FILE_COPY_NONE,
                          GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                          NULL,
                          NULL);
}


static gboolean on_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdMakeCopyDialog *dialog)
{
    g_return_val_if_fail (dialog, TRUE);
    auto priv = static_cast<GnomeCmdMakeCopyDialogPrivate*>(gnome_cmd_make_copy_dialog_get_instance_private (dialog));
    g_return_val_if_fail (priv->f, TRUE);

    const gchar *filename = values[0];

    if (!filename)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("No file name entered")));
        return FALSE;
    }

    if (filename[0] == '/')
    {
        gchar *parent_dir = g_path_get_dirname (filename);
        gchar *dest_fn = g_path_get_basename (filename);

        auto con = gnome_cmd_dir_get_connection (priv->dir);
        auto conPath = gnome_cmd_con_create_path (con, parent_dir);
        auto dir = gnome_cmd_dir_new (con, conPath);
        delete conPath;
        g_free (parent_dir);

        copy_file (priv->f, dir, dest_fn);
        g_free (dest_fn);
    }
    else
        copy_file (priv->f, priv->dir, filename);

    return TRUE;
}


static void on_cancel (GtkWidget *widget, GnomeCmdMakeCopyDialog *dialog)
{
    auto priv = static_cast<GnomeCmdMakeCopyDialogPrivate*>(gnome_cmd_make_copy_dialog_get_instance_private (dialog));
    priv->f->unref();
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_make_copy_dialog_class_init (GnomeCmdMakeCopyDialogClass *klass)
{
}


static void gnome_cmd_make_copy_dialog_init (GnomeCmdMakeCopyDialog *dialog)
{
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_make_copy_dialog_new (GnomeCmdFile *f, GnomeCmdDir *dir)
{
    g_return_val_if_fail (f != NULL, NULL);

    const gchar *labels[] = {""};

    GnomeCmdMakeCopyDialog *dialog = (GnomeCmdMakeCopyDialog *) g_object_new (GNOME_CMD_TYPE_MAKE_COPY_DIALOG, NULL);
    auto priv = static_cast<GnomeCmdMakeCopyDialogPrivate*>(gnome_cmd_make_copy_dialog_get_instance_private (dialog));

    priv->f = f->ref();
    priv->dir = gnome_cmd_dir_ref (dir);

    gchar *msg = g_strdup_printf (_("Copy “%s” to"), f->get_name());
    GtkWidget *msg_label = create_label (GTK_WIDGET (dialog), msg);
    g_free (msg);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), msg_label);

    gnome_cmd_string_dialog_setup_with_cancel (GNOME_CMD_STRING_DIALOG (dialog),
                                               _("Copy File"),
                                               labels,
                                               1,
                                               (GnomeCmdStringDialogCallback) on_ok,
                                               (GnomeCmdCallback<GtkButton*>) on_cancel,
                                               dialog);

    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, g_file_info_get_display_name(f->get_file_info()));

    return GTK_WIDGET (dialog);
}

