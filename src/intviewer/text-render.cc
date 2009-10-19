/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
        GNOME Commander - A GNOME based file manager
        Copyright (C) 2001-2006 Marcus Bjurman
        Copyright (C) 2007-2009 Piotr Eljasiak

    Partly based on "gtkhex.c" module from "GHex2" Project.
    Author: Jaka Mocnik <jaka@gnu.org>

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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmarshal.h>

#include "gvtypes.h"
#include "fileops.h"
#include "inputmodes.h"
#include "datapresentation.h"
#include "text-render.h"

using namespace std;


#define HEXDUMP_FIXED_LIMIT 16
#define MAX_CLIPBOARD_COPY_LENGTH 0xFFFFFF

#define NEED_PANGO_ESCAPING(x) ((x)=='<' || (x)=='>' || (x)=='&')

static GtkWidgetClass *parent_class = NULL;

enum
{
  TEXT_STATUS_CHANGED,
  LAST_SIGNAL
};

static guint text_render_signals[LAST_SIGNAL] = { 0 };

typedef int (*display_line_proc)(TextRender *w, int y, int column, offset_type start_of_line, offset_type end_of_line);
typedef offset_type (*pixel_to_offset_proc) (TextRender *obj, int x, int y, gboolean start_marker);
typedef void (*copy_to_clipboard_proc)(TextRender *obj, offset_type start_offset, offset_type end_offset);


// Class Private Data
struct TextRenderPrivate
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

    ViewerFileOps *fops;
    GVInputModesData *im;
    GVDataPresentation *dp;

    gchar *encoding;
    int tab_size;
    int fixed_limit;
    int font_size;
    gboolean wrapmode;
    int column;
    int max_column;
    offset_type current_offset;
    offset_type last_displayed_offset;
    TEXTDISPLAYMODE dispmode;
    gboolean hex_offset_display;
    gchar *fixed_font_name;

    // Pango and Fonts variables
    gint    char_width;
    gint     chars_per_line;
    gint    char_height;
    gint     lines_displayed;
    PangoFontMetrics *disp_font_metrics;
    PangoFontDescription *font_desc;
    PangoLayout *layout;
    GdkGC    *gc;

    unsigned char *utf8buf;
    int           utf8alloc;
    int           utf8buf_length;

    offset_type marker_start;
    offset_type marker_end;
    gboolean hexmode_marker_on_hexdump;

    display_line_proc display_line;
    pixel_to_offset_proc pixel_to_offset;
    copy_to_clipboard_proc copy_to_clipboard;
};

// Gtk class related static functions
static void text_render_init (TextRender *w);
static void text_render_class_init (TextRenderClass *klass);
static void text_render_destroy (GtkObject *object);

static void text_render_redraw(TextRender *w);
static void text_render_position_changed(TextRender *w);

static void text_render_realize (GtkWidget *widget);
static void text_render_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void text_render_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean text_render_expose(GtkWidget *widget, GdkEventExpose *event);
static gboolean text_render_scroll(GtkWidget *widget, GdkEventScroll *event);
static gboolean text_render_button_press(GtkWidget *widget, GdkEventButton *event);
static gboolean text_render_button_release(GtkWidget *widget, GdkEventButton *event);
static gboolean text_render_motion_notify(GtkWidget *widget, GdkEventMotion *event);
static void text_render_h_adjustment_update (TextRender *obj);
static void text_render_h_adjustment_changed (GtkAdjustment *adjustment, gpointer data);
static void text_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data);
static void text_render_v_adjustment_update (TextRender *obj);
static void text_render_v_adjustment_changed (GtkAdjustment *adjustment, gpointer data);
static void text_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data);
static gboolean text_render_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

static void text_render_update_adjustments_limits(TextRender *w);
static void text_render_free_data(TextRender *w);
static void text_render_setup_font(TextRender*w, const gchar *fontname, gint fontsize);
static void text_render_free_font(TextRender*w);
static void text_render_reserve_utf8buf(TextRender *w, int minlength);

static void text_render_utf8_clear_buf(TextRender *w);
static int text_render_utf8_printf(TextRender *w, const char *format, ...);
static int text_render_utf8_print_char(TextRender *w, char_type value);

static void text_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset);
static int text_mode_display_line(TextRender *w, int y, int column, offset_type start_of_line, offset_type end_of_line);
static offset_type text_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker);

static int binary_mode_display_line(TextRender *w, int y, int column, offset_type start_of_line, offset_type end_of_line);

static int hex_mode_display_line(TextRender *w, int y, int column, offset_type start_of_line, offset_type end_of_line);
static void hex_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset);
static offset_type hex_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker);

/*****************************************
    public functions
    (defined in the header file)
*****************************************/
GtkType text_render_get_type ()
{
    static GtkType type = 0;
    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "TextRender",
            sizeof (TextRender),
            sizeof (TextRenderClass),
            (GtkClassInitFunc) text_render_class_init,
            (GtkObjectInitFunc) text_render_init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };
        type = gtk_type_unique (gtk_widget_get_type(), &info);
    }
    return type;
}

GtkWidget* text_render_new ()
{
    TextRender *w = (TextRender *) gtk_type_new (text_render_get_type ());

    return GTK_WIDGET (w);
}


void text_render_set_h_adjustment (TextRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_TEXT_RENDER(obj));

    if (obj->priv->h_adjustment)
    {
        gtk_signal_disconnect_by_data (GTK_OBJECT (obj->priv->h_adjustment), (gpointer) obj);
        g_object_unref (obj->priv->h_adjustment);
    }

    obj->priv->h_adjustment = adjustment;
    gtk_object_ref (GTK_OBJECT (obj->priv->h_adjustment));

    gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
              (GtkSignalFunc) text_render_h_adjustment_changed ,
              (gpointer) obj);
    gtk_signal_connect (GTK_OBJECT (adjustment), "value-changed",
              (GtkSignalFunc) text_render_h_adjustment_value_changed ,
              (gpointer) obj);

    obj->priv->old_h_adj_value = adjustment->value;
    obj->priv->old_h_adj_lower = adjustment->lower;
    obj->priv->old_h_adj_upper = adjustment->upper;

    text_render_h_adjustment_update (obj);
}


void text_render_set_v_adjustment (TextRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_TEXT_RENDER(obj));

    if (obj->priv->v_adjustment)
    {
        gtk_signal_disconnect_by_data (GTK_OBJECT (obj->priv->v_adjustment), (gpointer) obj);
        g_object_unref (obj->priv->v_adjustment);
    }

    obj->priv->v_adjustment = adjustment;
    gtk_object_ref (GTK_OBJECT (obj->priv->v_adjustment));

    gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
              (GtkSignalFunc) text_render_v_adjustment_changed ,
              (gpointer) obj);
    gtk_signal_connect (GTK_OBJECT (adjustment), "value-changed",
              (GtkSignalFunc) text_render_v_adjustment_value_changed ,
              (gpointer) obj);

    obj->priv->old_v_adj_value = adjustment->value;
    obj->priv->old_v_adj_lower = adjustment->lower;
    obj->priv->old_v_adj_upper = adjustment->upper;

    text_render_v_adjustment_update (obj);
}


static void text_render_class_init (TextRenderClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkWidgetClass *) gtk_type_class (gtk_widget_get_type ());

    object_class->destroy = text_render_destroy;

    widget_class->scroll_event = text_render_scroll;
    widget_class->button_press_event = text_render_button_press;
    widget_class->button_release_event = text_render_button_release;
    widget_class->motion_notify_event = text_render_motion_notify;

    widget_class->expose_event = text_render_expose;

    widget_class->size_request = text_render_size_request;
    widget_class->size_allocate = text_render_size_allocate;

    widget_class->realize = text_render_realize;

    text_render_signals[TEXT_STATUS_CHANGED] =
        gtk_signal_new ("text-status-changed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (TextRenderClass, text_status_changed),
            gtk_marshal_NONE__POINTER,
            GTK_TYPE_NONE,
            1, GTK_TYPE_POINTER);
}


static void text_render_init (TextRender *w)
{
    w->priv = g_new0(TextRenderPrivate, 1);

    w->priv->button = 0;
    w->priv->dispmode = TR_DISP_MODE_TEXT;
    w->priv->h_adjustment = NULL;
    w->priv->old_h_adj_value = 0.0;
    w->priv->old_h_adj_lower = 0.0;
    w->priv->old_h_adj_upper = 0.0;
    w->priv->hex_offset_display = FALSE;
    w->priv->column = 0;
    w->priv->chars_per_line = 0;
    w->priv->display_line = text_mode_display_line;
    w->priv->pixel_to_offset = text_mode_pixel_to_offset;
    w->priv->copy_to_clipboard = text_mode_copy_to_clipboard;

    w->priv->marker_start = 0;
    w->priv->marker_end = 0;

    w->priv->v_adjustment = NULL;
    w->priv->old_v_adj_value = 0.0;
    w->priv->old_v_adj_lower = 0.0;
    w->priv->old_v_adj_upper = 0.0;

    w->priv->current_offset = 0;

    w->priv->encoding = g_strdup ("ASCII");
    w->priv->utf8alloc = 0;

    w->priv->tab_size = 8;
    w->priv->font_size = 14;

    w->priv->fixed_font_name = g_strdup ("Monospace");

    g_signal_connect(G_OBJECT (w), "key-press-event", G_CALLBACK (text_render_key_pressed), NULL);

    w->priv->layout = gtk_widget_create_pango_layout(GTK_WIDGET (w), NULL);

    GTK_WIDGET_SET_FLAGS(GTK_WIDGET (w), GTK_CAN_FOCUS);
}


void text_render_notify_status_changed(TextRender *w)
{
    TextRenderStatus stat;

    g_return_if_fail (w!= NULL);
    g_return_if_fail (IS_TEXT_RENDER (w));

    memset(&stat, 0, sizeof(stat));

    stat.current_offset = w->priv->current_offset;
    stat.column = w->priv->column;
    stat.wrap_mode = w->priv->wrapmode;

    stat.size = w->priv->fops ? gv_file_get_max_offset(w->priv->fops) : 0;

    stat.encoding = w->priv->encoding;

    gtk_signal_emit (GTK_OBJECT(w), text_render_signals[TEXT_STATUS_CHANGED], &stat);
}


static void text_render_destroy (GtkObject *object)
{
    g_return_if_fail (object != NULL);
    g_return_if_fail (IS_TEXT_RENDER (object));

    TextRender *w = TEXT_RENDER (object);

    if (w->priv)
    {
        g_free(w->priv->fixed_font_name);
        w->priv->fixed_font_name = NULL;

        if (w->priv->v_adjustment)
            g_object_unref (w->priv->v_adjustment);
        w->priv->v_adjustment = NULL;

        if (w->priv->h_adjustment)
            g_object_unref (w->priv->h_adjustment);
        w->priv->h_adjustment = NULL;

        g_free(w->priv->encoding);
        w->priv->encoding = NULL;

        text_render_free_font(w);

        text_render_free_data(w);

        g_free(w->priv->utf8buf);
        w->priv->utf8buf = NULL;

        g_free(w->priv);
        w->priv = NULL;
    }

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void text_render_position_changed(TextRender *w)
{
    if (!GTK_WIDGET_REALIZED(GTK_WIDGET (w)))
        return;

    text_render_notify_status_changed(w);

    // update the hotz & vert adjustments
    if (w->priv->v_adjustment)
    {
        w->priv->v_adjustment->value = w->priv->current_offset;
        gtk_adjustment_changed(w->priv->v_adjustment);
    }

    if (w->priv->h_adjustment)
    {
        w->priv->h_adjustment->value = w->priv->column;
        gtk_adjustment_changed(w->priv->h_adjustment);
    }
}


static void text_render_redraw(TextRender *w)
{
    if (!GTK_WIDGET_REALIZED(GTK_WIDGET (w)))
        return;

    gdk_window_invalidate_rect(GTK_WIDGET (w)->window, NULL, FALSE);
}


static gboolean text_render_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER (widget), FALSE);

    TextRender *obj = TEXT_RENDER(widget);
    if (!obj->priv->dp)
        return FALSE;

    switch (event->keyval)
    {
    case GDK_Up:
        obj->priv->current_offset =
            gv_scroll_lines(obj->priv->dp, obj->priv->current_offset, -1);
        break;

    case GDK_Page_Up:
        obj->priv->current_offset =
            gv_scroll_lines(obj->priv->dp, obj->priv->current_offset,
                -1 *(obj->priv->lines_displayed-1));
        break;

    case GDK_Page_Down:
        obj->priv->current_offset =
            gv_scroll_lines(obj->priv->dp, obj->priv->current_offset,
                (obj->priv->lines_displayed-1));
        break;

    case GDK_Down:
        obj->priv->current_offset =
            gv_scroll_lines(obj->priv->dp, obj->priv->current_offset, 1);
        break;

    case GDK_Left:
        if (!obj->priv->wrapmode)
            if (obj->priv->column>0)
                obj->priv->column--;
        break;

    case GDK_Right:
        if (!obj->priv->wrapmode)
            obj->priv->column++;
        break;

    case GDK_Home:
        obj->priv->current_offset = 0;
        break;

    case GDK_End:
        obj->priv->current_offset =
            gv_align_offset_to_line_start(
                obj->priv->dp, gv_file_get_max_offset(obj->priv->fops)-1);
        break;

    default:
        return FALSE;
    }

    text_render_position_changed(obj);
    text_render_redraw(obj);

    return TRUE;
}


static void text_render_realize (GtkWidget *widget)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_TEXT_RENDER (widget));

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    TextRender *obj = TEXT_RENDER (widget);

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

    obj->priv->gc = gdk_gc_new(GTK_WIDGET (obj)->window);
    gdk_gc_set_exposures(obj->priv->gc, TRUE);

    text_render_setup_font(obj, obj->priv->fixed_font_name, obj->priv->font_size);
}


static void text_render_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
    requisition->width = TEXT_RENDER_DEFAULT_WIDTH;
    requisition->height = TEXT_RENDER_DEFAULT_HEIGHT;
}


static void text_render_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_TEXT_RENDER (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;
    TextRender *w = TEXT_RENDER (widget);

    if (GTK_WIDGET_REALIZED (widget))
        gdk_window_move_resize (widget->window, allocation->x, allocation->y, allocation->width, allocation->height);

    if (w->priv->dp && (w->priv->char_width>0))
    {
        w->priv->chars_per_line = allocation->width / w->priv->char_width;
        gv_set_wrap_limit(w->priv->dp, allocation->width / w->priv->char_width);
        text_render_redraw(w);
    }

    w->priv->lines_displayed = w->priv->char_height>0 ? allocation->height / w->priv->char_height : 10;
}


static gboolean text_render_expose(GtkWidget *widget, GdkEventExpose *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    gint y, rc;
    offset_type ofs;

    if (event->count > 0)
        return FALSE;

    TextRender *w = TEXT_RENDER (widget);

    g_return_val_if_fail (w->priv->display_line!=NULL, FALSE);

    if (w->priv->dp==NULL)
        return FALSE;

    gdk_window_clear_area (widget->window,
                0, 0,
                widget->allocation.width,
                widget->allocation.height);

    ofs = w->priv->current_offset;
    y = 0;

    while (TRUE)
    {
        offset_type eol_offset;

        eol_offset = gv_get_end_of_line_offset(w->priv->dp, ofs);
        if (eol_offset == ofs)
            break;

        rc=  w->priv->display_line(w, y, w->priv->column, ofs, eol_offset);

        if (rc==-1)
            break;

        ofs = eol_offset;

        y += w->priv->char_height;
        if (y>=widget->allocation.height)
            break;

    }

    w->priv->last_displayed_offset = ofs;

    return FALSE;
}


static gboolean text_render_scroll(GtkWidget *widget, GdkEventScroll *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    TextRender *w = TEXT_RENDER (widget);

    if (!w->priv->dp)
        return FALSE;

    // Mouse scroll wheel
    switch (event->direction)
    {
        case GDK_SCROLL_UP:
            w->priv->current_offset = gv_scroll_lines(w->priv->dp, w->priv->current_offset, -4);
            break;

        case GDK_SCROLL_DOWN:
            w->priv->current_offset = gv_scroll_lines(w->priv->dp, w->priv->current_offset, 4);
            break;

        default:
            return FALSE;
    }

    text_render_position_changed (w);
    text_render_redraw (w);

    return TRUE;
}


void  text_render_copy_selection(TextRender *w)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (w->priv);
    g_return_if_fail (w->priv->copy_to_clipboard!=NULL);

    offset_type marker_start;
    offset_type marker_end;

    if (w->priv->marker_start==w->priv->marker_end)
        return;

    marker_start = w->priv->marker_start;
    marker_end   = w->priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    w->priv->copy_to_clipboard(w, marker_start, marker_end);
}


static gboolean text_render_button_press(GtkWidget *widget, GdkEventButton *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    TextRender *w = TEXT_RENDER (widget);

    g_return_val_if_fail (w->priv->pixel_to_offset!=NULL, FALSE);

    if (!w->priv->button)
    {
        gtk_grab_add (widget);
        w->priv->button = event->button;
        w->priv->marker_start  = w->priv->pixel_to_offset(w, (int) event->x, (int) event->y, TRUE);
    }

    return FALSE;
}


static gboolean text_render_button_release(GtkWidget *widget, GdkEventButton *event)
{
    TextRender *w;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    w = TEXT_RENDER (widget);

    g_return_val_if_fail (w->priv->pixel_to_offset!=NULL, FALSE);

    if (w->priv->button == event->button)
    {
        gtk_grab_remove (widget);
        w->priv->button = 0;

        w->priv->marker_end = w->priv->pixel_to_offset(w, (int)event->x, (int)event->y, FALSE);
        text_render_redraw(w);
    }

    return FALSE;
}


static gboolean text_render_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    TextRender *w = TEXT_RENDER (widget);

    g_return_val_if_fail (w->priv->pixel_to_offset!=NULL, FALSE);

    GdkModifierType mods;
    gint x, y;
    offset_type new_marker;

    if (w->priv->button != 0)
    {
        x = event->x;
        y = event->y;

        if (event->is_hint || (event->window != widget->window))
            gdk_window_get_pointer (widget->window, &x, &y, &mods);

        // TODO: respond to motion event
        new_marker = w->priv->pixel_to_offset(w, x, y, FALSE);

        if (new_marker != w->priv->marker_end)
        {
            w->priv->marker_end = new_marker;
            text_render_redraw(w);
        }
    }

    return FALSE;
}


static void text_render_h_adjustment_update (TextRender *obj)
{
    gfloat new_value;

    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_TEXT_RENDER(obj));

    new_value = obj->priv->h_adjustment->value;

    if (new_value < obj->priv->h_adjustment->lower)
    new_value = obj->priv->h_adjustment->lower;

    if (new_value > obj->priv->h_adjustment->upper)
    new_value = obj->priv->h_adjustment->upper;

    if (new_value != obj->priv->h_adjustment->value)
    {
        obj->priv->h_adjustment->value = new_value;
        gtk_signal_emit_by_name (GTK_OBJECT (obj->priv->h_adjustment), "value-changed");
    }

    obj->priv->column = (int) new_value;

    text_render_redraw(obj);
}


static void text_render_h_adjustment_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    if ((obj->priv->old_h_adj_value != adjustment->value) ||
        (obj->priv->old_h_adj_lower != adjustment->lower) ||
        (obj->priv->old_h_adj_upper != adjustment->upper))
    {
        text_render_h_adjustment_update (obj);

        obj->priv->old_h_adj_value = adjustment->value;
        obj->priv->old_h_adj_lower = adjustment->lower;
        obj->priv->old_h_adj_upper = adjustment->upper;
    }
}


static void text_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    if (obj->priv->old_h_adj_value != adjustment->value)
    {
        text_render_h_adjustment_update (obj);
        obj->priv->old_h_adj_value = adjustment->value;
    }
}


static void text_render_v_adjustment_update (TextRender *obj)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_TEXT_RENDER(obj));

    gfloat new_value = obj->priv->v_adjustment->value;

    if (new_value < obj->priv->v_adjustment->lower)
    new_value = obj->priv->v_adjustment->lower;

    if (new_value > obj->priv->v_adjustment->upper-1)
    new_value = obj->priv->v_adjustment->upper-1;

    if ((offset_type)new_value==obj->priv->current_offset)
        return;

    if (obj->priv->dp)
        new_value = gv_align_offset_to_line_start(obj->priv->dp, (offset_type) new_value);

    if (new_value != obj->priv->v_adjustment->value)
    {
        obj->priv->v_adjustment->value = new_value;
        gtk_signal_emit_by_name (GTK_OBJECT (obj->priv->v_adjustment), "value-changed");
    }

    obj->priv->current_offset = (offset_type)new_value;

    text_render_redraw(obj);
}


static void text_render_v_adjustment_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    if ((obj->priv->old_v_adj_value != adjustment->value) ||
        (obj->priv->old_v_adj_lower != adjustment->lower) ||
        (obj->priv->old_v_adj_upper != adjustment->upper))
    {
        text_render_v_adjustment_update (obj);

        obj->priv->old_v_adj_value = adjustment->value;
        obj->priv->old_v_adj_lower = adjustment->lower;
        obj->priv->old_v_adj_upper = adjustment->upper;
    }
}


static void text_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    if (obj->priv->old_v_adj_value != adjustment->value)
    {
        text_render_v_adjustment_update (obj);
        obj->priv->old_v_adj_value = adjustment->value;
    }
}


static void text_render_free_data(TextRender *w)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    if (w->priv->dp)
        gv_free_data_presentation(w->priv->dp);
    w->priv->dp = NULL;

    if (w->priv->im)
        gv_free_input_modes(w->priv->im);
    w->priv->im = NULL;

    if (w->priv->fops)
        gv_file_free(w->priv->fops);
    w->priv->fops = NULL;
    w->priv->current_offset = 0;
}


/*
  This function assumes W->PRIV->FOPS has been initialized correctly
   with wither a file name or a file descriptor
*/
static void text_render_internal_load(TextRender *w)
{
    w->priv->current_offset = 0;
    w->priv->column = 0;
    w->priv->max_column = 0;

    // Setup the input mode translations
    w->priv->im = gv_input_modes_new();
    gv_init_input_modes(w->priv->im, (get_byte_proc)gv_file_get_byte, w->priv->fops);
    gv_set_input_mode(w->priv->im, w->priv->encoding);

    // Setup the data presentation mode
    w->priv->dp = gv_data_presentation_new();
    gv_init_data_presentation(w->priv->dp, w->priv->im,
        gv_file_get_max_offset(w->priv->fops));

    gv_set_wrap_limit(w->priv->dp, 50);
    gv_set_fixed_count(w->priv->dp, w->priv->fixed_limit);
    gv_set_tab_size(w->priv->dp, w->priv->tab_size);

    text_render_set_display_mode (w, TR_DISP_MODE_TEXT);

    text_render_update_adjustments_limits(w);
}


void text_render_load_filedesc(TextRender *w, int filedesc)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    text_render_free_data(w);

    w->priv->fops = gv_fileops_new();
    if (gv_file_open_fd(w->priv->fops, filedesc)==-1)
    {
        g_warning("Failed to load file descriptor (%d)", filedesc);
        return;
    }

    text_render_internal_load(w);
}


void text_render_load_file(TextRender *w, const gchar *filename)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    text_render_free_data(w);

    w->priv->fops = gv_fileops_new();
    if (gv_file_open(w->priv->fops, filename)==-1)
    {
        g_warning("Failed to load file (%s)", filename);
        return;
    }

    text_render_internal_load(w);
}


static void text_render_update_adjustments_limits(TextRender *w)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    if (!w->priv->fops)
        return;

    if (w->priv->v_adjustment)
    {
        w->priv->v_adjustment->lower = 0;
        w->priv->v_adjustment->upper = gv_file_get_max_offset(w->priv->fops)-1;
        gtk_adjustment_changed(w->priv->v_adjustment);
    }

    if (w->priv->h_adjustment)
    {
        w->priv->h_adjustment->step_increment = 1;
        w->priv->h_adjustment->page_increment = 5;
        w->priv->h_adjustment->page_size = w->priv->chars_per_line;
        w->priv->h_adjustment->lower = 0;
        if (gv_get_data_presentation_mode(w->priv->dp)==PRSNT_NO_WRAP)
            w->priv->h_adjustment->upper = w->priv->max_column; // TODO: find our the real horz limit
        else
            w->priv->h_adjustment->upper = 0;
        gtk_adjustment_changed(w->priv->h_adjustment);
    }
}

static gboolean text_render_vscroll_change_value(GtkRange *range,
                                            GtkScrollType scroll,
                                            gdouble value,
                                            TextRender *obj)
{
    if (!obj->priv->dp)
        return FALSE;

    switch (scroll)
    {
        case GTK_SCROLL_STEP_BACKWARD:
            obj->priv->current_offset =
                gv_scroll_lines(obj->priv->dp, obj->priv->current_offset, -4);
            break;

        case GTK_SCROLL_STEP_FORWARD:
            obj->priv->current_offset =
                gv_scroll_lines(obj->priv->dp, obj->priv->current_offset, 4);
            break;

        case GTK_SCROLL_PAGE_BACKWARD:
            obj->priv->current_offset =
                gv_scroll_lines(obj->priv->dp, obj->priv->current_offset,
                    -1 *(obj->priv->lines_displayed-1));
            break;

        case GTK_SCROLL_PAGE_FORWARD:
            obj->priv->current_offset =
                gv_scroll_lines(obj->priv->dp, obj->priv->current_offset,
                    (obj->priv->lines_displayed-1));
            break;

        case GTK_SCROLL_JUMP:
        default:
            return FALSE;
    }

    text_render_position_changed(obj);
    text_render_redraw(obj);

    return TRUE;
}


void text_render_attach_external_v_range(TextRender *obj, GtkRange *range)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(obj));
    g_return_if_fail (range!=NULL);

    g_signal_connect (G_OBJECT (range), "change-value",
            G_CALLBACK (text_render_vscroll_change_value), (gpointer)obj);
}


static void text_render_free_font(TextRender*w)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    if (w->priv->disp_font_metrics)
        pango_font_metrics_unref(w->priv->disp_font_metrics);
    w->priv->disp_font_metrics = NULL;

    if (w->priv->font_desc)
        pango_font_description_free(w->priv->font_desc);
    w->priv->font_desc = NULL;
}


static PangoFontMetrics *load_font (const char *font_name)
{
    PangoFontDescription *new_desc = pango_font_description_from_string (font_name);
    PangoContext *context = gdk_pango_context_get();
    PangoFont *new_font = pango_context_load_font (context, new_desc);
    PangoFontMetrics *new_metrics = pango_font_get_metrics (new_font, pango_context_get_language (context));

    pango_font_description_free (new_desc);
    g_object_unref (G_OBJECT (context));
    g_object_unref (G_OBJECT (new_font));

    return new_metrics;
}


static guint get_max_char_width(GtkWidget *widget, PangoFontDescription *font_desc, PangoFontMetrics *font_metrics)
{
    PangoLayout *layout = gtk_widget_create_pango_layout (widget, "");
    pango_layout_set_font_description (layout, font_desc);

    /* this is, I guess, a rather dirty trick, but
       right now i can't think of anything better */
    guint maxwidth = 0;
    PangoRectangle logical_rect;
    gchar str[2];
    guint i;

    for (i=1; i<0x100; i++)
    {
        logical_rect.width = 0;
        // Check if the char is displayable. Caused trouble to pango
        if (is_displayable((guchar)i))
        {
            sprintf (str, "%c", (gchar) i);
            pango_layout_set_text(layout, str, -1);
            pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
        }
        maxwidth = MAX(maxwidth, logical_rect.width);
    }

    g_object_unref (G_OBJECT (layout));
    return maxwidth;
}


static guint text_render_filter_undisplayable_chars(TextRender *obj)
{
    PangoRectangle logical_rect;
    guint i;

    if (!obj->priv->im)
        return 0;

    PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (obj), "");
    pango_layout_set_font_description (layout, obj->priv->font_desc);

    for (i=0; i<256; i++)
    {
        char_type value = gv_input_mode_byte_to_utf8(obj->priv->im, (unsigned char)i);
        text_render_utf8_clear_buf(obj);
        text_render_utf8_print_char(obj, value);
        pango_layout_set_text(layout, (char *) obj->priv->utf8buf, obj->priv->utf8buf_length);
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
            gv_input_mode_update_utf8_translation(obj->priv->im, i, '.');
    }

    g_object_unref (G_OBJECT (layout));
    return 0;
}


static void text_render_setup_font(TextRender*w, const gchar *fontname, gint fontsize)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));
    g_return_if_fail (fontname!=NULL);
    g_return_if_fail (fontsize>0);

    text_render_free_font(w);

    gchar *fontlabel = g_strdup_printf("%s %d", fontname, fontsize);

    w->priv->disp_font_metrics = load_font (fontlabel);
    w->priv->font_desc = pango_font_description_from_string (fontlabel);

    gtk_widget_modify_font (GTK_WIDGET (w), w->priv->font_desc);

    w->priv->char_width = get_max_char_width(GTK_WIDGET (w),
            w->priv->font_desc,
            w->priv->disp_font_metrics);

    w->priv->char_height =
       PANGO_PIXELS(pango_font_metrics_get_ascent(w->priv->disp_font_metrics)) +
       PANGO_PIXELS(pango_font_metrics_get_descent(w->priv->disp_font_metrics));

    g_free(fontlabel);
}


static void text_render_reserve_utf8buf(TextRender *w, int minlength)
{
    if (w->priv->utf8alloc < minlength)
    {
        w->priv->utf8alloc = minlength*2;
        w->priv->utf8buf = (unsigned char*) g_realloc(w->priv->utf8buf, w->priv->utf8alloc);
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
    w->priv->utf8buf_length = 0;
}


static int text_render_utf8_printf(TextRender *w, const char *format, ...)
{
    va_list ap;
    int new_length;

    text_render_reserve_utf8buf(w, w->priv->utf8buf_length+101);

    va_start(ap, format);
    new_length = vsnprintf((char *) &w->priv->utf8buf[w->priv->utf8buf_length], 100, format, ap);
    va_end(ap);

    w->priv->utf8buf_length += new_length;
    return w->priv->utf8buf_length;
}


static int text_render_utf8_print_char(TextRender *w, char_type value)
{
    int current_length = w->priv->utf8buf_length;

    text_render_reserve_utf8buf(w, current_length+4);
    w->priv->utf8buf[current_length++] = GV_FIRST_BYTE(value);
    if (GV_SECOND_BYTE(value))
    {
        w->priv->utf8buf[current_length++] = GV_SECOND_BYTE(value);
        if (GV_THIRD_BYTE(value))
        {
            w->priv->utf8buf[current_length++] = GV_THIRD_BYTE(value);
            if (GV_FOURTH_BYTE(value))
                w->priv->utf8buf[current_length++] = GV_FOURTH_BYTE(value);
        }
    }

    w->priv->utf8buf_length = current_length;
    return current_length;
}


void  text_render_set_display_mode (TextRender *w, TEXTDISPLAYMODE mode)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));
    g_return_if_fail (w->priv->fops!=NULL);
    g_return_if_fail (w->priv->im!=NULL);
    g_return_if_fail (w->priv->dp!=NULL);

    if (mode==w->priv->dispmode)
        return;

    w->priv->column = 0;

    switch (mode)
    {
    case TR_DISP_MODE_TEXT:
        gv_set_data_presentation_mode(w->priv->dp,
            w->priv->wrapmode ? PRSNT_WRAP : PRSNT_NO_WRAP);

        w->priv->display_line = text_mode_display_line;
        w->priv->pixel_to_offset = text_mode_pixel_to_offset;
        w->priv->copy_to_clipboard = text_mode_copy_to_clipboard;
        break;

    case TR_DISP_MODE_BINARY:

        // Binary display mode doesn't support UTF8
        // TODO: switch back to the previous encoding, not just ASCII
        //        gv_set_input_mode(w->priv->im, "ASCII");

        gv_set_fixed_count(w->priv->dp, w->priv->fixed_limit);
        gv_set_data_presentation_mode(w->priv->dp, PRSNT_BIN_FIXED);

        w->priv->display_line = binary_mode_display_line;
        w->priv->pixel_to_offset = text_mode_pixel_to_offset;
        w->priv->copy_to_clipboard = text_mode_copy_to_clipboard;
        break;

    case TR_DISP_MODE_HEXDUMP:

        // HEX display mode doesn't support UTF8
        // TODO: switch back to the previous encoding, not just ASCII
        //        gv_set_input_mode(w->priv->im, "ASCII");

        gv_set_fixed_count(w->priv->dp, HEXDUMP_FIXED_LIMIT);
        gv_set_data_presentation_mode(w->priv->dp, PRSNT_BIN_FIXED);

        w->priv->display_line = hex_mode_display_line;
        w->priv->pixel_to_offset = hex_mode_pixel_to_offset;
        w->priv->copy_to_clipboard = hex_mode_copy_to_clipboard;
        break;
    }

    text_render_setup_font(w, w->priv->fixed_font_name, w->priv->font_size);
    w->priv->dispmode = mode;
    w->priv->current_offset = gv_align_offset_to_line_start(
            w->priv->dp, w->priv->current_offset);

    text_render_redraw(w);
}


TEXTDISPLAYMODE text_render_get_display_mode(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, TR_DISP_MODE_TEXT);
    g_return_val_if_fail (IS_TEXT_RENDER(w), TR_DISP_MODE_TEXT);

    return w->priv->dispmode;
}


ViewerFileOps *text_render_get_file_ops(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, NULL);
    g_return_val_if_fail (IS_TEXT_RENDER(w), NULL);
    g_return_val_if_fail (w->priv->fops!=NULL, NULL);

    return w->priv->fops;
}


GVInputModesData *text_render_get_input_mode_data(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, NULL);
    g_return_val_if_fail (IS_TEXT_RENDER(w), NULL);
    g_return_val_if_fail (w->priv->im!=NULL, NULL);

    return w->priv->im;
}


GVDataPresentation *text_render_get_data_presentation(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, NULL);
    g_return_val_if_fail (IS_TEXT_RENDER(w), NULL);
    g_return_val_if_fail (w->priv->dp!=NULL, NULL);

    return w->priv->dp;
}


void text_render_set_tab_size(TextRender *w, int tab_size)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    if (w->priv->dp==NULL)
        return;

    if (tab_size<=0)
        return;

    w->priv->tab_size = tab_size;
    gv_set_tab_size(w->priv->dp, tab_size);

    text_render_redraw(w);
}


int text_render_get_tab_size(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, 0);
    g_return_val_if_fail (IS_TEXT_RENDER(w), 0);

    return w->priv->tab_size;
}


void text_render_set_wrap_mode(TextRender *w, gboolean ACTIVE)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    if (w->priv->dp==NULL)
        return;

    w->priv->wrapmode = ACTIVE;
    if (w->priv->dispmode==TR_DISP_MODE_TEXT)
    {
        w->priv->column = 0;
        gv_set_data_presentation_mode(w->priv->dp, w->priv->wrapmode ? PRSNT_WRAP : PRSNT_NO_WRAP);
        text_render_update_adjustments_limits(w);
    }
    text_render_redraw(w);
}


gboolean text_render_get_wrap_mode(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER(w), FALSE);

    return w->priv->wrapmode;
}


void text_render_set_fixed_limit(TextRender *w, int fixed_limit)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    // this is saved later, for binary display mode. Hex display mode always use 16 bytes
    w->priv->fixed_limit = fixed_limit;

    // always 16 bytes in hex dump
    if (w->priv->dispmode==TR_DISP_MODE_HEXDUMP)
        fixed_limit = HEXDUMP_FIXED_LIMIT;

    if (w->priv->dp)
        gv_set_fixed_count(w->priv->dp, fixed_limit);
    text_render_redraw(w);
}


int text_render_get_fixed_limit(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, 0);
    g_return_val_if_fail (IS_TEXT_RENDER(w), 0);

    return w->priv->fixed_limit;
}


offset_type text_render_get_current_offset(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, 0);
    g_return_val_if_fail (IS_TEXT_RENDER(w), 0);

    return w->priv->current_offset;
}


offset_type text_render_get_last_displayed_offset(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, 0);
    g_return_val_if_fail (IS_TEXT_RENDER(w), 0);

    return w->priv->last_displayed_offset;
}


void text_render_ensure_offset_visible(TextRender *w, offset_type offset)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    if (offset < w->priv->current_offset || offset > w->priv->last_displayed_offset)
    {
        offset = gv_align_offset_to_line_start(w->priv->dp, offset);
        offset = gv_scroll_lines(w->priv->dp, offset, -w->priv->lines_displayed/2);

        w->priv->current_offset = offset;
        text_render_redraw(w);
        text_render_position_changed(w);
    }
}


void text_render_set_marker(TextRender *w, offset_type start, offset_type end)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    w->priv->marker_start = start;
    w->priv->marker_end = end;
    text_render_redraw(w);
}


void text_render_set_encoding(TextRender *w, const char *encoding)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    if (w->priv->im==NULL)
        return;

    // Ugly hack: UTF-8 is not acceptable encoding in Binary/Hexdump modes
    if (g_strcasecmp(encoding, "UTF8")==0 && (
        w->priv->dispmode==TR_DISP_MODE_BINARY || w->priv->dispmode==TR_DISP_MODE_HEXDUMP))
        {
            g_warning("Can't set UTF8 encoding when in Binary or HexDump display mode");
            return;
        }

    g_free(w->priv->encoding);
    w->priv->encoding = g_strdup (encoding);
    gv_set_input_mode(w->priv->im, encoding);
    text_render_filter_undisplayable_chars(w);
    text_render_redraw(w);
}


const gchar *text_render_get_encoding(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, NULL);
    g_return_val_if_fail (IS_TEXT_RENDER(w), NULL);

    return w->priv->encoding;
}


void text_render_set_hex_offset_display(TextRender *w, gboolean HEX_OFFSET)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));

    w->priv->hex_offset_display = HEX_OFFSET;
    text_render_redraw(w);
}


gboolean text_render_get_hex_offset_display(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, FALSE);
    g_return_val_if_fail (IS_TEXT_RENDER(w), FALSE);

    return w->priv->hex_offset_display;
}


void text_render_set_font_size(TextRender *w, int font_size)
{
    gchar *font;

    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER(w));
    g_return_if_fail (font_size>=4);

    w->priv->font_size = font_size;
    font = w->priv->fixed_font_name;

    text_render_setup_font(w, font, font_size);
    text_render_redraw(w);
}


int  text_render_get_font_size(TextRender *w)
{
    g_return_val_if_fail (w!=NULL, 0);
    g_return_val_if_fail (IS_TEXT_RENDER(w), 0);

    return w->priv->font_size;
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
            text_render_utf8_printf(w, "<span background=\"blue\">");
        }
    }
    else
        if (current >= marker_end)
        {
            marker_shown = FALSE;
            text_render_utf8_printf(w, "</span>");
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
            text_render_utf8_printf(w, "<span %s=\"blue\">",
            primary_color?"background":"foreground");
        }
    }
    else
        if (current >= marker_end)
        {
            marker_shown = FALSE;
            text_render_utf8_printf(w, "</span>");
        }


    return marker_shown;
}


static void marker_closer (TextRender *w, gboolean marker_shown)
{
    g_return_if_fail (w!=NULL);

    if (marker_shown)
        text_render_utf8_printf(w, "</span>");
}


static offset_type text_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker)
{
    int line = 0;
    int column = 0;
    offset_type offset;
    offset_type next_line_offset;

    g_return_val_if_fail (obj!=NULL, 0);
    g_return_val_if_fail (obj->priv->dp!=NULL, 0);

    if (x<0)
        x = 0;
    if (y<0)
        return obj->priv->current_offset;

    if (obj->priv->char_height<=0)
        return obj->priv->current_offset;

    if (obj->priv->char_width<=0)
        return obj->priv->current_offset;

    line = y / obj->priv->char_height;
    column = x / obj->priv->char_width;

    if (!start_marker)
        column++;

    offset = gv_scroll_lines(obj->priv->dp, obj->priv->current_offset, line);
    next_line_offset = gv_scroll_lines(obj->priv->dp, offset, 1);

    while (column>0 && offset<next_line_offset)
    {
        offset = gv_input_get_next_char_offset(obj->priv->im, offset);
        column--;
    }

    return offset;
}


static void text_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (start_offset!=end_offset);
    g_return_if_fail (obj->priv->dp!=NULL);
    g_return_if_fail (obj->priv->im!=NULL);

    GtkClipboard *clip = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
    g_return_if_fail (clip!=NULL);

    text_render_utf8_clear_buf(obj);
    offset_type current = start_offset;
    while (current < end_offset &&  obj->priv->utf8buf_length<MAX_CLIPBOARD_COPY_LENGTH)
    {
        char_type value = gv_input_mode_get_utf8_char(obj->priv->im, current);
        if (value==INVALID_CHAR)
            break;

        current = gv_input_get_next_char_offset(obj->priv->im, current);
        text_render_utf8_print_char(obj, value);
    }

    gtk_clipboard_set_text(clip, (const gchar *) obj->priv->utf8buf, obj->priv->utf8buf_length);
}


static int text_mode_display_line(TextRender *w, int y, int column, offset_type start_of_line, offset_type end_of_line)
{
    g_return_val_if_fail (w!=NULL, -1);
    g_return_val_if_fail (IS_TEXT_RENDER(w), -1);

    offset_type current;
    char_type value;
    int rc=0;
    int char_count = 0;
    offset_type marker_start;
    offset_type marker_end;
    gboolean show_marker;
    gboolean marker_shown = FALSE;

    marker_start = w->priv->marker_start;
    marker_end = w->priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    show_marker = marker_start!=marker_end;

    if (w->priv->wrapmode)
        column = 0;

    text_render_utf8_clear_buf(w);

    current = start_of_line;
    while (current < end_of_line)
    {
        if (show_marker)
            marker_shown = marker_helper(w, marker_shown, current, marker_start, marker_end);

        // Read a UTF8 character from the input file. The "inputmode" module is responsible for converting the file into UTF8
        value = gv_input_mode_get_utf8_char(w->priv->im, current);
        if (value==INVALID_CHAR)
        {
            rc = -1;
            break;
        }

        // move to the next character's offset
        current = gv_input_get_next_char_offset(w->priv->im, current);

        if (value=='\r' || value=='\n')
            continue;

        if (value=='\t')
        {
            for (int i=0; i<w->priv->tab_size; i++)
                text_render_utf8_print_char(w, ' ');
            char_count += w->priv->tab_size;
            continue;
        }

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf(w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);

        char_count++;
    }

    if (char_count > w->priv->max_column)
    {
        w->priv->max_column = char_count;
        text_render_update_adjustments_limits(w);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_markup (w->priv->layout, (gchar *) w->priv->utf8buf, w->priv->utf8buf_length);
    gdk_draw_layout(GTK_WIDGET (w)->window, w->priv->gc, -(w->priv->char_width*column), y, w->priv->layout);

    return 0;
}


static int binary_mode_display_line(TextRender *w, int y, int column, offset_type start_of_line, offset_type end_of_line)
{
    g_return_val_if_fail (w!=NULL, -1);
    g_return_val_if_fail (IS_TEXT_RENDER(w), -1);

    offset_type current;
    char_type value;
    int rc=0;
    offset_type marker_start;
    offset_type marker_end;
    gboolean show_marker;
    gboolean marker_shown = FALSE;

    marker_start = w->priv->marker_start;
    marker_end = w->priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    show_marker = (marker_start!=marker_end);
    text_render_utf8_clear_buf(w);

    current = start_of_line;
    while (current < end_of_line)
    {

        if (show_marker)
            marker_shown = marker_helper(w, marker_shown, current, marker_start, marker_end);

        /* Read a UTF8 character from the input file.
           The "inputmode" module is responsible for converting the file into UTF8 */
        value = gv_input_mode_get_utf8_char(w->priv->im, current);
        if (value==INVALID_CHAR)
        {
            rc = -1;
            break;
        }

        // move to the next character's offset
        current = gv_input_get_next_char_offset(w->priv->im, current);

        if (value=='\r' || value=='\n' || value=='\t')
            value = gv_input_mode_byte_to_utf8(w->priv->im, (unsigned char)value);

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf(w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_markup (w->priv->layout, (gchar *) w->priv->utf8buf, w->priv->utf8buf_length);
    gdk_draw_layout(GTK_WIDGET (w)->window, w->priv->gc, -(w->priv->char_width*column), y, w->priv->layout);

    return 0;
}


static offset_type hex_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker)
{
    g_return_val_if_fail (obj!=NULL, 0);
    g_return_val_if_fail (obj->priv->dp!=NULL, 0);

    int line = 0;
    int column = 0;
    offset_type offset;
    offset_type next_line_offset;

    if (x<0)
        x = 0;
    if (y<0)
        return obj->priv->current_offset;

    if (obj->priv->char_height<=0)
        return obj->priv->current_offset;

    if (obj->priv->char_width<=0)
        return obj->priv->current_offset;

    line = y / obj->priv->char_height;
    column = x / obj->priv->char_width;

    offset = gv_scroll_lines(obj->priv->dp, obj->priv->current_offset, line);
    next_line_offset = gv_scroll_lines(obj->priv->dp, offset, 1);

    if (column<10)
        return offset;

    if (start_marker)
    {
        // the first 10 characters are the offset number
        obj->priv->hexmode_marker_on_hexdump = TRUE;

        if (column<10+16*3)
            column = (column-10)/3; // the user selected the hex dump portion
        else
        {
            // the user selected the ascii portion
            column = (column-10-16*3);
            obj->priv->hexmode_marker_on_hexdump = FALSE;
        }
    }
    else
    {
        if (obj->priv->hexmode_marker_on_hexdump)
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
        offset = gv_input_get_next_char_offset(obj->priv->im, offset);
        column--;
    }

    return offset;
}


static void hex_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (start_offset!=end_offset);
    g_return_if_fail (obj->priv->dp!=NULL);
    g_return_if_fail (obj->priv->im!=NULL);

    if (!obj->priv->hexmode_marker_on_hexdump)
    {
        text_mode_copy_to_clipboard(obj, start_offset, end_offset);
        return;
    }

    GtkClipboard *clip = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
    g_return_if_fail (clip!=NULL);

    text_render_utf8_clear_buf(obj);

    for (offset_type current = start_offset; current < end_offset &&  obj->priv->utf8buf_length<MAX_CLIPBOARD_COPY_LENGTH; current++)
    {
        char_type value = gv_input_mode_get_raw_byte(obj->priv->im, current);
        if (value==INVALID_CHAR)
            break;
        text_render_utf8_printf(obj, "%02x ", (unsigned char) value);
    }

    gtk_clipboard_set_text(clip, (const gchar *) obj->priv->utf8buf, obj->priv->utf8buf_length);
}


static int hex_mode_display_line(TextRender *w, int y, int column, offset_type start_of_line, offset_type end_of_line)
{
    int byte_value;
    char_type value;
    offset_type current;

    offset_type marker_start;
    offset_type marker_end;
    gboolean show_marker;
    gboolean marker_shown = FALSE;

    g_return_val_if_fail (w!=NULL, -1);
    g_return_val_if_fail (IS_TEXT_RENDER(w), -1);

    marker_start = w->priv->marker_start;
    marker_end = w->priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    show_marker = (marker_start!=marker_end);
    text_render_utf8_clear_buf(w);

    if (w->priv->hex_offset_display)
        text_render_utf8_printf(w, "%08lx  ", (unsigned long)start_of_line);
    else
        text_render_utf8_printf(w, "%09lu ", (unsigned long)start_of_line);

    current = start_of_line;
    while (current < end_of_line)
    {
        if (show_marker)
        {
            marker_shown = hex_marker_helper(w, marker_shown,
                current, marker_start, marker_end,
                w->priv->hexmode_marker_on_hexdump);
        }

        byte_value = gv_input_mode_get_raw_byte(w->priv->im, current);
        if (byte_value==-1)
            break;
        current++;

        text_render_utf8_printf(w, "%02x ", (unsigned char)byte_value);
    }
    if (show_marker)
        marker_closer(w, marker_shown);

    marker_shown = FALSE;

    current = start_of_line;
    while (current < end_of_line)
    {
        if (show_marker)
        {
            marker_shown = hex_marker_helper(w, marker_shown,
                current, marker_start, marker_end,
                !w->priv->hexmode_marker_on_hexdump);
        }

        byte_value = gv_input_mode_get_raw_byte(w->priv->im, current);
        if (byte_value==-1)
            break;
        value = gv_input_mode_byte_to_utf8(w->priv->im, (unsigned char)byte_value);

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf(w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);

        current++;
    }
    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_markup (w->priv->layout, (gchar *) w->priv->utf8buf, w->priv->utf8buf_length);
    gdk_draw_layout(GTK_WIDGET (w)->window, w->priv->gc, 0, y, w->priv->layout);

    return 0;
}
