/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2003 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 


#ifndef __IMAGELOADER_H__
#define __IMAGELOADER_H__

#include "gnome-cmd-pixmap.h"


/**
 * If you add a pixmap id here be sure to add its filename in 
 * the array in imageloader.c
 */
typedef enum {
	PIXMAP_NONE,
	
	PIXMAP_FLIST_ARROW_UP,
	PIXMAP_FLIST_ARROW_DOWN,
	PIXMAP_FLIST_ARROW_BLANK,

	PIXMAP_SERVER_SMALL,
	PIXMAP_LOGO,
	PIXMAP_EXEC,
	PIXMAP_EXEC_WHEEL,
	PIXMAP_MKDIR,
	PIXMAP_LOCK,
	PIXMAP_HOME,
	PIXMAP_SMB_NETWORK,
	PIXMAP_SMB_COMPUTER,
	PIXMAP_BOOKMARK,

	PIXMAP_OVERLAY_SYMLINK,
	PIXMAP_OVERLAY_UMOUNT,
	PIXMAP_PARENT_DIR,
	PIXMAP_ROOT_DIR,
	PIXMAP_FTP_CONNECT,
	PIXMAP_FTP_DISCONNECT,
	PIXMAP_MENU_FTP_CONNECT,
	PIXMAP_MENU_FTP_DISCONNECT,
	PIXMAP_SWITCH_V,
	PIXMAP_SWITCH_H,
	
	NUM_PIXMAPS
} Pixmap;


void IMAGE_init (void);
void IMAGE_free (void);

GdkPixmap *IMAGE_get_pixmap (Pixmap pixmap_id);
GdkBitmap *IMAGE_get_mask (Pixmap pixmap_id);
GdkPixbuf *IMAGE_get_pixbuf (Pixmap pixmap_id);
GnomeCmdPixmap *IMAGE_get_gnome_cmd_pixmap (Pixmap pixmap_id);

gboolean IMAGE_get_pixmap_and_mask (GnomeVFSFileType type,
									const gchar *mime_type,
									gboolean symlink,
									GdkPixmap **pixmap,
									GdkBitmap **mask);

void IMAGE_clear_mime_cache (void);


#endif //__IMAGELOADER_H__
