/**
 * @file text-render.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
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

#include <config.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "gvtypes.h"
#include "fileops.h"
#include "inputmodes.h"
#include "datapresentation.h"
#include "text-render.h"

using namespace std;


#define HEXDUMP_FIXED_LIMIT              16
#define MAX_CLIPBOARD_COPY_LENGTH  0xFFFFFF

#define NEED_PANGO_ESCAPING(x) ((x)=='<' || (x)=='>' || (x)=='&')


// Class Private Data
struct TextRenderPrivate
{
    PangoLayout *layout;

    unsigned char *utf8buf;
    int           utf8alloc;
    int           utf8buf_length;

    gboolean hexmode_marker_on_hexdump;
};


extern "C" void text_render_update_adjustments_limits(TextRender *w);
static void text_render_reserve_utf8buf(TextRender *w, int minlength);

static void text_render_utf8_clear_buf(TextRender *w);
static int text_render_utf8_printf (TextRender *w, const char *format, ...);
static int text_render_utf8_print_char(TextRender *w, char_type value);

extern "C" void text_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset);
extern "C" void text_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line, offset_type marker_start, offset_type marker_end);
extern "C" offset_type text_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker);

extern "C" void binary_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line, offset_type marker_start, offset_type marker_end);

extern "C" void hex_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line, offset_type marker_start, offset_type marker_end);
extern "C" void hex_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset);
extern "C" offset_type hex_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker);

extern "C" PangoFontDescription *text_render_get_font_description(TextRender *w);
extern "C" gint text_render_get_char_width(TextRender *w);
extern "C" gint text_render_get_char_height(TextRender *w);


static TextRenderPrivate* text_render_priv (TextRender *w)
{
    return static_cast<TextRenderPrivate*>(g_object_get_data (G_OBJECT (w), "priv"));
}


extern "C" void text_render_init (TextRender *w)
{
    auto priv = g_new0 (TextRenderPrivate, 1);
    g_object_set_data_full (G_OBJECT (w), "priv", priv, g_free);

    priv->utf8alloc = 0;

    priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (w), NULL);
}


extern "C" void text_render_finalize (TextRender *w)
{
    auto priv = text_render_priv (w);
    g_clear_pointer (&priv->utf8buf, g_free);
}


extern "C" void text_render_filter_undisplayable_chars(TextRender *obj)
{
    auto priv = text_render_priv (obj);
    auto im = text_render_get_input_mode_data (obj);

    if (!im)
        return;

    PangoRectangle logical_rect;

    PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (obj), "");
    pango_layout_set_font_description (layout, text_render_get_font_description (obj));

    for (guint i=0; i<256; i++)
    {
        char_type value = gv_input_mode_byte_to_utf8(im, (unsigned char) i);
        text_render_utf8_clear_buf(obj);
        text_render_utf8_print_char(obj, value);
        pango_layout_set_text(layout, (char *) priv->utf8buf, priv->utf8buf_length);
        pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
#if 0
        printf("char (%03d, %02x), utf8buf_len(%d) utf8((%02x %02x %02x %02x), width = %d\n",
               i, i,
               obj->priv->utf8buf_length,
               obj->priv->utf8buf[0],
               obj->priv->utf8buf[1],
               obj->priv->utf8buf[2],
               obj->priv->utf8buf[3],
               logical_rect.width);
#endif

        // Pango can't display this UTF8 character, so filter it out
        if (logical_rect.width==0)
            gv_input_mode_update_utf8_translation(im, i, '.');
    }

    g_object_unref (layout);
}


static void text_render_reserve_utf8buf(TextRender *w, int minlength)
{
    auto priv = text_render_priv (w);

    if (priv->utf8alloc < minlength)
    {
        priv->utf8alloc = minlength*2;
        priv->utf8buf = (unsigned char*) g_realloc (priv->utf8buf, priv->utf8alloc);
    }
}


static const char *escape_pango_char(char_type ch)
{
    switch (ch)
    {
        case '<':
            return "&lt;";
        case '>':
            return "&gt;";
        case '&':
            return "&amp;";
        default:
            return "";
    }
}


static void text_render_utf8_clear_buf(TextRender *w)
{
    auto priv = text_render_priv (w);

    priv->utf8buf_length = 0;
}


static int text_render_utf8_printf (TextRender *w, const char *format, ...)
{
    auto priv = text_render_priv (w);

    va_list ap;
    int new_length;

    text_render_reserve_utf8buf(w, priv->utf8buf_length+101);

    va_start(ap, format);
    new_length = vsnprintf((char *) &priv->utf8buf[priv->utf8buf_length], 100, format, ap);
    va_end(ap);

    priv->utf8buf_length += new_length;
    return priv->utf8buf_length;
}


static int text_render_utf8_print_char(TextRender *w, char_type value)
{
    auto priv = text_render_priv (w);

    int current_length = priv->utf8buf_length;

    text_render_reserve_utf8buf(w, current_length+4);
    priv->utf8buf[current_length++] = GV_FIRST_BYTE(value);
    if (GV_SECOND_BYTE(value))
    {
        priv->utf8buf[current_length++] = GV_SECOND_BYTE(value);
        if (GV_THIRD_BYTE(value))
        {
            priv->utf8buf[current_length++] = GV_THIRD_BYTE(value);
            if (GV_FOURTH_BYTE(value))
                priv->utf8buf[current_length++] = GV_FOURTH_BYTE(value);
        }
    }

    priv->utf8buf_length = current_length;
    return current_length;
}


offset_type text_render_get_current_offset(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);

    GtkAdjustment *v_adjustment;
    g_object_get (w, "vadjustment", &v_adjustment, nullptr);
    return v_adjustment ? (offset_type) gtk_adjustment_get_value (v_adjustment) : 0;
}


int text_render_get_column(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);

    GtkAdjustment *h_adjustment;
    g_object_get (w, "hadjustment", &h_adjustment, nullptr);
    return h_adjustment ? (int) gtk_adjustment_get_value (h_adjustment) : 0;
}


/******************************************************
 Display mode specific functions
******************************************************/


static gboolean marker_helper(TextRender *w, gboolean marker_shown, offset_type current, offset_type marker_start, offset_type marker_end)
{
    g_return_val_if_fail (w!=NULL, FALSE);

    if (!marker_shown)
    {
        if (current >= marker_start && current<marker_end)
        {
            marker_shown = TRUE;
            text_render_utf8_printf (w, "<span background=\"blue\">");
        }
    }
    else
        if (current >= marker_end)
        {
            marker_shown = FALSE;
            text_render_utf8_printf (w, "</span>");
        }


    return marker_shown;
}


static gboolean hex_marker_helper(TextRender *w, gboolean marker_shown, offset_type current, offset_type marker_start, offset_type marker_end, gboolean primary_color)
{
    g_return_val_if_fail (w!=NULL, FALSE);

    if (!marker_shown)
    {
        if (current >= marker_start && current<marker_end)
        {
            marker_shown = TRUE;
            text_render_utf8_printf (w, "<span %s=\"blue\">", primary_color?"background":"foreground");
        }
    }
    else
        if (current >= marker_end)
        {
            marker_shown = FALSE;
            text_render_utf8_printf (w, "</span>");
        }


    return marker_shown;
}


static void marker_closer (TextRender *w, gboolean marker_shown)
{
    g_return_if_fail (w!=NULL);

    if (marker_shown)
        text_render_utf8_printf (w, "</span>");
}


offset_type text_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker)
{
    g_return_val_if_fail (obj!=NULL, 0);
    auto priv = text_render_priv (obj);
    auto dp = text_render_get_data_presentation (obj);
    g_return_val_if_fail (dp != NULL, 0);
    auto im = text_render_get_input_mode_data (obj);

    auto char_width = text_render_get_char_width (obj);
    auto char_height = text_render_get_char_height (obj);

    guint tab_size;
    g_object_get (obj, "tab-size", &tab_size, nullptr);

    int line = 0;
    int column = 0;
    offset_type current_offset = text_render_get_current_offset (obj);
    offset_type offset;
    offset_type next_line_offset;
    char_type choff; // character at offset
    int choffcol = 0; // last column occupied by choff

    if (x<0)
        x = 0;
    if (y<0)
        return current_offset;

    if (char_height<=0)
        return current_offset;

    if (char_width<=0)
        return current_offset;

    line = y / char_height;
    column = x / char_width + text_render_get_column (obj);

    // Determine offset corresponding to start of line, the character at this offset and the last column occupied by character
    offset = gv_scroll_lines (dp, current_offset, line);
    choff = gv_input_mode_get_utf8_char(im, offset);
    choffcol = (choff=='\t') ? tab_size-1 : 0;

    next_line_offset = gv_scroll_lines (dp, offset, 1);

    // While the current character does not occupy column 'column', check next character
    while (column>choffcol && offset<next_line_offset)
    {
        offset = gv_input_get_next_char_offset(im, offset);
        choff = gv_input_mode_get_utf8_char(im, offset);
        choffcol += (choff=='\t') ? tab_size : 1;
    }

    // Increment offset if doing end-marker
    if (!start_marker)
        offset++;

    return offset;
}


void text_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (start_offset!=end_offset);
    auto priv = text_render_priv (obj);
    auto dp = text_render_get_data_presentation (obj);
    g_return_if_fail (dp!=NULL);
    auto im = text_render_get_input_mode_data (obj);
    g_return_if_fail (im != NULL);

    GdkClipboard *clip = gtk_widget_get_clipboard (GTK_WIDGET (obj));
    g_return_if_fail (clip!=NULL);

    text_render_utf8_clear_buf(obj);
    offset_type current = start_offset;
    while (current < end_offset && priv->utf8buf_length<MAX_CLIPBOARD_COPY_LENGTH)
    {
        char_type value = gv_input_mode_get_utf8_char(im, current);
        if (value==INVALID_CHAR)
            break;

        current = gv_input_get_next_char_offset(im, current);
        text_render_utf8_print_char(obj, value);
    }

    gdk_clipboard_set_text (clip, (const gchar *) priv->utf8buf);
}


void text_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line, offset_type marker_start, offset_type marker_end)
{
    auto priv = text_render_priv (w);

    auto im = text_render_get_input_mode_data (w);
    auto char_width = text_render_get_char_width (w);

    offset_type current;
    char_type value;
    int char_count = 0;
    gboolean show_marker;
    gboolean marker_shown = FALSE;

    gboolean wrap_mode;
    guint tab_size;
    guint max_column;
    g_object_get (w,
        "wrap-mode", &wrap_mode,
        "tab-size", &tab_size,
        "max-column", &max_column,
        nullptr);

    show_marker = marker_start!=marker_end;

    if (wrap_mode)
        column = 0;

    text_render_utf8_clear_buf(w);

    current = start_of_line;
    while (current < end_of_line)
    {
        if (show_marker)
            marker_shown = marker_helper(w, marker_shown, current, marker_start, marker_end);

        // Read a UTF8 character from the input file. The "inputmode" module is responsible for converting the file into UTF8
        value = gv_input_mode_get_utf8_char(im, current);
        if (value==INVALID_CHAR)
            break;

        // move to the next character's offset
        current = gv_input_get_next_char_offset(im, current);

        if (value=='\r' || value=='\n')
            continue;

        if (value=='\t')
        {
            for (int i=0; i<tab_size; i++)
                text_render_utf8_print_char(w, ' ');
            char_count += tab_size;
            continue;
        }

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf (w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);

        char_count++;
    }

    if (char_count > max_column)
    {
        g_object_set (w, "max-column", char_count, nullptr);
        text_render_update_adjustments_limits(w);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_font_description (priv->layout, text_render_get_font_description (w));
    pango_layout_set_markup (priv->layout, (gchar *) priv->utf8buf, priv->utf8buf_length);
    graphene_point_t pt = { .x = -(char_width*column), .y = 0 };
    gtk_snapshot_translate (snapshot, &pt);
    GdkRGBA color = { .red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0 };
    gtk_snapshot_append_layout (snapshot, priv->layout, &color);
}


void binary_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line, offset_type marker_start, offset_type marker_end)
{
    auto priv = text_render_priv (w);

    auto im = text_render_get_input_mode_data (w);
    auto char_width = text_render_get_char_width (w);

    offset_type current;
    char_type value;
    gboolean show_marker;
    gboolean marker_shown = FALSE;

    show_marker = marker_start!=marker_end;
    text_render_utf8_clear_buf(w);

    current = start_of_line;
    while (current < end_of_line)
    {

        if (show_marker)
            marker_shown = marker_helper(w, marker_shown, current, marker_start, marker_end);

        /* Read a UTF8 character from the input file.
           The "inputmode" module is responsible for converting the file into UTF8 */
        value = gv_input_mode_get_utf8_char(im, current);
        if (value==INVALID_CHAR)
            break;

        // move to the next character's offset
        current = gv_input_get_next_char_offset(im, current);

        if (value=='\r' || value=='\n' || value=='\t')
            value = gv_input_mode_byte_to_utf8(im, (unsigned char)value);

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf (w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_font_description (priv->layout, text_render_get_font_description (w));
    pango_layout_set_markup (priv->layout, (gchar *) priv->utf8buf, priv->utf8buf_length);
    graphene_point_t pt = { .x = -(char_width*column), .y = 0 };
    gtk_snapshot_translate (snapshot, &pt);
    GdkRGBA color = { .red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0 };
    gtk_snapshot_append_layout (snapshot, priv->layout, &color);
}


offset_type hex_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker)
{
    g_return_val_if_fail (obj!=NULL, 0);
    auto priv = text_render_priv (obj);
    auto dp = text_render_get_data_presentation (obj);
    g_return_val_if_fail (dp != NULL, 0);
    auto im = text_render_get_input_mode_data (obj);

    int line = 0;
    int column = 0;
    offset_type current_offset = text_render_get_current_offset (obj);
    offset_type offset;
    offset_type next_line_offset;

    if (x<0)
        x = 0;
    if (y<0)
        return current_offset;

    auto char_width = text_render_get_char_width (obj);
    auto char_height = text_render_get_char_height (obj);

    if (char_height<=0)
        return current_offset;

    if (char_width<=0)
        return current_offset;

    line = y / char_height;
    column = x / char_width;

    offset = gv_scroll_lines (dp, current_offset, line);
    next_line_offset = gv_scroll_lines (dp, offset, 1);

    if (column<10)
        return offset;

    if (start_marker)
    {
        // the first 10 characters are the offset number
        priv->hexmode_marker_on_hexdump = TRUE;

        if (column<10+16*3)
            column = (column-10)/3; // the user selected the hex dump portion
        else
        {
            // the user selected the ascii portion
            column = column-10-16*3;
            priv->hexmode_marker_on_hexdump = FALSE;
        }
    }
    else
    {
        if (priv->hexmode_marker_on_hexdump)
        {
            if (column<10+16*3)
                column = (column-10)/3; // the user selected the hex dump portion
            else
                column = HEXDUMP_FIXED_LIMIT;
        }
        else
        {
            if (column<10+16*3)
                return offset;
            column = column-10-16*3;
        }
    }

    while (column>0 && offset<next_line_offset)
    {
        offset = gv_input_get_next_char_offset(im, offset);
        column--;
    }

    return offset;
}


void hex_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (start_offset!=end_offset);
    auto priv = text_render_priv (obj);
    auto dp = text_render_get_data_presentation (obj);
    g_return_if_fail (dp != NULL);
    auto im = text_render_get_input_mode_data (obj);
    g_return_if_fail (im != NULL);

    if (!priv->hexmode_marker_on_hexdump)
    {
        text_mode_copy_to_clipboard(obj, start_offset, end_offset);
        return;
    }

    GdkClipboard *clip = gtk_widget_get_clipboard (GTK_WIDGET (obj));
    g_return_if_fail (clip!=NULL);

    text_render_utf8_clear_buf(obj);

    for (offset_type current = start_offset; current < end_offset && priv->utf8buf_length<MAX_CLIPBOARD_COPY_LENGTH; current++)
    {
        char_type value = gv_input_mode_get_raw_byte(im, current);
        if (value==INVALID_CHAR)
            break;
        text_render_utf8_printf (obj, "%02x ", (unsigned char) value);
    }

    gdk_clipboard_set_text (clip, (const gchar *) priv->utf8buf);
}


void hex_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line, offset_type marker_start, offset_type marker_end)
{
    auto priv = text_render_priv (w);
    auto im = text_render_get_input_mode_data (w);

    gboolean hexadecimal_offset;
    g_object_get (w,
        "hexadecimal-offset", &hexadecimal_offset,
        nullptr);

    gboolean marker_shown = FALSE;
    gboolean show_marker = marker_start!=marker_end;
    text_render_utf8_clear_buf(w);

    if (hexadecimal_offset)
        text_render_utf8_printf (w, "%08lx  ", (unsigned long)start_of_line);
    else
        text_render_utf8_printf (w, "%09lu ", (unsigned long)start_of_line);

    int bytes_printed = 0;
    for (offset_type current=start_of_line; current<end_of_line; ++current)
    {
        if (show_marker)
        {
            marker_shown = hex_marker_helper(w, marker_shown,
                current, marker_start, marker_end,
                priv->hexmode_marker_on_hexdump);
        }

        int byte_value = gv_input_mode_get_raw_byte(im, current);

        if (byte_value==-1)
            break;

        text_render_utf8_printf (w, "%02x ", (unsigned char) byte_value);
        ++bytes_printed;
    }
    for (; bytes_printed < HEXDUMP_FIXED_LIMIT; ++bytes_printed)
        text_render_utf8_printf (w, "   ");

    if (show_marker)
        marker_closer(w, marker_shown);

    marker_shown = FALSE;

    for (offset_type current=start_of_line; current<end_of_line; ++current)
    {
        if (show_marker)
        {
            marker_shown = hex_marker_helper(w, marker_shown,
                current, marker_start, marker_end,
                !priv->hexmode_marker_on_hexdump);
        }

        int byte_value = gv_input_mode_get_raw_byte(im, current);

        if (byte_value==-1)
            break;

        char_type value = gv_input_mode_byte_to_utf8(im, (unsigned char) byte_value);

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf (w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_font_description (priv->layout, text_render_get_font_description (w));
    pango_layout_set_markup (priv->layout, (gchar *) priv->utf8buf, priv->utf8buf_length);
    GdkRGBA color = { .red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0 };
    gtk_snapshot_append_layout (snapshot, priv->layout, &color);
}
