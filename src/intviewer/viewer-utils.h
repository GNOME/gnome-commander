/**
 * @file viewer-utils.h
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#ifndef __LIBGVIEWER_UTILS_H__
#define __LIBGVIEWER_UTILS_H__

#define GVIEWER_DEFAULT_PATH_PREFIX "/gnome-commander/internal_viewer/"

int unicode2utf8(unsigned int unicode, unsigned char *out);
char_type *convert_utf8_to_chartype_array(const gchar *utf8text, /*out*/ int &array_length);

guint8 *mem_reverse(const guint8 *buffer, guint buflen);

/* returns NULL if 'text' is not a valid hex string (whitespaces are OK, and are ignored) */
guint8 *text2hex (const gchar *text, /*out*/ guint &buflen);

/*  if "ch" is lower case english letter (a-z), returns UPPER case letter, otherwise returns unmodified "ch" */
inline char_type chartype_toupper(char_type ch)
{
    return (ch>='a' && ch<='z') ? (char_type) (ch & ~0x20) : ch;
}

#define CHARTYPE_CASE(ch,casesens) ((casesens)?ch:chartype_toupper(ch))

#endif
