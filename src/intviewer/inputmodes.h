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

#ifndef __LIBGVIEWER_INPUT_MODES_H__
#define __LIBGVIEWER_INPUT_MODES_H__

#define is_displayable(c) (((c) >= 0x20) && ((c) < 0x7f))

struct GVInputModesData;

/* input function types */
typedef char_type (*input_get_char_proc)(GVInputModesData *imd, offset_type offset);
typedef offset_type (*input_get_offset_proc)(GVInputModesData *imd, offset_type offset);


/*
  This function will be used by the input functions to retrive bytes from the input file.
  Should return -1 for failure (or EOF),
    and 0->255 value for success
*/
typedef int (*get_byte_proc)(void *user_data, offset_type offset);


GVInputModesData *gv_input_modes_new();

/*
  Initializes internal state of the input mode functions.
  If an input mode functino requires the scanning of the entire file - this is were it happens.
  (hopefully this will not happen).

  Also activates the default ASCII input mode, without any character encodings
*/
void gv_init_input_modes(GVInputModesData *imd, get_byte_proc proc, void *get_byte_user_data);

/*
   Free any internal data used by the input mode translators
*/
void gv_free_input_modes(GVInputModesData *imd);

/*
    returns the current input mode.
    ASCII,
    UTF-8,
    Other possible character encodings (such as "CP-1255", "ISO8859-8" etc.)

    or (in the future)
    HTML, PS, PDF, UNICODE etc.

    This string can be saved to the user preferences, and fed back to "gc_set_input_mode".
*/
const char*gv_get_input_mode(GVInputModesData *imd);

/*
    Sets a new input mode.
    If this is an invalid input mode (e.g. invalid character encoding),
    Use ASCII as a fallback
*/
void gv_set_input_mode(GVInputModesData *imd, const gchar *input_mode);

/*
    returns a UTF-8 character in the specified offset.

    'offset' is ALWAYS BYTE OFFSET IN THE FILE. never logical offset.

    Implementors note:
     you must handle gracefully an 'offset' which is not on a character alignemnt.
     (e.g. in the second byte of a UTF-8 character)
*/
char_type gv_input_mode_get_utf8_char(GVInputModesData *imd, offset_type offset);

/*
    Special hack:
    Control Characters (\r \n \t) are NOT translated by 'gv_input_mode_get_utf8_char, ever.
    But higher levels that want to display them, can use this function to get a UTF8 displayable character. */
char_type gv_input_mode_byte_to_utf8(GVInputModesData *imd, unsigned char data);

/*
  Used by highler layers (text-render) to update the translation table,
filter out utf8 characters that IConv returned but Pango can't display
*/
void gv_input_mode_update_utf8_translation(GVInputModesData *imd, unsigned char index, char_type new_value);

/*
    returns the RAW Byte at 'offset'.
    Does no input mode translations.

    returns -1 (INVALID_CHAR) on failure or EOF.
*/
int gv_input_mode_get_raw_byte(GVInputModesData *imd, offset_type offset);

/*
    returns the BYTE offset of the next logical character.

    For ASCII input mode, each character is one byte, so the function only increments offset by 1.
    For UTF-8 input mode, a character is 1 to 6 bytes.
    Other input modes can return diferent values.
*/
offset_type gv_input_get_next_char_offset(GVInputModesData *imd, offset_type current_offset);

/*
    returns the BYTE offset of the previous logical character.

*/
offset_type gv_input_get_previous_char_offset(GVInputModesData *imd, offset_type current_offset);

#endif // __LIBGVIEWER_INPUT_MODES_H__
