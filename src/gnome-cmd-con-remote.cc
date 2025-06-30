/**
 * @file gnome-cmd-con-remote.cc
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
#include "gnome-cmd-con-remote.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-path.h"
#include "imageloader.h"
#include "utils.h"

using namespace std;

G_DEFINE_TYPE (GnomeCmdConRemote, gnome_cmd_con_remote, GNOME_CMD_TYPE_CON)


static void set_con_mount_failed(GnomeCmdCon *con)
{
    g_return_if_fail(GNOME_CMD_IS_CON(con));
    gnome_cmd_con_set_base_file_info(con, nullptr);
    con->open_result = GnomeCmdCon::OPEN_FAILED;
    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_failed_msg = con->open_failed_error->message;
}

static void mount_remote_finish_callback(GObject *gobj, GAsyncResult *result, gpointer user_data)
{
    auto con = GNOME_CMD_CON(user_data);
    g_return_if_fail(GNOME_CMD_IS_CON(user_data));
    g_return_if_fail(gobj != nullptr);
    auto gFile = G_FILE(gobj);
    g_return_if_fail(G_IS_FILE(gFile));

    GError *error = nullptr;

    g_file_mount_enclosing_volume_finish(gFile, result, &error);
    if (error && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED))
    {
        DEBUG('m', "Unable to mount enclosing volume: %s\n", error->message);
        con->open_failed_error = g_error_copy(error);
        g_error_free(error);
        set_con_mount_failed(con);
        g_object_unref(gFile);
        return;
    }

    auto base_gFile = gnome_cmd_con_create_gfile (con, nullptr);
    g_clear_error (&error);
    auto base_gFileInfo = g_file_query_info(base_gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    g_object_unref(base_gFile);
    gnome_cmd_con_set_base_file_info (con, base_gFileInfo);
    if (error)
    {
        g_critical("mount_remote: error: %s", error->message);
        g_error_free(error);
    }

    con->state = GnomeCmdCon::STATE_OPEN;
    con->open_result = GnomeCmdCon::OPEN_OK;
    g_object_unref(gFile);
}


static void remote_open (GnomeCmdCon *con, GtkWindow *parent_window)
{
    DEBUG('m', "Opening remote connection\n");

    GUri *uri = gnome_cmd_con_get_uri (con);
    g_return_if_fail (uri != nullptr);

    con->state = GnomeCmdCon::STATE_OPENING;
    con->open_result = GnomeCmdCon::OPEN_IN_PROGRESS;

    if (gnome_cmd_con_get_base_path (con) == nullptr)
        gnome_cmd_con_set_base_path (con, gnome_cmd_plain_path_new (G_DIR_SEPARATOR_S));

    auto gFile = gnome_cmd_con_create_gfile(con);

    auto uri_string = g_file_get_uri (gFile);
    DEBUG('m', "Connecting to %s\n", uri_string);
    g_free (uri_string);

    auto gMountOperation = gtk_mount_operation_new (parent_window);

    g_file_mount_enclosing_volume (gFile,
                                   G_MOUNT_MOUNT_NONE,
                                   gMountOperation,
                                   nullptr,
                                   mount_remote_finish_callback,
                                   con);
}


struct UnmountClosure
{
    GnomeCmdCon *con;
    GtkWindow *parent_window;
};


static void remote_close_callback(GObject *gobj, GAsyncResult *result, gpointer user_data)
{
    auto gMount = (GMount*) gobj;
    auto closure = (UnmountClosure *) user_data;
    auto con = closure->con;
    auto parent_window = closure->parent_window;
    GError *error = nullptr;

    g_free (user_data);

    g_mount_unmount_with_operation_finish(gMount, result, &error);

    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CLOSED))
    {
        if (parent_window)
            gnome_cmd_error_message (parent_window, _("Disconnect error"), error);
        else
            g_warning ("%s: %s", _("Disconnect error"), error->message);
        return;
    }
    if (error)
        g_error_free(error);

    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_result = GnomeCmdCon::OPEN_NOT_STARTED;
}

static void remote_close (GnomeCmdCon *con, GtkWindow *parent_window)
{
    GError *error = nullptr;

    gnome_cmd_con_set_default_dir (con, nullptr);
    gnome_cmd_con_set_base_path (con, nullptr);

    auto uri = gnome_cmd_con_get_uri_string (con);
    auto gFileTmp = g_file_new_for_uri(uri);
    DEBUG ('m', "Closing connection to %s\n", uri);

    auto gMount = g_file_find_enclosing_mount (gFileTmp, nullptr, &error);
    if (error)
    {
        g_warning("remote_close - g_file_find_enclosing_mount error: %s - %s", uri, error->message);
        g_error_free(error);
        g_object_unref(gFileTmp);
        g_free(uri);
        return;
    }
    g_free(uri);

    UnmountClosure *closure = g_new0 (UnmountClosure, 1);
    closure->con = con;
    closure->parent_window = parent_window;

    g_mount_unmount_with_operation (
        gMount,
        G_MOUNT_UNMOUNT_NONE,
        nullptr,
        nullptr,
        remote_close_callback,
        closure
    );

    g_object_unref(gFileTmp);
}


static void remote_cancel_open (GnomeCmdCon *con)
{
    DEBUG('m', "Setting state CANCELLING\n");
    con->state = GnomeCmdCon::STATE_CANCELLING;
}


static gboolean remote_open_is_needed (GnomeCmdCon *con)
{
    return TRUE;
}


static GFile *create_remote_gfile_with_path(GnomeCmdCon *con, const gchar *path)
{
    auto uri = gnome_cmd_con_get_uri (con);
    auto gUri = g_uri_build(G_URI_FLAGS_NONE,
        g_uri_get_scheme (uri),
        g_uri_get_userinfo (uri),
        g_uri_get_host (uri),
        g_uri_get_port (uri),
        path,
        nullptr,
        nullptr);
    auto gFilePathUriString = g_uri_to_string(gUri);
    auto gFile = g_file_new_for_uri(gFilePathUriString);
    g_free(gFilePathUriString);
    return gFile;
}


static GFile *remote_create_gfile (GnomeCmdCon *con, const gchar *path)
{
    gchar *uri = gnome_cmd_con_get_uri_string (con);
    g_return_val_if_fail (uri != nullptr, nullptr);

    if (path)
    {
        g_free (uri);
        return create_remote_gfile_with_path(con, path);
    }
    else
    {
        auto gFile = g_file_new_for_uri (uri);
        g_free (uri);
        return gFile;
    }
}


static GnomeCmdPath *remote_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return gnome_cmd_plain_path_new (path_str);
}


static gchar *remote_get_open_tooltip (GnomeCmdCon *con)
{
    GUri *uri = gnome_cmd_con_get_uri (con);
    const gchar *hostname = uri ? g_uri_get_host (uri) : nullptr;
    return hostname
        ? g_strdup_printf (_("Opens remote connection to %s"), hostname)
        : nullptr;
}


static gchar *remote_get_close_tooltip (GnomeCmdCon *con)
{
    GUri *uri = gnome_cmd_con_get_uri (con);
    const gchar *hostname = uri ? g_uri_get_host (uri) : nullptr;
    return hostname
        ? g_strdup_printf (_("Closes remote connection to %s"), hostname)
        : nullptr;
}


extern "C" gchar *gnome_cmd_con_remote_get_icon_name(GnomeCmdConRemote *con);

static GIcon *remote_get_icon (GnomeCmdCon *con)
{
    gchar *icon_name = gnome_cmd_con_remote_get_icon_name (GNOME_CMD_CON_REMOTE (con));
    GIcon *icon = g_themed_icon_new (icon_name);
    g_free (icon_name);
    return icon;
}


static GIcon *remote_get_close_icon (GnomeCmdCon *con)
{
    GIcon *icon = remote_get_icon (con);
    if (!icon)
        return nullptr;

    GIcon *unmount = g_themed_icon_new (OVERLAY_UMOUNT_ICON);
    GEmblem *emblem = g_emblem_new (unmount);

    return g_emblemed_icon_new (icon, emblem);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_con_remote_class_init (GnomeCmdConRemoteClass *klass)
{
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    con_class->open = remote_open;
    con_class->close = remote_close;
    con_class->cancel_open = remote_cancel_open;
    con_class->open_is_needed = remote_open_is_needed;
    con_class->create_gfile = remote_create_gfile;
    con_class->create_path = remote_create_path;

    con_class->get_open_tooltip = remote_get_open_tooltip;
    con_class->get_close_tooltip = remote_get_close_tooltip;

    con_class->get_go_icon = remote_get_icon;
    con_class->get_open_icon = remote_get_icon;
    con_class->get_close_icon = remote_get_close_icon;
}


static void gnome_cmd_con_remote_init (GnomeCmdConRemote *remote_con)
{
    GnomeCmdCon *con = GNOME_CMD_CON (remote_con);

    con->should_remember_dir = TRUE;
    con->needs_open_visprog = TRUE;
    con->needs_list_visprog = TRUE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = TRUE;
}
