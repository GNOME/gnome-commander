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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-dir.h"
#include "utils.h"

struct _GnomeCmdConPrivate
{
    GnomeCmdDir    *cwd;           // current working directory
    GnomeCmdDir    *root_dir;      // the root dir of this connection
    GnomeCmdDir    *default_dir;   // the start dir of this connection
    History        *dir_history;
    GnomeCmdBookmarkGroup *bookmarks;
    GnomeCmdDirPool *dir_pool;
    GList          *all_dirs;
    GHashTable     *all_dirs_map;
};

enum {
    UPDATED,
    CLOSE,
    OPEN_DONE,
    OPEN_FAILED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class = NULL;


static void
on_open_done (GnomeCmdCon *con)
{
    gnome_cmd_con_updated (con);
}


static void
on_open_failed (GnomeCmdCon *con, const gchar *msg, GnomeVFSResult result)
{
    //gnome_cmd_con_updated (con);
}



/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdCon *con = GNOME_CMD_CON (object);

    if (con->base_path)
        gtk_object_unref (GTK_OBJECT (con->base_path));
    if (con->alias)
        g_free (con->alias);
    if (con->open_text)
        g_free (con->open_text);
    if (con->open_tooltip)
        g_free (con->open_tooltip);
    if (con->open_pixmap)
        gnome_cmd_pixmap_free (con->open_pixmap);
    if (con->close_text)
        g_free (con->close_text);
    if (con->close_tooltip)
        g_free (con->close_tooltip);
    if (con->close_pixmap)
        gnome_cmd_pixmap_free (con->close_pixmap);

    if (con->priv->cwd)
        gnome_cmd_dir_unref (con->priv->cwd);
    if (con->priv->default_dir)
        gnome_cmd_dir_unref (con->priv->default_dir);
    if (con->priv->root_dir)
        gnome_cmd_dir_unref (con->priv->root_dir);
    g_free (con->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdConClass *class)
{
    GtkObjectClass *object_class;

    object_class = GTK_OBJECT_CLASS (class);
    parent_class = gtk_type_class (gtk_object_get_type ());

    signals[UPDATED] =
        gtk_signal_new ("updated",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, updated),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    signals[CLOSE] =
        gtk_signal_new ("close",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, close),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    signals[OPEN_DONE] =
        gtk_signal_new ("open_done",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, open_done),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    signals[OPEN_FAILED] =
        gtk_signal_new ("open_failed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, open_failed),
            gtk_marshal_NONE__POINTER_INT,
            GTK_TYPE_NONE,
            2, GTK_TYPE_POINTER, GTK_TYPE_INT);

    object_class->destroy = destroy;

    class->updated = NULL;
    class->open_done = on_open_done;
    class->open_failed = on_open_failed;

    class->open = NULL;
    class->close = NULL;
    class->cancel_open = NULL;
    class->open_is_needed = NULL;
    class->create_uri = NULL;
    class->create_path = NULL;
}


static void
init (GnomeCmdCon *con)
{
    con->base_path = NULL;
    con->base_info = NULL;
    con->alias = NULL;
    con->open_msg = NULL;
    con->should_remember_dir = FALSE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = FALSE;
    con->go_text = NULL;
    con->go_tooltip = NULL;
    con->go_pixmap = NULL;
    con->open_text = NULL;
    con->open_tooltip = NULL;
    con->open_pixmap = NULL;
    con->close_text = NULL;
    con->close_tooltip = NULL;
    con->close_pixmap = NULL;

    con->state = CON_STATE_CLOSED;
    con->open_result = CON_OPEN_NOT_STARTED;
    con->open_failed_reason = GNOME_VFS_OK;
    con->open_failed_msg = NULL;

    con->priv = g_new (GnomeCmdConPrivate, 1);
    con->priv->cwd = NULL;
    con->priv->default_dir = NULL;
    con->priv->dir_history = history_new (20);
    con->priv->dir_pool = gnome_cmd_dir_pool_new ();
    con->priv->bookmarks = g_new (GnomeCmdBookmarkGroup, 1);
    con->priv->bookmarks->bookmarks = NULL;
    con->priv->bookmarks->con = con;
    con->priv->bookmarks->data = NULL;
    con->priv->all_dirs = NULL;
    con->priv->all_dirs_map = NULL;
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_con_get_type         (void)
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdCon",
            sizeof (GnomeCmdCon),
            sizeof (GnomeCmdConClass),
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


static gboolean
check_con_open_progress (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    g_return_val_if_fail (con->open_result != CON_OPEN_NOT_STARTED, FALSE);

    if (con->open_result == CON_OPEN_OK) {
        GnomeCmdDir *dir;

        DEBUG('m', "CON_OPEN_OK detected\n");

        dir = gnome_cmd_dir_new_with_con (
            con->base_info, con->base_path, con);

        gnome_cmd_con_set_default_dir (con, dir);
        gnome_cmd_con_set_cwd (con, dir);

        DEBUG ('m', "Emitting open_done signal\n");
        gtk_signal_emit (GTK_OBJECT (con), signals[OPEN_DONE]);
    }
    else if (con->open_result == CON_OPEN_FAILED) {
        DEBUG ('m', "CON_OPEN_FAILED detected\n");
        DEBUG ('m', "Emitting open_failed signal\n");
        gtk_signal_emit (GTK_OBJECT (con), signals[OPEN_FAILED],
                         con->open_failed_msg, con->open_failed_reason);
    }

    return con->open_result == CON_OPEN_IN_PROGRESS;
}


void
gnome_cmd_con_open (GnomeCmdCon *con)
{
    GnomeCmdConClass *klass;

    g_return_if_fail (GNOME_CMD_IS_CON (con));
    DEBUG ('m', "Opening connection\n");

    klass = GNOME_CMD_CON_GET_CLASS (con);
    if (con->state != CON_STATE_OPEN)
        klass->open (con);

    gtk_timeout_add (gnome_cmd_data_get_gui_update_rate (),
                     (GtkFunction)check_con_open_progress,
                     con);
}


gboolean
gnome_cmd_con_is_open (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    return con->state == CON_STATE_OPEN;
}


void
gnome_cmd_con_cancel_open (GnomeCmdCon *con)
{
    GnomeCmdConClass *klass;

    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (con->state == CON_STATE_OPENING) {
        klass = GNOME_CMD_CON_GET_CLASS (con);
        klass->cancel_open (con);
    }
}


gboolean
gnome_cmd_con_close (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    if (gnome_cmd_con_is_closeable (con)) {
        gtk_signal_emit (GTK_OBJECT (con), signals[CLOSE]);
        gtk_signal_emit (GTK_OBJECT (con), signals[UPDATED]);
    }

    return TRUE;
}


gboolean
gnome_cmd_con_open_is_needed (GnomeCmdCon *con)
{
    GnomeCmdConClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->open_is_needed (con);
}


GnomeVFSURI *
gnome_cmd_con_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
    GnomeCmdConClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->create_uri (con, path);
}


GnomeCmdPath *
gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    GnomeCmdConClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->create_path (con, path_str);
}


gboolean
gnome_cmd_con_is_local (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    return con->is_local;
}


gboolean
gnome_cmd_con_is_closeable (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    return con->is_closeable;
}


const gchar *
gnome_cmd_con_get_open_msg (GnomeCmdCon *con)
{
    return con->open_msg;
}


const gchar *
gnome_cmd_con_get_alias (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->alias;
}


void
gnome_cmd_con_set_cwd (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (GNOME_CMD_IS_DIR (dir) &&
        gnome_cmd_dir_get_connection (dir) != con)
        return;

    if (dir == con->priv->cwd)
        return;

    if (dir)
        gnome_cmd_dir_ref (dir);
    if (con->priv->cwd)
        gnome_cmd_dir_unref (con->priv->cwd);
    con->priv->cwd = dir;
}


GnomeCmdDir *
gnome_cmd_con_get_cwd (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    /*
    if (!con->priv->cwd) {
        GnomeCmdDir *dir = gnome_cmd_dir_new (con, con->base_path);
        if (dir)
            gnome_cmd_con_set_cwd (con, dir);
    }
    */

    return con->priv->cwd;
}


void
gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (dir == con->priv->default_dir)
        return;

    if (dir)
        gnome_cmd_dir_ref (dir);
    if (con->priv->default_dir)
        gnome_cmd_dir_unref (con->priv->default_dir);
    con->priv->default_dir = dir;
}


GnomeCmdDir *
gnome_cmd_con_get_default_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    /*
    if (!con->priv->default_dir) {
        GnomeCmdDir *dir = gnome_cmd_dir_new (con, con->base_path);
        if (dir)
            gnome_cmd_con_set_default_dir (con, dir);
    }
    */

    return con->priv->default_dir;
}


void
gnome_cmd_con_set_root_dir (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (dir)
        gnome_cmd_dir_ref (dir);
    if (con->priv->root_dir)
        gnome_cmd_dir_unref (con->priv->root_dir);
    con->priv->root_dir = dir;
}


GnomeCmdDir *
gnome_cmd_con_get_root_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->priv->root_dir;
}



gboolean
gnome_cmd_con_should_remember_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    return con->should_remember_dir;
}


gboolean
gnome_cmd_con_needs_open_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    return con->needs_open_visprog;
}


gboolean
gnome_cmd_con_needs_list_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    return con->needs_list_visprog;
}


gboolean
gnome_cmd_con_can_show_free_space (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    return con->can_show_free_space;
}


History *
gnome_cmd_con_get_dir_history (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->priv->dir_history;
}


const gchar *
gnome_cmd_con_get_go_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->go_text;
}


const gchar *
gnome_cmd_con_get_open_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->open_text;
}


const gchar *
gnome_cmd_con_get_close_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->close_text;
}


const gchar *
gnome_cmd_con_get_go_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->go_tooltip;
}


const gchar *
gnome_cmd_con_get_open_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->open_tooltip;
}


const gchar *
gnome_cmd_con_get_close_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->close_tooltip;
}


GnomeCmdPixmap *
gnome_cmd_con_get_go_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->go_pixmap;
}


GnomeCmdPixmap *
gnome_cmd_con_get_open_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->open_pixmap;
}


GnomeCmdPixmap *
gnome_cmd_con_get_close_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->close_pixmap;
}



GnomeCmdBookmarkGroup *
gnome_cmd_con_get_bookmarks (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->priv->bookmarks;
}


void
gnome_cmd_con_set_bookmarks (GnomeCmdCon *con, GnomeCmdBookmarkGroup *bookmarks)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    con->priv->bookmarks = bookmarks;
}


GnomeCmdDirPool *
gnome_cmd_con_get_dir_pool (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->priv->dir_pool;
}


void
gnome_cmd_con_updated (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gtk_signal_emit (GTK_OBJECT (con), signals[UPDATED]);
}


/**
 * Get the type of the file at the specified path. If the operation
 * succeeds GNOME_VFS_OK is returned and type is set
 */
GnomeVFSResult
gnome_cmd_con_get_path_target_type (GnomeCmdCon *con,
                                    const gchar *path_str,
                                    GnomeVFSFileType *type)
{
    GnomeVFSFileInfo *info;
    GnomeVFSURI *uri;
    GnomeCmdPath *path;
    GnomeVFSResult res;
    GnomeVFSFileInfoOptions infoOpts = 0;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), GNOME_VFS_ERROR_BAD_PARAMETERS);
    g_return_val_if_fail (path_str != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);

    path = gnome_cmd_con_create_path (con, path_str);
    uri = gnome_cmd_con_create_uri (con, path);
    info = gnome_vfs_file_info_new ();
    res = gnome_vfs_get_file_info_uri (
        uri, info, infoOpts);

    if (res == GNOME_VFS_OK)
        *type = info->type;

    gnome_vfs_uri_unref (uri);
    gtk_object_destroy (GTK_OBJECT (path));
    gnome_vfs_file_info_unref (info);

    return res;
}


GnomeVFSResult
gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str)
{
    GnomeCmdPath *path;
    GnomeVFSURI *uri;
    GnomeVFSResult result;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), GNOME_VFS_ERROR_BAD_PARAMETERS);
    g_return_val_if_fail (path_str != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);

    path = gnome_cmd_con_create_path (con, path_str);
    uri = gnome_cmd_con_create_uri (con, path);

    result = gnome_vfs_make_directory_for_uri (
        uri,
        GNOME_VFS_PERM_USER_READ|GNOME_VFS_PERM_USER_WRITE|GNOME_VFS_PERM_USER_EXEC|
        GNOME_VFS_PERM_GROUP_READ|GNOME_VFS_PERM_GROUP_EXEC|
        GNOME_VFS_PERM_OTHER_READ|GNOME_VFS_PERM_OTHER_EXEC);

    gnome_vfs_uri_unref (uri);
    gtk_object_destroy (GTK_OBJECT (path));

    return result;
}


void
gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    gchar *uri_str;

    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    uri_str = gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir));

    if (!con->priv->all_dirs_map)
        con->priv->all_dirs_map = g_hash_table_new_full (
            g_str_hash, g_str_equal, g_free, NULL);

    DEBUG ('p', "ADDING 0x%x %s to the cache\n", (guint)dir, uri_str);
    g_hash_table_insert (con->priv->all_dirs_map, uri_str, dir);
}


void
gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    gchar *uri_str;

    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    uri_str = gnome_cmd_file_get_uri_str (GNOME_CMD_FILE (dir));

    DEBUG ('p', "REMOVING 0x%x %s from the cache\n", (guint)dir, uri_str);
    g_hash_table_remove (con->priv->all_dirs_map, uri_str);
    g_free (uri_str);
}


GnomeCmdDir *
gnome_cmd_con_cache_lookup (GnomeCmdCon *con, const gchar *uri_str)
{
    GnomeCmdDir *dir = NULL;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    g_return_val_if_fail (uri_str != NULL, NULL);

    if (con->priv->all_dirs_map)
        dir = g_hash_table_lookup (con->priv->all_dirs_map, uri_str);

    if (dir) {
        DEBUG ('p', "FOUND 0x%x %s in the hash-table, reusing it!\n", (guint)dir, uri_str);
        return dir;
    }

    DEBUG ('p', "FAILED to find %s in the hash-table\n", uri_str);
    return NULL;
}

