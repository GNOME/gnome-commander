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

#ifndef __GNOME_CMD_XFER_H__
#define __GNOME_CMD_XFER_H__

#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"

void
gnome_cmd_xfer_start (GList *src_files,
                      GnomeCmdDir *to,
                      GnomeCmdFileList *src_fl,
                      gchar *dest_fn,
                      GnomeVFSXferOptions xferOptions,
                      GnomeVFSXferOverwriteMode xferOverwriteMode,
                      GtkSignalFunc on_completed_func,
                      gpointer on_completed_data);


void
gnome_cmd_xfer_uris_start (GList *src_uri_list,
                           GnomeCmdDir *to,
                           GnomeCmdFileList *src_fl,
                           GList *src_files,
                           gchar *dest_fn,
                           GnomeVFSXferOptions xferOptions,
                           GnomeVFSXferOverwriteMode xferOverwriteMode,
                           GtkSignalFunc on_completed_func,
                           gpointer on_completed_data);

void
gnome_cmd_xfer_tmp_download (GnomeVFSURI *src_uri,
                             GnomeVFSURI *dest_uri,
                             GnomeVFSXferOptions xferOptions,
                             GnomeVFSXferOverwriteMode xferOverwriteMode,
                             GtkSignalFunc on_completed_func,
                             gpointer on_completed_data);

void
gnome_cmd_xfer_tmp_download_multiple (GList *src_uri_list,
                                      GList *dest_uri_list,
                                      GnomeVFSXferOptions xferOptions,
                                      GnomeVFSXferOverwriteMode xferOverwriteMode,
                                      GtkSignalFunc on_completed_func,
                                      gpointer on_completed_data);

#endif // __GNOME_CMD_XFER_H__
