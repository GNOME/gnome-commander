/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2004 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-ftp.h"
#include "gnome-cmd-plain-path.h"
#include "imageloader.h"
#include "utils.h"

struct _GnomeCmdConFtpPrivate
{
	gchar *alias;
	gchar *host_name;
	gushort host_port;
	gchar *user_name;
	gchar *pw;
};

static GnomeCmdConClass *parent_class = NULL;


static void
get_file_info_func (GnomeCmdCon *con)
{
	GnomeVFSResult res;
	GnomeVFSURI *uri;
	GnomeVFSFileInfoOptions infoOpts = GNOME_VFS_FILE_INFO_FOLLOW_LINKS
		| GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		| GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE;

	uri = gnome_cmd_con_create_uri (con, con->base_path);
	DEBUG('m', "FTP: Connecting to %s\n", gnome_vfs_uri_to_string (uri, 0));
	con->base_info = gnome_vfs_file_info_new ();

	res = gnome_vfs_get_file_info_uri (uri, con->base_info, infoOpts);

	gnome_vfs_uri_unref (uri);

	if (con->state == CON_STATE_OPENING) {
		DEBUG('m', "State was opening, setting flags\n");
		if (res == GNOME_VFS_OK) {
			con->state = CON_STATE_OPEN;
			con->open_result = CON_OPEN_OK;
		}
		else {
			con->state = CON_STATE_CLOSED;
			con->open_failed_reason = res;
			con->open_result = CON_OPEN_FAILED;
		}
	}
	else if (con->state == CON_STATE_CANCELLING)
		DEBUG('m', "The open operation was cancelled, doing nothing\n");
	else
		DEBUG('m', "Strange ConState %d\n", con->state);
}


static gboolean
start_get_file_info (GnomeCmdCon *con)
{
	g_thread_create ((GThreadFunc)get_file_info_func, con, FALSE, NULL);

	return FALSE;
}


static void
ftp_open (GnomeCmdCon *con)
{
	DEBUG('m', "Opening FTP-connection\n");

	if (!con->base_path) {
		con->base_path = gnome_cmd_plain_path_new ("/");
		gtk_object_ref (GTK_OBJECT (con->base_path));
	}

	con->state = CON_STATE_OPENING;
	con->open_result = CON_OPEN_IN_PROGRESS;

	gtk_timeout_add (1, (GtkFunction)start_get_file_info, con);
}


static gboolean
ftp_close (GnomeCmdCon *con)
{
	gnome_cmd_con_set_default_dir (con, NULL);
	gnome_cmd_con_set_cwd (con, NULL);
	gtk_object_unref (GTK_OBJECT (con->base_path));
	con->base_path = NULL;
	con->state = CON_STATE_CLOSED;
	con->open_result = CON_OPEN_NOT_STARTED;

	return TRUE;
}


static void
ftp_cancel_open (GnomeCmdCon *con)
{
	DEBUG('m', "Setting state canceling\n");
	con->state = CON_STATE_CANCELLING;
}


static gboolean
ftp_open_is_needed (GnomeCmdCon *con)
{
	return TRUE;
}


static GnomeVFSURI *
ftp_create_uri (GnomeCmdCon *con, GnomeCmdPath *path)
{
	GnomeVFSURI *u1, *u2;

	GnomeCmdConFtp *ftp_con;

	ftp_con = GNOME_CMD_CON_FTP (con);

	u1 = gnome_vfs_uri_new ("ftp:");
	u2 = gnome_vfs_uri_append_path (u1, gnome_cmd_path_get_path (path));
	gnome_vfs_uri_unref (u1);
	gnome_vfs_uri_set_host_name (u2, ftp_con->priv->host_name);
	gnome_vfs_uri_set_host_port (u2, ftp_con->priv->host_port);
	gnome_vfs_uri_set_user_name (u2, ftp_con->priv->user_name);
	gnome_vfs_uri_set_password (u2, ftp_con->priv->pw);

	return u2;
}


static GnomeCmdPath *
ftp_create_path (GnomeCmdCon *con, const gchar *path_str)
{
	return gnome_cmd_plain_path_new (path_str);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdConFtp *con = GNOME_CMD_CON_FTP (object);

	if (con->priv->alias)
		g_free (con->priv->alias);
	if (con->priv->host_name)
		g_free (con->priv->host_name);
	if (con->priv->user_name)
		g_free (con->priv->user_name);
	if (con->priv->pw)
		g_free (con->priv->pw);
	g_free (con->priv);
		
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdConFtpClass *class)
{
	GtkObjectClass *object_class;
	GnomeCmdConClass *con_class;

	object_class = GTK_OBJECT_CLASS (class);
	con_class = GNOME_CMD_CON_CLASS (class);
	parent_class = gtk_type_class (gnome_cmd_con_get_type ());
	
	object_class->destroy = destroy;

	con_class->open = ftp_open;
	con_class->close = ftp_close;
	con_class->cancel_open = ftp_cancel_open;
	con_class->open_is_needed = ftp_open_is_needed;
	con_class->create_uri = ftp_create_uri;
	con_class->create_path = ftp_create_path;
}


static void
init (GnomeCmdConFtp *ftp_con)
{
	GnomeCmdCon *con = GNOME_CMD_CON (ftp_con);
	
	ftp_con->priv = g_new0 (GnomeCmdConFtpPrivate, 1);

	con->should_remember_dir = TRUE;
	con->needs_open_visprog = TRUE;
	con->needs_list_visprog = TRUE;
	con->can_show_free_space = FALSE;
	con->is_local = FALSE;
	con->is_closeable = TRUE;
	con->go_pixmap = IMAGE_get_gnome_cmd_pixmap (PIXMAP_SERVER_SMALL);
	con->open_pixmap = IMAGE_get_gnome_cmd_pixmap (PIXMAP_FTP_CONNECT);
	con->close_pixmap = IMAGE_get_gnome_cmd_pixmap (PIXMAP_FTP_DISCONNECT);
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_con_ftp_get_type         (void)
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


GnomeCmdConFtp*
gnome_cmd_con_ftp_new     (const gchar *alias,
						   const gchar *host_name, 
						   gint host_port,
						   const gchar *user_name,
						   const gchar *pw)
{
	GnomeCmdConFtp *con;

	con = gtk_type_new (gnome_cmd_con_ftp_get_type ());

	gnome_cmd_con_ftp_set_alias (con, alias);
	gnome_cmd_con_ftp_set_host_name (con, host_name);
	gnome_cmd_con_ftp_set_host_port (con, host_port);
	gnome_cmd_con_ftp_set_user_name (con, user_name);
	gnome_cmd_con_ftp_set_pw (con, pw);

	GNOME_CMD_CON (con)->open_msg = g_strdup_printf (_("Connecting to %s\n"), host_name);

	return con;
}


void
gnome_cmd_con_ftp_set_alias           (GnomeCmdConFtp *con,
									   const gchar *alias)
{
	g_return_if_fail (con != NULL);
	g_return_if_fail (con->priv != NULL);
	g_return_if_fail (alias != NULL);
	
	if (con->priv->alias)
		g_free (con->priv->alias);
	
	con->priv->alias = g_strdup (alias);
	GNOME_CMD_CON (con)->alias = g_strdup (alias);
	GNOME_CMD_CON (con)->go_text = g_strdup_printf (_("Go to: %s"), alias);
	GNOME_CMD_CON (con)->open_text = g_strdup_printf (_("Connect to: %s"), alias);
	GNOME_CMD_CON (con)->close_text = g_strdup_printf (_("Disconnect from: %s"), alias);
}


void
gnome_cmd_con_ftp_set_host_name       (GnomeCmdConFtp *con,
									   const gchar *host_name)
{
	g_return_if_fail (con != NULL);
	g_return_if_fail (con->priv != NULL);
	g_return_if_fail (host_name != NULL);
	
	if (con->priv->host_name)
		g_free (con->priv->host_name);
	
	con->priv->host_name = g_strdup (host_name);

	GNOME_CMD_CON (con)->open_tooltip =
		g_strdup_printf (_("Creates an FTP connection to %s"), host_name);
	GNOME_CMD_CON (con)->close_tooltip =
		g_strdup_printf (_("Removes the FTP connection to %s"), host_name);
}


void
gnome_cmd_con_ftp_set_host_port       (GnomeCmdConFtp *con,
									   gint host_port)
{
	g_return_if_fail (con != NULL);
	g_return_if_fail (con->priv != NULL);
	
	con->priv->host_port = host_port;
}


void
gnome_cmd_con_ftp_set_user_name       (GnomeCmdConFtp *con,
									   const gchar *user_name)
{
	g_return_if_fail (con != NULL);
	g_return_if_fail (con->priv != NULL);
	g_return_if_fail (user_name != NULL);
	
	if (con->priv->user_name)
		g_free (con->priv->user_name);
	
	con->priv->user_name = g_strdup (user_name);
}


void
gnome_cmd_con_ftp_set_pw              (GnomeCmdConFtp *con,
									   const gchar *pw)
{
	g_return_if_fail (con != NULL);
	g_return_if_fail (con->priv != NULL);

	if (pw == con->priv->pw)
		return;
	
	if (con->priv->pw)
		g_free (con->priv->pw);

	if (pw)
		con->priv->pw = g_strdup (pw);
	else
		con->priv->pw = NULL;
}


const gchar *
gnome_cmd_con_ftp_get_alias           (GnomeCmdConFtp *con)
{
	g_return_val_if_fail (con != NULL, NULL);
	g_return_val_if_fail (con->priv != NULL, NULL);

	return con->priv->alias;
}


const gchar *
gnome_cmd_con_ftp_get_host_name       (GnomeCmdConFtp *con)
{
	g_return_val_if_fail (con != NULL, NULL);
	g_return_val_if_fail (con->priv != NULL, NULL);

	return con->priv->host_name;
}


gint
gnome_cmd_con_ftp_get_host_port       (GnomeCmdConFtp *con)
{
	g_return_val_if_fail (con != NULL, -1);
	g_return_val_if_fail (con->priv != NULL, -1);

	return con->priv->host_port;
}


const gchar *
gnome_cmd_con_ftp_get_user_name       (GnomeCmdConFtp *con)
{
	g_return_val_if_fail (con != NULL, NULL);
	g_return_val_if_fail (con->priv != NULL, NULL);

	return con->priv->user_name;
}


const gchar *
gnome_cmd_con_ftp_get_pw              (GnomeCmdConFtp *con)
{
	g_return_val_if_fail (con != NULL, NULL);
	g_return_val_if_fail (con->priv != NULL, NULL);

	return con->priv->pw;
}
