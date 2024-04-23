/**
 * @file gnome-cmd-con.h
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

#pragma once

#define GNOME_CMD_TYPE_CON              (gnome_cmd_con_get_type ())
#define GNOME_CMD_CON(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_CON, GnomeCmdCon))
#define GNOME_CMD_CON_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_CON, GnomeCmdConClass))
#define GNOME_CMD_IS_CON(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_CON))
#define GNOME_CMD_IS_CON_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_CON))
#define GNOME_CMD_CON_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_CON, GnomeCmdConClass))

struct GnomeCmdConPrivate;

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
#ifdef HAVE_SAMBA
    CON_SMB,
#endif
    CON_DAV,
    CON_DAVS,
    CON_URI,
    CON_FILE
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

    gchar               *alias;                 // coded as UTF-8
    gchar               *uri;
    gchar               *scheme;
    ConnectionMethodID  method;

    gchar               *username;
    gchar               *hostname;
    gint16              port{-1};
    gchar               *open_msg;
    GnomeCmdPath        *base_path;
    GFileInfo           *base_gFileInfo;
    GString             *root_path;             // Root path of the connection, used for calculation of relative paths
    gboolean            should_remember_dir;
    gboolean            needs_open_visprog;
    gboolean            needs_list_visprog;     // Defines if a graphical progress bar should be drawn when opening a folder
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
    GError              *open_failed_error;
    gchar               *open_failed_msg;

    GnomeCmdConPrivate  *priv;
};

struct GnomeCmdConClass
{
    GtkObjectClass parent_class;

    /* signals */
    void (* updated) (GnomeCmdCon *con);
    void (* open_done) (GnomeCmdCon *con);
    void (* open_failed) (GnomeCmdCon *con);

    /* virtual functions */
    void (* open) (GnomeCmdCon *con);
    void (* cancel_open) (GnomeCmdCon *con);
    gboolean (* close) (GnomeCmdCon *con);
    gboolean (* open_is_needed) (GnomeCmdCon *con);
    GFile *(* create_gfile) (GnomeCmdCon *con, GnomeCmdPath *path);
    GnomeCmdPath *(* create_path) (GnomeCmdCon *con, const gchar *path_str);
};


GtkType gnome_cmd_con_get_type ();

void gnome_cmd_con_set_base_path(GnomeCmdCon *con, GnomeCmdPath *path);
void set_con_base_path_for_gmount(GnomeCmdCon *con, GMount *gMount);
gboolean set_con_base_gfileinfo(GnomeCmdCon *con);

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

inline void gnome_cmd_con_set_uri (GnomeCmdCon *con, const gchar *uri = nullptr)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->uri);
    con->uri = g_strdup(uri);
}

inline void gnome_cmd_con_set_uri (GnomeCmdCon *con, const std::string &uri)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->uri);
    con->uri = uri.empty() ? NULL : g_strdup (uri.c_str());
}

inline const gchar *gnome_cmd_con_get_scheme (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->scheme;
}

inline void gnome_cmd_con_set_scheme (GnomeCmdCon *con, const gchar *scheme = nullptr)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->scheme);
    con->scheme = g_strdup(scheme);
}

inline void gnome_cmd_con_set_scheme (GnomeCmdCon *con, const std::string &scheme)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->scheme);
    con->scheme = scheme.empty() ? NULL : g_strdup (scheme.c_str());
}

GFile *gnome_cmd_con_create_gfile (GnomeCmdCon *con, GnomeCmdPath *path = nullptr);

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

inline const char *gnome_cmd_con_get_user_name (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    return con->username;
}

inline void gnome_cmd_con_set_user_name (GnomeCmdCon *con, const gchar *username)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->username);
    con->username = g_strdup(username);
}

inline const char *gnome_cmd_con_get_host_name (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), nullptr);
    return con->hostname;
}

inline void gnome_cmd_con_set_host_name (GnomeCmdCon *con, const gchar *host)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    g_free (con->open_msg);
    g_free (con->hostname);
    con->hostname = g_strdup(host);
    con->open_msg = g_strdup_printf (_("Connecting to %s\n"), host ? host : "<?>");
}

inline void gnome_cmd_con_set_port (GnomeCmdCon *con, gint16 port)
{
    g_return_if_fail (GNOME_CMD_IS_CON (con));
    con->port = port;
}

inline gint16 gnome_cmd_con_get_port (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), 0);
    return con->port;
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

void gnome_cmd_con_add_bookmark (GnomeCmdCon *con, gchar *name, gchar *path);

void gnome_cmd_con_erase_bookmark (GnomeCmdCon *con);

void gnome_cmd_con_updated (GnomeCmdCon *con);

gboolean gnome_cmd_con_get_path_target_type (GnomeCmdCon *con, const gchar *path, GFileType *type);

gboolean gnome_cmd_con_mkdir (GnomeCmdCon *con, const gchar *path_str, GError *error);

void gnome_cmd_con_add_to_cache (GnomeCmdCon *con, GnomeCmdDir *dir, gchar *uri_str = nullptr);

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    gchar *retval = g_strdup_printf (fmt, free_space);
#pragma GCC diagnostic pop
    g_free (free_space);

    return retval;
}

inline ConnectionMethodID gnome_cmd_con_get_scheme (const gchar *uriString)
{
    gchar *scheme = nullptr;
    gchar *user = nullptr;
    GError *error = nullptr;

    g_uri_split_with_user (
        uriString,
        G_URI_FLAGS_HAS_PASSWORD,
        &scheme, //scheme
        &user, //user
        nullptr, //password
        nullptr, //auth_params
        nullptr, //host
        nullptr, //port
        nullptr, //path
        nullptr, //query
        nullptr, //fragment
        &error
    );

    if(error)
    {
        g_warning("gnome_cmd_con_get_scheme - g_uri_split_with_user error: %s", error->message);
        g_error_free(error);
        return CON_SSH;
    }

    ConnectionMethodID retValue = scheme == nullptr ? CON_FILE :
           g_str_equal (scheme, "file") ? CON_FILE :
           g_str_equal (scheme, "ftp")  ? (user && g_str_equal (user, "anonymous") ? CON_ANON_FTP : CON_FTP) :
           g_str_equal (scheme, "ftp")  ? CON_FTP :
           g_str_equal (scheme, "sftp") ? CON_SSH :
           g_str_equal (scheme, "dav")  ? CON_DAV :
           g_str_equal (scheme, "davs") ? CON_DAVS :
#ifdef HAVE_SAMBA
           g_str_equal (scheme, "smb")  ? CON_SMB :
#endif
                                          CON_URI;

    g_free(user);
    g_free(scheme);
    return retValue;
}

std::string &__gnome_cmd_con_make_uri (std::string &s, const gchar *method, std::string &server, std::string &port, std::string &folder);

inline std::string &gnome_cmd_con_make_custom_uri (std::string &s, const std::string &uri)
{
    GError *error = nullptr;
    auto gUri = g_uri_parse (uri.c_str(), G_URI_FLAGS_NONE, &error);
    if (error != nullptr)
    {
        g_warning ("g_uri_parse error of \"%s\": %s", uri.c_str(), error->message);
        g_error_free(error);
        return s;
    }
    stringify (s, g_uri_to_string(gUri));

    if (!uri_is_valid (s))
        s.erase();
    return s;
}

inline std::string &gnome_cmd_con_make_ssh_uri (std::string &s, std::string &server, std::string &port, std::string &folder)
{
    return __gnome_cmd_con_make_uri (s, "sftp://", server, port, folder);
}

inline std::string &gnome_cmd_con_make_ftp_uri (std::string &s, std::string &server, std::string &port, std::string &folder)
{
    return __gnome_cmd_con_make_uri (s, "ftp://", server, port, folder);
}

#ifdef HAVE_SAMBA
std::string &gnome_cmd_con_make_smb_uri (std::string &s, std::string &server, std::string &folder, std::string &domain);
#endif

inline std::string &gnome_cmd_con_make_dav_uri (std::string &s, std::string &server, std::string &port, std::string &folder)
{
    return __gnome_cmd_con_make_uri (s, "dav://", server, port, folder);
}

inline std::string &gnome_cmd_con_make_davs_uri (std::string &s, std::string &server, std::string &port, std::string &folder)
{
    return __gnome_cmd_con_make_uri (s, "davs://", server, port, folder);
}

inline std::string &gnome_cmd_con_make_uri (std::string &s, ConnectionMethodID method, std::string &uri, std::string &server, std::string &port, std::string &folder, std::string &domain)
{
    switch (method)
    {
        case CON_FTP:
        case CON_ANON_FTP:  return gnome_cmd_con_make_ftp_uri (s, server, port, folder);

        case CON_SSH:       return gnome_cmd_con_make_ssh_uri (s, server, port, folder);

#ifdef HAVE_SAMBA
        case CON_SMB:       return gnome_cmd_con_make_smb_uri (s, server, folder, domain);
#endif

        case CON_DAV:       return gnome_cmd_con_make_dav_uri (s, server, port, folder);

        case CON_DAVS:      return gnome_cmd_con_make_davs_uri (s, server, port, folder);

        case CON_URI:       return gnome_cmd_con_make_custom_uri (s, uri);

        case CON_FILE:
        default:            return s;
    }
}

void gnome_cmd_con_close_active_or_inactive_connection (GMount *gMount);
