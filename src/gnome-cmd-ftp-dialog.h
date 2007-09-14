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
#ifndef __GNOME_CMD_FTP_DIALOG_H__
#define __GNOME_CMD_FTP_DIALOG_H__

#define GNOME_CMD_FTP_DIALOG(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_ftp_dialog_get_type (), GnomeCmdFtpDialog)
#define GNOME_CMD_FTP_DIALOG_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_ftp_dialog_get_type (), GnomeCmdFtpDialogClass)
#define GNOME_CMD_IS_FTP_DIALOG(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_ftp_dialog_get_type ())


typedef struct _GnomeCmdFtpDialog GnomeCmdFtpDialog;
typedef struct _GnomeCmdFtpDialogPrivate GnomeCmdFtpDialogPrivate;
typedef struct _GnomeCmdFtpDialogClass GnomeCmdFtpDialogClass;



struct _GnomeCmdFtpDialog
{
    GnomeCmdDialog parent;

    GnomeCmdFtpDialogPrivate *priv;
};


struct _GnomeCmdFtpDialogClass
{
    GnomeCmdDialogClass parent_class;
};



GtkType
gnome_cmd_ftp_dialog_get_type            (void);

GtkWidget*
gnome_cmd_ftp_dialog_new                 (void);

void
show_ftp_quick_connect_dialog            (void);

#endif // __GNOME_CMD_FTP_DIALOG_H__
