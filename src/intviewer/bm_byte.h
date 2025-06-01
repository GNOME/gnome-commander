/**
 *  @file bm_byte.h
 *  @brief Part of GNOME Commander - A GNOME based file manager
 * 
 *  @copyright (C) 2006 Assaf Gordon\n
 *  @copyright (C) 2007-2012 Piotr Eljasiak\n
 *  @copyright (C) 2013-2024 Uwe Scholz\n
 * 
 *  @copyright This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  @copyright This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  @copyright You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#pragma once

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
extern "C" GViewerBMByteData *create_bm_byte_data(const guint8 *pattern, const gint length);

extern "C" int bm_byte_data_pattern_len(GViewerBMByteData *data);
extern "C" int *bm_byte_data_good(GViewerBMByteData *data);
extern "C" int *bm_byte_data_bad(GViewerBMByteData *data);

extern "C" void free_bm_byte_data(GViewerBMByteData *data);
