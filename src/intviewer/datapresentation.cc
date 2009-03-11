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

/*
 TODO:    change "eol" to "next_line_offset"
*/

#include <config.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "gvtypes.h"

#include "inputmodes.h"
#include "datapresentation.h"

using namespace std;


typedef offset_type (*align_offset_to_line_start_proc)(GVDataPresentation *dp, offset_type offset);
typedef offset_type (*scroll_lines_proc)(GVDataPresentation *dp, offset_type current_offset, int delta);
typedef offset_type (*get_end_of_line_offset_proc)(GVDataPresentation *dp, offset_type start_of_line);

struct GVDataPresentation
{
    GVInputModesData *imd;
    guint wrap_limit;
    guint fixed_count;
    offset_type max_offset;
    guint tab_size;

    PRESENTATION presentation_mode;

    align_offset_to_line_start_proc align_offset_to_line_start;
    scroll_lines_proc               scroll_lines;
    get_end_of_line_offset_proc     get_end_of_line_offset;
};

static offset_type nowrap_align_offset(GVDataPresentation *dp, offset_type offset);
static offset_type nowrap_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta);
static offset_type nowrap_get_eol(GVDataPresentation *dp, offset_type start_of_line);

static offset_type wrap_align_offset(GVDataPresentation *dp, offset_type offset);
static offset_type wrap_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta);
static offset_type wrap_get_eol(GVDataPresentation *dp, offset_type start_of_line);

static offset_type binfixed_align_offset(GVDataPresentation *dp, offset_type offset);
static offset_type binfixed_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta);
static offset_type binfixed_get_eol(GVDataPresentation *dp, offset_type start_of_line);


/*********************************************************
   Data presentation public functions
*********************************************************/
GVDataPresentation *gv_data_presentation_new()
{
    return g_new0(GVDataPresentation, 1);
}


void gv_init_data_presentation(GVDataPresentation *dp, GVInputModesData *imd, offset_type max_offset)
{
    g_return_if_fail (dp!=NULL);
    g_return_if_fail (imd!=NULL);

    memset(dp, 0, sizeof(GVDataPresentation));
    dp->imd = imd;
    dp->max_offset = max_offset;
    dp->tab_size = 8;

    gv_set_data_presentation_mode(dp, PRSNT_NO_WRAP);
}


void gv_free_data_presentation(GVDataPresentation *dp)
{
}


void gv_set_data_presentation_mode(GVDataPresentation *dp, PRESENTATION present)
{
    g_return_if_fail (dp!=NULL);
    dp->presentation_mode = present;

    switch (present)
    {
        case PRSNT_NO_WRAP:
            dp->align_offset_to_line_start = nowrap_align_offset;
            dp->scroll_lines = nowrap_scroll_lines;
            dp->get_end_of_line_offset = nowrap_get_eol;
            break;

        case PRSNT_WRAP:
            dp->align_offset_to_line_start = wrap_align_offset;
            dp->scroll_lines = wrap_scroll_lines;
            dp->get_end_of_line_offset = wrap_get_eol;
            break;

        case PRSNT_BIN_FIXED:
            dp->align_offset_to_line_start = binfixed_align_offset;
            dp->scroll_lines = binfixed_scroll_lines;
            dp->get_end_of_line_offset = binfixed_get_eol;
            break;
    }
}


PRESENTATION gv_get_data_presentation_mode(GVDataPresentation *dp)
{
    g_return_val_if_fail (dp!=NULL, PRSNT_NO_WRAP);
    return dp->presentation_mode;
}


void gv_set_tab_size(GVDataPresentation *dp, guint tab_size)
{
    g_return_if_fail (dp!=NULL);
    dp->tab_size = tab_size;
}


void gv_set_fixed_count(GVDataPresentation *dp, guint chars_per_line)
{
    g_return_if_fail (dp!=NULL);
    dp->fixed_count = chars_per_line;
}


void gv_set_wrap_limit(GVDataPresentation *dp, guint chars_per_line)
{
    g_return_if_fail (dp!=NULL);
    dp->wrap_limit= chars_per_line;
}


offset_type gv_align_offset_to_line_start(GVDataPresentation *dp, offset_type offset)
{
    g_return_val_if_fail (dp!=NULL, 0);
    g_return_val_if_fail (dp->align_offset_to_line_start!=NULL, 0);
    return dp->align_offset_to_line_start(dp, offset);
}


offset_type gv_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta)
{
    g_return_val_if_fail (dp!=NULL, 0);
    g_return_val_if_fail (dp->scroll_lines!=NULL, 0);
    return dp->scroll_lines(dp, current_offset, delta);
}


offset_type gv_get_end_of_line_offset(GVDataPresentation *dp, offset_type start_of_line)
{
    g_return_val_if_fail (dp!=NULL, 0);
    g_return_val_if_fail (dp->get_end_of_line_offset!=NULL, 0);
    return dp->get_end_of_line_offset(dp, start_of_line);
}


/***********************************************************************
  Data presentation specific implementations
***********************************************************************/
/*
 scans the file from offset "start" backwards, until a CR/LF is found.
 returns the offset of the previous CR/LF, or 0 (if we've reached the start of the file)
*/
static offset_type find_previous_crlf(GVDataPresentation *dp, offset_type start)
{
    offset_type offset = start;

    while (TRUE)
    {
        if (offset<=0)
            return 0;

        offset = gv_input_get_previous_char_offset(dp->imd, offset);
        char_type value = gv_input_mode_get_utf8_char(dp->imd, offset);

        if (value==INVALID_CHAR)
            break;

        // break upon end of line
        if (value=='\n' || value=='\r')
            break;
    }

    return offset;
}


static offset_type nowrap_align_offset(GVDataPresentation *dp, offset_type offset)
{
    while (offset>0)
    {
        char_type value = gv_input_mode_get_utf8_char(dp->imd, offset);
        if (value==INVALID_CHAR)
            return 0;
        if (value=='\r' || value=='\n')
            break;
        offset = gv_input_get_previous_char_offset(dp->imd, offset);
    }
    if (offset>0)
        return gv_input_get_next_char_offset(dp->imd, offset);
    return 0;
}


static offset_type nowrap_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta)
{
    gboolean forward = TRUE;

    if (delta==0)
        return current_offset;

    if (delta<0)
    {
        delta = abs(delta);
        forward = FALSE;
    }
    while (delta--)
    {
        offset_type temp;

        if (forward)
            temp = nowrap_get_eol(dp, current_offset);
        else
        {
            // We need TWO CR/LF. this is not an error
            temp = find_previous_crlf(dp, current_offset);
            temp = find_previous_crlf(dp, temp);
            if (temp>0)
                temp = gv_input_get_next_char_offset(dp->imd, temp);
        }

        // Offset didn't changed ? we've reached eof
        if (temp==current_offset)
            break;

        current_offset = temp;
    }

    return current_offset;
}


static offset_type nowrap_get_eol(GVDataPresentation *dp, offset_type start_of_line)
{
    offset_type offset = start_of_line;

    while (TRUE)
    {
        char_type value = gv_input_mode_get_utf8_char(dp->imd, offset);

        if (value==INVALID_CHAR)
            break;

        offset = gv_input_get_next_char_offset(dp->imd, offset);

        // break upon end of line
        if (value=='\n' || value=='\r')
            break;
    }

    return offset;
}


/*
    returns the start offset of the previous line,
    with special handling for wrap mode.
*/
static offset_type find_previous_wrapped_text_line(GVDataPresentation *dp, offset_type start)
{
    offset_type offset = start;

    /* step 1:
        find TWO previous CR/LF = start offset of previous text line
    */
    offset = find_previous_crlf(dp, offset);
    offset = find_previous_crlf(dp, offset);
    if (offset>0)
        offset = gv_input_get_next_char_offset(dp->imd, offset);

    /* Step 2
    */

    while (TRUE)
    {
        offset_type next_line_offset = wrap_get_eol (dp, offset);

        // this is the line we want: When the next line's offset is the current
        // offset ('start' parameter), 'offset' will point to the previous
        // displayable line

        if (next_line_offset>=start)
            return offset;

        offset = next_line_offset;
    }

    return 0;  // should never get here
}


static offset_type wrap_align_offset(GVDataPresentation *dp, offset_type offset)
{
    offset_type line_start = nowrap_align_offset(dp, offset);

    for (offset_type temp=line_start; temp<=offset; temp=wrap_scroll_lines(dp, temp, 1))
        line_start = temp;

    return line_start;
}


static offset_type wrap_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta)
{
    gboolean forward = TRUE;

    if (delta==0)
        return current_offset;

    if (delta<0)
    {
        delta = abs(delta);
        forward = FALSE;
    }
    while (delta--)
    {
        offset_type temp;

        if (forward)
            temp = wrap_get_eol(dp, current_offset);
        else
            temp = find_previous_wrapped_text_line(dp, current_offset);


        // Offset didn't changed ? we've reached eof
        if (temp==current_offset)
            break;

        current_offset = temp;
    }

    return current_offset;
}


static offset_type wrap_get_eol(GVDataPresentation *dp, offset_type start_of_line)
{
    offset_type offset;
    char_type value;

    /* A Single TAB character in the file,
       Translates to several displayable characters on the screen.
       We need to take that into account when calculating number of
       characters before wraping the line */
    guint char_count = 0;

    offset = start_of_line;

    while (TRUE)
    {
        value = gv_input_mode_get_utf8_char(dp->imd, offset);

        if (value==INVALID_CHAR)
            break;

        offset = gv_input_get_next_char_offset(dp->imd, offset);

        // break upon end of line
        if (value=='\n' || value=='\r')
            break;

        if (value=='\t')
            char_count += dp->tab_size;
        else
            char_count++;

        if (char_count >= dp->wrap_limit)
            break;
    }

    return offset;
}


static offset_type binfixed_align_offset(GVDataPresentation *dp, offset_type offset)
{
    offset_type o;

    g_return_val_if_fail (dp->fixed_count>0, offset);

    if (offset > dp->max_offset)
        offset = dp->max_offset;
    o = ((offset_type)(offset / dp->fixed_count) * dp->fixed_count);

    return o;
}


static offset_type binfixed_scroll_lines(GVDataPresentation *dp, offset_type current_offset, int delta)
{
    g_return_val_if_fail (dp->fixed_count>0, current_offset);

    if (delta > 0)
    {
        if (current_offset + delta * dp->fixed_count > dp->max_offset)
            delta = (dp->max_offset-current_offset) / dp->fixed_count;

        return delta * dp->fixed_count + current_offset;
    }
    else
    {
        delta = abs(delta);

        if (delta * dp->fixed_count > current_offset)
            return 0;

        return current_offset - delta * dp->fixed_count;
    }
}


static offset_type binfixed_get_eol(GVDataPresentation *dp, offset_type start_of_line)
{
    g_return_val_if_fail (dp->fixed_count>0, start_of_line);

    if (start_of_line + dp->fixed_count > dp->max_offset)
        return dp->max_offset;

    return start_of_line + dp->fixed_count;
}
