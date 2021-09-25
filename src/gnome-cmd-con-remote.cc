/**
 * @file gnome-cmd-con-remote.cc
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
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-remote.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"
#include "utils.h"

using namespace std;


static GnomeCmdConClass *parent_class = nullptr;


static void get_file_info_func (GnomeCmdCon *con)
{
    g_return_if_fail(GNOME_CMD_IS_CON(con));

    GError *error = nullptr;
    auto gFile = gnome_cmd_con_create_gfile(con, con->base_path);

    auto uri = g_file_get_uri(gFile);
    DEBUG('m', "Connecting to %s\n", uri);
    g_free(uri);

    con->base_gFileInfo = g_file_query_info(gFile, "*", G_FILE_QUERY_INFO_NONE, nullptr, &error);
    if (error)
    {
        DEBUG('m', "g_file_query_info error: %s\n", error->message);
    }
    g_object_unref (gFile);

    if (con->state == GnomeCmdCon::STATE_OPENING)
    {
        DEBUG('m', "State was OPENING, setting flags\n");
        if (!error)
        {
            con->state = GnomeCmdCon::STATE_OPEN;
            con->open_result = GnomeCmdCon::OPEN_OK;
        }
        else
        {
            con->state = GnomeCmdCon::STATE_CLOSED;
            con->open_failed_error = error;
            con->open_result = GnomeCmdCon::OPEN_FAILED;
        }
    }
    else
    {
        if (con->state == GnomeCmdCon::STATE_CANCELLING)
            DEBUG('m', "The open operation was cancelled, doing nothing\n");
        else
            DEBUG('m', "Strange ConState %d\n", con->state);
        con->state = GnomeCmdCon::STATE_CLOSED;
    }
}

static gboolean start_get_file_info (GnomeCmdCon *con)
{
    g_thread_new (nullptr, (GThreadFunc) get_file_info_func, con);

    return FALSE;
}


static void remote_open (GnomeCmdCon *con)
{
    DEBUG('m', "Opening remote connection\n");

    g_return_if_fail (con->uri != nullptr);

    con->state = GnomeCmdCon::STATE_OPENING;
    con->open_result = GnomeCmdCon::OPEN_IN_PROGRESS;

    if (!con->base_path)
        con->base_path = new GnomeCmdPlainPath(G_DIR_SEPARATOR_S);

    g_timeout_add (1, (GSourceFunc) start_get_file_info, con);
}


static gboolean remote_close (GnomeCmdCon *con)
{
    gnome_cmd_con_set_default_dir (con, nullptr);
    delete con->base_path;
    con->base_path = nullptr;
    con->state = GnomeCmdCon::STATE_CLOSED;
    con->open_result = GnomeCmdCon::OPEN_NOT_STARTED;

    return TRUE;
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


static GFile *remote_create_gfile (GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_val_if_fail (con->uri != nullptr, nullptr);

    auto *gFileTmp = g_file_new_for_uri (con->uri);
    auto gFile = g_file_resolve_relative_path (gFileTmp, path->get_path());

    g_object_unref (gFileTmp);

    return gFile;
}


static GnomeCmdPath *remote_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return new GnomeCmdPlainPath(path_str);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdConRemote *con_remote = GNOME_CMD_CON_REMOTE (object);

    gnome_cmd_pixmap_free (con_remote->parent.go_pixmap);
    gnome_cmd_pixmap_free (con_remote->parent.open_pixmap);
    gnome_cmd_pixmap_free (con_remote->parent.close_pixmap);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConRemoteClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    parent_class = static_cast<GnomeCmdConClass*> (gtk_type_class (GNOME_CMD_TYPE_CON));

    object_class->destroy = destroy;

    con_class->open = remote_open;
    con_class->close = remote_close;
    con_class->cancel_open = remote_cancel_open;
    con_class->open_is_needed = remote_open_is_needed;
    con_class->create_gfile = remote_create_gfile;
    con_class->create_path = remote_create_path;
}


static void init (GnomeCmdConRemote *remote_con)
{
    guint dev_icon_size = gnome_cmd_data.dev_icon_size;
    gint icon_size;

    g_assert (gtk_icon_size_lookup (GTK_ICON_SIZE_LARGE_TOOLBAR, &icon_size, nullptr));

    GnomeCmdCon *con = GNOME_CMD_CON (remote_con);

    con->method = CON_FTP;
    con->should_remember_dir = TRUE;
    con->needs_open_visprog = TRUE;
    con->needs_list_visprog = TRUE;
    con->can_show_free_space = FALSE;
    con->is_local = FALSE;
    con->is_closeable = TRUE;
    con->go_pixmap = gnome_cmd_pixmap_new_from_icon (gnome_cmd_con_get_icon_name (con), dev_icon_size);
    con->open_pixmap = gnome_cmd_pixmap_new_from_icon (gnome_cmd_con_get_icon_name (con), dev_icon_size);
    con->close_pixmap = gnome_cmd_pixmap_new_from_icon (gnome_cmd_con_get_icon_name (con), icon_size);

    if (con->close_pixmap)
    {
        GdkPixbuf *overlay = gdk_pixbuf_copy (con->close_pixmap->pixbuf);

        if (overlay)
        {
            GdkPixbuf *umount = IMAGE_get_pixbuf (PIXMAP_OVERLAY_UMOUNT);

            if (umount)
            {
                gdk_pixbuf_copy_area (umount, 0, 0,
                                      MIN (gdk_pixbuf_get_width (umount), icon_size),
                                      MIN (gdk_pixbuf_get_height (umount), icon_size),
                                      overlay, 0, 0);
                // FIXME: free con->close_pixmap here
                con->close_pixmap = gnome_cmd_pixmap_new_from_pixbuf (overlay);
            }

            g_object_unref (overlay);
        }
    }
}



/***********************************
 * Public functions
 ***********************************/

GtkType gnome_cmd_con_remote_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            (gchar*) "GnomeCmdConRemote",
            sizeof (GnomeCmdConRemote),
            sizeof (GnomeCmdConRemoteClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        type = gtk_type_unique (GNOME_CMD_TYPE_CON, &info);
    }
    return type;
}

/**
 * Logic for setting up a new remote connection accordingly to the given uri_str.
 */
GnomeCmdConRemote *gnome_cmd_con_remote_new (const gchar *alias, const string &uri_str, GnomeCmdCon::Authentication auth)
{
    auto server = static_cast<GnomeCmdConRemote*> (g_object_new (GNOME_CMD_TYPE_CON_REMOTE, nullptr));

    g_return_val_if_fail (server != nullptr, nullptr);

    GError *error = nullptr;

    gchar *host = nullptr;
    gint port = 0;
    gchar *path = nullptr;

    g_uri_split (
        uri_str.c_str(),
        G_URI_FLAGS_NONE,
        nullptr, //scheme
        nullptr, //userinfo
        &host,
        &port,
        &path,
        nullptr, //query
        nullptr, //fragment
        &error
    );
    if (error)
    {
        g_warning("gnome_cmd_con_remote_new - g_uri_split error: %s", error->message);
        g_error_free(error);
        return nullptr;
    }

    GnomeCmdCon *con = GNOME_CMD_CON (server);

    gnome_cmd_con_set_alias (con, alias);
    gnome_cmd_con_set_uri (con, uri_str.c_str());
    gnome_cmd_con_set_host_name (con, host);
    gnome_cmd_con_set_port (con, port);
    gnome_cmd_con_set_root_path (con, path);

    gnome_cmd_con_remote_set_host_name (server, host);

    con->method = gnome_cmd_con_get_scheme (uri_str.c_str());
    con->auth = con->method==CON_ANON_FTP ? GnomeCmdCon::NOT_REQUIRED : GnomeCmdCon::SAVE_FOR_SESSION;

    g_free (path);
    g_free(host);

    return server;
}


void gnome_cmd_con_remote_set_host_name (GnomeCmdConRemote *con, const gchar *host_name)
{
    g_return_if_fail (con != nullptr);
    g_return_if_fail (host_name != nullptr);

    GNOME_CMD_CON (con)->open_tooltip = g_strdup_printf (_("Opens remote connection to %s"), host_name);
    GNOME_CMD_CON (con)->close_tooltip = g_strdup_printf (_("Closes remote connection to %s"), host_name);
}
