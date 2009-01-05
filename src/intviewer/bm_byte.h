/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
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

#ifndef __GVIEWER_BM_BYTE_H__
#define __GVIEWER_BM_BYTE_H__

#include <glib.h>
#include <glib-object.h>

struct GViewerBMByteData
{
    /* good-suffix-shift array, one element for each (unique) character in the search pattern */
    int *good;

    /* bad characters jump table.
        since we're searching on BYTES, we can use an array of 256 elements */
    int *bad;

    guint8 *pattern;
    int pattern_len;
};

/* Create the Boyer-Moore jump tables.
    pattern is the search pattern, UTF8 string (null-terminated)*/
GViewerBMByteData *create_bm_byte_data(const guint8 *pattern, const gint length);

void free_bm_byte_data(GViewerBMByteData *data);

#endif /* __GLIBVIEWER_BM_BYTE_H__ */
