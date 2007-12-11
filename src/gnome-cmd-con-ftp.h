/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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

#ifndef __GNOME_CMD_CON_FTP_H__
#define __GNOME_CMD_CON_FTP_H__

#include "gnome-cmd-con.h"

#define GNOME_CMD_CON_FTP(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_con_ftp_get_type (), GnomeCmdConFtp)
#define GNOME_CMD_CON_FTP_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_con_ftp_get_type (), GnomeCmdConFtpClass)
#define GNOME_CMD_IS_CON_FTP(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_con_ftp_get_type ())


typedef struct _GnomeCmdConFtp GnomeCmdConFtp;
typedef struct _GnomeCmdConFtpClass GnomeCmdConFtpClass;
typedef struct _GnomeCmdConFtpPrivate GnomeCmdConFtpPrivate;


struct _GnomeCmdConFtp
{
    GnomeCmdCon parent;

    GnomeCmdConFtpPrivate *priv;
};

struct _GnomeCmdConFtpClass
{
    GnomeCmdConClass parent_class;
};


GtkType gnome_cmd_con_ftp_get_type (void);

GnomeCmdConFtp *gnome_cmd_con_ftp_new (const gchar *alias, const std::string &uri);
GnomeCmdConFtp *gnome_cmd_con_ftp_new (const gchar *alias, const gchar *host, guint port, const gchar *user, const gchar *password, const gchar *remote_dir);

void gnome_cmd_con_ftp_set_alias (GnomeCmdConFtp *fs, const gchar *alias);

void gnome_cmd_con_ftp_set_host_name (GnomeCmdConFtp *fs, const gchar *host_name);

void gnome_cmd_con_ftp_set_host_port (GnomeCmdConFtp *fs, gushort host_port);

void gnome_cmd_con_ftp_set_remote_dir (GnomeCmdConFtp *fs, const gchar *pw);

void gnome_cmd_con_ftp_set_user_name (GnomeCmdConFtp *fs, const gchar *user_name);

void gnome_cmd_con_ftp_set_pw (GnomeCmdConFtp *fs, const gchar *pw);

#endif // __GNOME_CMD_CON_FTP_H__
