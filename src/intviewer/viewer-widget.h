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

#ifndef __GVIEWER_H__
#define __GVIEWER_H__

#define GVIEWER(obj)          GTK_CHECK_CAST (obj, gviewer_get_type (), GViewer)
#define GVIEWER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gviewer_get_type (), GViewerClass)
#define IS_GVIEWER(obj)       GTK_CHECK_TYPE (obj, gviewer_get_type ())

struct GViewerPrivate;

 enum VIEWERDISPLAYMODE
{
    DISP_MODE_TEXT_FIXED,
    DISP_MODE_BINARY,
    DISP_MODE_HEXDUMP,
    DISP_MODE_IMAGE
};

struct GViewer
{
    GtkTable table;
    GViewerPrivate *priv;
};

struct GViewerClass
{
    GtkTableClass parent_class;
    void (*status_line_changed)  (GViewer *obj, const gchar *statusline);
};

GtkWidget     *gviewer_new ();
GtkType        gviewer_get_type ();
void           gviewer_set_client (GViewer *obj, GtkWidget *client);
GtkWidget     *gviewer_get_client (GViewer *obj);

GtkAdjustment *gviewer_get_h_adjustment (GViewer *obj);
void           gviewer_set_h_adjustment (GViewer *obj, GtkAdjustment *adjustment);
GtkAdjustment *gviewer_get_v_adjustment (GViewer *obj);
void           gviewer_set_v_adjustment (GViewer *obj, GtkAdjustment *adjustment);

void           gviewer_set_display_mode(GViewer *obj, VIEWERDISPLAYMODE mode);
VIEWERDISPLAYMODE gviewer_get_display_mode(GViewer *obj);

void           gviewer_load_file(GViewer *obj, const gchar *filename);
void           gviewer_load_filedesc(GViewer *obj, int fd);
const gchar   *gviewer_get_filename(GViewer *obj);

/* Text Render related settings */
void        gviewer_set_tab_size(GViewer *obj, int tab_size);
int         gviewer_get_tab_size(GViewer *obj);

void        gviewer_set_wrap_mode(GViewer *obj, gboolean ACTIVE);
gboolean    gviewer_get_wrap_mode(GViewer *obj);

void        gviewer_set_fixed_limit(GViewer *obj, int fixed_limit);
int         gviewer_get_fixed_limit(GViewer *obj);

void        gviewer_set_encoding(GViewer *obj, const char *encoding);
const gchar *gviewer_get_encoding(GViewer *obj);

void        gviewer_set_hex_offset_display(GViewer *obj, gboolean HEX_OFFSET);
gboolean    gviewer_get_hex_offset_display(GViewer *obj);

void        gviewer_set_font_size(GViewer *obj, int font_size);
int         gviewer_get_font_size(GViewer *obj);

/* Image Render related Settings */
void        gviewer_set_best_fit(GViewer *obj, gboolean active);
gboolean    gviewer_get_best_fit(GViewer *obj);

void        gviewer_set_scale_factor(GViewer *obj, double scalefactor);
double      gviewer_get_scale_factor(GViewer *obj);

void        gviewer_image_operation(GViewer *obj, IMAGEOPERATION op);
void        gviewer_copy_selection(GViewer *obj);

TextRender  *gviewer_get_text_render(GViewer *obj);

#endif /* __GVIEWER_H__ */
