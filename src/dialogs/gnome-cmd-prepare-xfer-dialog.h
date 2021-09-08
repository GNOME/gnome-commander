/**
 * @file gnome-cmd-prepare-xfer-dialog.h
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
#pragma once

#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-selector.h"

#define GNOME_CMD_TYPE_PREPARE_XFER_DIALOG              (gnome_cmd_prepare_xfer_dialog_get_type ())
#define GNOME_CMD_PREPARE_XFER_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_PREPARE_XFER_DIALOG, GnomeCmdPrepareXferDialog))
#define GNOME_CMD_PREPARE_XFER_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_PREPARE_XFER_DIALOG, GnomeCmdPrepareXferDialogClass))
#define GNOME_CMD_IS_PREPARE_XFER_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_PREPARE_XFER_DIALOG))
#define GNOME_CMD_IS_PREPARE_XFER_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_PREPARE_XFER_DIALOG))
#define GNOME_CMD_PREPARE_XFER_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_PREPARE_XFER_DIALOG, GnomeCmdPrepareXferDialogClass))


struct GnomeCmdPrepareXferDialog
{
    GnomeCmdDialog parent;

    GtkWidget *dest_dir_frame;
    GtkWidget *dest_dir_entry;
    GtkWidget *left_vbox;
    GtkWidget *right_vbox;
    GtkWidget *left_vbox_frame;
    GtkWidget *right_vbox_frame;
    GtkWidget *ok_button;
    GtkWidget *cancel_button;

    GFileCopyFlags gFileCopyFlags;
    GnomeVFSXferOptions xferOptions;
    GnomeCmdConfirmOverwriteMode overwriteMode;

    GList *src_files;
    GnomeCmdFileSelector *src_fs;
    GnomeCmdDir *default_dest_dir;
};


struct GnomeCmdPrepareXferDialogClass
{
    GnomeCmdDialogClass parent_class;
};


GtkWidget *gnome_cmd_prepare_xfer_dialog_new (GnomeCmdFileSelector *from, GnomeCmdFileSelector *to);

GtkType gnome_cmd_prepare_xfer_dialog_get_type ();
