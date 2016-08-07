/**
 * @file viewer-utils.cc
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

#include <glib.h>

#include "gvtypes.h"
#include "viewer-utils.h"

using namespace std;

int unicode2utf8 (unsigned int unicode, unsigned char *out)
{
    int bytes_needed = 0;
    if (unicode<0x80)
    {
        bytes_needed = 1;
        out[0] = (unsigned char)(unicode&0xFF);
    }
    else
    if (unicode<0x0800)
    {
        bytes_needed = 2;
        out[0] = (unsigned char)(unicode>>6 | 0xC0);
        out[1] = (unsigned char)((unicode&0x3F)| 0x80);
    }
    else
    if (unicode<0x10000)
    {
        bytes_needed = 3;
        out[0] = (unsigned char)((unicode>>12) | 0xE0);
        out[1] = (unsigned char)(((unicode>>6) & 0x3F) | 0x80);
        out[2] = (unsigned char)((unicode & 0x3F) | 0x80);
    }
    else
    {
        bytes_needed = 4;
        out[0] = (unsigned char)((unicode>>18) | 0xE0);
        out[1] = (unsigned char)(((unicode>>12) & 0x3F) | 0x80);
        out[2] = (unsigned char)(((unicode>>6) & 0x3F) | 0x80);
        out[3] = (unsigned char)((unicode & 0x3F) | 0x80);
    }

    return bytes_needed;
}


char_type *convert_utf8_to_chartype_array (const gchar *utf8text, /*out*/ int &array_length)
{
    g_return_val_if_fail (utf8text!=NULL, NULL);
    g_return_val_if_fail (g_utf8_validate(utf8text, -1, NULL), NULL);

    guint32 unicode_char;

    glong length = g_utf8_strlen(utf8text, -1);
    g_return_val_if_fail (length>0, NULL);

    char_type *result = g_new0 (char_type, length);
    array_length = length;

    const gchar *pos = utf8text;

    for (glong index=0; index<length; ++index)
    {
        unicode_char = g_utf8_get_char(pos);

        unicode2utf8(unicode_char, (unsigned char*)&result[index]);

        pos = g_utf8_next_char(pos);
        if (!pos)
        {
            g_warning ("unexpected NULL found in UTF8 string");
            break;
        }
    }

    return result;
}


guint8 *mem_reverse (const guint8 *buffer, guint buflen)
{
    g_return_val_if_fail (buffer!=NULL, NULL);
    g_return_val_if_fail (buflen>0, NULL);

    guint i, j;

    guint8 *result = g_new0 (guint8, buflen);

    for (i=0, j=buflen-1;i<buflen;i++, j--)
        result[i] = buffer[j];

    return result;
}


guint8 *text2hex (const gchar *text, /*out*/ guint &buflen)
{
    g_return_val_if_fail (text!=NULL, NULL);

    guint8 *result;
    int len;
    guint8 value;
    gboolean high_nib;

    int idx = 0;
    len = 0;
    while (text[idx])
        if (text[idx]==' ')
            idx++;
        else
            if (g_ascii_isxdigit (text[idx]))
            {
                idx++;
                len++;
            }
            else
                return NULL;

    if (len % 2 != 0)
        return NULL;

    result = g_new0 (guint8, len);

    len = 0;
    high_nib = TRUE;
    value = 0;
    for (gint idx=0; text[idx]; ++idx)
        if (g_ascii_isxdigit (text[idx]))
        {
            if (high_nib)
                value = g_ascii_xdigit_value(text[idx]) * 16;
            else
            {
                value += g_ascii_xdigit_value(text[idx]);
                result[len] = value;
                len++;
            }
            high_nib = !high_nib;
        };
    buflen = len;
    return result;
}
