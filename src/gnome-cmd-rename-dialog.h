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

#ifndef __GNOME_CMD_RENAME_DIALOG_H__
#define __GNOME_CMD_RENAME_DIALOG_H__

#include "gnome-cmd-file.h"

#define GNOME_CMD_RENAME_DIALOG(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_rename_dialog_get_type (), GnomeCmdRenameDialog)
#define GNOME_CMD_RENAME_DIALOG_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_rename_dialog_get_type (), GnomeCmdRenameDialogClass)
#define GNOME_CMD_IS_RENAME_DIALOG(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_rename_dialog_get_type ())


struct GnomeCmdRenameDialogPrivate;


struct GnomeCmdRenameDialog
{
    GtkWindow parent;

    GnomeCmdRenameDialogPrivate *priv;
};


struct GnomeCmdRenameDialogClass
{
    GtkWindowClass parent_class;
};


GtkWidget *gnome_cmd_rename_dialog_new (GnomeCmdFile *f, gint x, gint y, gint width, gint height);

GtkType gnome_cmd_rename_dialog_get_type ();

#endif // __GNOME_CMD_RENAME_DIALOG_H__
