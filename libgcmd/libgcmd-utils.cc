/** 
 * @file libgcmd-utils.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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

#include <config.h>
#include <string.h>
#include "libgcmd-deps.h"
#include "libgcmd-utils.h"


inline gchar *get_trashed_string (const gchar *in)
{
    gchar *out = g_strdup (in);
    gchar *end;

    while (!g_utf8_validate (out, -1, (const gchar **)&end))
        *end = '?';

    return out;
}


gchar *get_utf8 (const gchar *unknown)
{
    gchar *out;

    if (!unknown) return NULL;

    if (g_utf8_validate (unknown, -1, NULL))
        out = g_strdup (unknown);
    else 
    {
        gsize i;
        out = g_locale_to_utf8 (unknown, strlen (unknown), &i, &i, NULL);
        if (!out)
            out = get_trashed_string (unknown);
    }

    return out;
}
