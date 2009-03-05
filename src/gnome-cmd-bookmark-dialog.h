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
#ifndef __GNOME_CMD_BOOKMARK_DIALOG_H__
#define __GNOME_CMD_BOOKMARK_DIALOG_H__

#include "gnome-cmd-types.h"

#define GNOME_CMD_BOOKMARK_DIALOG(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_bookmark_dialog_get_type (), GnomeCmdBookmarkDialog)
#define GNOME_CMD_BOOKMARK_DIALOG_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_bookmark_dialog_get_type (), GnomeCmdBookmarkDialogClass)
#define GNOME_CMD_IS_BOOKMARK_DIALOG(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_bookmark_dialog_get_type ())


struct GnomeCmdBookmarkDialogPrivate;


struct GnomeCmdBookmarkDialog
{
    GnomeCmdDialog parent;
    GnomeCmdBookmarkDialogPrivate *priv;
};


struct GnomeCmdBookmarkDialogClass
{
    GnomeCmdDialogClass parent_class;
};

#define BOOKMARK_DIALOG_NUM_COLUMNS 3

extern guint bookmark_dialog_default_column_width[BOOKMARK_DIALOG_NUM_COLUMNS];


GtkWidget *gnome_cmd_bookmark_dialog_new ();

GtkType gnome_cmd_bookmark_dialog_get_type ();

void gnome_cmd_bookmark_add_current ();

void gnome_cmd_bookmark_goto (GnomeCmdBookmark *bookmark);

#endif // __GNOME_CMD_BOOKMARK_DIALOG_H__
