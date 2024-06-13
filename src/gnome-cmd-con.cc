/**
 * @file gnome-cmd-con.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"

using namespace std;


struct GnomeCmdConPrivate
{
    gchar          *uuid;
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


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdCon, gnome_cmd_con, G_TYPE_OBJECT)


// Keep this in sync with enum ConnectionMethodID in gnome-cmd-con.h
const gchar *icon_name[] = {"folder-remote",           // CON_SSH
                            "folder-remote",           // CON_FTP
                            "folder-remote",           // CON_ANON_FTP
#ifdef HAVE_SAMBA
                            "folder-remote",           // CON_SMB
#endif
                            "folder-remote",           // CON_DAV
                            "folder-remote",           // CON_DAVS
                            "network-workgroup",       // CON_URI
                            "folder"};                 // CON_FILE


static void on_open_done (GnomeCmdCon *con)
{
    gnome_cmd_con_updated (con);
}


static void on_open_failed (GnomeCmdCon *con)
{
    // gnome_cmd_con_updated (con);
    // Free the error because the error handling is done now. (Logging happened already.)
    g_error_free(con->open_failed_error);
    con->open_failed_msg = nullptr;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void dispose (GObject *object)
{
    GnomeCmdCon *con = GNOME_CMD_CON (object);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    g_clear_pointer (&priv->uuid, g_free);
    g_clear_pointer (&con->alias, g_free);
    g_clear_pointer (&con->uri, g_free);
    g_clear_pointer (&con->scheme, g_free);

    if (con->base_path != nullptr)
    {
        delete con->base_path;
        con->base_path = nullptr;
    }
    if (con->root_path != nullptr)
    {
        g_string_free (con->root_path, TRUE);
        con->root_path = nullptr;
    }
    g_clear_object (&con->base_gFileInfo);
    g_clear_error (&con->open_failed_error);

    g_clear_pointer (&priv->default_dir, gnome_cmd_dir_unref);

    if (priv->dir_history != nullptr)
    {
        delete priv->dir_history;
        priv->dir_history = nullptr;
    }

    G_OBJECT_CLASS (gnome_cmd_con_parent_class)->dispose (object);
}


static void gnome_cmd_con_class_init (GnomeCmdConClass *klass)
{
    signals[UPDATED] =
        g_signal_new ("updated",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, updated),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[CLOSE] =
        g_signal_new ("close",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, close),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[OPEN_DONE] =
        g_signal_new ("open-done",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, open_done),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[OPEN_FAILED] =
        g_signal_new ("open-failed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdConClass, open_failed),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    G_OBJECT_CLASS (klass)->dispose = dispose;

    klass->updated = nullptr;
    klass->open_done = on_open_done;
    klass->open_failed = on_open_failed;

    klass->open = nullptr;
    klass->close = nullptr;
    klass->cancel_open = nullptr;
    klass->open_is_needed = nullptr;
    klass->create_gfile = nullptr;
    klass->create_path = nullptr;

    klass->get_go_text = nullptr;
    klass->get_open_text = nullptr;
    klass->get_close_text = nullptr;
    klass->get_go_tooltip = nullptr;
    klass->get_open_tooltip = nullptr;
    klass->get_close_tooltip = nullptr;
    klass->get_go_icon = nullptr;
    klass->get_open_icon = nullptr;
    klass->get_close_icon = nullptr;
}


static void gnome_cmd_con_init (GnomeCmdCon *con)
{
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    priv->uuid = g_uuid_string_random ();

    con->alias = nullptr;
    con->uri = nullptr;
    con->scheme = nullptr;
    con->method = CON_URI;

    con->base_path = nullptr;
    con->root_path = g_string_sized_new (128);
    con->open_msg = nullptr;
    con->should_remember_dir = FALSE;
    con->needs_open_visprog = FALSE;
    con->needs_list_visprog = FALSE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = FALSE;

    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_result = GnomeCmdCon::OPEN_NOT_STARTED;
    con->open_failed_msg = nullptr;
    con->open_failed_error = nullptr;

    priv->default_dir = nullptr;
    priv->dir_history = new History(20);
    priv->bookmarks = g_new0 (GnomeCmdBookmarkGroup, 1);
    priv->bookmarks->con = con;
    // priv->bookmarks->bookmarks = nullptr;
    // priv->bookmarks->data = nullptr;
    priv->all_dirs = nullptr;
    priv->all_dirs_map = nullptr;
}


/***********************************
 * Public functions
 ***********************************/

const gchar *gnome_cmd_con_get_uuid (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));
    return priv->uuid;
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
                g_signal_emit (con, signals[OPEN_DONE], 0);
            }
            return FALSE;

        case GnomeCmdCon::OPEN_FAILED:
            {
                DEBUG ('m', "GnomeCmdCon::OPEN_FAILED detected\n");
                DEBUG ('m', "Emitting 'open-failed' signal\n");
                g_signal_emit (con, signals[OPEN_FAILED], 0);
            }
            return FALSE;

        default:
            return FALSE;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


void gnome_cmd_con_set_base_path(GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_if_fail (con != nullptr);
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (path != nullptr);

    if (con->base_path)
        delete con->base_path;

    con->base_path = path;
}


void set_con_base_path_for_gmount(GnomeCmdCon *con, GMount *gMount)
{
    g_return_if_fail (con != nullptr);
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (gMount != nullptr);
    g_return_if_fail (G_IS_MOUNT(gMount));

    auto gFile = g_mount_get_default_location(gMount);
    auto pathString = g_file_get_path(gFile);
    g_object_unref(gFile);

    gnome_cmd_con_set_base_path(con, new GnomeCmdPlainPath(pathString));

    g_free(pathString);
}

gboolean set_con_base_gfileinfo(GnomeCmdCon *con)
{
    g_return_val_if_fail (con != nullptr, FALSE);
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    GError *error = nullptr;

    if (con->base_gFileInfo)
    {
        g_object_unref(con->base_gFileInfo);
        con->base_gFileInfo = nullptr;
    }

    auto gFile = con->is_local
        ? gnome_cmd_con_create_gfile (con, con->base_path)
        : gnome_cmd_con_create_gfile (con, nullptr);
    con->base_gFileInfo = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    g_object_unref(gFile);
    if (error)
    {
        g_critical("set_con_base_gfileinfo: error: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    return TRUE;
}

void gnome_cmd_con_open (GnomeCmdCon *con, GtkWindow *parent_window)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    DEBUG ('m', "Opening connection\n");

    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);

    if (con->state != GnomeCmdCon::STATE_OPEN)
        klass->open (con, parent_window);

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

    if (gnome_cmd_con_is_closeable (con) && gnome_cmd_con_is_open(con))
    {
        g_signal_emit (con, signals[CLOSE], 0);
        g_signal_emit (con, signals[UPDATED], 0);
    }

    return TRUE;
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
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    if (dir == priv->default_dir)
        return;

    if (dir)
        gnome_cmd_dir_ref (dir);
    if (priv->default_dir)
        gnome_cmd_dir_unref (priv->default_dir);
    priv->default_dir = dir;
}


GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    return priv->default_dir;
}


History *gnome_cmd_con_get_dir_history (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    return priv->dir_history;
}


GnomeCmdBookmarkGroup *gnome_cmd_con_get_bookmarks (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    return priv->bookmarks;
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
    if (!con)
        return;
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    GnomeCmdBookmarkGroup *group = priv->bookmarks;
    for(GList *l = group->bookmarks; l; l = l->next)
    {
        auto bookmark = static_cast<GnomeCmdBookmark*> (l->data);
        g_free (bookmark->name);
        g_free (bookmark->path);
        g_free (bookmark);
    }
    g_list_free(group->bookmarks);
    priv->bookmarks = g_new0 (GnomeCmdBookmarkGroup, 1);
    priv->bookmarks->con = con;
}


void gnome_cmd_con_updated (GnomeCmdCon *con)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));

    g_signal_emit (con, signals[UPDATED], 0);
}


/**
 *  Get the type of the file at the specified path.
 *  If the file does not exists, the function will return false.
 *  If the operation succeeds, true is returned and type is set.
 */
gboolean gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path_str, GFileType *gFileType)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), false);
    g_return_val_if_fail (path_str != nullptr, false);

    GnomeCmdPath *path = gnome_cmd_con_create_path (con, path_str);
    auto gFile = gnome_cmd_con_create_gfile(con, path);

    if (!gFile || !g_file_query_exists(gFile, nullptr))
    {
        if (gFile)
            g_object_unref(gFile);
        *gFileType = G_FILE_TYPE_UNKNOWN;
        delete path;
        return false;
    }

    *gFileType = g_file_query_file_type(gFile, G_FILE_QUERY_INFO_NONE, nullptr);
    g_object_unref(gFile);
    delete path;

    return true;
}


gboolean gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str, GError *error)
{
    GError *tmpError = nullptr;

    g_return_val_if_fail (GNOME_CMD_IS_CON (con), false);
    g_return_val_if_fail (path_str != nullptr, false);

    auto path = gnome_cmd_con_create_path (con, path_str);
    auto gFile = gnome_cmd_con_create_gfile (con, path);

    if (!g_file_make_directory (gFile, nullptr, &tmpError))
    {
        g_warning("g_file_make_directory error: %s\n", tmpError ? tmpError->message : "unknown error");
        g_propagate_error(&error, tmpError);
        return false;
    }

    g_object_unref (gFile);
    delete path;

    return true;
}


void gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir, gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    if (!uri_str)
    {
        uri_str = GNOME_CMD_FILE (dir)->get_uri_str();
    }

    if (!uri_str)
    {
        return;
    }

    if (!priv->all_dirs_map)
        priv->all_dirs_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, nullptr);

    DEBUG ('k', "ADDING %p %s to the cache\n", dir, uri_str);
    g_hash_table_insert (priv->all_dirs_map, uri_str, dir);
}


void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    DEBUG ('k', "REMOVING %p %s from the cache\n", dir, uri_str);
    g_hash_table_remove (priv->all_dirs_map, uri_str);
    g_free (uri_str);
}


void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, const gchar *uri_str)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (uri_str != nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    DEBUG ('k', "REMOVING %s from the cache\n", uri_str);
    g_hash_table_remove (priv->all_dirs_map, uri_str);
}


GnomeCmdDir *gnome_cmd_con_cache_lookup (GnomeCmdCon *con, const gchar *uri_str)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    g_return_val_if_fail (uri_str != nullptr, nullptr);
    auto priv = static_cast<GnomeCmdConPrivate *> (gnome_cmd_con_get_instance_private (con));

    GnomeCmdDir *dir = nullptr;

    if (priv->all_dirs_map)
    {
        dir = static_cast<GnomeCmdDir*> (g_hash_table_lookup (priv->all_dirs_map, uri_str));
    }

    if (dir)
        DEBUG ('k', "FOUND %p %s in the hash-table, reusing it!\n", dir, uri_str);
    else
        DEBUG ('k', "FAILED to find %s in the hash-table\n", uri_str);

    return dir;
}


gchar *gnome_cmd_con_get_go_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    if (klass->get_go_text)
        return klass->get_go_text (con);

    const gchar *alias = con->alias;
    if (!alias)
        alias = _("<New connection>");

    return g_strdup_printf (_("Go to: %s"), alias);
}


gchar *gnome_cmd_con_get_go_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_go_tooltip ? klass->get_go_tooltip (con) : nullptr;
}


GIcon *gnome_cmd_con_get_go_icon (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_go_icon ? klass->get_go_icon (con) : nullptr;
}


gchar *gnome_cmd_con_get_open_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    if (klass->get_open_text)
        return klass->get_open_text (con);

    const gchar *alias = con->alias;
    if (!alias)
        alias = _("<New connection>");

    return g_strdup_printf (_("Connect to: %s"), alias);
}


gchar *gnome_cmd_con_get_open_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_open_tooltip ? klass->get_open_tooltip (con) : nullptr;
}


GIcon *gnome_cmd_con_get_open_icon (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_open_icon ? klass->get_open_icon (con) : nullptr;
}


gchar *gnome_cmd_con_get_close_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    if (klass->get_close_text)
        return klass->get_close_text (con);

    const gchar *alias = con->alias;
    if (!alias)
        alias = _("<New connection>");

    return g_strdup_printf (_("Disconnect from: %s"), alias);
}


gchar *gnome_cmd_con_get_close_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_close_tooltip ? klass->get_close_tooltip (con) : nullptr;
}


GIcon *gnome_cmd_con_get_close_icon (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->get_close_icon ? klass->get_close_icon (con) : nullptr;
}


const gchar *gnome_cmd_con_get_icon_name (ConnectionMethodID method)
{
    return icon_name[method];
}


string &__gnome_cmd_con_make_uri (string &s, const gchar *method, string &server, string &port, string &folder)
{
    if (!folder.empty())
    {
        // remove initial '/' characters
        size_t len = 0;
        while (folder[len] == '/')
            ++len;
        folder.erase(0, len);
    }

    folder = stringify (g_uri_escape_string (folder.c_str(), nullptr, true));

    s = method;

    s += server;

    if (!port.empty())
        s += ':' + port;

    if (!folder.empty())
        s += '/' + folder;

    return s;
}

#ifdef HAVE_SAMBA
std::string &gnome_cmd_con_make_smb_uri (std::string &uriString, std::string &server, std::string &folder, std::string &domain)
{
    if (!folder.empty())
    {
        // remove initial '/' characters
        size_t len = 0;
        while (folder[len] == '/')
            ++len;
        folder.erase(0, len);
    }

    uriString = "smb://";

    if (!domain.empty())
        uriString += domain + ';';

    uriString += server;

    if (!folder.empty())
        uriString += '/' + folder;

    return uriString;
}
#endif


/**
 * This function checks if the active or inactive file pane is showing
 * files from the given GMount. If yes, close the connection to this GMount
 * and open the home connection instead.
 */
void gnome_cmd_con_close_active_or_inactive_connection (GMount *gMount)
{
    g_return_if_fail(gMount != nullptr);
    g_return_if_fail(G_IS_MOUNT(gMount));
    auto gFile = g_mount_get_root(gMount);
    auto uriString = g_file_get_uri(gFile);

    auto activeConUri = gnome_cmd_con_get_uri(main_win->fs(ACTIVE)->get_connection());
    auto activeConGFile = activeConUri ? g_file_new_for_uri(activeConUri) : nullptr;
    auto inactiveConUri = gnome_cmd_con_get_uri(main_win->fs(INACTIVE)->get_connection());
    auto inactiveConGFile = inactiveConUri ? g_file_new_for_uri(inactiveConUri) : nullptr;

    if (activeConUri && g_file_equal(gFile, activeConGFile))
    {
        gnome_cmd_con_close (main_win->fs(ACTIVE)->get_connection());
        main_win->fs(ACTIVE)->set_connection(get_home_con());
    }
    if (inactiveConUri && g_file_equal(gFile, inactiveConGFile))
    {
        gnome_cmd_con_close (main_win->fs(INACTIVE)->get_connection());
        main_win->fs(INACTIVE)->set_connection(get_home_con());
    }

    g_free(uriString);
    g_object_unref(gFile);
}

