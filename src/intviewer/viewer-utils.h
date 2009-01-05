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

#ifndef __LIBGVIEWER_UTILS_H__
#define __LIBGVIEWER_UTILS_H__

#define GVIEWER_DEFAULT_PATH_PREFIX "/gnome-commander/internal_viewer/"

gchar   *gviewer_get_string (const gchar *path, const gchar *def);
gint     gviewer_get_int (const gchar *path, int def);
gboolean gviewer_get_bool (const gchar *path, gboolean def);

int unicode2utf8(unsigned int unicode, unsigned char *out);
char_type *convert_utf8_to_chartype_array(const gchar *utf8text, /*out*/ int *array_length);

guint8 *mem_reverse(const guint8 *buffer, guint buflen);

/* returns NULL if 'text' is not a valid hex string (whitespaces are OK, and are ignored) */
guint8 *text2hex(const gchar *text, /*out*/ guint *buflen);

/*  if "ch" is lower case english letter (a-z), returns UPPER case letter, otherwise returns unmodified "ch" */
inline char_type chartype_toupper(char_type ch)
{
    return (ch>='a' && ch<='z') ? (char_type) (ch & ~0x20) : ch;
}

#define CHARTYPE_CASE(ch,casesens) ((casesens)?ch:chartype_toupper(ch))

#endif
