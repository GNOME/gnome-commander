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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-collection.h"

struct _GnomeCmdFileCollectionPrivate
{
    GHashTable *map;
    GList *list;
};

static GtkObjectClass *parent_class = NULL;


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *obj)
{
    GnomeCmdFileCollection *collection = GNOME_CMD_FILE_COLLECTION (obj);

    g_hash_table_destroy (collection->priv->map);
    g_list_free (collection->priv->list);
    g_free (collection->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (obj);
}


static void
class_init (GnomeCmdFileCollectionClass *klass)
{
    GtkObjectClass *object_class;

    object_class = GTK_OBJECT_CLASS (klass);
    parent_class = gtk_type_class (gtk_object_get_type ());

    object_class->destroy = destroy;
}


static void
init (GnomeCmdFileCollection *collection)
{
    collection->priv = g_new (GnomeCmdFileCollectionPrivate, 1);
    collection->priv->map = g_hash_table_new_full (
        g_str_hash, g_str_equal, g_free, (GDestroyNotify)gnome_cmd_file_unref);
    collection->priv->list = NULL;
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_file_collection_get_type         (void)
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdFileCollection",
            sizeof (GnomeCmdFileCollection),
            sizeof (GnomeCmdFileCollectionClass),
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


GnomeCmdFileCollection *
gnome_cmd_file_collection_new (void)
{
    GnomeCmdFileCollection *collection = gtk_type_new (gnome_cmd_file_collection_get_type ());

    return collection;
}


void
gnome_cmd_file_collection_add (GnomeCmdFileCollection *collection,
                               GnomeCmdFile *file)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_COLLECTION (collection));
    g_return_if_fail (GNOME_CMD_IS_FILE (file));

    collection->priv->list = g_list_append (collection->priv->list, file);

    gchar *uri_str = gnome_cmd_file_get_uri_str (file);
    g_hash_table_insert (collection->priv->map, uri_str, file);
    gnome_cmd_file_ref (file);
}


void
gnome_cmd_file_collection_add_list (GnomeCmdFileCollection *collection,
                                    GList *files)
{
    for (; files; files = files->next)
        gnome_cmd_file_collection_add (collection, GNOME_CMD_FILE (files->data));
}


void
gnome_cmd_file_collection_remove (GnomeCmdFileCollection *collection,
                                  GnomeCmdFile *file)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_COLLECTION (collection));
    g_return_if_fail (GNOME_CMD_IS_FILE (file));

    collection->priv->list = g_list_remove (collection->priv->list, file);

    gchar *uri_str = gnome_cmd_file_get_uri_str (file);
    g_hash_table_remove (collection->priv->map, uri_str);
    g_free (uri_str);
}


void
gnome_cmd_file_collection_remove_by_uri (GnomeCmdFileCollection *collection,
                                         const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_COLLECTION (collection));
    g_return_if_fail (uri_str != NULL);

    GnomeCmdFile *file = gnome_cmd_file_collection_lookup (collection, uri_str);
    collection->priv->list = g_list_remove (collection->priv->list, file);

    g_hash_table_remove (collection->priv->map, uri_str);
}


GnomeCmdFile *
gnome_cmd_file_collection_lookup (GnomeCmdFileCollection *collection,
                                  const gchar *uri_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_COLLECTION (collection), NULL);
    g_return_val_if_fail (uri_str != NULL, NULL);

    return GNOME_CMD_FILE (g_hash_table_lookup (collection->priv->map, uri_str));
}


gint
gnome_cmd_file_collection_get_size (GnomeCmdFileCollection *collection)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_COLLECTION (collection), 0);

    return g_list_length (collection->priv->list);
}


void
gnome_cmd_file_collection_clear (GnomeCmdFileCollection *collection)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_COLLECTION (collection));

    g_list_free (collection->priv->list);
    collection->priv->list = NULL;
    g_hash_table_destroy (collection->priv->map);
    collection->priv->map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)gnome_cmd_file_unref);
}


GList *
gnome_cmd_file_collection_get_list (GnomeCmdFileCollection *collection)
{
    g_return_val_if_fail (GNOME_CMD_IS_FILE_COLLECTION (collection), NULL);

    return collection->priv->list;
}
