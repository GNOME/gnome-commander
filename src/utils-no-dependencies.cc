/** 
 * @file utils_no_dependencies.cc
 * 
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#include <glib.h>
#include <string.h>
#include "utils-no-dependencies.h"


gchar* str_uri_basename (const gchar *uri)
{
    if (!uri)
        return NULL;

    int len = strlen (uri);

    if (len < 2)
        return NULL;

    int last_slash = 0;

    for (int i = 0; i < len; i++)
        if (uri[i] == '/')
            last_slash = i;

    return g_uri_unescape_string (&uri[last_slash+1], NULL);
}


