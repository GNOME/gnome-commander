/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2014 Uwe Scholz

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

#ifndef __LIB_GCMD_UTILS_H__
#define __LIB_GCMD_UTILS_H__

gchar *get_utf8 (const gchar *unknown);

inline gchar *get_bold_text (const gchar *in)
{
    return g_strdup_printf ("<span weight=\"bold\">%s</span>", in);
}

inline gchar *get_mono_text (const gchar *in)
{
    return g_strdup_printf ("<span font_family=\"monospace\">%s</span>", in);
}

inline gchar *get_bold_mono_text (const gchar *in)
{
    return g_strdup_printf ("<span font_family=\"monospace\" weight=\"bold\">%s</span>", in);
}

#endif //__LIB_GCMD_UTILS_H__
