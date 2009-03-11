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

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkmarshal.h>

#include <iostream>

#include "image-render.h"
#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "utils.h"

using namespace std;


static GtkWidgetClass *parent_class = NULL;

enum {
  IMAGE_STATUS_CHANGED,
  LAST_SIGNAL
};

static guint image_render_signals[LAST_SIGNAL] = { 0 };


// Class Private Data
struct ImageRenderPrivate
{
    guint8 button; // The button pressed in "button_press_event"

    GtkAdjustment *h_adjustment;
    // Old values from h_adjustment stored so we know when something changes
    gfloat old_h_adj_value;
    gfloat old_h_adj_lower;
    gfloat old_h_adj_upper;

    GtkAdjustment *v_adjustment;
    // Old values from v_adjustment stored so we know when something changes
    gfloat old_v_adj_value;
    gfloat old_v_adj_lower;
    gfloat old_v_adj_upper;

    gchar      *filename;
    gboolean    scaled_pixbuf_loaded;
    GdkPixbuf  *orig_pixbuf;
    GdkPixbuf  *disp_pixbuf;
    gboolean    best_fit;
    gdouble     scale_factor;

    GThread    *pixbuf_loading_thread;
    gint        orig_pixbuf_loaded;
};

// Gtk class related static functions
static void image_render_init (ImageRender *w);
static void image_render_class_init (ImageRenderClass *klass);
static void image_render_destroy (GtkObject *object);

static void image_render_redraw (ImageRender *w);

static gboolean image_render_key_press (GtkWidget *widget, GdkEventKey *event);

static void image_render_realize (GtkWidget *widget);
static void image_render_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void image_render_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean image_render_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean image_render_button_press (GtkWidget *widget, GdkEventButton *event);
static gboolean image_render_button_release (GtkWidget *widget, GdkEventButton *event);
static gboolean image_render_motion_notify (GtkWidget *widget, GdkEventMotion *event);
static void image_render_h_adjustment_update (ImageRender *obj);
static void image_render_h_adjustment_changed (GtkAdjustment *adjustment, gpointer data);
static void image_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data);
static void image_render_v_adjustment_update (ImageRender *obj);
static void image_render_v_adjustment_changed (GtkAdjustment *adjustment, gpointer data);
static void image_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data);

void image_render_start_background_pixbuf_loading (ImageRender *w);
void image_render_load_scaled_pixbuf (ImageRender *obj);
void image_render_wait_for_loader_thread (ImageRender *obj);

static void image_render_free_pixbuf (ImageRender *obj);
static void image_render_prepare_disp_pixbuf (ImageRender *obj);
static void image_render_update_adjustments (ImageRender *obj);

/*****************************************
    public functions
    (defined in the header file)
*****************************************/
GtkType image_render_get_type ()
{
    static GtkType type = 0;
    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "ImageRender",
            sizeof (ImageRender),
            sizeof (ImageRenderClass),
            (GtkClassInitFunc) image_render_class_init,
            (GtkObjectInitFunc) image_render_init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };
        type = gtk_type_unique (gtk_widget_get_type(), &info);
    }
    return type;
}


GtkWidget *image_render_new ()
{
    ImageRender *w = (ImageRender *) gtk_type_new (image_render_get_type ());

    return GTK_WIDGET (w);
}


void image_render_set_h_adjustment (ImageRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    if (obj->priv->h_adjustment)
    {
        gtk_signal_disconnect_by_data (GTK_OBJECT (obj->priv->h_adjustment), (gpointer) obj);
        g_object_unref (obj->priv->h_adjustment);
    }

    obj->priv->h_adjustment = adjustment;
    gtk_object_ref (GTK_OBJECT (obj->priv->h_adjustment));

    gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
              (GtkSignalFunc) image_render_h_adjustment_changed ,
              (gpointer) obj);
    gtk_signal_connect (GTK_OBJECT (adjustment), "value-changed",
              (GtkSignalFunc) image_render_h_adjustment_value_changed ,
              (gpointer) obj);

    obj->priv->old_h_adj_value = adjustment->value;
    obj->priv->old_h_adj_lower = adjustment->lower;
    obj->priv->old_h_adj_upper = adjustment->upper;

    image_render_h_adjustment_update (obj);
}


void image_render_set_v_adjustment (ImageRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    if (obj->priv->v_adjustment)
    {
        gtk_signal_disconnect_by_data (GTK_OBJECT (obj->priv->v_adjustment), (gpointer) obj);
        g_object_unref (obj->priv->v_adjustment);
    }

    obj->priv->v_adjustment = adjustment;
    gtk_object_ref (GTK_OBJECT (obj->priv->v_adjustment));

    gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
              (GtkSignalFunc) image_render_v_adjustment_changed ,
              (gpointer) obj);
    gtk_signal_connect (GTK_OBJECT (adjustment), "value-changed",
              (GtkSignalFunc) image_render_v_adjustment_value_changed ,
              (gpointer) obj);

    obj->priv->old_v_adj_value = adjustment->value;
    obj->priv->old_v_adj_lower = adjustment->lower;
    obj->priv->old_v_adj_upper = adjustment->upper;

    image_render_v_adjustment_update (obj);
}


static void image_render_class_init (ImageRenderClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkWidgetClass *) gtk_type_class (gtk_widget_get_type ());

    object_class->destroy = image_render_destroy;

    widget_class->key_press_event = image_render_key_press;
    widget_class->button_press_event = image_render_button_press;
    widget_class->button_release_event = image_render_button_release;
    widget_class->motion_notify_event = image_render_motion_notify;

    widget_class->expose_event = image_render_expose;

    widget_class->size_request = image_render_size_request;
    widget_class->size_allocate = image_render_size_allocate;

    widget_class->realize = image_render_realize;

    image_render_signals[IMAGE_STATUS_CHANGED] =
        gtk_signal_new ("image-status-changed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (ImageRenderClass, image_status_changed),
            gtk_marshal_NONE__POINTER,
            GTK_TYPE_NONE,
            1, GTK_TYPE_POINTER);

}


static void image_render_init (ImageRender *w)
{
    w->priv = g_new0(ImageRenderPrivate, 1);

    w->priv->button = 0;

    w->priv->scaled_pixbuf_loaded = FALSE;
    w->priv->filename = NULL;

    w->priv->h_adjustment = NULL;
    w->priv->old_h_adj_value = 0.0;
    w->priv->old_h_adj_lower = 0.0;
    w->priv->old_h_adj_upper = 0.0;

    w->priv->v_adjustment = NULL;
    w->priv->old_v_adj_value = 0.0;
    w->priv->old_v_adj_lower = 0.0;
    w->priv->old_v_adj_upper = 0.0;

    w->priv->best_fit = FALSE;
    w->priv->scale_factor = 1.3;
    w->priv->orig_pixbuf = NULL;
    w->priv->disp_pixbuf = NULL;

    GTK_WIDGET_SET_FLAGS(GTK_WIDGET (w), GTK_CAN_FOCUS);
}


static void image_render_destroy (GtkObject *object)
{
    g_return_if_fail (object != NULL);
    g_return_if_fail (IS_IMAGE_RENDER (object));

    ImageRender *w = IMAGE_RENDER (object);

    if (w->priv)
    {
        /* there are TWO references to the ImageRender object:
            one in the parent widget, the other in the loader thread.

            "Destroy" might be called twice (don't know why, yet) - if the viewer is closed (by the user)
            before the loader thread finishes.

           If this is the case, we don't want to block while waiting for the loader thread (bad user responsiveness).
           So if "destroy" is called while the loader thread is still running, we simply exit, know "destroy" will be called again
          once the loader thread is done (because it calls "g_object_unref" on the ImageRender object).
        */
        if (w->priv->pixbuf_loading_thread && g_atomic_int_get (&w->priv->orig_pixbuf_loaded)==0)
        {
                // Loader thread still running, so do nothing
        }
        else
        {
            image_render_free_pixbuf (w);

            if (w->priv->v_adjustment)
                g_object_unref (w->priv->v_adjustment);
            w->priv->v_adjustment = NULL;

            if (w->priv->h_adjustment)
                g_object_unref (w->priv->h_adjustment);
            w->priv->h_adjustment = NULL;

            g_free (w->priv);
            w->priv = NULL;
        }
    }

    if (GTK_OBJECT_CLASS(parent_class)->destroy)
        (*GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}


void image_render_notify_status_changed (ImageRender *w)
{
    g_return_if_fail (w!= NULL);
    g_return_if_fail (IS_IMAGE_RENDER (w));

    ImageRenderStatus stat;

    memset(&stat, 0, sizeof(stat));

    stat.best_fit = w->priv->best_fit;
    stat.scale_factor = w->priv->scale_factor;

    if (w->priv->orig_pixbuf)
    {
        stat.image_width = gdk_pixbuf_get_width (w->priv->orig_pixbuf);
        stat.image_height = gdk_pixbuf_get_height (w->priv->orig_pixbuf);
        stat.bits_per_sample = gdk_pixbuf_get_bits_per_sample(w->priv->orig_pixbuf);
    }

    gtk_signal_emit (GTK_OBJECT(w), image_render_signals[IMAGE_STATUS_CHANGED], &stat);
}


static gboolean image_render_key_press (GtkWidget *widget, GdkEventKey *event)
{
    return FALSE;
}


static void image_render_realize (GtkWidget *widget)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_IMAGE_RENDER (widget));

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    ImageRender *obj = IMAGE_RENDER (widget);

    GdkWindowAttr attributes;

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget) |
        GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
        GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
        GDK_POINTER_MOTION_HINT_MASK | GDK_KEY_PRESS_MASK;
    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);

    gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach (widget->style, widget->window);

    gdk_window_set_user_data (widget->window, widget);

    gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);

    // image_render_prepare_disp_pixbuf (obj);
    if (!obj->priv->scaled_pixbuf_loaded)
        image_render_load_scaled_pixbuf (obj);
}


static void image_render_redraw (ImageRender *w)
{
    if (!GTK_WIDGET_REALIZED(GTK_WIDGET (w)))
        return;

    image_render_notify_status_changed (w);
    gdk_window_invalidate_rect (GTK_WIDGET (w)->window, NULL, FALSE);
}


static void image_render_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
    requisition->width = IMAGE_RENDER_DEFAULT_WIDTH;
    requisition->height = IMAGE_RENDER_DEFAULT_HEIGHT;
}


static void image_render_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_IMAGE_RENDER (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED (widget))
    {
        gdk_window_move_resize (widget->window, allocation->x, allocation->y, allocation->width, allocation->height);

        image_render_prepare_disp_pixbuf (IMAGE_RENDER (widget));
    }
}


static gboolean image_render_expose (GtkWidget *widget, GdkEventExpose *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    ImageRender *w = IMAGE_RENDER (widget);

    gdk_window_clear_area (widget->window, 0, 0, widget->allocation.width, widget->allocation.height);

    if (!w->priv->disp_pixbuf)
        return FALSE;

    gint xc, yc;

    if (w->priv->best_fit ||
        (gdk_pixbuf_get_width(w->priv->disp_pixbuf) < widget->allocation.width &&
         gdk_pixbuf_get_height(w->priv->disp_pixbuf) < widget->allocation.height))
    {
        xc = widget->allocation.width / 2 - gdk_pixbuf_get_width(w->priv->disp_pixbuf)/2;
        yc = widget->allocation.height / 2 - gdk_pixbuf_get_height(w->priv->disp_pixbuf)/2;

        gdk_draw_pixbuf(widget->window,
                NULL,
                w->priv->disp_pixbuf,
                0, 0, // source X, Y
                xc, yc, // Dest X, Y
                -1, -1, // Source W, H
                 GDK_RGB_DITHER_NONE, 0, 0);
    }
    else
    {
        gint src_x, src_y;
        gint dst_x, dst_y;
        gint width, height;

        if (widget->allocation.width > gdk_pixbuf_get_width(w->priv->disp_pixbuf))
        {
            src_x = 0;
            dst_x = widget->allocation.width / 2 - gdk_pixbuf_get_width(w->priv->disp_pixbuf)/2;
            width = gdk_pixbuf_get_width(w->priv->disp_pixbuf);
        }
        else
        {
            src_x = (int)w->priv->h_adjustment->value;
            dst_x = 0;
            width = MIN(widget->allocation.width, gdk_pixbuf_get_width(w->priv->disp_pixbuf));
            if (src_x + width > gdk_pixbuf_get_width(w->priv->disp_pixbuf))
                src_x = gdk_pixbuf_get_width(w->priv->disp_pixbuf) - width;
        }


        if ((int)w->priv->h_adjustment->value > gdk_pixbuf_get_height(w->priv->disp_pixbuf))
        {
            src_y = 0;
            dst_y = widget->allocation.height / 2 - gdk_pixbuf_get_height(w->priv->disp_pixbuf)/2;
            height = gdk_pixbuf_get_height(w->priv->disp_pixbuf);
        }
        else
        {
            src_y = (int)w->priv->v_adjustment->value;
            dst_y = 0;
            height = MIN(widget->allocation.height, gdk_pixbuf_get_height(w->priv->disp_pixbuf));

            if (src_y + height > gdk_pixbuf_get_height(w->priv->disp_pixbuf))
                src_y = gdk_pixbuf_get_height(w->priv->disp_pixbuf) - height;
        }

#if 0
        fprintf(stderr, "src(%d, %d), dst(%d, %d), size(%d, %d) origsize(%d, %d) alloc(%d, %d) ajd(%d, %d)\n",
                src_x, src_y,
                dst_x, dst_y,
                width, height,
                gdk_pixbuf_get_width(w->priv->disp_pixbuf),
                gdk_pixbuf_get_height(w->priv->disp_pixbuf),
                widget->allocation.width,
                widget->allocation.height,
                (int)w->priv->h_adjustment->value,
                (int)w->priv->v_adjustment->value);
#endif
        gdk_draw_pixbuf(widget->window,
                        NULL,
                        w->priv->disp_pixbuf,
                        src_x, src_y,
                        dst_x, dst_y,
                        width, height,
                        GDK_RGB_DITHER_NONE, 0, 0);
    }

    if (g_atomic_int_get (&w->priv->orig_pixbuf_loaded)==0)
        image_render_start_background_pixbuf_loading (w);
    return FALSE;
}


static gboolean image_render_button_press (GtkWidget *widget, GdkEventButton *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    ImageRender *w = IMAGE_RENDER (widget);

    // TODO: Replace (1) with your on conditional for grabbing the mouse
    if (!w->priv->button && (1))
    {
        gtk_grab_add (widget);

        w->priv->button = event->button;

        // gtk_dial_update_mouse (dial, event->x, event->y);
    }

    return FALSE;
}


static gboolean image_render_button_release (GtkWidget *widget, GdkEventButton *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    ImageRender *w = IMAGE_RENDER (widget);

    if (w->priv->button == event->button)
    {
        gtk_grab_remove (widget);

        w->priv->button = 0;
    }

    return FALSE;
}


static gboolean image_render_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    ImageRender *w = IMAGE_RENDER (widget);

    if (w->priv->button != 0)
    {
        GdkModifierType mods;

        gint x = event->x;
        gint y = event->y;

        if (event->is_hint || (event->window != widget->window))
            gdk_window_get_pointer (widget->window, &x, &y, &mods);

        // TODO: respond to motion event
    }

    return FALSE;
}


static void image_render_h_adjustment_update (ImageRender *obj)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    gfloat new_value = obj->priv->h_adjustment->value;

    if (new_value < obj->priv->h_adjustment->lower)
        new_value = obj->priv->h_adjustment->lower;

    if (new_value > obj->priv->h_adjustment->upper)
        new_value = obj->priv->h_adjustment->upper;

    if (new_value != obj->priv->h_adjustment->value)
    {
        obj->priv->h_adjustment->value = new_value;
        gtk_signal_emit_by_name (GTK_OBJECT (obj->priv->h_adjustment), "value-changed");
    }

    /* TODO: Update the widget in response to the adjusments' change
        Note: The change can be in 'value' or in 'lower'/'upper's */

    image_render_redraw(obj);
}


static void image_render_h_adjustment_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    if ((obj->priv->old_h_adj_value != adjustment->value) ||
        (obj->priv->old_h_adj_lower != adjustment->lower) ||
        (obj->priv->old_h_adj_upper != adjustment->upper))
    {
        image_render_h_adjustment_update (obj);

        obj->priv->old_h_adj_value = adjustment->value;
        obj->priv->old_h_adj_lower = adjustment->lower;
        obj->priv->old_h_adj_upper = adjustment->upper;
    }
}


static void image_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    if (obj->priv->old_h_adj_value != adjustment->value)
    {
        image_render_h_adjustment_update (obj);
        obj->priv->old_h_adj_value = adjustment->value;
    }
}


static void image_render_v_adjustment_update (ImageRender *obj)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    gfloat new_value = obj->priv->v_adjustment->value;

    if (new_value < obj->priv->v_adjustment->lower)
        new_value = obj->priv->v_adjustment->lower;

    if (new_value > obj->priv->v_adjustment->upper)
        new_value = obj->priv->v_adjustment->upper;

    if (new_value != obj->priv->v_adjustment->value)
    {
        obj->priv->v_adjustment->value = new_value;
        gtk_signal_emit_by_name (GTK_OBJECT (obj->priv->v_adjustment), "value-changed");
    }

    /* TODO: Update the widget in response to the adjusments' change
        Note: The change can be in 'value' or in 'lower'/'upper's */

    image_render_redraw(obj);
}


static void image_render_v_adjustment_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    if ((obj->priv->old_v_adj_value != adjustment->value) ||
        (obj->priv->old_v_adj_lower != adjustment->lower) ||
        (obj->priv->old_v_adj_upper != adjustment->upper))
    {
        image_render_v_adjustment_update (obj);

        obj->priv->old_v_adj_value = adjustment->value;
        obj->priv->old_v_adj_lower = adjustment->lower;
        obj->priv->old_v_adj_upper = adjustment->upper;
    }
}


static void image_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    if (obj->priv->old_v_adj_value != adjustment->value)
    {
        image_render_v_adjustment_update (obj);
        obj->priv->old_v_adj_value = adjustment->value;
    }
}


static void image_render_free_pixbuf (ImageRender *obj)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    image_render_wait_for_loader_thread (obj);

    obj->priv->orig_pixbuf_loaded = 0;
    obj->priv->scaled_pixbuf_loaded = FALSE;

    if (obj->priv->orig_pixbuf)
        g_object_unref (G_OBJECT (obj->priv->orig_pixbuf));
    obj->priv->orig_pixbuf = NULL;

    if (obj->priv->disp_pixbuf)
        g_object_unref (G_OBJECT (obj->priv->disp_pixbuf));
    obj->priv->disp_pixbuf = NULL;

    g_free (obj->priv->filename);
    obj->priv->filename = NULL;
}


static gpointer image_render_pixbuf_loading_thread (gpointer data)
{
    GError *err = NULL;
    ImageRender *obj = (ImageRender *) data;

    obj->priv->orig_pixbuf = gdk_pixbuf_new_from_file(obj->priv->filename, &err);

    g_atomic_int_inc (&obj->priv->orig_pixbuf_loaded);

    g_object_unref (G_OBJECT (obj));

    return NULL;
}


inline void image_render_wait_for_loader_thread (ImageRender *obj)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER (obj));

    if (obj->priv->pixbuf_loading_thread !=NULL)
    {
        /*
            ugly hack: use a busy wait loop, until the loader thread is done.

            rational: the loader thread might be running after the widget is destroyed (if the user closed the viewer very quickly,
                      and the loader is still reading a very large image). If this happens, this (and all the 'destroy' functions)
                      will be called from the loader thread's context, and using g_thread_join() will crash the application.
        */

        while (g_atomic_int_get (&obj->priv->orig_pixbuf_loaded)==0)
            g_usleep(1000);

        obj->priv->orig_pixbuf_loaded = 0;
        obj->priv->pixbuf_loading_thread = NULL;
    }
}


void image_render_load_scaled_pixbuf (ImageRender *obj)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    g_return_if_fail (obj->priv->filename!=NULL);
    g_return_if_fail (obj->priv->scaled_pixbuf_loaded==FALSE);

    g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET (obj)));

    int width = GTK_WIDGET (obj)->allocation.width;
    int height = GTK_WIDGET (obj)->allocation.height;

    GError *err = NULL;

    obj->priv->disp_pixbuf  = gdk_pixbuf_new_from_file_at_scale(obj->priv->filename, width, height, TRUE, &err);

    if (err)
    {
        g_warning("pixbuf loading failed: %s", err->message);
        g_error_free(err);
        obj->priv->orig_pixbuf = NULL;
        obj->priv->disp_pixbuf = NULL;
        return;
    }

    obj->priv->scaled_pixbuf_loaded = TRUE;
}


inline void image_render_start_background_pixbuf_loading (ImageRender *obj)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER (obj));
    g_return_if_fail (obj->priv->filename!=NULL);

    if (obj->priv->pixbuf_loading_thread!=NULL)
        return;

    obj->priv->orig_pixbuf_loaded = 0;

    // Start background loading
    g_object_ref (G_OBJECT (obj));
    obj->priv->pixbuf_loading_thread = g_thread_create(image_render_pixbuf_loading_thread, (gpointer) obj, FALSE, NULL);
}


void image_render_load_file (ImageRender *obj, const gchar *filename)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    image_render_free_pixbuf (obj);

    g_return_if_fail (obj->priv->filename==NULL);

    obj->priv->filename = g_strdup (filename);
    obj->priv->scaled_pixbuf_loaded = FALSE;
    obj->priv->orig_pixbuf_loaded = 0;
}


static void image_render_prepare_disp_pixbuf (ImageRender *obj)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    // this will be set only if the loader thread finished loading the big pixbuf
    if (g_atomic_int_get (&obj->priv->orig_pixbuf_loaded)==0)
        return;

    if (!GTK_WIDGET_REALIZED (GTK_WIDGET (obj)))
        return;

    if (obj->priv->disp_pixbuf)
        g_object_unref (G_OBJECT (obj->priv->disp_pixbuf));
    obj->priv->disp_pixbuf = NULL;

    if (gdk_pixbuf_get_height(obj->priv->orig_pixbuf)==0)
        return;

    if (obj->priv->best_fit)
    {
        if (gdk_pixbuf_get_height(obj->priv->orig_pixbuf) < GTK_WIDGET (obj)->allocation.height &&
            gdk_pixbuf_get_width(obj->priv->orig_pixbuf) < GTK_WIDGET (obj)->allocation.width)
        {
            // no need to scale down

            obj->priv->disp_pixbuf = obj->priv->orig_pixbuf;
            g_object_ref (G_OBJECT (obj->priv->disp_pixbuf));
            return;
        }

        int height = GTK_WIDGET (obj)->allocation.height;
        int width = (((double) GTK_WIDGET (obj)->allocation.height) /
                  gdk_pixbuf_get_height(obj->priv->orig_pixbuf))*
                    gdk_pixbuf_get_width(obj->priv->orig_pixbuf);

        if (width >= GTK_WIDGET (obj)->allocation.width)
        {
            width = GTK_WIDGET (obj)->allocation.width;
            height = (((double)GTK_WIDGET (obj)->allocation.width) /
                  gdk_pixbuf_get_width(obj->priv->orig_pixbuf))*
                    gdk_pixbuf_get_height(obj->priv->orig_pixbuf);
        }

        if (width<=1 || height<=1)
        {
            obj->priv->disp_pixbuf = NULL;
            return;
        }

        obj->priv->disp_pixbuf = gdk_pixbuf_scale_simple( obj->priv->orig_pixbuf, width, height, GDK_INTERP_NEAREST);
    }
    else
    {
        // not "best_fit" = scaling mode
        obj->priv->disp_pixbuf = gdk_pixbuf_scale_simple(
                obj->priv->orig_pixbuf,
                (int)(gdk_pixbuf_get_width(obj->priv->orig_pixbuf) * obj->priv->scale_factor),
                (int)(gdk_pixbuf_get_height(obj->priv->orig_pixbuf) * obj->priv->scale_factor),
                GDK_INTERP_NEAREST);
    }

    image_render_update_adjustments (obj);
}


static void image_render_update_adjustments (ImageRender *obj)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    if (!obj->priv->disp_pixbuf)
        return;

    if (obj->priv->best_fit ||
        (gdk_pixbuf_get_width(obj->priv->disp_pixbuf) < GTK_WIDGET (obj)->allocation.width  &&
         gdk_pixbuf_get_height(obj->priv->disp_pixbuf) < GTK_WIDGET (obj)->allocation.height))
    {
        if (obj->priv->h_adjustment)
        {
            obj->priv->h_adjustment->lower = 0;
            obj->priv->h_adjustment->upper = 0;
            obj->priv->h_adjustment->value = 0;
            gtk_adjustment_changed(obj->priv->h_adjustment);
        }
        if (obj->priv->v_adjustment)
        {
            obj->priv->v_adjustment->lower = 0;
            obj->priv->v_adjustment->upper = 0;
            obj->priv->v_adjustment->value = 0;
            gtk_adjustment_changed(obj->priv->v_adjustment);
        }
    }
    else
    {
        if (obj->priv->h_adjustment)
        {
            obj->priv->h_adjustment->lower = 0;
            obj->priv->h_adjustment->upper = gdk_pixbuf_get_width(obj->priv->disp_pixbuf);
            obj->priv->h_adjustment->page_size = GTK_WIDGET (obj)->allocation.width;
            gtk_adjustment_changed(obj->priv->h_adjustment);
        }
        if (obj->priv->v_adjustment)
        {
            obj->priv->v_adjustment->lower = 0;
            obj->priv->v_adjustment->upper = gdk_pixbuf_get_height(obj->priv->disp_pixbuf);
            obj->priv->v_adjustment->page_size = GTK_WIDGET (obj)->allocation.height;
            gtk_adjustment_changed(obj->priv->v_adjustment);
        }
    }
}


void image_render_set_best_fit(ImageRender *obj, gboolean active)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    obj->priv->best_fit = active;
    image_render_prepare_disp_pixbuf (obj);
    image_render_redraw (obj);
}


gboolean image_render_get_best_fit(ImageRender *obj)
{
    g_return_val_if_fail (obj!=NULL, FALSE);
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), FALSE);

    return obj->priv->best_fit;
}


void image_render_set_scale_factor(ImageRender *obj, double scalefactor)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    obj->priv->scale_factor = scalefactor;
    image_render_prepare_disp_pixbuf (obj);
    image_render_redraw(obj);
}


double image_render_get_scale_factor(ImageRender *obj)
{
    g_return_val_if_fail (obj!=NULL, 1);
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), 1);

    return obj->priv->scale_factor;
}


void image_render_operation(ImageRender *obj, IMAGEOPERATION op)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    g_return_if_fail (obj->priv->orig_pixbuf);

    GdkPixbuf *temp = NULL;

    switch (op)
    {
        case ROTATE_CLOCKWISE:
            temp = gdk_pixbuf_rotate_simple (obj->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
            break;
        case ROTATE_COUNTERCLOCKWISE:
            temp = gdk_pixbuf_rotate_simple (obj->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
            break;
        case ROTATE_UPSIDEDOWN:
            temp = gdk_pixbuf_rotate_simple (obj->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
            break;
        case FLIP_VERTICAL:
            temp = gdk_pixbuf_flip (obj->priv->orig_pixbuf, FALSE);
            break;
        case FLIP_HORIZONTAL:
            temp = gdk_pixbuf_flip (obj->priv->orig_pixbuf, TRUE);
            break;
        default:
            g_return_if_fail (!"Unknown image operation");
    }

    g_object_unref (obj->priv->orig_pixbuf);

    obj->priv->orig_pixbuf = temp;

    image_render_prepare_disp_pixbuf (obj);
}
