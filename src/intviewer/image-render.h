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

#ifndef __IMAGE_RENDER_H__
#define __IMAGE_RENDER_H__

#define IMAGE_RENDER(obj)          GTK_CHECK_CAST (obj, image_render_get_type (), ImageRender)
#define IMAGE_RENDER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, image_render_get_type (), ImageRenderClass)
#define IS_IMAGE_RENDER(obj)       GTK_CHECK_TYPE (obj, image_render_get_type ())

#define IMAGE_RENDER_DEFAULT_WIDTH    (100)
#define IMAGE_RENDER_DEFAULT_HEIGHT   (200)

struct ImageRenderPrivate;
struct ImageRenderStatus;

struct ImageRender
{
    GtkWidget widget;
    ImageRenderPrivate *priv;
};

struct ImageRenderClass
{
    GtkWidgetClass parent_class;
    void (*image_status_changed)  (ImageRender *obj, ImageRenderStatus *status);
};

struct ImageRenderStatus
{
    gboolean best_fit;
    gdouble  scale_factor;
    gint     image_width;
    gint     image_height;
    gint     bits_per_sample;
};

typedef enum
{
    ROTATE_CLOCKWISE,
    ROTATE_COUNTERCLOCKWISE,
    ROTATE_UPSIDEDOWN,
    FLIP_VERTICAL,
    FLIP_HORIZONTAL
} IMAGEOPERATION;

GtkWidget     *image_render_new ();
GtkType        image_render_get_type ();

GtkAdjustment *image_render_get_h_adjustment (ImageRender *obj);
void           image_render_set_h_adjustment (ImageRender *obj, GtkAdjustment *adjustment);
GtkAdjustment *image_render_get_v_adjustment (ImageRender *obj);
void           image_render_set_v_adjustment (ImageRender *obj, GtkAdjustment *adjustment);

void           image_render_load_file (ImageRender *obj, const gchar *filename);

void           image_render_notify_status_changed (ImageRender *w);

void           image_render_set_best_fit (ImageRender *obj, gboolean active);
gboolean       image_render_get_best_fit (ImageRender *obj);

void           image_render_set_scale_factor (ImageRender *obj, double scalefactor);
double         image_render_get_scale_factor (ImageRender *obj);

void           image_render_operation (ImageRender *obj, IMAGEOPERATION op);

#endif /* __IMAGE_RENDER_H__ */
