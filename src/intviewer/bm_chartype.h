/**
 * @file bm_chartype.h
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
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
 *
 */

#pragma once

#include <glib.h>
#include <glib-object.h>

struct GViewerBMChartypeData;


/* Create the Boyer-Moore jump tables.
    pattern is the search pattern, UTF8 string (null-terminated)*/
extern "C" GViewerBMChartypeData *create_bm_chartype_data(const gchar*pattern, gboolean case_sensitive);

extern "C" void free_bm_chartype_data(GViewerBMChartypeData*data);

extern "C" int bch_get_value(GViewerBMChartypeData *data, char_type key, int default_value);

/* will compare data->pattern[pattern_index] with "ch", using "data->case_sensitive" if needed */
extern "C" gboolean bm_chartype_equal(GViewerBMChartypeData *data, int pattern_index, char_type ch);

/* returns MAX(good_table[pattern_index], bad_table[ch]) */
extern "C" int bm_chartype_get_advancement(GViewerBMChartypeData *data, int pattern_index,  char_type ch);

extern "C" int bm_chartype_get_good_match_advancement(GViewerBMChartypeData *data);

extern "C" int bm_chartype_data_pattern_len(GViewerBMChartypeData* data);
