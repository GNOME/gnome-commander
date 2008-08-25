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

using namespace std;


struct GnomeCmdFileCollectionClass
{
    GtkObjectClass parent_class;
};


struct GnomeCmdFileCollection::Private
{
    GHashTable *map;
    GList *list;
};

static GtkObjectClass *gnome_cmd_file_collection_parent_class = NULL;


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_file_collection_destroy (GtkObject *obj)
{
    GnomeCmdFileCollection *collection = GNOME_CMD_FILE_COLLECTION (obj);

    g_hash_table_destroy (collection->priv->map);
    g_list_free (collection->priv->list);
    g_free (collection->priv);

    if (GTK_OBJECT_CLASS (gnome_cmd_file_collection_parent_class)->destroy)
        (*GTK_OBJECT_CLASS (gnome_cmd_file_collection_parent_class)->destroy) (obj);
}


static void gnome_cmd_file_collection_class_init (GnomeCmdFileCollectionClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

    gnome_cmd_file_collection_parent_class = (GtkObjectClass *) gtk_type_class (gtk_object_get_type ());

    object_class->destroy = gnome_cmd_file_collection_destroy;
}


static void gnome_cmd_file_collection_init (GnomeCmdFileCollection *collection)
{
    collection->priv = g_new0 (GnomeCmdFileCollection::Private, 1);
    collection->priv->map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gnome_cmd_file_unref);
    collection->priv->list = NULL;
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_file_collection_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdFileCollection",
            sizeof (GnomeCmdFileCollection),
            sizeof (GnomeCmdFileCollectionClass),
            (GtkClassInitFunc) gnome_cmd_file_collection_class_init,
            (GtkObjectInitFunc) gnome_cmd_file_collection_init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_object_get_type (), &info);
    }
    return type;
}


void GnomeCmdFileCollection::add(GnomeCmdFile *file)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (file));

    priv->list = g_list_append (priv->list, file);

    gchar *uri_str = gnome_cmd_file_get_uri_str (file);
    g_hash_table_insert (priv->map, uri_str, file);
    gnome_cmd_file_ref (file);
}


void GnomeCmdFileCollection::remove(GnomeCmdFile *file)
{
    g_return_if_fail (GNOME_CMD_IS_FILE (file));

    priv->list = g_list_remove (priv->list, file);

    gchar *uri_str = gnome_cmd_file_get_uri_str (file);
    g_hash_table_remove (priv->map, uri_str);
    g_free (uri_str);
}


void GnomeCmdFileCollection::remove(const gchar *uri_str)
{
    g_return_if_fail (uri_str != NULL);

    GnomeCmdFile *file = find(uri_str);
    priv->list = g_list_remove (priv->list, file);

    g_hash_table_remove (priv->map, uri_str);
}


GnomeCmdFile *GnomeCmdFileCollection::find(const gchar *uri_str)
{
    g_return_val_if_fail (uri_str != NULL, NULL);

    return GNOME_CMD_FILE (g_hash_table_lookup (priv->map, uri_str));
}


void GnomeCmdFileCollection::clear()
{
    g_list_free (priv->list);
    priv->list = NULL;
    g_hash_table_destroy (priv->map);
    priv->map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gnome_cmd_file_unref);
}


GList *GnomeCmdFileCollection::get_list()
{
    return priv->list;
}


GList *GnomeCmdFileCollection::sort(GCompareDataFunc compare_func, gpointer user_data)
{
    priv->list = g_list_sort_with_data (priv->list, compare_func, user_data);

    return priv->list;
}
