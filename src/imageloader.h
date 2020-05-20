/**
 * @file imageloader.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2020 Uwe Scholz\n
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
#define COPYFILENAMES_STOCKID       "gnome-commander-copy-file-names"
#define MAILSEND_STOCKID            "gnome-commander-mail-send"
#define TERMINAL_STOCKID            "gnome-commander-terminal"
#define FILETYPEDIR_STOCKID         "gnome-commander-file-type-dir"
#define FILETYPEREGULARFILE_STOCKID "gnome-commander-file-type-regular-file"

#include "gnome-cmd-pixmap.h"

/**
 * If you add a pixmap id here be sure to add its filename in
 * the array in imageloader.c
 */
enum Pixmap
{
    PIXMAP_NONE,

    PIXMAP_FLIST_ARROW_UP,
    PIXMAP_FLIST_ARROW_DOWN,
    PIXMAP_FLIST_ARROW_BLANK,

    PIXMAP_LOGO,
    PIXMAP_EXEC_WHEEL,
    PIXMAP_BOOKMARK,

    PIXMAP_OVERLAY_SYMLINK,
    PIXMAP_OVERLAY_UMOUNT,

    PIXMAP_INTERNAL_VIEWER,

    NUM_PIXMAPS
};


void IMAGE_init ();
void IMAGE_free ();

GnomeCmdPixmap *IMAGE_get_gnome_cmd_pixmap (Pixmap pixmap_id);

inline GdkPixmap *IMAGE_get_pixmap (Pixmap pixmap_id)
{
    GnomeCmdPixmap *pixmap = IMAGE_get_gnome_cmd_pixmap (pixmap_id);
    return pixmap ? pixmap->pixmap : NULL;
}

inline GdkBitmap *IMAGE_get_mask (Pixmap pixmap_id)
{
    GnomeCmdPixmap *pixmap = IMAGE_get_gnome_cmd_pixmap (pixmap_id);
    return pixmap ? pixmap->mask : NULL;
}

inline GdkPixbuf *IMAGE_get_pixbuf (Pixmap pixmap_id)
{
    GnomeCmdPixmap *pixmap = IMAGE_get_gnome_cmd_pixmap (pixmap_id);
    return pixmap ? pixmap->pixbuf : NULL;
}

gboolean IMAGE_get_pixmap_and_mask (GnomeVFSFileType type,
                                    const gchar *mime_type,
                                    gboolean symlink,
                                    GdkPixmap **pixmap,
                                    GdkBitmap **mask);

void IMAGE_clear_mime_cache ();

void register_gnome_commander_stock_icons (void);

char* register_application_stock_icon(const char* openWithDefaultLabel, const char* defaultAppIconPath);