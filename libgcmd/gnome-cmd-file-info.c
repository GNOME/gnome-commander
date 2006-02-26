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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <config.h>
#include "libgcmd-deps.h"
#include "gnome-cmd-file-info.h"

static GtkObjectClass *parent_class = NULL;

struct _GnomeCmdFileInfoPrivate
{
};


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdFileInfo *file = GNOME_CMD_FILE_INFO (object);

    gnome_vfs_file_info_unref (file->info);
    if (file->uri)
        gnome_vfs_uri_unref (file->uri);

    g_free (file->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdFileInfoClass *class)
{
    GtkObjectClass *object_class;

    object_class = GTK_OBJECT_CLASS (class);
    parent_class = gtk_type_class (gtk_object_get_type ());

    object_class->destroy = destroy;
}

static void
init (GnomeCmdFileInfo *file)
{
    file->info = NULL;
    file->uri = NULL;

    file->priv = g_new0 (GnomeCmdFileInfoPrivate, 1);
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_file_info_get_type         (void)
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdFileInfo",
            sizeof (GnomeCmdFileInfo),
            sizeof (GnomeCmdFileInfoClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_object_get_type (), &info);
    }
    return type;
}


void
gnome_cmd_file_info_setup (GnomeCmdFileInfo *finfo, GnomeVFSURI *uri, GnomeVFSFileInfo *info)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_INFO (finfo));

    finfo->info = info;
    finfo->uri = uri;
}


