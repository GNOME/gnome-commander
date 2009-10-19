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

#include "inputmodes.h"
#include "viewer-utils.h"
#include "cp437.h"

using namespace std;


struct GVInputModesData
{
    gchar *input_mode_name;

    get_byte_proc   get_byte;
    void            *get_byte_user_data;

    /*
       Changing these function pointers is what constitues of an input mode chagne
    */
    input_get_char_proc get_char;
    input_get_offset_proc get_next_offset;
    input_get_offset_proc get_prev_offset;

    /*
        Input mode implementors:
        Add specific state variables here.
    */

    /* ASCII input mode variable:
       translates an ASCII byte value to a UTF-8 character value,
       using the requested character encoding. */
    char_type ascii_charset_translation[256];
};

/*
    Input mode implementors:
    Declare your functions here
*/
static offset_type inputmode_ascii_get_next_offset(GVInputModesData *imd, offset_type offset);
static offset_type inputmode_ascii_get_previous_offset(GVInputModesData *imd, offset_type offset);
static char_type inputmode_ascii_get_char(GVInputModesData *imd, offset_type offset);
static void inputmode_ascii_activate(GVInputModesData *imd, const gchar *encoding);

static char_type inputmode_utf8_get_char(GVInputModesData *imd, offset_type offset);
static offset_type inputmode_utf8_get_previous_offset(GVInputModesData *imd, offset_type offset);
static offset_type inputmode_utf8_get_next_offset(GVInputModesData *imd, offset_type offset);
static void inputmode_utf8_activate(GVInputModesData *imd);

GVInputModesData *gv_input_modes_new()
{
    return g_new0(GVInputModesData, 1);
}

/*
  General Input Mode Public Functions
*/
void gv_init_input_modes(GVInputModesData *imd, get_byte_proc proc, void *get_byte_user_data)
{
    g_return_if_fail (imd!=NULL);
    memset(imd, 0, sizeof(GVInputModesData));

    g_return_if_fail (proc!=NULL);
    imd->get_byte = proc;
    imd->get_byte_user_data = get_byte_user_data;

    /*
        Input mode implementors:
        Add specific 'init' functions here.
    */


    // Start with a default of ASCII input mode
    gv_set_input_mode(imd, "ASCII");
}


void gv_free_input_modes(GVInputModesData *imd)
{
    g_return_if_fail (imd!=NULL);

    g_free (imd->input_mode_name);

    /*
        Input mode implementors:
        Add specific 'free' functions here.
    */
}


const char*gv_get_input_mode(GVInputModesData *imd)
{
    g_return_val_if_fail (imd!=NULL, "");
    g_return_val_if_fail (imd->input_mode_name!=NULL, "");

    return imd->input_mode_name;
}


void gv_set_input_mode(GVInputModesData *imd, const gchar *input_mode)
{
    if (g_ascii_strcasecmp(input_mode, "ASCII")==0 || g_ascii_strcasecmp(input_mode, "CP437")==0)
    {
        inputmode_ascii_activate(imd, input_mode);
        return;
    }
    if (g_ascii_strcasecmp(input_mode, "UTF8")==0)
    {
        inputmode_utf8_activate(imd);
        return;
    }
    /*
        Input mode implementors:
        Add specific 'activate' functions here.
    */

    // If we got here, assume it is a character encoding in ASCII mode
    inputmode_ascii_activate(imd, input_mode);
}


char_type gv_input_mode_get_utf8_char(GVInputModesData *imd, offset_type offset)
{
    g_return_val_if_fail (imd!=NULL, INVALID_CHAR);
    g_return_val_if_fail (imd->get_char!=NULL, INVALID_CHAR);

    return imd->get_char(imd, offset);
}


offset_type gv_input_get_next_char_offset(GVInputModesData *imd, offset_type current_offset)
{
    g_return_val_if_fail (imd!=NULL, 0);
    g_return_val_if_fail (imd->get_next_offset!=NULL, 0);

    return imd->get_next_offset(imd, current_offset);
}


offset_type gv_input_get_previous_char_offset(GVInputModesData *imd, offset_type current_offset)
{
    g_return_val_if_fail (imd!=NULL, 0);
    g_return_val_if_fail (imd->get_prev_offset!=NULL, 0);

    return imd->get_prev_offset(imd, current_offset);
}


static int gv_input_mode_get_byte(GVInputModesData *imd, offset_type offset)
{
    g_return_val_if_fail (imd->get_byte!=NULL, INVALID_CHAR);

    return imd->get_byte(imd->get_byte_user_data, offset);
}


int gv_input_mode_get_raw_byte(GVInputModesData *imd, offset_type offset)
{
    return gv_input_mode_get_byte(imd, offset);
}


/*****************************************************************************
  Specific Input mode related function
******************************************************************************/

/************ ASCII input mode *****************/

static void inputmode_ascii_activate(GVInputModesData *imd, const gchar *encoding)
{
    int i;
    GIConv icnv;        // GLib's IConvert interface/emulation

    g_return_if_fail (imd!=NULL);

    // First thing, set ASCII input mode, which will be the default if anything fails
    memset(imd->ascii_charset_translation, 0, sizeof(imd->ascii_charset_translation));
    for (i=0; i<256; i++)
        imd->ascii_charset_translation[i] = is_displayable(i) ? i : '.';
    imd->get_char = inputmode_ascii_get_char;
    imd->get_next_offset = inputmode_ascii_get_next_offset;
    imd->get_prev_offset = inputmode_ascii_get_previous_offset;
    g_free (imd->input_mode_name);
    imd->input_mode_name = g_strdup ("ASCII");

    if (g_ascii_strcasecmp(encoding, "ASCII")==0)
        return;

    /* Is this CP437 encoding ?
       If so, use our special translation table.
       (I could not get IConv to work with CP437....) */
    if (g_ascii_strcasecmp(encoding, "CP437")==0)
    {
        for (i=0;i<256;i++)
        {
            // these are defined in 'cp437.c'
            unsigned int unicode = ascii_cp437_to_unicode[i];
            unicode2utf8(unicode, (unsigned char*)&imd->ascii_charset_translation[i]);
        }
        g_free (imd->input_mode_name);
        imd->input_mode_name = g_strdup ("CP437");
        return;
    }


    /* If we got here, the user asked for ASCII input mode,
       with some special character encoding.
       Build the translation table for the current charset */
    icnv = g_iconv_open("UTF8", encoding);
    if (icnv == (GIConv)-1)
    {
        g_warning("Failed to load charset conversions, using ASCII fallback.");
        return;
    }
    for (i=0;i<256;i++)
    {
        gchar inbuf[2];
        unsigned char outbuf[5];

        gchar *ginbuf = (gchar *) inbuf;
        gchar *goutbuf = (gchar *) outbuf;
        gsize ginleft = 1;
        gsize goutleft = sizeof(outbuf);

        inbuf[0] = i;
        inbuf[1] = 0;

        memset(outbuf, 0, sizeof(outbuf));

        size_t result = g_iconv(icnv, &ginbuf, &ginleft, &goutbuf, &goutleft);
        if (result != 0 || i<32)
            imd->ascii_charset_translation[i] = '.';
        else
            imd->ascii_charset_translation[i] = outbuf[0] + (outbuf[1]<<8) + (outbuf[2]<<16) + (outbuf[3]<<24);
    }
    g_iconv_close(icnv);
    g_free (imd->input_mode_name);
    imd->input_mode_name = g_strdup (encoding);
}


static char_type inputmode_ascii_get_char(GVInputModesData *imd, offset_type offset)
{
    int value = gv_input_mode_get_byte(imd, offset);

    if (value<0)
        return INVALID_CHAR;

    if (value>255)
    {
        g_warning("Got BYTE>255 (%d) ?!\n", value);
        value = ' ';
    }

    /*
    There sepcial control characters are never translated.
    To get thier UTF8 displayable counterpart, use "gv_input_mode_byte_to_utf8".
    */
    if (value=='\r' || value=='\n' || value=='\t')
        return value;

    return imd->ascii_charset_translation[value];
}


char_type gv_input_mode_byte_to_utf8(GVInputModesData *imd, unsigned char data)
{
    g_return_val_if_fail (imd!=NULL, '.');

    return imd->ascii_charset_translation[data];
}


void gv_input_mode_update_utf8_translation(GVInputModesData *imd, unsigned char index, char_type new_value)
{
    g_return_if_fail (imd!=NULL);

    imd->ascii_charset_translation[index] = new_value;
}


static offset_type inputmode_ascii_get_previous_offset(GVInputModesData *imd, offset_type offset)
{
    char_type current_char, prev_char;

    if (offset>0)
        offset--;

    if (offset>0)
    {
        current_char = inputmode_ascii_get_char(imd, offset);
        if (current_char=='\n')
        {
            prev_char = inputmode_ascii_get_char(imd, offset-1);
            if (prev_char=='\r')
                offset--;
        }
    }

    return offset;
}


static offset_type inputmode_ascii_get_next_offset(GVInputModesData *imd, offset_type offset)
{
    char_type current_char, next_char;

    current_char = inputmode_ascii_get_char(imd, offset);
    if (current_char=='\r')
    {
        next_char = inputmode_ascii_get_char(imd, offset+1);
        if (next_char=='\n')
            offset++;
    }
    return offset+1;
}


/************************* UTF-8 input mode functions **************************/

static void inputmode_utf8_activate(GVInputModesData *imd)
{
    g_return_if_fail (imd!=NULL);

    imd->get_char = inputmode_utf8_get_char;
    imd->get_prev_offset = inputmode_utf8_get_previous_offset;
    imd->get_next_offset = inputmode_utf8_get_next_offset;
    g_free (imd->input_mode_name);
    imd->input_mode_name = g_strdup ("UTF8");
}

#define UTF8_SINGLE_CHAR(c) (((c)&0x80)==0)
#define UTF8_HEADER_CHAR(c) (((c)&0xC0)==0xC0)
#define UTF8_HEADER_2BYTES(c) (((c)&0xE0)==0xC0)
#define UTF8_HEADER_3BYTES(c) (((c)&0xF0)==0xE0)
#define UTF8_HEADER_4BYTES(c) (((c)&0xF8)==0xF0)
#define UTF8_TRAILER_CHAR(c) (((c)&0xC0)==0x80)
inline guint utf8_get_char_len(GVInputModesData *imd, offset_type offset)
{
    int value = gv_input_mode_get_byte(imd, offset);
    
    if (value<0 || value>255)  return 0;

    if (UTF8_SINGLE_CHAR(value))  return 1;

    if (UTF8_HEADER_CHAR(value))
    {
        if (UTF8_HEADER_2BYTES(value))
            return 2;
        if (UTF8_HEADER_3BYTES(value))
            return 3;
        if (UTF8_HEADER_4BYTES(value))
            return 4;
    }

    // fall back: this is an invalid UTF8 character
    return 0;
}


inline gboolean utf8_is_valid_char(GVInputModesData *imd, offset_type offset)
{
    int len = utf8_get_char_len(imd, offset);
    
    if (len==0 || (gv_input_mode_get_byte(imd, offset+len)==INVALID_CHAR))
        return FALSE;

    if (len==1)
        return TRUE;

    if (!UTF8_TRAILER_CHAR(gv_input_mode_get_byte(imd, offset+1)))
        return FALSE;

    if (len==2)
        return TRUE;

    if (!UTF8_TRAILER_CHAR(gv_input_mode_get_byte(imd, offset+2)))
        return FALSE;

    if (len==3)
        return TRUE;

    if (!UTF8_TRAILER_CHAR(gv_input_mode_get_byte(imd, offset+3)))
        return FALSE;

    if (len==4)
        return TRUE;

    return FALSE;
}


static char_type inputmode_utf8_get_char(GVInputModesData *imd, offset_type offset)
{
    int value;
    int len;

    value = gv_input_mode_get_byte(imd, offset);
    if (value<0)
        return INVALID_CHAR;

    if (!utf8_is_valid_char(imd, offset))
    {
        g_warning ("invalid UTF character at offset %lu (%02x)", offset,
            (unsigned char) gv_input_mode_get_byte(imd, offset));
        return '.';
    }

    len = utf8_get_char_len(imd, offset);

    if (len==1)
        return gv_input_mode_get_byte(imd, offset);

    if (len==2)
        return (char_type) gv_input_mode_get_byte(imd, offset) +
               (char_type) (gv_input_mode_get_byte(imd, offset+1)<<8);
    if (len==3)
        return (char_type) gv_input_mode_get_byte(imd, offset) +
               (char_type) (gv_input_mode_get_byte(imd, offset+1)<<8)+
               (char_type) (gv_input_mode_get_byte(imd, offset+2)<<16);
    if (len==4)
        return (char_type) gv_input_mode_get_byte(imd, offset) +
               (char_type) (gv_input_mode_get_byte(imd, offset+1)<<8)+
               (char_type) (gv_input_mode_get_byte(imd, offset+2)<<16)+
               (char_type) (gv_input_mode_get_byte(imd, offset+3)<<24);

    return -1;
}


static offset_type inputmode_utf8_get_previous_offset(GVInputModesData *imd, offset_type offset)
{
    if (offset==0)
        return 0;

    if (offset>0 && utf8_is_valid_char(imd, offset-1))
        return offset-1;

    if (offset>1 && utf8_is_valid_char(imd, offset-2))
        return offset-2;

    if (offset>2 && utf8_is_valid_char(imd, offset-3))
        return offset-3;

    if (offset>3 && utf8_is_valid_char(imd, offset-4))
        return offset-4;

    return offset-1;
}


static offset_type inputmode_utf8_get_next_offset(GVInputModesData *imd, offset_type offset)
{
    if (!utf8_is_valid_char(imd, offset))
        return offset+1;

    int len = utf8_get_char_len(imd, offset);
    if (len==0)
        len=1;

    return offset+len;
}
