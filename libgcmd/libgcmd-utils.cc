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
#include <string.h>
#include "libgcmd-deps.h"
#include "libgcmd-utils.h"


static gchar *get_trashed_string (const gchar *in)
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


gchar *get_bold_text (const gchar *in)
{
    return g_strdup_printf ("<span weight=\"bold\">%s</span>", in);
}


gchar *get_mono_text (const gchar *in)
{
    return g_strdup_printf ("<span font_family=\"monospace\">%s</span>", in);
}


gchar *get_bold_mono_text (const gchar *in)
{
    return g_strdup_printf ("<span font_family=\"monospace\" weight=\"bold\">%s</span>", in);
}
