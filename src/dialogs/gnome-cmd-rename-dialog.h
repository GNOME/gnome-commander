/** 
 * @file gnome-cmd-rename-dialog.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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

#ifndef __GNOME_CMD_RENAME_DIALOG_H__
#define __GNOME_CMD_RENAME_DIALOG_H__

#include "gnome-cmd-file.h"

#define GNOME_CMD_TYPE_RENAME_DIALOG              (gnome_cmd_rename_dialog_get_type ())
#define GNOME_CMD_RENAME_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_RENAME_DIALOG, GnomeCmdRenameDialog))
#define GNOME_CMD_RENAME_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_RENAME_DIALOG, GnomeCmdRenameDialogClass))
#define GNOME_CMD_IS_RENAME_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_RENAME_DIALOG))
#define GNOME_CMD_IS_RENAME_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_RENAME_DIALOG))
#define GNOME_CMD_RENAME_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_RENAME_DIALOG, GnomeCmdRenameDialogClass))


GType gnome_cmd_rename_dialog_get_type ();


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

#endif // __GNOME_CMD_RENAME_DIALOG_H__
