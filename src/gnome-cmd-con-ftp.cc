/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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
#include "gnome-cmd-data.h"
#include "gnome-cmd-con-ftp.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"
#include "utils.h"

using namespace std;


static GnomeCmdConClass *parent_class = NULL;


static void get_file_info_func (GnomeCmdCon *con)
{
    GnomeVFSURI *uri = gnome_cmd_con_create_uri (con, con->base_path);

    GnomeVFSFileInfoOptions infoOpts = (GnomeVFSFileInfoOptions) (GNOME_VFS_FILE_INFO_FOLLOW_LINKS | GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);

    DEBUG('m', "Connecting to %s\n", gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));
    con->base_info = gnome_vfs_file_info_new ();

    GnomeVFSResult res = gnome_vfs_get_file_info_uri (uri, con->base_info, infoOpts);

    gnome_vfs_uri_unref (uri);

    if (con->state == CON_STATE_OPENING)
    {
        DEBUG('m', "State was OPENING, setting flags\n");
        if (res == GNOME_VFS_OK)
        {
            con->state = CON_STATE_OPEN;
            con->open_result = CON_OPEN_OK;
        }
        else
        {
            con->state = CON_STATE_CLOSED;
            con->open_failed_reason = res;
            con->open_result = CON_OPEN_FAILED;
        }
    }
    else
        if (con->state == CON_STATE_CANCELLING)
            DEBUG('m', "The open operation was cancelled, doing nothing\n");
        else
            DEBUG('m', "Strange ConState %d\n", con->state);
}


static gboolean start_get_file_info (GnomeCmdCon *con)
{
    g_thread_create ((GThreadFunc) get_file_info_func, con, FALSE, NULL);

    return FALSE;
}


static void ftp_open (GnomeCmdCon *con)
{
    DEBUG('m', "Opening remote connection\n");

    if (!con->base_path)
    {
        con->base_path = gnome_cmd_plain_path_new (G_DIR_SEPARATOR_S);
        gtk_object_ref (GTK_OBJECT (con->base_path));
    }

    con->state = CON_STATE_OPENING;
    con->open_result = CON_OPEN_IN_PROGRESS;

    g_timeout_add (1, (GSourceFunc) start_get_file_info, con);
}


static gboolean ftp_close (GnomeCmdCon *con)
{
    gnome_cmd_con_set_default_dir (con, NULL);
    gnome_cmd_con_set_cwd (con, NULL);
    g_object_unref (con->base_path);
    con->base_path = NULL;
    con->state = CON_STATE_CLOSED;
    con->open_result = CON_OPEN_NOT_STARTED;

    return TRUE;
}


static void ftp_cancel_open (GnomeCmdCon *con)
{
    DEBUG('m', "Setting state CANCELLING\n");
    con->state = CON_STATE_CANCELLING;
}


static gboolean ftp_open_is_needed (GnomeCmdCon *con)
{
    return TRUE;
}


static GnomeVFSURI *ftp_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
    g_return_val_if_fail (con->uri != NULL, NULL);

    GnomeVFSURI *u0 = gnome_vfs_uri_new (con->uri);
    GnomeVFSURI *u1 = gnome_vfs_uri_append_path (u0, gnome_cmd_path_get_path (path));

    gnome_vfs_uri_unref (u0);

    return u1;
}


static GnomeCmdPath *ftp_create_path (GnomeCmdCon *con, const gchar *path_str)
{
    return gnome_cmd_plain_path_new (path_str);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdConFtp *con = GNOME_CMD_CON_FTP (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdConFtpClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GnomeCmdConClass *con_class = GNOME_CMD_CON_CLASS (klass);

    parent_class = (GnomeCmdConClass *) gtk_type_class (gnome_cmd_con_get_type ());

    object_class->destroy = destroy;

    con_class->open = ftp_open;
    con_class->close = ftp_close;
    con_class->cancel_open = ftp_cancel_open;
    con_class->open_is_needed = ftp_open_is_needed;
    con_class->create_uri = ftp_create_uri;
    con_class->create_path = ftp_create_path;
}


static void init (GnomeCmdConFtp *ftp_con)
{
    guint dev_icon_size = gnome_cmd_data.dev_icon_size;
    gint icon_size;

    g_assert (gtk_icon_size_lookup (GTK_ICON_SIZE_LARGE_TOOLBAR, &icon_size, NULL));

    GnomeCmdCon *con = GNOME_CMD_CON (ftp_con);

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

GtkType gnome_cmd_con_ftp_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdConFtp",
            sizeof (GnomeCmdConFtp),
            sizeof (GnomeCmdConFtpClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gnome_cmd_con_get_type (), &info);
    }
    return type;
}


GnomeCmdConFtp *gnome_cmd_con_ftp_new (const gchar *alias, const string &text_uri)
{
    GnomeVFSURI *uri = gnome_vfs_uri_new (text_uri.c_str());

    g_return_val_if_fail (uri != NULL, NULL);

    const gchar *scheme = gnome_vfs_uri_get_scheme (uri);       // do not g_free
    const gchar *host = gnome_vfs_uri_get_host_name (uri);      // do not g_free
    const gchar *remote_dir = gnome_vfs_uri_get_path (uri);     // do not g_free
    const gchar *user = gnome_vfs_uri_get_user_name (uri);      // do not g_free
    const gchar *password = gnome_vfs_uri_get_password (uri);   // do not g_free

    GnomeCmdConFtp *server = (GnomeCmdConFtp *) gtk_type_new (gnome_cmd_con_ftp_get_type ());

    g_return_val_if_fail (server != NULL, NULL);

    GnomeCmdCon *con = GNOME_CMD_CON (server);

    gnome_cmd_con_set_alias (con, alias);
    gnome_cmd_con_set_uri (con, text_uri);
    gnome_cmd_con_set_host_name (con, host);

    gnome_cmd_con_ftp_set_host_name (server, host);

    con->method = gnome_vfs_uri_is_local (uri) ? CON_LOCAL :
                  g_str_equal (scheme, "ftp")  ? (user && g_str_equal (user, "anonymous") ? CON_ANON_FTP : CON_FTP) :
                  g_str_equal (scheme, "sftp") ? CON_SSH :
                  g_str_equal (scheme, "dav")  ? CON_DAV :
                  g_str_equal (scheme, "davs") ? CON_DAVS :
                  g_str_equal (scheme, "smb")  ? CON_SMB :
                                                 CON_URI;

    con->gnome_auth = !password && con->method!=CON_ANON_FTP;          // ?????????

    gnome_vfs_uri_unref (uri);

    return server;
}


GnomeCmdConFtp *gnome_cmd_con_ftp_new (const gchar *alias, const gchar *host, guint port, const gchar *user, const gchar *password, const gchar *remote_dir)
{
    GnomeCmdConFtp *server = (GnomeCmdConFtp *) gtk_type_new (gnome_cmd_con_ftp_get_type ());

    g_return_val_if_fail (server != NULL, NULL);

    GnomeCmdCon *con = GNOME_CMD_CON (server);

    string _uri;
    string _host;
    string _port;
    string _remote_dir;
    string _user;
    string _password;

    if (port)                               // convert 0 --> ""
        stringify (_port, port);

    con->method = user && g_str_equal (user, "anonymous") ? CON_ANON_FTP : CON_FTP;

    gnome_cmd_con_make_ftp_uri (_uri,
                                con->gnome_auth,
                                stringify (_host, host),
                                _port,
                                stringify (_remote_dir, remote_dir),
                                stringify (_user, user),
                                stringify (_password, password));

    gnome_cmd_con_set_alias (con, alias);
    gnome_cmd_con_set_uri (con, _uri);
    gnome_cmd_con_set_host_name (con, _host);

    gnome_cmd_con_ftp_set_host_name (server, host);

    con->gnome_auth = !password && con->method!=CON_ANON_FTP;          // ?????????

    return server;
}


void gnome_cmd_con_ftp_set_host_name (GnomeCmdConFtp *con, const gchar *host_name)
{
    g_return_if_fail (con != NULL);
    g_return_if_fail (host_name != NULL);

    GNOME_CMD_CON (con)->open_tooltip = g_strdup_printf (_("Opens remote connection to %s"), host_name);
    GNOME_CMD_CON (con)->close_tooltip = g_strdup_printf (_("Closes remote connection to %s"), host_name);
}
