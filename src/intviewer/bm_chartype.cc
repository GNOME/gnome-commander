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

#include <config.h>
#include <glib.h>
#include "gvtypes.h"
#include "viewer-utils.h"
#include "bm_chartype.h"
#include "cp437.h"

using namespace std;


/***********************************
    Bad Character hash-table functions
***********************************/
inline GHashTable *bch_create()
{
    return g_hash_table_new(g_direct_hash, g_direct_equal);
}


inline void bch_free(GHashTable *bch)
{
    g_hash_table_destroy(bch);
}


static void bch_set_value(GHashTable *bch, int key, int value)
{
    g_hash_table_insert(bch, (gpointer) key, (gpointer) value);
}


int bch_get_value(GViewerBMChartypeData *data, char_type key, int default_value)
{
    gint value = GPOINTER_TO_INT (g_hash_table_lookup(data->bad, GINT_TO_POINTER (CHARTYPE_CASE(key, data->case_sensitive))));

    return value==0 ? default_value : value;
}


inline void bch_compute(GHashTable *bch, char_type *pattern, int m, gboolean case_sens)
{
   for (int i = 0; i < m - 1; ++i)
       bch_set_value(bch, CHARTYPE_CASE(pattern[i], case_sens), m-i-1);
}


/************************************************
  Helper function to compute the suffices of each character for the good-suffices array
************************************************/
inline void suffices(char_type *pattern, int m, gboolean case_sens, int *suff)
{
   int f, g, i;

   f = 0;
   suff[m - 1] = m;
   g = m - 1;
   for (i = m - 2; i >= 0; --i)
   {
      if (i > g && suff[i + m - 1 - f] < i - g)
         suff[i] = suff[i + m - 1 - f];
      else
      {
         if (i < g)
            g = i;
         f = i;
         while (g >= 0 && CHARTYPE_CASE(pattern[g], case_sens) == CHARTYPE_CASE(pattern[g + m - 1 - f], case_sens))
            --g;
         suff[i] = f - g;
      }
   }
}


static void goodsuff_compute(char_type *pattern, int m, gboolean case_sens, /*out*/ int *good)
{
   int *suff = g_new0(int, m);
   int j;

   suffices(pattern, m, case_sens, suff);

   for (int i = 0; i < m; ++i)
      good[i] = m;
   j = 0;
   for (int i = m - 1; i >= -1; --i)
      if (i == -1 || suff[i] == i + 1)
         for (; j < m - 1 - i; ++j)
            if (good[j] == m)
               good[j] = m - 1 - i;
   for (int i = 0; i <= m - 2; ++i)
      good[m - 1 - suff[i]] = m - 1 - i;

   g_free (suff);
}


GViewerBMChartypeData *create_bm_chartype_data(const gchar *pattern, gboolean case_sensitive)
{
    GViewerBMChartypeData *data = g_new0(GViewerBMChartypeData, 1);

    data->case_sensitive = case_sensitive;

    data->pattern = convert_utf8_to_chartype_array(pattern, &data->pattern_len);
    if (!data->pattern)
        goto error;

    data->bad = bch_create();
    bch_compute(data->bad, data->pattern, data->pattern_len, case_sensitive);

    data->good = g_new0(int, data->pattern_len);
    goodsuff_compute(data->pattern, data->pattern_len, case_sensitive, data->good);

    return data;

error:
    free_bm_chartype_data(data);
    return NULL;
}


void free_bm_chartype_data(GViewerBMChartypeData*data)
{
    if (data==NULL)
        return;

    g_free (data->good);
    data->good=NULL;

    if (data->bad!=NULL)
        bch_free(data->bad);
    data->bad = NULL;

    g_free (data->pattern);
    data->pattern = NULL;

    data->pattern_len = 0;

    g_free (data);
}


gboolean bm_chartype_equal(GViewerBMChartypeData *data, int pattern_index, char_type ch)
{
    return CHARTYPE_CASE(data->pattern[pattern_index], data->case_sensitive)
        ==
        CHARTYPE_CASE(ch, data->case_sensitive);
}


int bm_chartype_get_advancement(GViewerBMChartypeData *data, int pattern_index, char_type ch)
{
    int m = data->pattern_len;

    return MAX(data->good[pattern_index], bch_get_value(data, ch, m) - m + 1 + pattern_index);
}
