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
#ifndef __GNOME_CMD_REMOTE_DIALOG_H__
#define __GNOME_CMD_REMOTE_DIALOG_H__

#define GNOME_CMD_TYPE_REMOTE_DIALOG          (gnome_cmd_remote_dialog_get_type())
#define GNOME_CMD_REMOTE_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_REMOTE_DIALOG, GnomeCmdRemoteDialog))
#define GNOME_CMD_REMOTE_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_CMD_TYPE_REMOTE_DIALOG, GnomeCmdRemoteDialogClass))
#define GNOME_CMD_IS_REMOTE_DIALOG(obj)       (G_TYPE_INSTANCE_CHECK_TYPE ((obj), GNOME_CMD_TYPE_REMOTE_DIALOG)


typedef struct _GnomeCmdRemoteDialog GnomeCmdRemoteDialog;
typedef struct _GnomeCmdRemoteDialogPrivate GnomeCmdRemoteDialogPrivate;
typedef struct _GnomeCmdRemoteDialogClass GnomeCmdRemoteDialogClass;


struct _GnomeCmdRemoteDialog
{
    GnomeCmdDialog parent;

    GnomeCmdRemoteDialogPrivate *priv;
};


struct _GnomeCmdRemoteDialogClass
{
    GnomeCmdDialogClass parent_class;
};


GtkType gnome_cmd_remote_dialog_get_type (void);

GtkWidget *gnome_cmd_remote_dialog_new (void);

void show_quick_connect_dialog (void);

#endif // __GNOME_CMD_REMOTE_DIALOG_H__
