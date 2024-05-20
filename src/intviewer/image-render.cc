/**
 * @file image-render.cc
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
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY
};

enum {
  IMAGE_STATUS_CHANGED,
  LAST_SIGNAL
};

static guint image_render_signals[LAST_SIGNAL] = { 0 };


struct ImageRenderClass
{
    GtkDrawingAreaClass parent_class;
    void (*image_status_changed)  (ImageRender *obj, ImageRender::Status *status);
};

// Class Private Data
struct ImageRenderPrivate
{
    guint8 button; // The button pressed in "button_press_event"
    gint mouseX;   // Old x position before mouse move
    gint mouseY;   // Old y position before mouse move

    GtkAdjustment *h_adjustment;
    GtkScrollablePolicy hscroll_policy;

    GtkAdjustment *v_adjustment;
    GtkScrollablePolicy vscroll_policy;

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
static void image_render_redraw (ImageRender *w);

static gboolean image_render_key_press (GtkWidget *widget, GdkEventKey *event);
static gboolean image_render_scroll(GtkWidget *widget, GdkEventScroll *event);

static void image_render_realize (GtkWidget *widget, gpointer user_data);
static void image_render_get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width);
static void image_render_get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height);
static void image_render_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean image_render_draw (GtkWidget *widget, cairo_t *cr);
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


G_DEFINE_TYPE_EXTENDED (ImageRender,
                        image_render,
                        GTK_TYPE_DRAWING_AREA,
                        0,
                        G_ADD_PRIVATE (ImageRender)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))


static void image_render_set_h_adjustment (ImageRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    if (priv->h_adjustment)
    {
        g_signal_handlers_disconnect_matched (priv->h_adjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, obj);
        g_object_unref (priv->h_adjustment);
    }

    priv->h_adjustment = adjustment ? adjustment : gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    g_object_ref (priv->h_adjustment);

    gtk_adjustment_set_lower (priv->h_adjustment, 0.0);
    gtk_adjustment_set_upper (priv->h_adjustment, priv->disp_pixbuf ? gdk_pixbuf_get_width (priv->disp_pixbuf) : 0.0);
    gtk_adjustment_set_step_increment (priv->h_adjustment, INC_VALUE);
    gtk_adjustment_set_page_increment (priv->h_adjustment, 100.0);
    gtk_adjustment_set_page_size (priv->h_adjustment, 100.0);

    g_signal_connect (priv->h_adjustment, "changed", G_CALLBACK (image_render_h_adjustment_changed), obj);
    g_signal_connect (priv->h_adjustment, "value-changed", G_CALLBACK (image_render_h_adjustment_value_changed), obj);

    image_render_h_adjustment_update (obj);
}


static void image_render_set_v_adjustment (ImageRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    if (priv->v_adjustment)
    {
        g_signal_handlers_disconnect_matched (priv->v_adjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, obj);
        g_object_unref (priv->v_adjustment);
    }

    priv->v_adjustment = adjustment ? adjustment : gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    g_object_ref (priv->v_adjustment);

    gtk_adjustment_set_lower (priv->v_adjustment, 0.0);
    gtk_adjustment_set_upper (priv->v_adjustment, priv->disp_pixbuf ? gdk_pixbuf_get_height (priv->disp_pixbuf) : 0.0);
    gtk_adjustment_set_step_increment (priv->v_adjustment, INC_VALUE);
    gtk_adjustment_set_page_increment (priv->v_adjustment, 100.0);
    gtk_adjustment_set_page_size (priv->v_adjustment, 100.0);

    g_signal_connect (priv->v_adjustment, "changed", G_CALLBACK (image_render_v_adjustment_changed), obj);
    g_signal_connect (priv->v_adjustment, "value-changed",  G_CALLBACK (image_render_v_adjustment_value_changed), obj);

    image_render_v_adjustment_update (obj);
}


static void image_render_init (ImageRender *w)
{
    gtk_widget_add_events (GTK_WIDGET (w),
        GDK_EXPOSURE_MASK |
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
        GDK_SCROLL_MASK |
        GDK_KEY_PRESS_MASK);

    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (w));

    priv->button = 0;

    priv->scaled_pixbuf_loaded = FALSE;
    priv->filename = NULL;

    priv->h_adjustment = NULL;
    priv->hscroll_policy = GTK_SCROLL_MINIMUM;

    priv->v_adjustment = NULL;
    priv->vscroll_policy = GTK_SCROLL_MINIMUM;

    priv->best_fit = FALSE;
    priv->scale_factor = 1.3;
    priv->orig_pixbuf = NULL;
    priv->disp_pixbuf = NULL;

    gtk_widget_set_can_focus (GTK_WIDGET (w), TRUE);

    g_signal_connect_after (w, "realize", G_CALLBACK (image_render_realize), w);
}


static void image_render_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (IMAGE_RENDER (obj)));
    switch (prop_id)
    {
        case PROP_HADJUSTMENT:
            g_value_set_object (value, priv->h_adjustment);
            break;
        case PROP_VADJUSTMENT:
            g_value_set_object (value, priv->v_adjustment);
            break;
        case PROP_HSCROLL_POLICY:
            g_value_set_enum (value, priv->hscroll_policy);
            break;
        case PROP_VSCROLL_POLICY:
            g_value_set_enum (value, priv->vscroll_policy);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}


static void image_render_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    ImageRender *w = IMAGE_RENDER (obj);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (w));
    switch (prop_id)
    {
        case PROP_HADJUSTMENT:
            image_render_set_h_adjustment (w, GTK_ADJUSTMENT (g_value_get_object (value)));
            break;
        case PROP_VADJUSTMENT:
            image_render_set_v_adjustment (w, GTK_ADJUSTMENT (g_value_get_object (value)));
            break;
        case PROP_HSCROLL_POLICY:
            {
                GtkScrollablePolicy policy = static_cast<GtkScrollablePolicy> (g_value_get_enum (value));
                if (priv->hscroll_policy != policy)
                {
                    priv->hscroll_policy = policy;
                    gtk_widget_queue_resize (GTK_WIDGET (w));
                    g_object_notify_by_pspec (obj, pspec);
                }
            }
            break;
        case PROP_VSCROLL_POLICY:
            {
                GtkScrollablePolicy policy = static_cast<GtkScrollablePolicy> (g_value_get_enum (value));
                if (priv->vscroll_policy != policy)
                {
                    priv->vscroll_policy = policy;
                    gtk_widget_queue_resize (GTK_WIDGET (w));
                    g_object_notify_by_pspec (obj, pspec);
                }
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}


static void image_render_finalize (GObject *object)
{
    ImageRender *w = IMAGE_RENDER (object);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (w));

    /* there are TWO references to the ImageRender object:
        one in the parent widget, the other in the loader thread.

        "Destroy" might be called twice (don't know why, yet) - if the viewer is closed (by the user)
        before the loader thread finishes.

       If this is the case, we don't want to block while waiting for the loader thread (bad user responsiveness).
       So if "destroy" is called while the loader thread is still running, we simply exit, know "destroy" will be called again
      once the loader thread is done (because it calls "g_object_unref" on the ImageRender object).
    */
    if (priv->pixbuf_loading_thread && g_atomic_int_get (&priv->orig_pixbuf_loaded)==0)
    {
            // Loader thread still running, so do nothing
    }
    else
    {
        image_render_free_pixbuf (w);

        g_clear_object (&priv->v_adjustment);
        g_clear_object (&priv->h_adjustment);
    }

    G_OBJECT_CLASS (image_render_parent_class)->finalize (object);
}


static void image_render_class_init (ImageRenderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->set_property = image_render_set_property;
    object_class->get_property = image_render_get_property;
    object_class->finalize = image_render_finalize;

    widget_class->scroll_event = image_render_scroll;
    widget_class->key_press_event = image_render_key_press;
    widget_class->button_press_event = image_render_button_press;
    widget_class->button_release_event = image_render_button_release;
    widget_class->motion_notify_event = image_render_motion_notify;

    widget_class->draw = image_render_draw;

    widget_class->get_preferred_width = image_render_get_preferred_width;
    widget_class->get_preferred_height = image_render_get_preferred_height;
    widget_class->size_allocate = image_render_size_allocate;

    g_object_class_override_property (object_class, PROP_HADJUSTMENT,    "hadjustment");
    g_object_class_override_property (object_class, PROP_VADJUSTMENT,    "vadjustment");
    g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
    g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

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
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (w));

    ImageRender::Status stat;

    memset(&stat, 0, sizeof(stat));

    stat.best_fit = priv->best_fit;
    stat.scale_factor = priv->scale_factor;

    if (priv->orig_pixbuf)
    {
        stat.image_width = gdk_pixbuf_get_width (priv->orig_pixbuf);
        stat.image_height = gdk_pixbuf_get_height (priv->orig_pixbuf);
        stat.bits_per_sample = gdk_pixbuf_get_bits_per_sample (priv->orig_pixbuf);
    }

    g_signal_emit (w, image_render_signals[IMAGE_STATUS_CHANGED], 0, &stat);
}


static gboolean image_render_key_press (GtkWidget *widget, GdkEventKey *event)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);

    auto imageRender = IMAGE_RENDER (widget);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (imageRender));

    switch (event->keyval)
    {
        case GDK_KEY_Up:
        {
            auto current = gtk_adjustment_get_value(priv->v_adjustment);
            auto lower = gtk_adjustment_get_lower(priv->v_adjustment);
            if (current - INC_VALUE > lower)
            {
                gtk_adjustment_set_value(priv->v_adjustment, current - INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(priv->v_adjustment, lower);
            }

            return TRUE;
        }
        case GDK_KEY_Down:
        {
            auto current = gtk_adjustment_get_value(priv->v_adjustment);
            auto upper = gtk_adjustment_get_upper(priv->v_adjustment);
            auto page_size = gtk_adjustment_get_page_size(priv->v_adjustment);
            if (current + INC_VALUE < upper - page_size)
            {
                gtk_adjustment_set_value(priv->v_adjustment, current + INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(priv->v_adjustment, upper - page_size);
            }
            return TRUE;
        }
        case GDK_KEY_Left:
        {
            auto current = gtk_adjustment_get_value(priv->h_adjustment);
            auto lower = gtk_adjustment_get_lower(priv->h_adjustment);
            if (current - INC_VALUE > lower)
            {
                gtk_adjustment_set_value(priv->h_adjustment, current - INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(priv->h_adjustment, lower);
            }

            return TRUE;
        }
        case GDK_KEY_Right:
        {
            auto current = gtk_adjustment_get_value(priv->h_adjustment);
            auto upper = gtk_adjustment_get_upper(priv->h_adjustment);
            auto page_size = gtk_adjustment_get_page_size(priv->h_adjustment);
            if (current + INC_VALUE < upper - page_size)
            {
                gtk_adjustment_set_value(priv->h_adjustment, current + INC_VALUE);
            }
            else
            {
                gtk_adjustment_set_value(priv->h_adjustment, upper - page_size);
            }

            return TRUE;
        }

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
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (imageRender));

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
                auto current = gtk_adjustment_get_value(priv->h_adjustment);
                auto lower = gtk_adjustment_get_lower(priv->h_adjustment);
                if (current > lower)
                {
                    gtk_adjustment_set_value(priv->h_adjustment, current - INC_VALUE);
                }
                return TRUE;
            }
            case GDK_SCROLL_DOWN:
            {
                auto current = gtk_adjustment_get_value(priv->h_adjustment);
                auto upper = gtk_adjustment_get_upper(priv->h_adjustment);
                auto page_size = gtk_adjustment_get_page_size(priv->h_adjustment);
                if (current < upper - page_size)
                {
                    gtk_adjustment_set_value(priv->h_adjustment, current + INC_VALUE);
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
            auto current = gtk_adjustment_get_value(priv->v_adjustment);
            auto lower = gtk_adjustment_get_lower(priv->v_adjustment);
            if (current > lower)
            {
                gtk_adjustment_set_value(priv->v_adjustment, current - INC_VALUE);
            }
            return TRUE;
        }
        case GDK_SCROLL_DOWN:
        {
            auto current = gtk_adjustment_get_value(priv->v_adjustment);
            auto upper = gtk_adjustment_get_upper(priv->v_adjustment);
            auto page_size = gtk_adjustment_get_page_size(priv->v_adjustment);
            if (current < upper - page_size)
            {
                gtk_adjustment_set_value(priv->v_adjustment, current + INC_VALUE);
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


static void image_render_realize (GtkWidget *widget, gpointer user_data)
{
    ImageRender *obj = IMAGE_RENDER (widget);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    if (!priv->scaled_pixbuf_loaded)
        image_render_load_scaled_pixbuf (obj);
}


static void image_render_redraw (ImageRender *w)
{
    if (!gtk_widget_get_realized (GTK_WIDGET (w)))
        return;

    image_render_notify_status_changed (w);
    gtk_widget_queue_draw (GTK_WIDGET (w));
}


static void image_render_get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width)
{
    *minimal_width = *natural_width = IMAGE_RENDER_DEFAULT_WIDTH;
}


static void image_render_get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height)
{
    *minimal_height = *natural_height = IMAGE_RENDER_DEFAULT_HEIGHT;
}


static void image_render_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    g_return_if_fail (IS_IMAGE_RENDER (widget));
    g_return_if_fail (allocation != NULL);

    gtk_widget_set_allocation (widget, allocation);

    if (gtk_widget_get_realized (widget))
    {
        gdk_window_move_resize (gtk_widget_get_window (widget), allocation->x, allocation->y, allocation->width, allocation->height);
        image_render_prepare_disp_pixbuf (IMAGE_RENDER (widget));
    }
}


static gboolean image_render_draw (GtkWidget *widget, cairo_t *cr)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (cr != NULL, FALSE);

    ImageRender *w = IMAGE_RENDER (widget);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (w));

    GtkAllocation widget_allocation;
    gtk_widget_get_allocation (widget, &widget_allocation);

    if (!priv->disp_pixbuf)
        return FALSE;

    gint xc, yc;

    if (priv->best_fit ||
        (gdk_pixbuf_get_width (priv->disp_pixbuf) < widget_allocation.width &&
         gdk_pixbuf_get_height (priv->disp_pixbuf) < widget_allocation.height))
    {
        xc = widget_allocation.width / 2 - gdk_pixbuf_get_width (priv->disp_pixbuf)/2;
        yc = widget_allocation.height / 2 - gdk_pixbuf_get_height (priv->disp_pixbuf)/2;

        cairo_t *cr = gdk_cairo_create (gtk_widget_get_window (GTK_WIDGET (widget)));
        cairo_translate (cr, xc, yc);
        gdk_cairo_set_source_pixbuf (cr, priv->disp_pixbuf, 0.0, 0.0);
        cairo_paint (cr);
        cairo_destroy (cr);
    }
    else
    {
        gint src_x, src_y;
        gint dst_x, dst_y;
        gint width, height;

        if (widget_allocation.width > gdk_pixbuf_get_width (priv->disp_pixbuf))
        {
            src_x = 0;
            dst_x = widget_allocation.width / 2 - gdk_pixbuf_get_width (priv->disp_pixbuf)/2;
            width = gdk_pixbuf_get_width (priv->disp_pixbuf);
        }
        else
        {
            src_x = (int) gtk_adjustment_get_value (priv->h_adjustment);
            dst_x = 0;
            width = MIN(widget_allocation.width, gdk_pixbuf_get_width (priv->disp_pixbuf));
            if (src_x + width > gdk_pixbuf_get_width (priv->disp_pixbuf))
                src_x = gdk_pixbuf_get_width (priv->disp_pixbuf) - width;
        }


        if ((int) gtk_adjustment_get_value (priv->h_adjustment) > gdk_pixbuf_get_height (priv->disp_pixbuf))
        {
            src_y = 0;
            dst_y = widget_allocation.height / 2 - gdk_pixbuf_get_height (priv->disp_pixbuf)/2;
            height = gdk_pixbuf_get_height (priv->disp_pixbuf);
        }
        else
        {
            src_y = (int) gtk_adjustment_get_value (priv->v_adjustment);
            dst_y = 0;
            height = MIN(widget_allocation.height, gdk_pixbuf_get_height (priv->disp_pixbuf));

            if (src_y + height > gdk_pixbuf_get_height (priv->disp_pixbuf))
                src_y = gdk_pixbuf_get_height (priv->disp_pixbuf) - height;
        }

#if 0
        fprintf(stderr, "src(%d, %d), dst(%d, %d), size(%d, %d) origsize(%d, %d) alloc(%d, %d) ajd(%d, %d)\n",
                src_x, src_y,
                dst_x, dst_y,
                width, height,
                gdk_pixbuf_get_width (w->priv->disp_pixbuf),
                gdk_pixbuf_get_height (w->priv->disp_pixbuf),
                widget_allocation.width,
                widget_allocation.height,
                (int)w->priv->h_adjustment->value,
                (int)w->priv->v_adjustment->value);
#endif
        cairo_translate (cr, -src_x, -src_y);
        gdk_cairo_set_source_pixbuf (cr, priv->disp_pixbuf, dst_x, dst_y);
        cairo_paint (cr);
    }

    if (g_atomic_int_get (&priv->orig_pixbuf_loaded)==0)
        image_render_start_background_pixbuf_loading (w);
    return FALSE;
}


static gboolean image_render_button_press (GtkWidget *widget, GdkEventButton *event)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    ImageRender *w = IMAGE_RENDER (widget);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (w));

    // TODO: Replace (1) with your on conditional for grabbing the mouse
    if (!priv->button && (1))
    {
        gtk_grab_add (widget);

        priv->button = event->button;

        // gtk_dial_update_mouse (dial, event->x, event->y);

        // Store current mouse position
        priv->mouseX = event->x;
        priv->mouseY = event->y;
    }

    return FALSE;
}


static gboolean image_render_button_release (GtkWidget *widget, GdkEventButton *event)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    ImageRender *w = IMAGE_RENDER (widget);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (w));

    if (priv->button == event->button)
    {
        gtk_grab_remove (widget);

        priv->button = 0;
    }

    return FALSE;
}


static gboolean image_render_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
    g_return_val_if_fail (IS_IMAGE_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    auto imageRender = IMAGE_RENDER (widget);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (imageRender));

    if (priv->button != 0)
    {
        GdkModifierType mods;

        gint x = event->x;
        gint y = event->y;

        GdkWindow *window = gtk_widget_get_window (widget);

        if (event->is_hint || (event->window != window))
            gdk_window_get_pointer (window, &x, &y, &mods);

        // Update X position
        auto currentX = gtk_adjustment_get_value(priv->h_adjustment);
        auto lowerX = gtk_adjustment_get_lower(priv->h_adjustment);
        auto upperX = gtk_adjustment_get_upper(priv->h_adjustment);
        auto pageSizeX = gtk_adjustment_get_page_size(priv->h_adjustment);
        if (currentX + (priv->mouseX - x)    < upperX - pageSizeX
            && currentX + (priv->mouseX - x) > lowerX - pageSizeX)
        {
            gtk_adjustment_set_value(priv->h_adjustment, currentX + (priv->mouseX - x));
        }

        // Update Y position
        auto currentY = gtk_adjustment_get_value(priv->v_adjustment);
        auto lowerY = gtk_adjustment_get_lower(priv->v_adjustment);
        auto upperY = gtk_adjustment_get_upper(priv->v_adjustment);
        auto pageSizeY = gtk_adjustment_get_page_size(priv->v_adjustment);
        if (currentY + (priv->mouseY - y)    < upperY - pageSizeY
            && currentY + (priv->mouseY - y) > lowerY - pageSizeY)
        {
            gtk_adjustment_set_value(priv->v_adjustment, currentY + (priv->mouseY - y));
        }

        priv->mouseX = x;
        priv->mouseY = y;
    }

    return FALSE;
}


static void image_render_h_adjustment_update (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    gfloat new_value = gtk_adjustment_get_value (priv->h_adjustment);

    if (new_value < gtk_adjustment_get_lower (priv->h_adjustment))
        new_value = gtk_adjustment_get_lower (priv->h_adjustment);

    if (new_value > gtk_adjustment_get_upper (priv->h_adjustment))
        new_value = gtk_adjustment_get_upper (priv->h_adjustment);

    if (new_value != gtk_adjustment_get_value (priv->h_adjustment))
    {
        gtk_adjustment_set_value (priv->h_adjustment, new_value);
        g_signal_emit_by_name (priv->h_adjustment, "value-changed");
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

    image_render_h_adjustment_update (obj);
}


static void image_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    image_render_h_adjustment_update (obj);
}


static void image_render_v_adjustment_update (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    gfloat new_value = gtk_adjustment_get_value (priv->v_adjustment);

    if (new_value < gtk_adjustment_get_lower (priv->v_adjustment))
        new_value = gtk_adjustment_get_lower (priv->v_adjustment);

    if (new_value > gtk_adjustment_get_upper (priv->v_adjustment))
        new_value = gtk_adjustment_get_upper (priv->v_adjustment);

    if (new_value != gtk_adjustment_get_value (priv->v_adjustment))
    {
        gtk_adjustment_set_value (priv->v_adjustment, new_value);
        g_signal_emit_by_name (priv->v_adjustment, "value-changed");
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

    image_render_v_adjustment_update (obj);
}


static void image_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    ImageRender *obj = IMAGE_RENDER (data);

    image_render_v_adjustment_update (obj);
}


static void image_render_free_pixbuf (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    image_render_wait_for_loader_thread (obj);

    priv->orig_pixbuf_loaded = 0;
    priv->scaled_pixbuf_loaded = FALSE;
    g_clear_object (&priv->orig_pixbuf);
    g_clear_object (&priv->disp_pixbuf);
    g_clear_pointer (&priv->filename, g_free);
}


static gpointer image_render_pixbuf_loading_thread (ImageRender *obj)
{
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    GError *err = NULL;

    priv->orig_pixbuf = gdk_pixbuf_new_from_file (priv->filename, &err);

    g_atomic_int_inc (&priv->orig_pixbuf_loaded);

    g_object_unref (obj);

    return NULL;
}


inline void image_render_wait_for_loader_thread (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER (obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    if (priv->pixbuf_loading_thread)
    {
        /*
            ugly hack: use a busy wait loop, until the loader thread is done.

            rational: the loader thread might be running after the widget is destroyed (if the user closed the viewer very quickly,
                      and the loader is still reading a very large image). If this happens, this (and all the 'destroy' functions)
                      will be called from the loader thread's context, and using g_thread_join() will crash the application.
        */

        while (g_atomic_int_get (&priv->orig_pixbuf_loaded)==0)
            g_usleep(1000);

        priv->orig_pixbuf_loaded = 0;
        priv->pixbuf_loading_thread = NULL;
    }
}


void image_render_load_scaled_pixbuf (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));
    g_return_if_fail (priv->filename!=NULL);
    g_return_if_fail (priv->scaled_pixbuf_loaded==FALSE);

    g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (obj)));

    GtkAllocation obj_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (obj), &obj_allocation);

    GError *err = NULL;

    priv->disp_pixbuf = gdk_pixbuf_new_from_file_at_scale (priv->filename, obj_allocation.width, obj_allocation.height, TRUE, &err);

    if (err)
    {
        g_warning ("pixbuf loading failed: %s", err->message);
        g_error_free (err);
        priv->orig_pixbuf = NULL;
        priv->disp_pixbuf = NULL;
        return;
    }

    priv->scaled_pixbuf_loaded = TRUE;
}


inline void image_render_start_background_pixbuf_loading (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER (obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));
    g_return_if_fail (priv->filename!=NULL);

    if (priv->pixbuf_loading_thread!=NULL)
        return;

    priv->orig_pixbuf_loaded = 0;

    // Start background loading
    g_object_ref (obj);
    priv->pixbuf_loading_thread = g_thread_new("pixbuf_load", (GThreadFunc) image_render_pixbuf_loading_thread, (gpointer) obj);
}


void image_render_load_file (ImageRender *obj, const gchar *filename)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    image_render_free_pixbuf (obj);

    g_return_if_fail (priv->filename==NULL);

    priv->filename = g_strdup (filename);
    priv->scaled_pixbuf_loaded = FALSE;
    priv->orig_pixbuf_loaded = 0;
}


static void image_render_prepare_disp_pixbuf (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    // this will be set only if the loader thread finished loading the big pixbuf
    if (g_atomic_int_get (&priv->orig_pixbuf_loaded)==0)
        return;

    if (!gtk_widget_get_realized (GTK_WIDGET (obj)))
        return;

    g_clear_object (&priv->disp_pixbuf);

    if (gdk_pixbuf_get_height (priv->orig_pixbuf)==0)
        return;

    if (priv->best_fit)
    {
        GtkAllocation obj_allocation;
        gtk_widget_get_allocation (GTK_WIDGET (obj), &obj_allocation);

        if (gdk_pixbuf_get_height (priv->orig_pixbuf) < obj_allocation.height &&
            gdk_pixbuf_get_width (priv->orig_pixbuf) < obj_allocation.width)
        {
            // no need to scale down

            priv->disp_pixbuf = priv->orig_pixbuf;
            g_object_ref (priv->disp_pixbuf);
            return;
        }

        int height = obj_allocation.height;
        int width = (((double) obj_allocation.height) /
                  gdk_pixbuf_get_height (priv->orig_pixbuf))*
                    gdk_pixbuf_get_width (priv->orig_pixbuf);

        if (width >= obj_allocation.width)
        {
            width = obj_allocation.width;
            height = (((double)obj_allocation.width) /
                  gdk_pixbuf_get_width (priv->orig_pixbuf))*
                    gdk_pixbuf_get_height (priv->orig_pixbuf);
        }

        if (width<=1 || height<=1)
        {
            priv->disp_pixbuf = NULL;
            return;
        }

        priv->disp_pixbuf = gdk_pixbuf_scale_simple (priv->orig_pixbuf, width, height, GDK_INTERP_NEAREST);
    }
    else
    {
        // not "best_fit" = scaling mode
        priv->disp_pixbuf = gdk_pixbuf_scale_simple(
                priv->orig_pixbuf,
                (int)(gdk_pixbuf_get_width (priv->orig_pixbuf) * priv->scale_factor),
                (int)(gdk_pixbuf_get_height (priv->orig_pixbuf) * priv->scale_factor),
                GDK_INTERP_NEAREST);
    }

    image_render_update_adjustments (obj);
}


static void image_render_update_adjustments (ImageRender *obj)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    if (!priv->disp_pixbuf)
        return;

    GtkAllocation obj_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (obj), &obj_allocation);

    if (priv->best_fit ||
        (gdk_pixbuf_get_width (priv->disp_pixbuf) < obj_allocation.width  &&
         gdk_pixbuf_get_height (priv->disp_pixbuf) < obj_allocation.height))
    {
        if (priv->h_adjustment)
        {
            gtk_adjustment_set_lower (priv->h_adjustment, 0);
            gtk_adjustment_set_upper (priv->h_adjustment, 0);
            gtk_adjustment_set_value (priv->h_adjustment, 0);
            gtk_adjustment_changed (priv->h_adjustment);
        }
        if (priv->v_adjustment)
        {
            gtk_adjustment_set_lower (priv->v_adjustment, 0);
            gtk_adjustment_set_upper (priv->v_adjustment, 0);
            gtk_adjustment_set_value (priv->v_adjustment, 0);
            gtk_adjustment_changed (priv->v_adjustment);
        }
    }
    else
    {
        if (priv->h_adjustment)
        {
            gtk_adjustment_set_lower (priv->h_adjustment, 0);
            gtk_adjustment_set_upper (priv->h_adjustment, gdk_pixbuf_get_width (priv->disp_pixbuf));
            gtk_adjustment_set_page_size (priv->h_adjustment, obj_allocation.width);
            gtk_adjustment_changed (priv->h_adjustment);
        }
        if (priv->v_adjustment)
        {
            gtk_adjustment_set_lower (priv->v_adjustment, 0);
            gtk_adjustment_set_upper (priv->v_adjustment, gdk_pixbuf_get_height (priv->disp_pixbuf));
            gtk_adjustment_set_page_size (priv->v_adjustment, obj_allocation.height);
            gtk_adjustment_changed (priv->v_adjustment);
        }
    }
}


void image_render_set_best_fit(ImageRender *obj, gboolean active)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    priv->best_fit = active;
    image_render_prepare_disp_pixbuf (obj);
    image_render_redraw (obj);
}


gboolean image_render_get_best_fit(ImageRender *obj)
{
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), FALSE);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    return priv->best_fit;
}


void image_render_set_scale_factor(ImageRender *obj, double scalefactor)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    priv->scale_factor = scalefactor;
    image_render_prepare_disp_pixbuf (obj);
    image_render_redraw(obj);
}


double image_render_get_scale_factor(ImageRender *obj)
{
    g_return_val_if_fail (IS_IMAGE_RENDER(obj), 1);
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));

    return priv->scale_factor;
}


void image_render_operation(ImageRender *obj, ImageRender::DISPLAYMODE op)
{
    g_return_if_fail (IS_IMAGE_RENDER(obj));
    auto priv = static_cast<ImageRenderPrivate*>(image_render_get_instance_private (obj));
    g_return_if_fail (priv->orig_pixbuf);

    GdkPixbuf *temp = NULL;

    switch (op)
    {
        case ImageRender::ROTATE_CLOCKWISE:
            temp = gdk_pixbuf_rotate_simple (priv->orig_pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
            break;
        case ImageRender::ROTATE_COUNTERCLOCKWISE:
            temp = gdk_pixbuf_rotate_simple (priv->orig_pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
            break;
        case ImageRender::ROTATE_UPSIDEDOWN:
            temp = gdk_pixbuf_rotate_simple (priv->orig_pixbuf, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
            break;
        case ImageRender::FLIP_VERTICAL:
            temp = gdk_pixbuf_flip (priv->orig_pixbuf, FALSE);
            break;
        case ImageRender::FLIP_HORIZONTAL:
            temp = gdk_pixbuf_flip (priv->orig_pixbuf, TRUE);
            break;
        default:
            g_return_if_fail (!"Unknown image operation");
    }

    g_object_unref (priv->orig_pixbuf);

    priv->orig_pixbuf = temp;

    image_render_prepare_disp_pixbuf (obj);
}
