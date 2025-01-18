/**
 * @file imageloader.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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

#define COPYFILENAMES_STOCKID          "gnome-commander-copy-file-names"
#define GTK_MAILSEND_STOCKID           "mail-send"
#define GTK_TERMINAL_STOCKID           "utilities-terminal"
#define OVERLAY_UMOUNT_ICON            "overlay_umount"

struct GnomeCmdIconCache;

extern GnomeCmdIconCache *icon_cache;

extern "C" GnomeCmdIconCache *gnome_cmd_icon_cache_new();
extern "C" void gnome_cmd_icon_cache_free(GnomeCmdIconCache *ic);
extern "C" GIcon *gnome_cmd_icon_cache_get_file_type_icon(GnomeCmdIconCache *ic, GFileType file_type, gboolean symlink);
extern "C" GIcon *gnome_cmd_icon_cache_get_mime_type_icon(GnomeCmdIconCache *ic, GFileType file_type, const gchar *mime_type, gboolean symlink);
