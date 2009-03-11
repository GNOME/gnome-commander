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
#include <string.h>
#include "gvtypes.h"
#include "bm_byte.h"
#include "cp437.h"

using namespace std;


inline void badchar_compute(guint8 *pattern, int m, /*out*/ int *bad)
{
   int i;
   for (i=0;i<256;i++)
       bad[i] = m;

   for (i = 0; i < m - 1; ++i)
       bad[(int)pattern[i]] = m - i - 1;
}


/************************************************
  Helper function to compute the suffices of each character for the good-suffices array
************************************************/
inline void suffices(guint8 *pattern, int m, /* out */ int *suff)
{
   int f, g;

   f = 0;
   suff[m - 1] = m;
   g = m - 1;
   for (int i = m - 2; i >= 0; --i)
   {
      if (i > g && suff[i + m - 1 - f] < i - g)
         suff[i] = suff[i + m - 1 - f];
      else
      {
         if (i < g)
            g = i;
         f = i;
         while (g >= 0 && pattern[g] == pattern[g + m - 1 - f])
            --g;
         suff[i] = f - g;
      }
   }
}


inline void goodsuff_compute(guint8 *pattern, int m, /*out*/ int *good)
{
   int *suff = g_new0(int, m);

   suffices(pattern, m, suff);

   for (int i = 0; i < m; ++i)
      good[i] = m;
   int j = 0;
   for (int i = m - 1; i >= -1; --i)
      if (i == -1 || suff[i] == i + 1)
         for (; j < m - 1 - i; ++j)
            if (good[j] == m)
               good[j] = m - 1 - i;
   for (int i = 0; i <= m - 2; ++i)
      good[m - 1 - suff[i]] = m - 1 - i;

   g_free (suff);
}


GViewerBMByteData *create_bm_byte_data(const guint8 *pattern, const gint length)
{
    g_return_val_if_fail (pattern!=NULL, NULL);
    g_return_val_if_fail (length>0, NULL);

    GViewerBMByteData *data = g_new0 (GViewerBMByteData, 1);

    data->pattern_len = length;
    data->pattern = g_new(guint8, length);
    memcpy(data->pattern, pattern, length);

    data->bad = g_new0 (int, 256);
    badchar_compute(data->pattern, data->pattern_len, data->bad);

    data->good = g_new0 (int, data->pattern_len);
    goodsuff_compute(data->pattern, data->pattern_len, data->good);

    return data;
}


void free_bm_byte_data(GViewerBMByteData *data)
{
    if (data==NULL)
        return;

    g_free (data->good);
    data->good=NULL;

    g_free (data->bad);
    data->bad = NULL;

    g_free (data->pattern);
    data->pattern = NULL;

    data->pattern_len = 0;

    g_free (data);
}
