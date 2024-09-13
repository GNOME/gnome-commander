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

#define FILETYPEICONS_FOLDER        "file-type-icons"

#define COPYFILENAMES_STOCKID          "gnome-commander-copy-file-names"
#define GTK_PREFERENCES_SYSTEM_STOCKID "preferences-system"
#define FILETYPEDIR_STOCKID            "file_type_dir"
#define FILETYPEREGULARFILE_STOCKID    "file_type_regular"
#define GTK_MAILSEND_STOCKID           "mail-send"
#define GTK_TERMINAL_STOCKID           "utilities-terminal"
#define ROTATE_90_STOCKID              "gnome-commander-rotate-90"
#define ROTATE_270_STOCKID             "gnome-commander-rotate-270"
#define ROTATE_180_STOCKID             "gnome-commander-rotate-180"
#define FLIP_VERTICAL_STOCKID          "gnome-commander-flip-vertical"
#define FLIP_HORIZONTAL_STOCKID        "gnome-commander-flip-horizontal"

#define OVERLAY_UMOUNT_ICON            "overlay_umount"
#define OVERLAY_SYMLINK_ICON           "overlay_symlink"

void IMAGE_init ();
void IMAGE_free ();

GIcon *IMAGE_get_file_icon (guint32 type, const gchar *mime_type, gboolean symlink);

void IMAGE_clear_mime_cache ();
