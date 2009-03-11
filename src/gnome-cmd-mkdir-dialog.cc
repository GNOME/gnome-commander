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
#include "gnome-cmd-mkdir-dialog.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-main-win.h"
#include "utils.h"

using namespace std;


struct GnomeCmdMkdirDialogPrivate
{
    GnomeCmdDir *dir;
    GnomeCmdMainWin *mw;
};


static GnomeCmdStringDialogClass *parent_class = NULL;


inline GSList *make_uri_list (GnomeCmdDir *dir, string filename)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    // make an absolute filename from one that is starting with a tilde
    if (filename.compare(0, 2, "~/")==0)
        if (gnome_cmd_dir_is_local (dir))
            stringify (filename, gnome_vfs_expand_initial_tilde (filename.c_str()));
        else
            filename.erase(0,1);

    // smb exception handling: test if we are in a samba share...
    // if not - change filename so that we can get a proper error message
    GnomeVFSURI *dir_uri = gnome_cmd_dir_get_uri (dir);

    if (strcmp (gnome_vfs_uri_get_scheme (dir_uri), "smb")==0 && g_path_is_absolute (filename.c_str()))
    {
        string mime_type = stringify (gnome_vfs_get_mime_type (gnome_vfs_uri_to_string (dir_uri, GNOME_VFS_URI_HIDE_NONE)));

        if (mime_type=="x-directory/normal" && !gnome_vfs_uri_has_parent (dir_uri))
            filename.erase(0,1);
    }
    gnome_vfs_uri_unref (dir_uri);

    GSList *uri_list = NULL;

    if (g_path_is_absolute (filename.c_str()))
        while (filename.compare("/")!=0)
        {
            uri_list = g_slist_prepend (uri_list, gnome_cmd_dir_get_absolute_path_uri (dir, filename));
            stringify (filename, g_path_get_dirname (filename.c_str()));
        }
    else
        while (filename.compare(".")!=0)        // support for mkdir -p
        {
            uri_list = g_slist_prepend (uri_list, gnome_cmd_dir_get_child_uri (dir, filename.c_str()));
            stringify (filename, g_path_get_dirname (filename.c_str()));
        }

    return uri_list;
}


static gboolean on_ok (GnomeCmdStringDialog *string_dialog, const gchar **values, GnomeCmdMkdirDialog *dialog)
{
    // first create a list of uris...
    // ... then try to make a dir for each uri from uri list
    // focus created dir if it is in the active file selector

    gchar *filename = const_cast<gchar *> (values[0]);

    // don't create any directory if no name was passed or cancel was selected
    if (!filename || *filename==0)
    {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (_("A directory name must be entered")));
        return FALSE;
    }

    GnomeVFSURI *dir_uri = gnome_cmd_dir_get_uri (dialog->priv->dir);
    gboolean new_dir_focused = FALSE;

    // the list of uri's to be created
    GSList *uri_list = make_uri_list (dialog->priv->dir, filename);

    GnomeVFSResult result = GNOME_VFS_OK;

    for (GSList *i = uri_list; i; i = g_slist_next (i))
    {
        GnomeVFSURI *mkdir_uri = (GnomeVFSURI *) i->data;
        result = gnome_vfs_make_directory_for_uri (mkdir_uri,
                                                   GNOME_VFS_PERM_USER_READ|GNOME_VFS_PERM_USER_WRITE|GNOME_VFS_PERM_USER_EXEC|
                                                   GNOME_VFS_PERM_GROUP_READ|GNOME_VFS_PERM_GROUP_EXEC|
                                                   GNOME_VFS_PERM_OTHER_READ|GNOME_VFS_PERM_OTHER_EXEC);

        // focus the created directory (if possible)
        if (result==GNOME_VFS_OK)
            if (gnome_vfs_uri_equal (gnome_vfs_uri_get_parent (mkdir_uri), dir_uri) == 1 && !new_dir_focused)
            {
                string focus_filename = stringify (gnome_vfs_uri_extract_short_name (mkdir_uri));
                string mkdir_uri_str = stringify (gnome_vfs_uri_to_string (mkdir_uri, GNOME_VFS_URI_HIDE_NONE));

                gnome_cmd_dir_file_created (dialog->priv->dir, mkdir_uri_str.c_str());
                gnome_cmd_main_win_get_fs (main_win, ACTIVE)->file_list()->focus_file(focus_filename.c_str(), TRUE);
                new_dir_focused = TRUE;
            }
    }

    for (GSList *i = uri_list; i; i = g_slist_next (i))
        gnome_vfs_uri_unref ((GnomeVFSURI *) i->data);

    g_slist_free (uri_list);

    gnome_vfs_uri_unref (dir_uri);
    gnome_cmd_dir_unref (dialog->priv->dir);

    if (result!=GNOME_VFS_OK)
        gnome_cmd_string_dialog_set_error_desc (string_dialog, g_strdup (gnome_vfs_result_to_string (result)));

    return result==GNOME_VFS_OK;
}


static void on_cancel (GtkWidget *widget, GnomeCmdMkdirDialog *dialog)
{
    gnome_cmd_dir_unref (dialog->priv->dir);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdMkdirDialog *dialog = GNOME_CMD_MKDIR_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdMkdirDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdStringDialogClass *) gtk_type_class (gnome_cmd_string_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdMkdirDialog *dialog)
{
    dialog->priv = g_new0 (GnomeCmdMkdirDialogPrivate, 1);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_mkdir_dialog_new (GnomeCmdDir *dir)
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
        (GnomeCmdStringDialogCallback) on_ok,
        (GtkSignalFunc) on_cancel,
        dialog);

    return GTK_WIDGET (dialog);
}


GtkType gnome_cmd_mkdir_dialog_get_type ()
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
