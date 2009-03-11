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
#include "gnome-cmd-path.h"

using namespace std;


static GtkObjectClass *parent_class = NULL;


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdPathClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

    parent_class = (GtkObjectClass *) gtk_type_class (gtk_object_get_type ());

    object_class->destroy = destroy;

    klass->get_path = NULL;
    klass->get_display_path = NULL;
    klass->get_parent = NULL;
    klass->get_child = NULL;
}


static void init (GnomeCmdPath *path)
{
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_path_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdPath",
            sizeof (GnomeCmdPath),
            sizeof (GnomeCmdPathClass),
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


const gchar *gnome_cmd_path_get_path (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_PATH (path), NULL);

    GnomeCmdPathClass *klass = GNOME_CMD_PATH_GET_CLASS (path);

    return klass->get_path (path);
}


const gchar *gnome_cmd_path_get_display_path (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_PATH (path), NULL);

    GnomeCmdPathClass *klass = GNOME_CMD_PATH_GET_CLASS (path);

    return klass->get_display_path (path);
}


GnomeCmdPath *gnome_cmd_path_get_parent (GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_PATH (path), NULL);

    GnomeCmdPathClass *klass = GNOME_CMD_PATH_GET_CLASS (path);

    return klass->get_parent (path);
}


GnomeCmdPath *gnome_cmd_path_get_child (GnomeCmdPath *path, const gchar *child)
{
    g_return_val_if_fail (GNOME_CMD_IS_PATH (path), NULL);

    GnomeCmdPathClass *klass = GNOME_CMD_PATH_GET_CLASS (path);

    return klass->get_child (path, child);
}
