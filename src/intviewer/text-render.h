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

#ifndef __TEXT_RENDER_H__
#define __TEXT_RENDER_H__

#define TEXT_RENDER(obj)          GTK_CHECK_CAST (obj, text_render_get_type (), TextRender)
#define TEXT_RENDER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, text_render_get_type (), TextRenderClass)
#define IS_TEXT_RENDER(obj)       GTK_CHECK_TYPE (obj, text_render_get_type ())

#define TEXT_RENDER_DEFAULT_WIDTH   (100)
#define TEXT_RENDER_DEFAULT_HEIGHT  (200)

struct TextRenderPrivate;
struct TextRenderStatus;

struct TextRender
{
    GtkWidget widget;
    TextRenderPrivate *priv;
};

struct TextRenderClass
{
    GtkWidgetClass parent_class;

    void (* text_status_changed)  (TextRender *obj, TextRenderStatus *status);
};

enum TEXTDISPLAYMODE
{
    TR_DISP_MODE_TEXT,
    TR_DISP_MODE_BINARY,
    TR_DISP_MODE_HEXDUMP
};

struct TextRenderStatus
{
    offset_type current_offset;
    offset_type size;
    int         column;
    const char *encoding;
    gboolean    wrap_mode;
};


GtkWidget     *text_render_new ();
GtkType        text_render_get_type ();

GtkAdjustment *text_render_get_h_adjustment (TextRender *obj);
void           text_render_set_h_adjustment (TextRender *obj, GtkAdjustment *adjustment);
GtkAdjustment *text_render_get_v_adjustment (TextRender *obj);
void           text_render_set_v_adjustment (TextRender *obj, GtkAdjustment *adjustment);

void           text_render_attach_external_v_range(TextRender *obj, GtkRange *range);

void           text_render_load_file(TextRender *w, const gchar *filename);
void           text_render_load_filedesc(TextRender *w, int filedesc);

void           text_render_notify_status_changed(TextRender *w);

void           text_render_set_display_mode (TextRender *w, TEXTDISPLAYMODE mode);
TEXTDISPLAYMODE text_render_get_display_mode(TextRender *w);

ViewerFileOps      *text_render_get_file_ops(TextRender *w);
GVInputModesData   *text_render_get_input_mode_data(TextRender *w);
GVDataPresentation *text_render_get_data_presentation(TextRender *w);

void        text_render_set_tab_size(TextRender *w, int tab_size);
int         text_render_get_tab_size(TextRender *w);

void        text_render_set_wrap_mode(TextRender *w, gboolean ACTIVE);
gboolean    text_render_get_wrap_mode(TextRender *w);

void        text_render_set_fixed_limit(TextRender *w, int fixed_limit);
int         text_render_get_fixed_limit(TextRender *w);

void        text_render_set_hex_offset_display(TextRender *w, gboolean HEX_OFFSET);
gboolean    text_render_get_hex_offset_display(TextRender *w);

void        text_render_set_font_size(TextRender *w, int font_size);
int         text_render_get_font_size(TextRender *w);

void        text_render_set_encoding(TextRender *w, const char *encoding);
const gchar *text_render_get_encoding(TextRender *w);

void        text_render_copy_selection(TextRender *w);

offset_type text_render_get_current_offset(TextRender *w);

offset_type text_render_get_last_displayed_offset(TextRender *w);

void        text_render_ensure_offset_visible(TextRender *w, offset_type offset);

void        text_render_set_marker(TextRender *w, offset_type start, offset_type end);

#endif /* __TEXT_RENDER_H__ */
