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
#include "gnome-cmd-plain-path.h"

using namespace std;


struct GnomeCmdPlainPathPrivate
{
    gchar *path;
};


static GnomeCmdPathClass *parent_class = NULL;


inline const gchar *plain_path_get_path (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);

    return GNOME_CMD_PLAIN_PATH (path)->priv->path;
}


inline const gchar *plain_path_get_display_path (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);

    return GNOME_CMD_PLAIN_PATH (path)->priv->path;
}


inline GnomeCmdPath *plain_path_get_parent (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);

    GnomeVFSURI *t = gnome_vfs_uri_new (G_DIR_SEPARATOR_S);
    GnomeVFSURI *u1 = gnome_vfs_uri_append_path (t, GNOME_CMD_PLAIN_PATH (path)->priv->path);
    gnome_vfs_uri_unref (t);

    GnomeVFSURI *u2 = gnome_vfs_uri_get_parent (u1);
    gnome_vfs_uri_unref (u1);

    if (!u2)  return NULL;

    gchar *s = gnome_vfs_uri_to_string (u2, GNOME_VFS_URI_HIDE_NONE);
    gnome_vfs_uri_unref (u2);

    GnomeCmdPath *parent_path = gnome_cmd_plain_path_new (gnome_vfs_get_local_path_from_uri (s));
    g_free (s);

    return parent_path;
}


inline GnomeCmdPath *plain_path_get_child (GnomeCmdPath *path, const gchar *child)
{
    g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);

    GnomeVFSURI *t = gnome_vfs_uri_new (G_DIR_SEPARATOR_S);
    GnomeVFSURI *u1 = gnome_vfs_uri_append_path (t, GNOME_CMD_PLAIN_PATH (path)->priv->path);
    gnome_vfs_uri_unref (t);

    GnomeVFSURI *u2 = strchr (child, '/')==NULL ?
                      gnome_vfs_uri_append_file_name (u1, child) :
                      gnome_vfs_uri_append_path (u1, child);
    gnome_vfs_uri_unref (u1);

    if (!u2)  return NULL;

    gchar *path_str = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (u2), 0);
    gnome_vfs_uri_unref (u2);

    GnomeCmdPath *child_path = gnome_cmd_plain_path_new (path_str);
    g_free (path_str);

    return child_path;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdPlainPath *path = GNOME_CMD_PLAIN_PATH (object);

    g_free (path->priv->path);
    g_free (path->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdPlainPathClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GnomeCmdPathClass *path_class = GNOME_CMD_PATH_CLASS (klass);

    parent_class = (GnomeCmdPathClass *) gtk_type_class (gnome_cmd_path_get_type ());

    object_class->destroy = destroy;

    path_class->get_path = plain_path_get_path;
    path_class->get_display_path = plain_path_get_display_path;
    path_class->get_parent = plain_path_get_parent;
    path_class->get_child = plain_path_get_child;
}


static void init (GnomeCmdPlainPath *path)
{
    path->priv = g_new0 (GnomeCmdPlainPathPrivate, 1);
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_plain_path_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdPlainPath",
            sizeof (GnomeCmdPlainPath),
            sizeof (GnomeCmdPlainPathClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gnome_cmd_path_get_type (), &info);
    }
    return type;
}


GnomeCmdPath *gnome_cmd_plain_path_new (const gchar *path)
{
    GnomeCmdPlainPath *plain_path = (GnomeCmdPlainPath *) gtk_type_new (gnome_cmd_plain_path_get_type ());
    plain_path->priv->path = g_strdup (path);

    return GNOME_CMD_PATH (plain_path);
}
