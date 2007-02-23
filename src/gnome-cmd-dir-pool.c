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
#include "gnome-cmd-dir-pool.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-clist.h"
#include "gnome-cmd-data.h"
#include "utils.h"


struct _GnomeCmdDirPoolPrivate
{
    GList *queue;
    GHashTable *map;
};


static GtkObjectClass *parent_class = NULL;


static void
check_cache_maxsize (GnomeCmdDirPool *pool)
{
    // remove the last dir if maxsize is exceeded
    while (g_list_length (pool->priv->queue) > gnome_cmd_data_get_dir_cache_size ())
    {
        GnomeCmdDir *dir = (GnomeCmdDir *) g_list_last (pool->priv->queue)->data;
        g_hash_table_remove (pool->priv->map, gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir)));
        pool->priv->queue = g_list_remove (pool->priv->queue, dir);
        DEBUG ('k', "removing %s 0x%x from the pool\n", gnome_cmd_dir_get_path (dir), dir);
        gnome_cmd_dir_unref (dir);

        DEBUG('p', "queue size: %d  map size: %d\n",
              g_list_length (pool->priv->queue),
              g_hash_table_size (pool->priv->map));
    }
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdDirPool *pool = GNOME_CMD_DIR_POOL (object);

    g_hash_table_destroy (pool->priv->map);
    g_list_foreach (pool->priv->queue, (GFunc)gnome_cmd_dir_unref, NULL);
    g_list_free (pool->priv->queue);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdDirPoolClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

    parent_class = (GtkObjectClass *) gtk_type_class (gtk_object_get_type ());

    object_class->destroy = destroy;
}


static void
init (GnomeCmdDirPool *pool)
{
    pool->priv = g_new (GnomeCmdDirPoolPrivate, 1);

    pool->priv->map = g_hash_table_new (g_str_hash, g_str_equal);
    pool->priv->queue = NULL;
}


/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_dir_pool_get_type         (void)
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdDirPool",
            sizeof (GnomeCmdDirPool),
            sizeof (GnomeCmdDirPoolClass),
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


GnomeCmdDirPool *
gnome_cmd_dir_pool_new (void)
{
    return (GnomeCmdDirPool *) gtk_type_new (gnome_cmd_dir_pool_get_type ());
}


GnomeCmdDir *
gnome_cmd_dir_pool_get (GnomeCmdDirPool *pool, const gchar *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR_POOL (pool), NULL);

    GnomeCmdDir *dir = (GnomeCmdDir *) g_hash_table_lookup (pool->priv->map, path);

    if (dir)
    {
        // move it first in the queue
        pool->priv->queue = g_list_remove (pool->priv->queue, dir);
        pool->priv->queue = g_list_prepend (pool->priv->queue, dir);
    }

    return dir;
}


void
gnome_cmd_dir_pool_add (GnomeCmdDirPool *pool, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_DIR_POOL (pool));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    DEBUG ('k', "adding %s 0x%x to the pool\n", gnome_cmd_dir_get_path (dir), dir);
    gnome_cmd_dir_ref (dir);
    pool->priv->queue = g_list_prepend (pool->priv->queue, dir);
    g_hash_table_insert (pool->priv->map, (gpointer)gnome_cmd_dir_get_path (dir), dir);

    check_cache_maxsize (pool);
}


void
gnome_cmd_dir_pool_remove (GnomeCmdDirPool *pool, GnomeCmdDir *dir)
{
}


extern GList *all_dirs;


static void
print_dir (GnomeCmdDir *dir, gpointer data)
{
    g_printerr ("%s\n", gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir)));
}


void
gnome_cmd_dir_pool_show_state (GnomeCmdDirPool *pool)
{
    g_printerr ("\n\n------------= All currently existing directories =-------------\n");
    g_list_foreach (all_dirs, (GFunc)print_dir, NULL);
}

