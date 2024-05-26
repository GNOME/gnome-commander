/**
 * @file image-render.h
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

#define TYPE_IMAGE_RENDER               (image_render_get_type ())
#define IMAGE_RENDER(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_IMAGE_RENDER, ImageRender))
#define IMAGE_RENDER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_IMAGE_RENDER, ImageRenderClass))
#define IS_IMAGE_RENDER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_IMAGE_RENDER))
#define IS_IMAGE_RENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_IMAGE_RENDER))
#define IMAGE_RENDER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_IMAGE_RENDER, ImageRenderClass))


GType image_render_get_type ();


struct ImageRender
{
    GtkDrawingArea parent;

    struct Status
    {
        gboolean best_fit;
        gdouble  scale_factor;
        gint     image_width;
        gint     image_height;
        gint     bits_per_sample;
    };

    enum DISPLAYMODE
    {
        ROTATE_CLOCKWISE,
        ROTATE_COUNTERCLOCKWISE,
        ROTATE_UPSIDEDOWN,
        FLIP_VERTICAL,
        FLIP_HORIZONTAL
    };
};

inline GtkWidget *image_render_new ()
{
    return (GtkWidget *) g_object_new (TYPE_IMAGE_RENDER, NULL);
}

void image_render_load_file (ImageRender *obj, const gchar *filename);

void image_render_notify_status_changed (ImageRender *w);

void image_render_set_best_fit (ImageRender *obj, gboolean active);
gboolean image_render_get_best_fit (ImageRender *obj);

void image_render_set_scale_factor (ImageRender *obj, double scalefactor);
double image_render_get_scale_factor (ImageRender *obj);

void image_render_operation (ImageRender *obj, ImageRender::DISPLAYMODE op);
