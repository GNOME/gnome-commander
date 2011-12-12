/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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

#ifndef __GNOME_CMD_CON_H__
#define __GNOME_CMD_CON_H__

#define GNOME_CMD_TYPE_CON              (gnome_cmd_con_get_type ())
#define GNOME_CMD_CON(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON, GnomeCmdCon))
#define GNOME_CMD_CON_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON, GnomeCmdConClass))
#define GNOME_CMD_IS_CON(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON))
#define GNOME_CMD_IS_CON_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON))
#define GNOME_CMD_CON_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON, GnomeCmdConClass))

struct GnomeCmdConPrivate;

#include <gnome-keyring.h>

#include <string>

#include "gnome-cmd-path.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-pixmap.h"
#include "gnome-cmd-data.h"
#include "history.h"
#include "utils.h"

enum ConnectionMethodID        // Keep this order in sync with strings in gnome-cmd-con-dialog.cc and gnome-cmd-con.cc
{
    CON_SSH,
    CON_SFTP = CON_SSH,
    CON_FTP,
    CON_ANON_FTP,
    CON_SMB,
    CON_DAV,
    CON_DAVS,
    CON_URI,
    CON_LOCAL      // CON_FILE ???
};

struct GnomeCmdCon
{
    GtkObject parent;

    enum State
    {
        STATE_CLOSED,
        STATE_OPEN,
        STATE_OPENING,
        STATE_CANCELLING
    };

    enum OpenResult
    {
        OPEN_OK,
        OPEN_FAILED,
        OPEN_CANCELLED,
        OPEN_IN_PROGRESS,
        OPEN_NOT_STARTED
    };

    enum Authentication
    {
        SAVE_NEVER,
        SAVE_FOR_SESSION,
        SAVE_PERMANENTLY,
        NOT_REQUIRED
    };

    gchar               *alias;                 // coded as UTF-8
    gchar               *uri;
    ConnectionMethodID  method;
    Authentication      auth;

    gchar               *open_msg;
    GnomeCmdPath        *base_path;
    GnomeVFSFileInfo    *base_info;
    GString             *root_path;             // root path of the connection, used for calculation of relative paths
    gboolean            should_remember_dir;
    gboolean            needs_open_visprog;
    gboolean            needs_list_visprog;
    gboolean            can_show_free_space;
    State               state;
    gboolean            is_local;
    gboolean            is_closeable;
    gchar               *go_text;
    gchar               *go_tooltip;
    GnomeCmdPixmap      *go_pixmap;
    gchar               *open_text;
    gchar               *open_tooltip;
    GnomeCmdPixmap      *open_pixmap;
    gchar               *close_text;
    gchar               *close_tooltip;
    GnomeCmdPixmap      *close_pixmap;

    OpenResult          open_result;
    GnomeVFSResult      open_failed_reason;
    gchar               *open_failed_msg;

    GnomeCmdConPrivate  *priv;

    GnomeKeyringAttributeList *create_keyring_attributes();

    friend XML::xstream &operator << (XML::xstream &xml, GnomeCmdCon &con);
};

struct GnomeCmdConClass
{
    GtkObjectClass parent_class;

    /* signals */
    void (* updated) (GnomeCmdCon *con);
    void (* open_done) (GnomeCmdCon *con);
    void (* open_failed) (GnomeCmdCon *con, const gchar *msg, GnomeVFSResult result);

    /* virtual functions */
    void (* open) (GnomeCmdCon *con);
    void (* cancel_open) (GnomeCmdCon *con);
    gboolean (* close) (GnomeCmdCon *con);
    gboolean (* open_is_needed) (GnomeCmdCon *con);
    GnomeVFSURI *(* create_uri) (GnomeCmdCon *con, GnomeCmdPath *path);
    GnomeCmdPath *(* create_path) (GnomeCmdCon *con, const gchar *path_str);
};


GtkType gnome_cmd_con_get_type ();


void gnome_cmd_con_open (GnomeCmdCon *con);

inline gboolean gnome_cmd_con_is_open (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->state == GnomeCmdCon::STATE_OPEN;
}

void gnome_cmd_con_cancel_open (GnomeCmdCon *con);

gboolean gnome_cmd_con_close (GnomeCmdCon *con);

inline gboolean gnome_cmd_con_open_is_needed (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    GnomeCmdConClass *klass = GNOME_CMD_CON_GET_CLASS (con);
    return klass->open_is_needed (con);
}

inline const gchar *gnome_cmd_con_get_uri (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->uri;
}

inline void gnome_cmd_con_set_uri (GnomeCmdCon *con, const gchar *uri=NULL)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->uri);
    con->uri = g_strdup (uri);
}

inline void gnome_cmd_con_set_uri (GnomeCmdCon *con, const std::string &uri)
{
    gnome_cmd_con_set_uri (con, uri.empty() ? NULL : uri.c_str());
}

GnomeVFSURI *gnome_cmd_con_create_uri (GnomeCmdCon *con, GnomeCmdPath *path);

GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str);

inline const gchar *gnome_cmd_con_get_open_msg (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_msg;
}

inline const gchar *gnome_cmd_con_get_alias (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->alias;
}

inline void gnome_cmd_con_set_alias (GnomeCmdCon *con, const gchar *alias=NULL)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->alias);
    con->alias = g_strdup (alias);

    if (!alias)
        alias = _("<New connection>");

    con->go_text = g_strdup_printf (_("Go to: %s"), alias);
    con->open_text = g_strdup_printf (_("Connect to: %s"), alias);
    con->close_text = g_strdup_printf (_("Disconnect from: %s"), alias);
}

inline void gnome_cmd_con_set_host_name (GnomeCmdCon *con, const gchar *host)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->open_msg);
    con->open_msg = g_strdup_printf (_("Connecting to %s\n"), host ? host : "<?>");
}

inline void gnome_cmd_con_set_host_name (GnomeCmdCon *con, const std::string &host)
{
    gnome_cmd_con_set_host_name (con, host.empty() ? NULL : host.c_str());
}

GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con);
void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir);

inline gchar *gnome_cmd_con_get_root_path (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->root_path->str;
}

inline void gnome_cmd_con_set_root_path (GnomeCmdCon *con, const gchar *path=NULL)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_string_assign (con->root_path, path);
}

inline gboolean gnome_cmd_con_should_remember_dir (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->should_remember_dir;
}

inline gboolean gnome_cmd_con_needs_open_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->needs_open_visprog;
}

inline gboolean gnome_cmd_con_needs_list_visprog (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->needs_list_visprog;
}

inline gboolean gnome_cmd_con_can_show_free_space (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->can_show_free_space;
}

inline gboolean gnome_cmd_con_is_local (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->is_local;
}

inline gboolean gnome_cmd_con_is_closeable (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), FALSE);
    return con->is_closeable;
}

History *gnome_cmd_con_get_dir_history (GnomeCmdCon *con);

inline const gchar *gnome_cmd_con_get_go_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->go_text;
}

inline const gchar *gnome_cmd_con_get_open_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_text;
}

inline const gchar *gnome_cmd_con_get_close_text (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);

    return con->close_text;
}

inline const gchar *gnome_cmd_con_get_go_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->go_tooltip;
}

inline const gchar *gnome_cmd_con_get_open_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_tooltip;
}

inline const gchar *gnome_cmd_con_get_close_tooltip (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->close_tooltip;
}

inline GnomeCmdPixmap *gnome_cmd_con_get_go_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->go_pixmap;
}

inline GnomeCmdPixmap *gnome_cmd_con_get_open_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_pixmap;
}

inline GnomeCmdPixmap *gnome_cmd_con_get_close_pixmap (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->close_pixmap;
}

GnomeCmdBookmarkGroup *gnome_cmd_con_get_bookmarks (GnomeCmdCon *con);

void gnome_cmd_con_set_bookmarks (GnomeCmdCon *con, GnomeCmdBookmarkGroup *bookmarks);

inline void gnome_cmd_con_add_bookmark (GnomeCmdCon *con, gchar *name, gchar *path)
{
    GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);
    GnomeCmdBookmark *bookmark = g_new (GnomeCmdBookmark, 1);
    bookmark->name = name;
    bookmark->path = path;
    bookmark->group = group;
    group->bookmarks = g_list_append (group->bookmarks, bookmark);
}

void gnome_cmd_con_updated (GnomeCmdCon *con);

GnomeVFSResult gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path, GnomeVFSFileType *type);

GnomeVFSResult gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str);

void gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir);

void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, GnomeCmdDir *dir);

void gnome_cmd_con_remove_from_cache (GnomeCmdCon *con, const gchar *uri);

GnomeCmdDir *gnome_cmd_con_cache_lookup (GnomeCmdCon *con, const gchar *uri);

const gchar *gnome_cmd_con_get_icon_name (ConnectionMethodID method);

inline const gchar *gnome_cmd_con_get_icon_name (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return gnome_cmd_con_get_icon_name (con->method);
}

inline gchar *gnome_cmd_con_get_free_space (GnomeCmdCon *con, GnomeCmdDir *dir, const gchar *fmt)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    g_return_val_if_fail (GNOME_CMD_IS_DIR (dir), NULL);

    if (!gnome_cmd_con_can_show_free_space (con))
        return NULL;

    gchar *free_space = gnome_cmd_dir_get_free_space (dir);

    if (!free_space)
        return _("Unknown disk usage");

    gchar *retval = g_strdup_printf (fmt, free_space);
    g_free (free_space);

    return retval;
}

inline ConnectionMethodID gnome_cmd_con_get_scheme (GnomeVFSURI *uri)
{
    const gchar *scheme = gnome_vfs_uri_get_scheme (uri);       // do not g_free
    const gchar *user = gnome_vfs_uri_get_user_name (uri);      // do not g_free

    return gnome_vfs_uri_is_local (uri) ? CON_LOCAL :
           g_str_equal (scheme, "ftp")  ? (user && g_str_equal (user, "anonymous") ? CON_ANON_FTP : CON_FTP) :
           g_str_equal (scheme, "sftp") ? CON_SSH :
           g_str_equal (scheme, "dav")  ? CON_DAV :
           g_str_equal (scheme, "davs") ? CON_DAVS :
           g_str_equal (scheme, "smb")  ? CON_SMB :
                                          CON_URI;
}

std::string &__gnome_cmd_con_make_uri (std::string &s, const gchar *method, gboolean use_auth, std::string &server, std::string &port, std::string &folder, std::string &user, std::string &password);

inline std::string &gnome_cmd_con_make_custom_uri (std::string &s, const std::string &uri)
{
    stringify (s, gnome_vfs_make_uri_from_input (uri.c_str()));

    if (!uri_is_valid (s))
        s.erase();

    return s;
}

inline std::string &gnome_cmd_con_make_ssh_uri (std::string &s, gboolean use_auth, std::string &server, std::string &port, std::string &folder, std::string &user, std::string &password)
{
    return __gnome_cmd_con_make_uri (s, "sftp://", use_auth, server, port, folder, user, password);
}

inline std::string &gnome_cmd_con_make_ftp_uri (std::string &s, gboolean use_auth, std::string &server, std::string &port, std::string &folder, std::string &user, std::string &password)
{
    if (user=="anonymous")
    {
        use_auth = FALSE;
        password = gnome_cmd_data_get_ftp_anonymous_password ();
    }

    return __gnome_cmd_con_make_uri (s, "ftp://", use_auth, server, port, folder, user, password);
}

inline std::string &gnome_cmd_con_make_smb_uri (std::string &s, gboolean use_auth, std::string &server, std::string &share, std::string &folder, std::string &domain, std::string &user, std::string &password)
{
    share = '/' + share;

    user = stringify (gnome_vfs_escape_string (user.c_str()));
    password = stringify (gnome_vfs_escape_string (password.c_str()));

    if (!password.empty() && !use_auth)
    {
        user += ':';
        user += password;
    }

    if (!domain.empty())
        user = domain + ';' + user;

    const gchar *join = !folder.empty() && folder[0] != '/' ? "/" : "";

    folder = share + join + folder;
    folder = stringify (gnome_vfs_escape_path_string (folder.c_str()));

    s = "smb://";

    if (!user.empty())
        s += user + '@';

    s += server;
    s += folder;

    return s;
}

inline std::string &gnome_cmd_con_make_dav_uri (std::string &s, gboolean use_auth, std::string &server, std::string &port, std::string &folder, std::string &user, std::string &password)
{
    return __gnome_cmd_con_make_uri (s, "dav://", use_auth, server, port, folder, user, password);
}

inline std::string &gnome_cmd_con_make_davs_uri (std::string &s, gboolean use_auth, std::string &server, std::string &port, std::string &folder, std::string &user, std::string &password)
{
    return __gnome_cmd_con_make_uri (s, "davs://", use_auth, server, port, folder, user, password);
}

inline std::string &gnome_cmd_con_make_uri (std::string &s, ConnectionMethodID method, gboolean use_auth, std::string &uri, std::string &server, std::string &share, std::string &port, std::string &folder, std::string &domain, std::string &user, std::string &password)
{
    switch (method)
    {
        case CON_FTP:
        case CON_ANON_FTP:  return gnome_cmd_con_make_ftp_uri (s, use_auth, server, port, folder, user, password);

        case CON_SSH:       return gnome_cmd_con_make_ssh_uri (s, use_auth, server, port, folder, user, password);

        case CON_SMB:       return gnome_cmd_con_make_smb_uri (s, use_auth, server, share, folder, domain, user, password);

        case CON_DAV:       return gnome_cmd_con_make_dav_uri (s, use_auth, server, port, folder, user, password);

        case CON_DAVS:      return gnome_cmd_con_make_davs_uri (s, use_auth, server, port, folder, user, password);

        case CON_URI:       return gnome_cmd_con_make_custom_uri (s, uri);

        default:            return s;
    }
}

GnomeKeyringAttributeList *gnome_cmd_con_create_keyring_attributes (const gchar *uri_str, const gchar *alias, ConnectionMethodID method);

inline GnomeKeyringAttributeList *GnomeCmdCon::create_keyring_attributes()
{
    return gnome_cmd_con_create_keyring_attributes (uri, alias, method);
}

#endif // __GNOME_CMD_CON_H__
