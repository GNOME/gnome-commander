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

#include <config.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-pixmap.h"


GnomeCmdPixmap *
gnome_cmd_pixmap_new_from_file (const gchar *filepath)
{
    //FIXME: Handle GError here
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (filepath, NULL);
    if (pixbuf)
        return gnome_cmd_pixmap_new_from_pixbuf (pixbuf);

    return NULL;
}


GnomeCmdPixmap *
gnome_cmd_pixmap_new_from_pixbuf (GdkPixbuf *pixbuf)
{
    GnomeCmdPixmap *pixmap;

    g_return_val_if_fail (pixbuf != NULL, NULL);

    pixmap = g_new (GnomeCmdPixmap, 1);
    pixmap->pixbuf = pixbuf;
//    gdk_pixbuf_ref (pixmap->pixbuf);

    pixmap->width = gdk_pixbuf_get_width (pixmap->pixbuf);
    pixmap->height = gdk_pixbuf_get_height (pixmap->pixbuf);

    gdk_pixbuf_render_pixmap_and_mask (pixmap->pixbuf, &pixmap->pixmap, &pixmap->mask, 128);
    gdk_pixmap_ref (pixmap->pixmap);
    gdk_bitmap_ref (pixmap->mask);

    return pixmap;
}


void
gnome_cmd_pixmap_free (GnomeCmdPixmap *pixmap)
{
    g_return_if_fail (pixmap != NULL);
    g_return_if_fail (pixmap->pixbuf != NULL);
    g_return_if_fail (pixmap->pixmap != NULL);
    g_return_if_fail (pixmap->mask != NULL);

    gdk_pixbuf_unref (pixmap->pixbuf);
    gdk_pixmap_unref (pixmap->pixmap);
    gdk_bitmap_unref (pixmap->mask);

    g_free (pixmap);
}

