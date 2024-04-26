/**
 * @file viewer-widget.h
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

#pragma once

#define GVIEWER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gviewer_get_type (), GViewer)
#define GVIEWER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gviewer_get_type (), GViewerClass)
#define IS_GVIEWER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gviewer_get_type ())

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
    GtkGrid parent;
    GViewerPrivate *priv;
};

struct GViewerClass
{
    GtkGridClass parent_class;
    void (*status_line_changed)  (GViewer *obj, const gchar *statusline);
};

GtkWidget     *gviewer_new ();
GType          gviewer_get_type ();
void           gviewer_set_client (GViewer *obj, GtkWidget *client);
GtkWidget     *gviewer_get_client (GViewer *obj);

GtkAdjustment *gviewer_get_h_adjustment (GViewer *obj);
void           gviewer_set_h_adjustment (GViewer *obj, GtkAdjustment *adjustment);
GtkAdjustment *gviewer_get_v_adjustment (GViewer *obj);
void           gviewer_set_v_adjustment (GViewer *obj, GtkAdjustment *adjustment);

void           gviewer_set_display_mode(GViewer *obj, VIEWERDISPLAYMODE mode);
VIEWERDISPLAYMODE gviewer_get_display_mode(GViewer *obj);

void           gviewer_load_file(GViewer *obj, const gchar *filename);

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

void        gviewer_image_operation(GViewer *obj, ImageRender::DISPLAYMODE op);
void        gviewer_copy_selection(GtkMenuItem *item, GViewer *obj);

TextRender  *gviewer_get_text_render(GViewer *obj);
