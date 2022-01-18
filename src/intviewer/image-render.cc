/**
 * @file image-render.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2022 Uwe Scholz\n
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
#include <gtk/gtkadjustment.h>
#include <gtk/gtkmarshal.h>

#include <iostream>

#include "image-render.h"
#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "utils.h"

using namespace std;


#define IMAGE_RENDER_DEFAULT_WIDTH      100
#define IMAGE_RENDER_DEFAULT_HEIGHT     200
#define INC_VALUE 25.0


enum {
  IMAGE_STATUS_CHANGED,
  LAST_SIGNAL
};

static guint image_render_signals[LAST_SIGNAL] = { 0 };


struct ImageRenderClass
{
    GtkWidgetClass parent_class;
    void (*image_status_changed)  (ImageRender *obj, ImageRender::Status *status);
};

// Class Private Data
struct ImageRender::Private
{
    guint8 button; // The button pressed in "button_press_event"
    gint mouseX;   // Old x position before mouse move
    gint mouseY;   // Old y position before mouse move

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


G_DEFINE_TYPE (ImageRender, image_render, GTK_TYPE_WIDGET)


// Gtk class related static functions
static void image_render_redraw (ImageRender *w);

static gboolean image_render_key_press (GtkWidget *widget, GdkEventKey *event);
static gboolean image_render_scroll(GtkWidget *widget, GdkEventScroll *event);

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
GtkAdjustment *image_render_get_h_adjustment (ImageRender *obj)
{
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), nullptr);

    if (obj->priv->h_adjustment)
    {
        return obj->priv->h_adjustment;
    }
    return nullptr;
}

void image_render_set_h_adjustment (ImageRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    if (obj->priv->h_adjustment)
    {
        g_signal_handlers_disconnect_matched (obj->priv->h_adjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, obj);
        g_object_unref (obj->priv->h_adjustment);
    }

    obj->priv->h_adjustment = adjustment;
    g_object_ref (obj->priv->h_adjustment);

    g_signal_connect (adjustment, "changed", G_CALLBACK (image_render_h_adjustment_changed), obj);
    g_signal_connect (adjustment, "value-changed", G_CALLBACK (image_render_h_adjustment_value_changed), obj);

    obj->priv->old_h_adj_value = gtk_adjustment_get_value (adjustment);
    obj->priv->old_h_adj_lower = gtk_adjustment_get_lower (adjustment);
    obj->priv->old_h_adj_upper = gtk_adjustment_get_upper (adjustment);

    image_render_h_adjustment_update (obj);
}


GtkAdjustment *image_render_get_v_adjustment (ImageRender *obj)
{
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), nullptr);

    if (obj->priv->v_adjustment)
    {
        return obj->priv->v_adjustment;
    }
    return nullptr;
}

void image_render_set_v_adjustment (ImageRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    if (obj->priv->v_adjustment)
    {
        g_signal_handlers_disconnect_matched (obj->priv->v_adjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, obj);
        g_object_unref (obj->priv->v_adjustment);
    }

    obj->priv->v_adjustment = adjustment;
    g_object_ref (obj->priv->v_adjustment);

    g_signal_connect (adjustment, "changed", G_CALLBACK (image_render_v_adjustment_changed), obj);
    g_signal_connect (adjustment, "value-changed",  G_CALLBACK (image_render_v_adjustment_value_changed), obj);

    obj->priv->old_v_adj_value = gtk_adjustment_get_value (adjustment);
    obj->priv->old_v_adj_lower = gtk_adjustment_get_lower (adjustment);
    obj->priv->old_v_adj_upper = gtk_adjustment_get_upper (adjustment);

    image_render_v_adjustment_update (obj);
}


static void image_render_init (ImageRender *w)
{
    w->priv = g_new0 (ImageRender::Private, 1);

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

    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (w), GTK_CAN_FOCUS);
}


static void image_render_finalize (GObject *object)
{
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

            if (w->priv->h_adjustment)
                g_object_unref (w->priv->h_adjustment);

            g_free (w->priv);
        }
    }

    G_OBJECT_CLASS (image_render_parent_class)->finalize (object);
}


static void image_render_class_init (ImageRenderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = image_render_finalize;

    widget_class->scroll_event = image_render_scroll;
    widget_class->key_press_event = image_render_key_press;
    widget_class->button_press_event = image_render_button_press;
    widget_class->button_release_event = image_render_button_release;
    widget_class->motion_notify_event = image_render_motion_notify;

    widget_class->expose_event = image_render_expose;

    widget_class->size_request = image_render_size_request;
    widget_class->size_allocate = image_render_size_allocate;

    widget_class->realize = image_render_realize;

    image_render_signals[IMAGE_STATUS_CHANGED] =
        g_signal_new ("image-status-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (ImageRenderClass, image_status_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);
}


void image_render_notify_status_changed (ImageRender *w)
{
    g_return_if_fail (IS_IMAGE_RENDER (w));

    ImageRender::Status stat;

    memset(&stat, 0, sizeof(stat));

    stat.best_fit = w->priv->best_fit;
    stat.scale_factor = w->priv->scale_factor;

    if (w->priv->orig_pixbuf)
    {
        stat.image_width = gdk_pixbuf_get_width (w->priv->orig_pixbuf);
        stat.image_height = gdk_pixbuf_get_height (w->priv->orig_pixbuf);
        stat.bits_per_sample = gdk_pixbuf_get_bits_per_sample(w->priv->orig_pixbuf);
    }

    g_signal_emit (w, image_render_signals[IMAGE_STATUS_CHANGED], 0, &stat);
}


static gboolean image_render_key_press (GtkWidget *widget, GdkEventKey *event)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);

    auto imageRender = IMAGE_RENDER (widget);

    switch (event->keyval)
    {
        case GDK_KEY_Up:
        {
            auto vAdjustment = image_render_get_v_adjustment(imageRender);
            auto current = gtk_adjustment_get_value(vAdjustment);
            auto lower = gtk_adjustment_get_lower(vAdjustment);
            if (current - INC_VALUE > lower)
            {
                gtk_adjustment_set_value(vAdjustment, current - INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(vAdjustment, lower);
            }

            return TRUE;
        }
        case GDK_KEY_Down:
        {
            auto vAdjustment = image_render_get_v_adjustment(imageRender);
            auto current = gtk_adjustment_get_value(vAdjustment);
            auto upper = gtk_adjustment_get_upper(vAdjustment);
            auto page_size = gtk_adjustment_get_page_size(vAdjustment);
            if (current + INC_VALUE < upper - page_size)
            {
                gtk_adjustment_set_value(vAdjustment, current + INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(vAdjustment, upper - page_size);
            }
            return TRUE;
        }
        case GDK_KEY_Left:
        {
            auto hAdjustment = image_render_get_h_adjustment(imageRender);
            auto current = gtk_adjustment_get_value(hAdjustment);
            auto lower = gtk_adjustment_get_lower(hAdjustment);
            if (current - INC_VALUE > lower)
            {
                gtk_adjustment_set_value(hAdjustment, current - INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(hAdjustment, lower);
            }

            return TRUE;
        }
        case GDK_KEY_Right:
        {
            auto hAdjustment = image_render_get_h_adjustment(imageRender);
            auto current = gtk_adjustment_get_value(hAdjustment);
            auto upper = gtk_adjustment_get_upper(hAdjustment);
            auto page_size = gtk_adjustment_get_page_size(hAdjustment);
            if (current + INC_VALUE < upper - page_size)
            {
                gtk_adjustment_set_value(hAdjustment, current + INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(hAdjustment, upper - page_size);
            }

            return TRUE;
        }

        default:
           break;
    }

    switch (state_is_blank(event->keyval))
    {
        default:
           break;
    }

    return FALSE;
}


static gboolean image_render_scroll(GtkWidget *widget, GdkEventScroll *event)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    auto imageRender = IMAGE_RENDER (widget);

    // Mouse scroll wheel
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    if (state_is_ctrl(event->state))
    {
        switch (event->direction)
        {
            case GDK_SCROLL_UP:
            {
                auto hAdjustment = image_render_get_h_adjustment(imageRender);
                auto current = gtk_adjustment_get_value(hAdjustment);
                auto lower = gtk_adjustment_get_lower(hAdjustment);
                if (current > lower)
                {
                    gtk_adjustment_set_value(hAdjustment, current - INC_VALUE);
                }
                return TRUE;
            }
            case GDK_SCROLL_DOWN:
            {
                auto hAdjustment = image_render_get_h_adjustment(imageRender);
                auto current = gtk_adjustment_get_value(hAdjustment);
                auto upper = gtk_adjustment_get_upper(hAdjustment);
                auto page_size = gtk_adjustment_get_page_size(hAdjustment);
                if (current < upper - page_size)
                {
                    gtk_adjustment_set_value(hAdjustment, current + INC_VALUE);
                }
                return TRUE;
            }
            default:
                return FALSE;
        }
    }

    switch (event->direction)
    {
        case GDK_SCROLL_UP:
        {
            auto vAdjustment = image_render_get_v_adjustment(imageRender);
            auto current = gtk_adjustment_get_value(vAdjustment);
            auto lower = gtk_adjustment_get_lower(vAdjustment);
            if (current > lower)
            {
                gtk_adjustment_set_value(vAdjustment, current - INC_VALUE);
            }
            return TRUE;
        }
        case GDK_SCROLL_DOWN:
        {
            auto vAdjustment = image_render_get_v_adjustment(imageRender);
            auto current = gtk_adjustment_get_value(vAdjustment);
            auto upper = gtk_adjustment_get_upper(vAdjustment);
            auto page_size = gtk_adjustment_get_page_size(vAdjustment);
            if (current < upper - page_size)
            {
                gtk_adjustment_set_value(vAdjustment, current + INC_VALUE);
            }
            return TRUE;
        }
        default:
            return FALSE;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    return TRUE;
}


static void image_render_realize (GtkWidget *widget)
{
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

    GdkWindow *window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
    gtk_widget_set_window (widget, window);

    widget->style = gtk_style_attach (widget->style, window);
    gdk_window_set_user_data (window, widget);
    gtk_style_set_background (widget->style, window, GTK_STATE_ACTIVE);

    // image_render_prepare_disp_pixbuf (obj);
    if (!obj->priv->scaled_pixbuf_loaded)
        image_render_load_scaled_pixbuf (obj);
}


static void image_render_redraw (ImageRender *w)
{
    if (!GTK_WIDGET_REALIZED(GTK_WIDGET (w)))
        return;

    image_render_notify_status_changed (w);
    gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (w)), NULL, FALSE);
}


static void image_render_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
    requisition->width = IMAGE_RENDER_DEFAULT_WIDTH;
    requisition->height = IMAGE_RENDER_DEFAULT_HEIGHT;
}


static void image_render_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    g_return_if_fail (IS_IMAGE_RENDER (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED (widget))
    {
        gdk_window_move_resize (gtk_widget_get_window (widget), allocation->x, allocation->y, allocation->width, allocation->height);
        image_render_prepare_disp_pixbuf (IMAGE_RENDER (widget));
    }
}


static gboolean image_render_expose (GtkWidget *widget, GdkEventExpose *event)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    ImageRender *w = IMAGE_RENDER (widget);

    gdk_window_clear_area (gtk_widget_get_window (widget), 0, 0, widget->allocation.width, widget->allocation.height);

    if (!w->priv->disp_pixbuf)
        return FALSE;

    gint xc, yc;

    if (w->priv->best_fit ||
        (gdk_pixbuf_get_width (w->priv->disp_pixbuf) < widget->allocation.width &&
         gdk_pixbuf_get_height (w->priv->disp_pixbuf) < widget->allocation.height))
    {
        xc = widget->allocation.width / 2 - gdk_pixbuf_get_width (w->priv->disp_pixbuf)/2;
        yc = widget->allocation.height / 2 - gdk_pixbuf_get_height (w->priv->disp_pixbuf)/2;

        gdk_draw_pixbuf (gtk_widget_get_window (widget),
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

        if (widget->allocation.width > gdk_pixbuf_get_width (w->priv->disp_pixbuf))
        {
            src_x = 0;
            dst_x = widget->allocation.width / 2 - gdk_pixbuf_get_width (w->priv->disp_pixbuf)/2;
            width = gdk_pixbuf_get_width (w->priv->disp_pixbuf);
        }
        else
        {
            src_x = (int) gtk_adjustment_get_value (w->priv->h_adjustment);
            dst_x = 0;
            width = MIN(widget->allocation.width, gdk_pixbuf_get_width (w->priv->disp_pixbuf));
            if (src_x + width > gdk_pixbuf_get_width (w->priv->disp_pixbuf))
                src_x = gdk_pixbuf_get_width (w->priv->disp_pixbuf) - width;
        }


        if ((int) gtk_adjustment_get_value (w->priv->h_adjustment) > gdk_pixbuf_get_height (w->priv->disp_pixbuf))
        {
            src_y = 0;
            dst_y = widget->allocation.height / 2 - gdk_pixbuf_get_height (w->priv->disp_pixbuf)/2;
            height = gdk_pixbuf_get_height (w->priv->disp_pixbuf);
        }
        else
        {
            src_y = (int) gtk_adjustment_get_value (w->priv->v_adjustment);
            dst_y = 0;
            height = MIN(widget->allocation.height, gdk_pixbuf_get_height (w->priv->disp_pixbuf));

            if (src_y + height > gdk_pixbuf_get_height (w->priv->disp_pixbuf))
                src_y = gdk_pixbuf_get_height (w->priv->disp_pixbuf) - height;
        }

#if 0
        fprintf(stderr, "src(%d, %d), dst(%d, %d), size(%d, %d) origsize(%d, %d) alloc(%d, %d) ajd(%d, %d)\n",
                src_x, src_y,
                dst_x, dst_y,
                width, height,
                gdk_pixbuf_get_width (w->priv->disp_pixbuf),
                gdk_pixbuf_get_height (w->priv->disp_pixbuf),
                widget->allocation.width,
                widget->allocation.height,
                (int)w->priv->h_adjustment->value,
                (int)w->priv->v_adjustment->value);
#endif
        gdk_draw_pixbuf(gtk_widget_get_window (widget),
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
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    ImageRender *w = IMAGE_RENDER (widget);

    // TODO: Replace (1) with your on conditional for grabbing the mouse
    if (!w->priv->button && (1))
    {
        gtk_grab_add (widget);

        w->priv->button = event->button;

        // gtk_dial_update_mouse (dial, event->x, event->y);

        // Store current mouse position
        w->priv->mouseX = event->x;
        w->priv->mouseY = event->y;
    }

    return FALSE;
}


static gboolean image_render_button_release (GtkWidget *widget, GdkEventButton *event)
{
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
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    auto imageRender = IMAGE_RENDER (widget);

    if (imageRender->priv->button != 0)
    {
        GdkModifierType mods;

        gint x = event->x;
        gint y = event->y;

        GdkWindow *window = gtk_widget_get_window (widget);

        if (event->is_hint || (event->window != window))
            gdk_window_get_pointer (window, &x, &y, &mods);

        // Update X position
        auto hAdjustment = image_render_get_h_adjustment(imageRender);
        auto currentX = gtk_adjustment_get_value(hAdjustment);
        auto lowerX = gtk_adjustment_get_lower(hAdjustment);
        auto upperX = gtk_adjustment_get_upper(hAdjustment);
        auto pageSizeX = gtk_adjustment_get_page_size(hAdjustment);
        if (currentX + (imageRender->priv->mouseX - x)    < upperX - pageSizeX
            && currentX + (imageRender->priv->mouseX - x) > lowerX - pageSizeX)
        {
            gtk_adjustment_set_value(hAdjustment, currentX + (imageRender->priv->mouseX - x));
        }

        // Update Y position
        auto vAdjustment = image_render_get_v_adjustment(imageRender);
        auto currentY = gtk_adjustment_get_value(vAdjustment);
        auto lowerY = gtk_adjustment_get_lower(vAdjustment);
        auto upperY = gtk_adjustment_get_upper(vAdjustment);
        auto pageSizeY = gtk_adjustment_get_page_size(vAdjustment);
        if (currentY + (imageRender->priv->mouseY - y)    < upperY - pageSizeY
            && currentY + (imageRender->priv->mouseY - y) > lowerY - pageSizeY)
        {
            gtk_adjustment_set_value(vAdjustment, currentY + (imageRender->priv->mouseY - y));
        }

        imageRender->priv->mouseX = x;
        imageRender->priv->mouseY = y;
    }

    return FALSE;
}


static void image_render_h_adjustment_update (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    gfloat new_value = gtk_adjustment_get_value (obj->priv->h_adjustment);

    if (new_value < gtk_adjustment_get_lower (obj->priv->h_adjustment))
        new_value = gtk_adjustment_get_lower (obj->priv->h_adjustment);

    if (new_value > gtk_adjustment_get_upper (obj->priv->h_adjustment))
        new_value = gtk_adjustment_get_upper (obj->priv->h_adjustment);

    if (new_value != gtk_adjustment_get_value (obj->priv->h_adjustment))
    {
        gtk_adjustment_set_value (obj->priv->h_adjustment, new_value);
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

    if ((obj->priv->old_h_adj_value != gtk_adjustment_get_value (adjustment)) ||
        (obj->priv->old_h_adj_lower != gtk_adjustment_get_lower (adjustment)) ||
        (obj->priv->old_h_adj_upper != gtk_adjustment_get_upper (adjustment)))
    {
        image_render_h_adjustment_update (obj);

        obj->priv->old_h_adj_value = gtk_adjustment_get_value (adjustment);
        obj->priv->old_h_adj_lower = gtk_adjustment_get_lower (adjustment);
        obj->priv->old_h_adj_upper = gtk_adjustment_get_upper (adjustment);
    }
}


static void image_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    if (obj->priv->old_h_adj_value != gtk_adjustment_get_value (adjustment))
    {
        image_render_h_adjustment_update (obj);
        obj->priv->old_h_adj_value = gtk_adjustment_get_value (adjustment);
    }
}


static void image_render_v_adjustment_update (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    gfloat new_value = gtk_adjustment_get_value (obj->priv->v_adjustment);

    if (new_value < gtk_adjustment_get_lower (obj->priv->v_adjustment))
        new_value = gtk_adjustment_get_lower (obj->priv->v_adjustment);

    if (new_value > gtk_adjustment_get_upper (obj->priv->v_adjustment))
        new_value = gtk_adjustment_get_upper (obj->priv->v_adjustment);

    if (new_value != gtk_adjustment_get_value (obj->priv->v_adjustment))
    {
        gtk_adjustment_set_value (obj->priv->v_adjustment, new_value);
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

    if ((obj->priv->old_v_adj_value != gtk_adjustment_get_value (adjustment)) ||
        (obj->priv->old_v_adj_lower != gtk_adjustment_get_lower (adjustment)) ||
        (obj->priv->old_v_adj_upper != gtk_adjustment_get_upper (adjustment)))
    {
        image_render_v_adjustment_update (obj);

        obj->priv->old_v_adj_value = gtk_adjustment_get_value (adjustment);
        obj->priv->old_v_adj_lower = gtk_adjustment_get_lower (adjustment);
        obj->priv->old_v_adj_upper = gtk_adjustment_get_upper (adjustment);
    }
}


static void image_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    if (obj->priv->old_v_adj_value != gtk_adjustment_get_value (adjustment))
    {
        image_render_v_adjustment_update (obj);
        obj->priv->old_v_adj_value = gtk_adjustment_get_value (adjustment);
    }
}


static void image_render_free_pixbuf (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    image_render_wait_for_loader_thread (obj);

    obj->priv->orig_pixbuf_loaded = 0;
    obj->priv->scaled_pixbuf_loaded = FALSE;

    if (obj->priv->orig_pixbuf)
        g_object_unref (obj->priv->orig_pixbuf);
    obj->priv->orig_pixbuf = NULL;

    if (obj->priv->disp_pixbuf)
        g_object_unref (obj->priv->disp_pixbuf);
    obj->priv->disp_pixbuf = NULL;

    g_free (obj->priv->filename);
    obj->priv->filename = NULL;
}


static gpointer image_render_pixbuf_loading_thread (ImageRender *obj)
{
    GError *err = NULL;

    obj->priv->orig_pixbuf = gdk_pixbuf_new_from_file (obj->priv->filename, &err);

    g_atomic_int_inc (&obj->priv->orig_pixbuf_loaded);

    g_object_unref (obj);

    return NULL;
}


inline void image_render_wait_for_loader_thread (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER (obj));

    if (obj->priv->pixbuf_loading_thread)
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
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    g_return_if_fail (obj->priv->filename!=NULL);
    g_return_if_fail (obj->priv->scaled_pixbuf_loaded==FALSE);

    g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET (obj)));

    int width = GTK_WIDGET (obj)->allocation.width;
    int height = GTK_WIDGET (obj)->allocation.height;

    GError *err = NULL;

    obj->priv->disp_pixbuf  = gdk_pixbuf_new_from_file_at_scale (obj->priv->filename, width, height, TRUE, &err);

    if (err)
    {
        g_warning ("pixbuf loading failed: %s", err->message);
        g_error_free (err);
        obj->priv->orig_pixbuf = NULL;
        obj->priv->disp_pixbuf = NULL;
        return;
    }

    obj->priv->scaled_pixbuf_loaded = TRUE;
}


inline void image_render_start_background_pixbuf_loading (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER (obj));
    g_return_if_fail (obj->priv->filename!=NULL);

    if (obj->priv->pixbuf_loading_thread!=NULL)
        return;

    obj->priv->orig_pixbuf_loaded = 0;

    // Start background loading
    g_object_ref (obj);
    obj->priv->pixbuf_loading_thread = g_thread_new("pixbuf_load", (GThreadFunc) image_render_pixbuf_loading_thread, (gpointer) obj);
}


void image_render_load_file (ImageRender *obj, const gchar *filename)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    image_render_free_pixbuf (obj);

    g_return_if_fail (obj->priv->filename==NULL);

    obj->priv->filename = g_strdup (filename);
    obj->priv->scaled_pixbuf_loaded = FALSE;
    obj->priv->orig_pixbuf_loaded = 0;
}


static void image_render_prepare_disp_pixbuf (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    // this will be set only if the loader thread finished loading the big pixbuf
    if (g_atomic_int_get (&obj->priv->orig_pixbuf_loaded)==0)
        return;

    if (!GTK_WIDGET_REALIZED (GTK_WIDGET (obj)))
        return;

    if (obj->priv->disp_pixbuf)
        g_object_unref (obj->priv->disp_pixbuf);
    obj->priv->disp_pixbuf = NULL;

    if (gdk_pixbuf_get_height (obj->priv->orig_pixbuf)==0)
        return;

    if (obj->priv->best_fit)
    {
        if (gdk_pixbuf_get_height (obj->priv->orig_pixbuf) < GTK_WIDGET (obj)->allocation.height &&
            gdk_pixbuf_get_width (obj->priv->orig_pixbuf) < GTK_WIDGET (obj)->allocation.width)
        {
            // no need to scale down

            obj->priv->disp_pixbuf = obj->priv->orig_pixbuf;
            g_object_ref (obj->priv->disp_pixbuf);
            return;
        }

        int height = GTK_WIDGET (obj)->allocation.height;
        int width = (((double) GTK_WIDGET (obj)->allocation.height) /
                  gdk_pixbuf_get_height (obj->priv->orig_pixbuf))*
                    gdk_pixbuf_get_width (obj->priv->orig_pixbuf);

        if (width >= GTK_WIDGET (obj)->allocation.width)
        {
            width = GTK_WIDGET (obj)->allocation.width;
            height = (((double)GTK_WIDGET (obj)->allocation.width) /
                  gdk_pixbuf_get_width (obj->priv->orig_pixbuf))*
                    gdk_pixbuf_get_height (obj->priv->orig_pixbuf);
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
                (int)(gdk_pixbuf_get_width (obj->priv->orig_pixbuf) * obj->priv->scale_factor),
                (int)(gdk_pixbuf_get_height (obj->priv->orig_pixbuf) * obj->priv->scale_factor),
                GDK_INTERP_NEAREST);
    }

    image_render_update_adjustments (obj);
}


static void image_render_update_adjustments (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    if (!obj->priv->disp_pixbuf)
        return;

    if (obj->priv->best_fit ||
        (gdk_pixbuf_get_width (obj->priv->disp_pixbuf) < GTK_WIDGET (obj)->allocation.width  &&
         gdk_pixbuf_get_height (obj->priv->disp_pixbuf) < GTK_WIDGET (obj)->allocation.height))
    {
        if (obj->priv->h_adjustment)
        {
            gtk_adjustment_set_lower (obj->priv->h_adjustment, 0);
            gtk_adjustment_set_upper (obj->priv->h_adjustment, 0);
            gtk_adjustment_set_value (obj->priv->h_adjustment, 0);
            gtk_adjustment_changed (obj->priv->h_adjustment);
        }
        if (obj->priv->v_adjustment)
        {
            gtk_adjustment_set_lower (obj->priv->v_adjustment, 0);
            gtk_adjustment_set_upper (obj->priv->v_adjustment, 0);
            gtk_adjustment_set_value (obj->priv->v_adjustment, 0);
            gtk_adjustment_changed (obj->priv->v_adjustment);
        }
    }
    else
    {
        if (obj->priv->h_adjustment)
        {
            gtk_adjustment_set_lower (obj->priv->h_adjustment, 0);
            gtk_adjustment_set_upper (obj->priv->h_adjustment, gdk_pixbuf_get_width (obj->priv->disp_pixbuf));
            gtk_adjustment_set_page_size (obj->priv->h_adjustment, GTK_WIDGET (obj)->allocation.width);
            gtk_adjustment_changed (obj->priv->h_adjustment);
        }
        if (obj->priv->v_adjustment)
        {
            gtk_adjustment_set_lower (obj->priv->v_adjustment, 0);
            gtk_adjustment_set_upper (obj->priv->v_adjustment, gdk_pixbuf_get_height (obj->priv->disp_pixbuf));
            gtk_adjustment_set_page_size (obj->priv->v_adjustment, GTK_WIDGET (obj)->allocation.height);
            gtk_adjustment_changed (obj->priv->v_adjustment);
        }
    }
}


void image_render_set_best_fit(ImageRender *obj, gboolean active)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    obj->priv->best_fit = active;
    image_render_prepare_disp_pixbuf (obj);
    image_render_redraw (obj);
}


gboolean image_render_get_best_fit(ImageRender *obj)
{
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), FALSE);

    return obj->priv->best_fit;
}


void image_render_set_scale_factor(ImageRender *obj, double scalefactor)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));

    obj->priv->scale_factor = scalefactor;
    image_render_prepare_disp_pixbuf (obj);
    image_render_redraw(obj);
}


double image_render_get_scale_factor(ImageRender *obj)
{
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), 1);

    return obj->priv->scale_factor;
}


void image_render_operation(ImageRender *obj, ImageRender::DISPLAYMODE op)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    g_return_if_fail (obj->priv->orig_pixbuf);

    GdkPixbuf *temp = NULL;

    switch (op)
    {
        case ImageRender::ROTATE_CLOCKWISE:
            temp = gdk_pixbuf_rotate_simple (obj->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
            break;
        case ImageRender::ROTATE_COUNTERCLOCKWISE:
            temp = gdk_pixbuf_rotate_simple (obj->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
            break;
        case ImageRender::ROTATE_UPSIDEDOWN:
            temp = gdk_pixbuf_rotate_simple (obj->priv->orig_pixbuf, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
            break;
        case ImageRender::FLIP_VERTICAL:
            temp = gdk_pixbuf_flip (obj->priv->orig_pixbuf, FALSE);
            break;
        case ImageRender::FLIP_HORIZONTAL:
            temp = gdk_pixbuf_flip (obj->priv->orig_pixbuf, TRUE);
            break;
        default:
            g_return_if_fail (!"Unknown image operation");
    }

    g_object_unref (obj->priv->orig_pixbuf);

    obj->priv->orig_pixbuf = temp;

    image_render_prepare_disp_pixbuf (obj);
}
