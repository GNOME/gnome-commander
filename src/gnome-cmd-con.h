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

#include <string>

#include "gnome-cmd-path.h"
#include "gnome-cmd-dir.h"
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
    CON_FILE
};

struct GnomeCmdCon
{
    GObject parent;

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
    ConnectionMethodID  method;

    gchar               *open_msg;
    gboolean            should_remember_dir;
    gboolean            needs_open_visprog;
    gboolean            needs_list_visprog;     // Defines if a graphical progress bar should be drawn when opening a folder
    gboolean            can_show_free_space;
    State               state;
    gboolean            is_local;
    gboolean            is_closeable;

    OpenResult          open_result;
    GError              *open_failed_error;
    gchar               *open_failed_msg;
};

struct GnomeCmdConClass
{
    GObjectClass parent_class;

    /* signals */
    void (* updated) (GnomeCmdCon *con);
    void (* open_done) (GnomeCmdCon *con);
    void (* open_failed) (GnomeCmdCon *con);

    /* virtual functions */
    void (* open) (GnomeCmdCon *con, GtkWindow *parent_window);
    void (* cancel_open) (GnomeCmdCon *con);
    gboolean (* close) (GnomeCmdCon *con, GtkWindow *parent_window);
    gboolean (* open_is_needed) (GnomeCmdCon *con);
    GFile *(* create_gfile) (GnomeCmdCon *con, const gchar *path_str);
    GnomeCmdPath *(* create_path) (GnomeCmdCon *con, const gchar *path_str);

    gchar *(* get_go_text) (GnomeCmdCon *con);
    gchar *(* get_open_text) (GnomeCmdCon *con);
    gchar *(* get_close_text) (GnomeCmdCon *con);
    gchar *(* get_go_tooltip) (GnomeCmdCon *con);
    gchar *(* get_open_tooltip) (GnomeCmdCon *con);
    gchar *(* get_close_tooltip) (GnomeCmdCon *con);
    GIcon *(* get_go_icon) (GnomeCmdCon *con);
    GIcon *(* get_open_icon) (GnomeCmdCon *con);
    GIcon *(* get_close_icon) (GnomeCmdCon *con);
};


extern "C" GType gnome_cmd_con_get_type ();

const gchar *gnome_cmd_con_get_uuid (GnomeCmdCon *con);

extern "C" GnomeCmdPath *gnome_cmd_con_get_base_path(GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_base_path(GnomeCmdCon *con, GnomeCmdPath *path);

extern "C" GFileInfo *gnome_cmd_con_get_base_file_info(GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_base_file_info(GnomeCmdCon *con, GFileInfo *file_info);

void gnome_cmd_con_open (GnomeCmdCon *con, GtkWindow *parent_window);

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

extern "C" GUri *gnome_cmd_con_get_uri (GnomeCmdCon *con);
extern "C" gchar *gnome_cmd_con_get_uri_string (GnomeCmdCon *con);

extern "C" void gnome_cmd_con_set_uri (GnomeCmdCon *con, GUri *uri);
extern "C" void gnome_cmd_con_set_uri_string (GnomeCmdCon *con, const gchar *uri_string);

GFile *gnome_cmd_con_create_gfile (GnomeCmdCon *con, const gchar *path = nullptr);

extern "C" GnomeCmdPath *gnome_cmd_con_create_path (GnomeCmdCon *con, const gchar *path_str);

inline const gchar *gnome_cmd_con_get_open_msg (GnomeCmdCon *con)
{
    g_return_val_if_fail (GNOME_CMD_IS_CON (con), NULL);
    return con->open_msg;
}

extern "C" const gchar *gnome_cmd_con_get_alias (GnomeCmdCon *con);
extern "C" void gnome_cmd_con_set_alias (GnomeCmdCon *con, const gchar *alias=NULL);

GnomeCmdDir *gnome_cmd_con_get_default_dir (GnomeCmdCon *con);
void gnome_cmd_con_set_default_dir (GnomeCmdCon *con, GnomeCmdDir *dir);

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

gchar *gnome_cmd_con_get_go_text (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_open_text (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_close_text (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_go_tooltip (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_open_tooltip (GnomeCmdCon *con);
gchar *gnome_cmd_con_get_close_tooltip (GnomeCmdCon *con);
GIcon *gnome_cmd_con_get_go_icon (GnomeCmdCon *con);
GIcon *gnome_cmd_con_get_open_icon (GnomeCmdCon *con);
GIcon *gnome_cmd_con_get_close_icon (GnomeCmdCon *con);

GnomeCmdBookmarkGroup *gnome_cmd_con_get_bookmarks (GnomeCmdCon *con);

void gnome_cmd_con_add_bookmark (GnomeCmdCon *con, const gchar *name, const gchar *path);

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

std::string &gnome_cmd_con_make_smb_uri (std::string &s, std::string &server, std::string &folder, std::string &domain);

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

        case CON_SMB:       return gnome_cmd_con_make_smb_uri (s, server, folder, domain);

        case CON_DAV:       return gnome_cmd_con_make_dav_uri (s, server, port, folder);

        case CON_DAVS:      return gnome_cmd_con_make_davs_uri (s, server, port, folder);

        case CON_URI:       return gnome_cmd_con_make_custom_uri (s, uri);

        case CON_FILE:
        default:            return s;
    }
}

void gnome_cmd_con_close_active_or_inactive_connection (GMount *gMount);

extern "C" int gnome_cmd_con_get_method (GnomeCmdCon *con);
