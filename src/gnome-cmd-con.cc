/**
 * @file gnome-cmd-con.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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
#include "gnome-cmd-con.h"

using namespace std;


struct GnomeCmdConPrivate
{
    GnomeCmdDir    *default_dir;   // the start dir of this connection
    History        *dir_history;
    GnomeCmdBookmarkGroup *bookmarks;
    GList          *all_dirs;
    GHashTable     *all_dirs_map;
};

enum
{
    UPDATED,
    CLOSE,
    OPEN_DONE,
    OPEN_FAILED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GtkObjectClass *parent_class = nullptr;


// Keep this in sync with enum ConnectionMethodID in gnome-cmd-con.h
const gchar *icon_name[] = {"gnome-fs-ssh",            // CON_SSH
                            "gnome-fs-ftp",            // CON_FTP
                            "gnome-fs-ftp",            // CON_ANON_FTP
#ifdef HAVE_SAMBA
                            "gnome-fs-smb",            // CON_SMB
#endif
                            "gnome-fs-web",            // CON_DAV
                            "gnome-fs-web",            // CON_DAVS
                            "gnome-fs-network",        // CON_URI
                            "gnome-fs-directory"};     // CON_LOCAL


static void on_open_done (GnomeCmdCon *con)
{
    gnome_cmd_con_updated (con);
}


static void on_open_failed (GnomeCmdCon *con, const gchar *msg, GnomeVFSResult result)
{
    // gnome_cmd_con_updated (con);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdCon *con = GNOME_CMD_CON (object);

    g_free (con->alias);
    g_free (con->uri);

    delete con->base_path;
    g_string_free (con->root_path, TRUE);
    g_free (con->open_text);
    g_free (con->open_tooltip);
    gnome_cmd_pixmap_free (con->open_pixmap);
    g_free (con->close_text);
    g_free (con->close_tooltip);
    gnome_cmd_pixmap_free (con->close_pixmap);
    gnome_cmd_pixmap_free (con->go_pixmap);

    if (con->priv->default_dir)
        gnome_cmd_dir_unref (con->priv->default_dir);

    delete con->priv->dir_history;

    g_free (con->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConClass *klass)
{
    GtkObjectClass *object_class;

    object_class = GTK_OBJECT_CLASS (klass);
    parent_class = (GtkObjectClass *) gtk_type_class (gtk_object_get_type ());

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
        gtk_signal_new ("open-done",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, open_done),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    signals[OPEN_FAILED] =
        gtk_signal_new ("open-failed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdConClass, open_failed),
            gtk_marshal_NONE__POINTER_INT,
            GTK_TYPE_NONE,
            2, GTK_TYPE_POINTER, GTK_TYPE_INT);

    object_class->destroy = destroy;

    klass->updated = nullptr;
    klass->open_done = on_open_done;
    klass->open_failed = on_open_failed;

    klass->open = nullptr;
    klass->close = nullptr;
    klass->cancel_open = nullptr;
    klass->open_is_needed = nullptr;
    klass->create_uri = nullptr;
    klass->create_path = nullptr;
}


static void init (GnomeCmdCon *con)
{
    con->alias = nullptr;
    con->uri = nullptr;
    con->method = CON_URI;
    con->auth = GnomeCmdCon::NOT_REQUIRED;

    con->base_path = nullptr;
    con->base_info = nullptr;
    con->root_path = g_string_sized_new (128);
    con->open_msg = nullptr;
    con->should_remember_dir = FALSE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = FALSE;
    con->go_text = nullptr;
    con->go_tooltip = nullptr;
    con->go_pixmap = nullptr;
    con->open_text = nullptr;
    con->open_tooltip = nullptr;
    con->open_pixmap = nullptr;
    con->close_text = nullptr;
    con->close_tooltip = nullptr;
    con->close_pixmap = nullptr;

    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_result = GnomeCmdCon::OPEN_NOT_STARTED;
    con->open_failed_reason = GNOME_VFS_OK;
    con->open_failed_msg = nullptr;

    con->priv = g_new0 (GnomeCmdConPrivate, 1);
    con->priv->default_dir = nullptr;
    con->priv->dir_history = new History(20);
    con->priv->bookmarks = g_new0 (GnomeCmdBookmarkGroup, 1);
    con->priv->bookmarks->con = con;
    // con->priv->bookmarks->bookmarks = nullptr;
    // con->priv->bookmarks->data = nullptr;
    con->priv->all_dirs = nullptr;
    con->priv->all_dirs_map = nullptr;
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_con_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            (gchar*) "GnomeCmdCon",
            sizeof (GnomeCmdCon),
            sizeof (GnomeCmdConClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        type = gtk_type_unique (gtk_object_get_type (), &info);
    }
    return type;
}


static gboolean check_con_open_progress (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    g_return_val_if_fail (con->open_result != GnomeCmdCon::OPEN_NOT_STARTED, FALSE);

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (con->open_result)
    {
        case GnomeCmdCon::OPEN_IN_PROGRESS:
            return TRUE;

        case GnomeCmdCon::OPEN_OK:
            {
                DEBUG('m', "GnomeCmdCon::OPEN_OK detected\n");

                GnomeCmdDir *dir = gnome_cmd_dir_new_with_con (con);

                gnome_cmd_con_set_default_dir (con, dir);

                DEBUG ('m', "Emitting 'open-done' signal\n");
                gtk_signal_emit (GTK_OBJECT (con), signals[OPEN_DONE]);
            }
            return FALSE;

        case GnomeCmdCon::OPEN_FAILED:
            {
                DEBUG ('m', "GnomeCmdCon::OPEN_FAILED detected\n");
                DEBUG ('m', "Emitting 'open-failed' signal\n");
                gtk_signal_emit (GTK_OBJECT (con), signals[OPEN_FAILED], con->open_failed_msg, con->open_failed_reason);
            }
            return FALSE;

        default:
            return FALSE;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


void gnome_cmd_con_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    DEBUG ('m', "Opening connection\n");

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    if (con->state != GnomeCmdCon::STATE_OPEN)
        klass->open (con);

    g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) check_con_open_progress, con);
}


void gnome_cmd_con_cancel_open (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    if (con->state == GnomeCmdCon::STATE_OPENING)
    {
        GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
        klass->cancel_open (con);
    }
}


gboolean gnome_cmd_con_close (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);

    if (gnome_cmd_con_is_closeable (con))
    {
        gtk_signal_emit (GTK_OBJECT (con), signals[CLOSE]);
        gtk_signal_emit (GTK_OBJECT (con), signals[UPDATED]);
    }

    return TRUE;
}


GnomeVFSURI *gnome_cmd_con_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    return klass->create_uri (con, path);
}


GFile *gnome_cmd_con_create_gfile (GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    return klass->create_gfile (con, path);
}


GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    return klass->create_path (con, path_str);
}


void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir)
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


GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    return con->priv->default_dir;
}


History *gnome_cmd_con_get_dir_history (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    return con->priv->dir_history;
}


GnomeCmdBookmarkGroup *gnome_cmd_con_get_bookmarks (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);

    return con->priv->bookmarks;
}


void gnome_cmd_con_add_bookmark (GnomeCmdCon *con, gchar *name, gchar *path)
{
    GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);
    GnomeCmdBookmark *bookmark = g_new (GnomeCmdBookmark, 1);
    bookmark->name = name;
    bookmark->path = path;
    bookmark->group = group;
    group->bookmarks = g_list_append (group->bookmarks, bookmark);
}


void gnome_cmd_con_erase_bookmark (GnomeCmdCon *con)
{
    GnomeCmdBookmarkGroup *group = con->priv->bookmarks;
    for(GList *l = group->bookmarks; l; l = l->next)
    {
        auto bookmark = static_cast<GnomeCmdBookmark*> (l->data);
        g_free (bookmark->name);
        g_free (bookmark->path);
        g_free (bookmark);
    }
    g_list_free(group->bookmarks);
    con->priv->bookmarks = g_new0 (GnomeCmdBookmarkGroup, 1);
    con->priv->bookmarks->con = con;
}


void gnome_cmd_con_updated (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    gtk_signal_emit (GTK_OBJECT (con), signals[UPDATED]);
}


// Get the type of the file at the specified path.
// If the operation succeeds GNOME_VFS_OK is returned and type is set
GnomeVFSResult gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path_str, GnomeVFSFileType *type)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), GNOME_VFS_ERROR_BAD_PARAMETERS);
    g_return_val_if_fail (path_str != nullptr, GNOME_VFS_ERROR_BAD_PARAMETERS);

    GnomeCmdPath *path = gnome_cmd_con_create_path (con, path_str);
    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, path);
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new ();
    GnomeVFSResult res = gnome_vfs_get_file_info_uri (uri, info, GNOME_VFS_FILE_INFO_DEFAULT);

    if (res == GNOME_VFS_OK && info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK)         // resolve the symlink to get the real type of it
        res = gnome_vfs_get_file_info_uri (uri, info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

    if (res == GNOME_VFS_OK)
        *type = info->type;

    gnome_vfs_uri_unref (uri);
    delete path;
    gnome_vfs_file_info_unref (info);

    return res;
}


GnomeVFSResult gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str)
{
    GnomeVFSResult result;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), GNOME_VFS_ERROR_BAD_PARAMETERS);
    g_return_val_if_fail (path_str != nullptr, GNOME_VFS_ERROR_BAD_PARAMETERS);

    GnomeCmdPath *path = gnome_cmd_con_create_path (con, path_str);
    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, path);

    result = gnome_vfs_make_directory_for_uri (
        uri,
        GNOME_CMD_PERM_USER_READ|GNOME_CMD_PERM_USER_WRITE|GNOME_CMD_PERM_USER_EXEC|
        GNOME_CMD_PERM_GROUP_READ|GNOME_CMD_PERM_GROUP_EXEC|
        GNOME_CMD_PERM_OTHER_READ|GNOME_CMD_PERM_OTHER_EXEC);

    gnome_vfs_uri_unref (uri);
    delete path;

    return result;
}


void gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    if (!con->priv->all_dirs_map)
        con->priv->all_dirs_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, nullptr);

    DEBUG ('k', "ADDING 0x%p %s to the cache\n", dir, uri_str);
    g_hash_table_insert (con->priv->all_dirs_map, uri_str, dir);
}


void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    DEBUG ('k', "REMOVING 0x%p %s from the cache\n", dir, uri_str);
    g_hash_table_remove (con->priv->all_dirs_map, uri_str);
    g_free (uri_str);
}


void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (uri_str != nullptr);

    DEBUG ('k', "REMOVING %s from the cache\n", uri_str);
    g_hash_table_remove (con->priv->all_dirs_map, uri_str);
}


GnomeCmdDir *gnome_cmd_con_cache_lookup (GnomeCmdCon *con, const gchar *uri_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    g_return_val_if_fail (uri_str != nullptr, nullptr);

    GnomeCmdDir *dir = nullptr;

    if (con->priv->all_dirs_map)
    {
        dir = static_cast<GnomeCmdDir*> (g_hash_table_lookup (con->priv->all_dirs_map, uri_str));
    }

    if (dir)
        DEBUG ('k', "FOUND 0x%p %s in the hash-table, reusing it!\n", dir, uri_str);
    else
        DEBUG ('k', "FAILED to find %s in the hash-table\n", uri_str);

    return dir;
}


const gchar *gnome_cmd_con_get_icon_name (ConnectionMethodID method)
{
    return icon_name[method];
}


string &__gnome_cmd_con_make_uri (string &s, const gchar *method, gboolean use_auth, string &server, string &port, string &folder, string &user, string &password)
{
    user = stringify (g_strescape (user.c_str(), nullptr));
    password = stringify (g_strescape (password.c_str(), nullptr));

    if (!password.empty() && !use_auth)
    {
        user += ':';
        user += password;
    }

    folder = stringify (gnome_vfs_escape_path_string (folder.c_str()));

    s = method;

    if (!user.empty())
        s += user + '@';

    s += server;

    if (!port.empty())
        s += ':' + port;

    if (!folder.empty())
        s += '/' + folder;

    return s;
}

#ifdef HAVE_SAMBA
std::string &gnome_cmd_con_make_smb_uri (std::string &s, gboolean use_auth, std::string &server, std::string &share, std::string &folder, std::string &domain, std::string &user, std::string &password)
{
    share = '/' + share;

    user = stringify (g_strescape (user.c_str(), nullptr));
    password = stringify (g_strescape (password.c_str(), nullptr));

    if (!password.empty() && !use_auth)
    {
        user += ':';
        user += password;
    }

    if (!domain.empty())
        user = domain + ';' + user;

    const gchar *joinSign = !folder.empty() && folder[0] != '/' ? "/" : "";

    folder = share + joinSign + folder;
    folder = stringify (gnome_vfs_escape_path_string (folder.c_str()));

    s = "smb://";

    if (!user.empty())
        s += user + '@';

    s += server;
    s += folder;

    return s;
}
#endif
