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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-pixmap.h"

using namespace std;


GnomeCmdPixmap *gnome_cmd_pixmap_new_from_file (const gchar *filepath, int width, int height)
{
    //FIXME: Handle GError here
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (filepath, width, height, NULL);

    return pixbuf ? gnome_cmd_pixmap_new_from_pixbuf (pixbuf) : NULL;
}


GnomeCmdPixmap *gnome_cmd_pixmap_new_from_icon (const gchar *icon_name, gint size, GtkIconLookupFlags flags)
{
   GError *error = NULL;

   GdkPixbuf *pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon_name, size, flags, &error);

   if (pixbuf)
       return gnome_cmd_pixmap_new_from_pixbuf (pixbuf);

   g_warning ("Couldn't load icon: %s", error->message);
   g_error_free (error);

   return NULL;
}


GnomeCmdPixmap *gnome_cmd_pixmap_new_from_pixbuf (GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, NULL);

    GnomeCmdPixmap *pixmap = g_new (GnomeCmdPixmap, 1);
    pixmap->pixbuf = pixbuf;
//    gdk_pixbuf_ref (pixmap->pixbuf);

    pixmap->width = gdk_pixbuf_get_width (pixmap->pixbuf);
    pixmap->height = gdk_pixbuf_get_height (pixmap->pixbuf);

    gdk_pixbuf_render_pixmap_and_mask (pixmap->pixbuf, &pixmap->pixmap, &pixmap->mask, 128);
    gdk_pixmap_ref (pixmap->pixmap);
    gdk_bitmap_ref (pixmap->mask);

    return pixmap;
}
