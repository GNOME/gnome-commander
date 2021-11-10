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
#include "gnome-cmd-plain-path.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"

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
                            "gnome-fs-directory"};     // CON_FILE


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
    g_object_unref(con->base_gFileInfo);
    g_error_free(con->open_failed_error);
    con->open_failed_msg = nullptr;
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
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    object_class->destroy = destroy;

    klass->updated = nullptr;
    klass->open_done = on_open_done;
    klass->open_failed = on_open_failed;

    klass->open = nullptr;
    klass->close = nullptr;
    klass->cancel_open = nullptr;
    klass->open_is_needed = nullptr;
    klass->create_gfile = nullptr;
    klass->create_path = nullptr;
}


static void init (GnomeCmdCon *con)
{
    con->alias = nullptr;
    con->uri = nullptr;
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
    con->open_failed_msg = nullptr;
    con->open_failed_error = nullptr;

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
                gtk_signal_emit (GTK_OBJECT (con), signals[OPEN_FAILED]);
            }
            return FALSE;

        default:
            return FALSE;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
}


static void set_con_base_path(GnomeCmdCon *con, GnomeCmdPath *path)
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

    set_con_base_path(con, new GnomeCmdPlainPath(pathString));

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

    auto gFile = gnome_cmd_con_create_gfile (con, con->base_path);
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

    if (gnome_cmd_con_is_closeable (con) && gnome_cmd_con_is_open(con))
    {
        gtk_signal_emit (GTK_OBJECT (con), signals[CLOSE]);
        gtk_signal_emit (GTK_OBJECT (con), signals[UPDATED]);
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
    if (!con)
        return;
    g_return_if_fail (GNOME_CMD_IS_CON (con));

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
        if (!gFile)
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

    if (!uri_str)
    {
        uri_str = GNOME_CMD_FILE (dir)->get_uri_str();
    }

    if (!uri_str)
    {
        return;
    }

    if (!con->priv->all_dirs_map)
        con->priv->all_dirs_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, nullptr);

    DEBUG ('k', "ADDING %p %s to the cache\n", dir, uri_str);
    g_hash_table_insert (con->priv->all_dirs_map, uri_str, dir);
}


void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gchar *uri_str = GNOME_CMD_FILE (dir)->get_uri_str();

    DEBUG ('k', "REMOVING %p %s from the cache\n", dir, uri_str);
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
        DEBUG ('k', "FOUND %p %s in the hash-table, reusing it!\n", dir, uri_str);
    else
        DEBUG ('k', "FAILED to find %s in the hash-table\n", uri_str);

    return dir;
}


const gchar *gnome_cmd_con_get_icon_name (ConnectionMethodID method)
{
    return icon_name[method];
}


string &__gnome_cmd_con_make_uri (string &s, const gchar *method, string &server, string &port, string &folder, string &user, string &password)
{
    user = stringify (g_strescape (user.c_str(), nullptr));
    password = stringify (g_strescape (password.c_str(), nullptr));

    if (!password.empty())
    {
        user += ':';
        user += password;
    }

    folder = stringify (g_uri_escape_string (folder.c_str(), nullptr, true));

    s = method;

    if (!user.empty())
        s += user + '@';

    s += server;

    if (!port.empty())
        s += ':' + port;

    if (!folder.empty())
    {
        if (folder[0] != '/')
        {
            s += '/';
        }
        s += folder;
    }

    return s;
}

#ifdef HAVE_SAMBA
std::string &gnome_cmd_con_make_smb_uri (std::string &uriString, std::string &server, std::string &share, std::string &folder, std::string &domain, std::string &user, std::string &password)
{
    user = stringify (g_strescape (user.c_str(), nullptr));
    password = stringify (g_strescape (password.c_str(), nullptr));

    if (!password.empty())
    {
        user += ':';
        user += password;
    }

    if (!domain.empty())
        user = domain + ';' + user;

    const gchar *joinSign = !folder.empty() && folder[0] != '/' ? "/" : "";

    folder = share + joinSign + folder;
    // remove initial '/' character
    if (folder.length() > 0 && folder[0] == '/')
    {
        folder = folder.length() == 1 ? folder.erase() : folder.substr(1);
    }

    uriString = "smb://";

    if (!user.empty())
        uriString += user + '@';

    uriString += server;
    uriString += "/";
    uriString += folder;

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