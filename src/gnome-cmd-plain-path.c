/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2004 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 

#include "gnome-cmd-includes.h"
#include "gnome-cmd-plain-path.h"

struct _GnomeCmdPlainPathPrivate
{
	gchar *path;
};

static GnomeCmdPathClass *parent_class = NULL;



static const gchar *
plain_path_get_path (GnomeCmdPath *path)
{
	g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);
	
	return GNOME_CMD_PLAIN_PATH (path)->priv->path;
}


static const gchar *
plain_path_get_display_path (GnomeCmdPath *path)
{
	g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);
	
	return GNOME_CMD_PLAIN_PATH (path)->priv->path;
}

static GnomeCmdPath *
plain_path_get_parent (GnomeCmdPath *path)
{
	GnomeVFSURI *u1, *u2, *t;
	GnomeCmdPath *parent;
	gchar *s;
	
	g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);

	t = gnome_vfs_uri_new ("/");
	u1 = gnome_vfs_uri_append_path (t, GNOME_CMD_PLAIN_PATH (path)->priv->path);
	gnome_vfs_uri_unref (t);
	
	u2 = gnome_vfs_uri_get_parent (u1);
	gnome_vfs_uri_unref (u1);
	if (!u2) return NULL;

	s = gnome_vfs_uri_to_string (u2, 0);
	parent = gnome_cmd_plain_path_new (gnome_vfs_get_local_path_from_uri (s));
	gnome_vfs_uri_unref (u2);
	g_free (s);

	return parent;
}

static GnomeCmdPath *
plain_path_get_child (GnomeCmdPath *path, const gchar *child)
{
	GnomeVFSURI *u1, *u2;
	GnomeCmdPath *out;
	gchar *path_str;
	
	g_return_val_if_fail (GNOME_CMD_IS_PLAIN_PATH (path), NULL);
	
	u1 = gnome_vfs_uri_new (GNOME_CMD_PLAIN_PATH (path)->priv->path);
	if (!strchr (child, '/'))
		u2 = gnome_vfs_uri_append_file_name (u1, child);
	else
		u2 = gnome_vfs_uri_append_path (u1, child);
	gnome_vfs_uri_unref (u1);
	if (!u2) return NULL;

	path_str = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (u2), 0);
	out = gnome_cmd_plain_path_new (path_str);
	g_free (path_str);
	gnome_vfs_uri_unref (u2);
	
	return out;
}



/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdPlainPath *path = GNOME_CMD_PLAIN_PATH (object);

	g_free (path->priv->path);
	g_free (path->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdPlainPathClass *klass)
{
	GtkObjectClass *object_class;
	GnomeCmdPathClass *path_class;

	object_class = GTK_OBJECT_CLASS (klass);
	path_class = GNOME_CMD_PATH_CLASS (klass);
	parent_class = gtk_type_class (gnome_cmd_path_get_type ());

	object_class->destroy = destroy;

	path_class->get_path = plain_path_get_path;
	path_class->get_display_path = plain_path_get_display_path;
	path_class->get_parent = plain_path_get_parent;
	path_class->get_child = plain_path_get_child;
}


static void
init (GnomeCmdPlainPath *path)
{
	path->priv = g_new (GnomeCmdPlainPathPrivate, 1);
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_plain_path_get_type         (void)
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


GnomeCmdPath *
gnome_cmd_plain_path_new (const gchar *path)
{
	GnomeCmdPlainPath *plain_path;

	plain_path = gtk_type_new (gnome_cmd_plain_path_get_type ());
	plain_path->priv->path = g_strdup (path);

	return GNOME_CMD_PATH (plain_path);
}

