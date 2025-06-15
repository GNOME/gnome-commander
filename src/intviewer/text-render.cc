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


#define TEXT_RENDER_DEFAULT_WIDTH       100
#define TEXT_RENDER_DEFAULT_HEIGHT      200

#define HEXDUMP_FIXED_LIMIT              16
#define MAX_CLIPBOARD_COPY_LENGTH  0xFFFFFF

#define NEED_PANGO_ESCAPING(x) ((x)=='<' || (x)=='>' || (x)=='&')

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_FONT_SIZE,
  PROP_TAB_SIZE,
  PROP_WRAP_MODE,
  PROP_ENCODING,
  PROP_FIXED_LIMIT,
  PROP_HEXADECIMAL_OFFSET,
};

enum
{
  TEXT_STATUS_CHANGED,
  LAST_SIGNAL
};

static guint text_render_signals[LAST_SIGNAL] = { 0 };

typedef void (*display_line_proc)(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line);
typedef offset_type (*pixel_to_offset_proc) (TextRender *obj, int x, int y, gboolean start_marker);
typedef void (*copy_to_clipboard_proc)(TextRender *obj, offset_type start_offset, offset_type end_offset);


struct TextRender
{
    GtkWidget parent;
};


struct TextRenderClass
{
    GtkWidgetClass parent_class;

    void (* text_status_changed) (TextRender *obj);
};

// Class Private Data
struct TextRenderPrivate
{
    guint8 button; // The button pressed to start a selection

    GtkAdjustment *h_adjustment;
    GtkScrollablePolicy hscroll_policy;

    GtkAdjustment *v_adjustment;
    GtkScrollablePolicy vscroll_policy;

    ViewerFileOps *fops;
    GVInputModesData *im;
    GVDataPresentation *dp;

    gchar *encoding;
    int tab_size;
    int fixed_limit;
    int font_size;
    gboolean wrapmode;
    int max_column;
    offset_type last_displayed_offset;
    DISPLAYMODE dispmode;
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


G_DEFINE_TYPE_EXTENDED (TextRender,
                        text_render,
                        GTK_TYPE_WIDGET,
                        0,
                        G_ADD_PRIVATE (TextRender)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))


// Gtk class related static functions
static void text_render_position_changed(TextRender *w);

static void text_render_measure (GtkWidget* widget, GtkOrientation orientation, int for_size, int* minimum, int* natural, int* minimum_baseline, int* natural_baseline);
static void text_render_size_allocate (GtkWidget *widget, int width, int height, int baseline);
static void text_render_draw (GtkWidget *widget, GtkSnapshot *snapshot);
static void text_render_scroll (GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data);
static void text_render_button_press (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
static void text_render_button_release (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
static void text_render_motion_notify (GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
static void text_render_h_adjustment_update (TextRender *obj);
static void text_render_h_adjustment_changed (GtkAdjustment *adjustment, gpointer data);
static void text_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data);
static void text_render_v_adjustment_update (TextRender *obj);
static void text_render_v_adjustment_changed (GtkAdjustment *adjustment, gpointer data);
static void text_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data);
static gboolean text_render_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

static void text_render_update_adjustments_limits(TextRender *w);
static void text_render_free_data(TextRender *w);
static void text_render_setup_font(TextRender*w, const gchar *fontname, gint fontsize);
static void text_render_free_font(TextRender*w);
static void text_render_reserve_utf8buf(TextRender *w, int minlength);

static void text_render_utf8_clear_buf(TextRender *w);
static int text_render_utf8_printf (TextRender *w, const char *format, ...);
static int text_render_utf8_print_char(TextRender *w, char_type value);

static void text_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset);
static void text_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line);
static offset_type text_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker);

static void binary_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line);

static void hex_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line);
static void hex_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset);
static offset_type hex_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker);


static TextRenderPrivate* text_render_priv (TextRender *w)
{
    return static_cast<TextRenderPrivate*>(text_render_get_instance_private (w));
}


static void text_render_set_h_adjustment (TextRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_TEXT_RENDER (obj));
    auto priv = text_render_priv (obj);

    if (priv->h_adjustment)
    {
        g_signal_handlers_disconnect_matched (priv->h_adjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, obj);
        g_object_unref (priv->h_adjustment);
    }

    priv->h_adjustment = adjustment ? adjustment : gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    g_object_ref (priv->h_adjustment);

    gtk_adjustment_set_lower (priv->h_adjustment, 0.0);
    if (priv->dp && gv_get_data_presentation_mode (priv->dp) == PRSNT_NO_WRAP)
        gtk_adjustment_set_upper (priv->h_adjustment, priv->max_column); // TODO: find our the real horz limit
    else
        gtk_adjustment_set_upper (priv->h_adjustment, 0);
    gtk_adjustment_set_step_increment (priv->h_adjustment, 1.0);
    gtk_adjustment_set_page_increment (priv->h_adjustment, 5.0);
    gtk_adjustment_set_page_size (priv->h_adjustment, priv->chars_per_line);

    g_signal_connect (priv->h_adjustment, "changed", G_CALLBACK (text_render_h_adjustment_changed), obj);
    g_signal_connect (priv->h_adjustment, "value-changed", G_CALLBACK (text_render_h_adjustment_value_changed), obj);

    text_render_h_adjustment_update (obj);
}


static void text_render_set_v_adjustment (TextRender *obj, GtkAdjustment *adjustment)
{
    g_return_if_fail (IS_TEXT_RENDER (obj));
    auto priv = text_render_priv (obj);

    if (priv->v_adjustment)
    {
        g_signal_handlers_disconnect_matched (priv->v_adjustment, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, obj);
        g_object_unref (priv->v_adjustment);
    }

    priv->v_adjustment = adjustment ? adjustment : gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    g_object_ref (priv->v_adjustment);

    gtk_adjustment_set_lower (priv->v_adjustment, 0.0);
    gtk_adjustment_set_upper (priv->v_adjustment, priv->fops ? gv_file_get_max_offset (priv->fops) - 1 : 0.0);
    gtk_adjustment_set_step_increment (priv->v_adjustment, 1.0);
    gtk_adjustment_set_page_increment (priv->v_adjustment, 10.0);
    gtk_adjustment_set_page_size (priv->v_adjustment, 10.0);

    g_signal_connect (priv->v_adjustment, "changed", G_CALLBACK (text_render_v_adjustment_changed), obj);
    g_signal_connect (priv->v_adjustment, "value-changed", G_CALLBACK (text_render_v_adjustment_value_changed), obj);

    text_render_v_adjustment_update (obj);
}


static void text_render_init (TextRender *w)
{
    auto priv = text_render_priv (w);

    priv->button = 0;
    priv->dispmode = DISPLAYMODE_TEXT;
    priv->h_adjustment = NULL;
    priv->hscroll_policy = GTK_SCROLL_MINIMUM;

    priv->hex_offset_display = FALSE;
    priv->chars_per_line = 0;
    priv->display_line = text_mode_display_line;
    priv->pixel_to_offset = text_mode_pixel_to_offset;
    priv->copy_to_clipboard = text_mode_copy_to_clipboard;

    priv->marker_start = 0;
    priv->marker_end = 0;

    priv->v_adjustment = NULL;
    priv->vscroll_policy = GTK_SCROLL_MINIMUM;

    priv->encoding = g_strdup ("ASCII");
    priv->utf8alloc = 0;

    priv->tab_size = 8;
    priv->font_size = 12;

    priv->fixed_font_name = g_strdup ("Monospace");

    priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (w), NULL);

    gtk_widget_set_can_focus(GTK_WIDGET (w), TRUE);

    text_render_setup_font(w, priv->fixed_font_name, priv->font_size);

    GtkEventController *scroll_controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_widget_add_controller (GTK_WIDGET (w), GTK_EVENT_CONTROLLER (scroll_controller));
    g_signal_connect (scroll_controller, "scroll", G_CALLBACK (text_render_scroll), w);

    GtkGesture *button_gesture = gtk_gesture_click_new ();
    gtk_widget_add_controller (GTK_WIDGET (w), GTK_EVENT_CONTROLLER (button_gesture));
    g_signal_connect (button_gesture, "pressed", G_CALLBACK (text_render_button_press), w);
    g_signal_connect (button_gesture, "released", G_CALLBACK (text_render_button_release), w);

    GtkEventController* motion_controller = gtk_event_controller_motion_new ();
    gtk_widget_add_controller (GTK_WIDGET (w), GTK_EVENT_CONTROLLER (motion_controller));
    g_signal_connect (motion_controller, "motion", G_CALLBACK (text_render_motion_notify), w);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (w), GTK_EVENT_CONTROLLER (key_controller));
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (text_render_key_pressed), w);
}


static void text_render_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
    auto priv = text_render_priv (TEXT_RENDER (obj));
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
        case PROP_FONT_SIZE:
            g_value_set_uint (value, priv->font_size);
            break;
        case PROP_TAB_SIZE:
            g_value_set_uint (value, priv->tab_size);
            break;
        case PROP_WRAP_MODE:
            g_value_set_boolean (value, priv->wrapmode);
            break;
        case PROP_ENCODING:
            g_value_set_string (value, priv->encoding);
            break;
        case PROP_FIXED_LIMIT:
            g_value_set_int (value, priv->fixed_limit);
            break;
        case PROP_HEXADECIMAL_OFFSET:
            g_value_set_boolean (value, priv->hex_offset_display);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}


static void text_render_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    TextRender *w = TEXT_RENDER (obj);
    auto priv = text_render_priv (w);
    switch (prop_id)
    {
        case PROP_HADJUSTMENT:
            text_render_set_h_adjustment (w, GTK_ADJUSTMENT (g_value_get_object (value)));
            break;
        case PROP_VADJUSTMENT:
            text_render_set_v_adjustment (w, GTK_ADJUSTMENT (g_value_get_object (value)));
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
        case PROP_FONT_SIZE:
            text_render_set_font_size (w, g_value_get_uint (value));
            break;
        case PROP_TAB_SIZE:
            text_render_set_tab_size (w, g_value_get_uint (value));
            break;
        case PROP_WRAP_MODE:
            text_render_set_wrap_mode (w, g_value_get_boolean (value));
            break;
        case PROP_ENCODING:
            text_render_set_encoding (w, g_value_get_string (value));
            break;
        case PROP_FIXED_LIMIT:
            text_render_set_fixed_limit (w, g_value_get_int (value));
            break;
        case PROP_HEXADECIMAL_OFFSET:
            text_render_set_hex_offset_display (w, g_value_get_boolean (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
    }
}


static void text_render_finalize (GObject *object)
{
    TextRender *w = TEXT_RENDER (object);
    auto priv = text_render_priv (w);

    g_clear_pointer (&priv->fixed_font_name, g_free);
    g_clear_object (&priv->v_adjustment);
    g_clear_object (&priv->h_adjustment);
    g_clear_pointer (&priv->encoding, g_free);
    text_render_free_font(w);
    text_render_free_data(w);
    g_clear_pointer (&priv->utf8buf, g_free);

    G_OBJECT_CLASS (text_render_parent_class)->finalize (object);
}


static void text_render_class_init (TextRenderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->set_property = text_render_set_property;
    object_class->get_property = text_render_get_property;
    object_class->finalize = text_render_finalize;

    widget_class->snapshot = text_render_draw;
    widget_class->measure = text_render_measure;
    widget_class->size_allocate = text_render_size_allocate;

    g_object_class_override_property (object_class, PROP_HADJUSTMENT,    "hadjustment");
    g_object_class_override_property (object_class, PROP_VADJUSTMENT,    "vadjustment");
    g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
    g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

    g_object_class_install_property (object_class, PROP_FONT_SIZE,
        g_param_spec_uint ("font-size", "Font size", "Font size",
            0, G_MAXUINT, 12,
            (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_TAB_SIZE,
        g_param_spec_uint ("tab-size", "Tab size", "Tab size",
            0, G_MAXUINT, 8,
            (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_WRAP_MODE,
        g_param_spec_boolean ("wrap-mode", "Wrap mode", "Wrap mode",
            FALSE,
            (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_ENCODING,
        g_param_spec_string ("encoding", "Encoding", "Encoding",
            "ASCII",
            (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_FIXED_LIMIT,
        g_param_spec_int ("fixed-limit", "Fixed limit", "Fixed limit",
            0, G_MAXINT, 80,
            (GParamFlags) G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_HEXADECIMAL_OFFSET,
        g_param_spec_boolean ("hexadecimal-offset", "Hexadecimal offset", "Hexadecimal offset",
            FALSE,
            (GParamFlags) G_PARAM_READWRITE));

    text_render_signals[TEXT_STATUS_CHANGED] =
        g_signal_new ("text-status-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (TextRenderClass, text_status_changed),
            NULL, NULL,
            NULL,
            G_TYPE_NONE,
            0);
}


void text_render_notify_status_changed(TextRender *w)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    g_signal_emit (w, text_render_signals[TEXT_STATUS_CHANGED], 0);
}


static void text_render_position_changed(TextRender *w)
{
    text_render_notify_status_changed(w);
}


static gboolean text_render_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    g_return_val_if_fail (IS_TEXT_RENDER (user_data), FALSE);
    TextRender *obj = TEXT_RENDER (user_data);
    auto priv = text_render_priv (obj);

    if (!priv->dp)
        return FALSE;

    auto column = text_render_get_column (obj);
    auto current_offset = text_render_get_current_offset (obj);

    switch (keyval)
    {
    case GDK_KEY_Up:
        current_offset = gv_scroll_lines (priv->dp, current_offset, -1);
        gtk_adjustment_set_value (priv->v_adjustment, current_offset);
        break;

    case GDK_KEY_Page_Up:
        current_offset = gv_scroll_lines (priv->dp, current_offset, -1 * (priv->lines_displayed - 1));
        gtk_adjustment_set_value (priv->v_adjustment, current_offset);
        break;

    case GDK_KEY_Page_Down:
        current_offset = gv_scroll_lines (priv->dp, current_offset, priv->lines_displayed - 1);
        gtk_adjustment_set_value (priv->v_adjustment, current_offset);
        break;

    case GDK_KEY_Down:
        current_offset = gv_scroll_lines (priv->dp, current_offset, 1);
        gtk_adjustment_set_value (priv->v_adjustment, current_offset);
        break;

    case GDK_KEY_Left:
        if (!priv->wrapmode)
            if (column > 0)
                gtk_adjustment_set_value (priv->h_adjustment, column - 1);
        break;

    case GDK_KEY_Right:
        if (!priv->wrapmode)
            gtk_adjustment_set_value (priv->h_adjustment, column + 1);
        break;

    case GDK_KEY_Home:
        gtk_adjustment_set_value (priv->v_adjustment, 0);
        break;

    case GDK_KEY_End:
        current_offset = gv_align_offset_to_line_start(priv->dp, gv_file_get_max_offset(priv->fops) - 1);
        gtk_adjustment_set_value (priv->v_adjustment, current_offset);
        break;

    default:
        return FALSE;
    }

    text_render_position_changed(obj);
    gtk_widget_queue_draw (GTK_WIDGET (obj));

    return TRUE;
}


static void text_render_measure (GtkWidget* widget, GtkOrientation orientation, int for_size, int* minimum, int* natural, int* minimum_baseline, int* natural_baseline)
{
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        *minimum = *natural = TEXT_RENDER_DEFAULT_WIDTH;
    else
        *minimum = *natural = TEXT_RENDER_DEFAULT_HEIGHT;
}

static void text_render_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
    g_return_if_fail (IS_TEXT_RENDER (widget));

    gtk_widget_allocate (widget, width, height, baseline, nullptr);
    TextRender *w = TEXT_RENDER (widget);
    auto priv = text_render_priv (w);

    if (priv->dp && (priv->char_width>0))
    {
        priv->chars_per_line = width / priv->char_width;
        gv_set_wrap_limit(priv->dp, width / priv->char_width);
        gtk_widget_queue_draw (widget);
    }

    priv->lines_displayed = priv->char_height>0 ? height / priv->char_height : 10;
}


static void text_render_draw (GtkWidget *widget, GtkSnapshot *snapshot)
{
    gint y, rc;
    offset_type ofs;

    TextRender *w = TEXT_RENDER (widget);
    auto priv = text_render_priv (w);

    g_return_if_fail (priv->display_line != NULL);

    if (priv->dp==NULL)
        return;

    GtkAllocation widget_allocation;
    gtk_widget_get_allocation (widget, &widget_allocation);

    graphene_rect_t rect = { { 0, 0 }, { (float) widget_allocation.width, (float) widget_allocation.height } };
    gtk_snapshot_push_clip (snapshot, &rect);

    int column = text_render_get_column (w);

    ofs = text_render_get_current_offset (w);
    y = 0;

    while (TRUE)
    {
        offset_type eol_offset;

        eol_offset = gv_get_end_of_line_offset(priv->dp, ofs);
        if (eol_offset == ofs)
            break;

        gtk_snapshot_save (snapshot);
        graphene_point_t pt = { .x = 0, .y = y };
        gtk_snapshot_translate (snapshot, &pt);
        priv->display_line(w, snapshot, column, ofs, eol_offset);
        gtk_snapshot_restore (snapshot);

        ofs = eol_offset;

        y += priv->char_height;
        if (y>=widget_allocation.height)
            break;

    }

    gtk_snapshot_pop (snapshot);

    priv->last_displayed_offset = ofs;
}


static void text_render_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data)
{
    g_return_if_fail (IS_TEXT_RENDER (user_data));
    TextRender *w = TEXT_RENDER (user_data);
    auto priv = text_render_priv (w);

    if (!priv->dp)
        return;

    auto current_offset = text_render_get_current_offset (w);
    current_offset = gv_scroll_lines (priv->dp, current_offset, 4 * dy);
    gtk_adjustment_set_value (priv->v_adjustment, current_offset);

    text_render_position_changed (w);
    gtk_widget_queue_draw (GTK_WIDGET (w));
}


void  text_render_copy_selection(TextRender *w)
{
    g_return_if_fail (w!=NULL);
    auto priv = text_render_priv (w);
    g_return_if_fail (priv->copy_to_clipboard != NULL);

    if (priv->marker_start == priv->marker_end)
        return;

    offset_type marker_start = priv->marker_start;
    offset_type marker_end   = priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    priv->copy_to_clipboard(w, marker_start, marker_end);
}


static void text_render_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    g_return_if_fail (IS_TEXT_RENDER (user_data));
    TextRender *w = TEXT_RENDER (user_data);
    auto priv = text_render_priv (w);

    g_return_if_fail (priv->pixel_to_offset != NULL);

    auto button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    if (n_press == 1 && !priv->button)
    {
        priv->button = button;
        priv->marker_start = priv->pixel_to_offset(w, (int) x, (int) y, TRUE);
    }
}


static void text_render_button_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    g_return_if_fail (IS_TEXT_RENDER (user_data));
    TextRender *w = TEXT_RENDER (user_data);
    auto priv = text_render_priv (w);

    g_return_if_fail (priv->pixel_to_offset != NULL);

    auto button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    if (priv->button == button)
    {
        priv->button = 0;

        priv->marker_end = priv->pixel_to_offset(w, (int) x, (int) y, FALSE);
        gtk_widget_queue_draw (GTK_WIDGET (w));
    }
}


static void text_render_motion_notify (GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    g_return_if_fail (IS_TEXT_RENDER (user_data));
    TextRender *w = TEXT_RENDER (user_data);
    auto priv = text_render_priv (w);

    g_return_if_fail (priv->pixel_to_offset != NULL);

    if (priv->button != 0)
    {
        offset_type new_marker = priv->pixel_to_offset (w, x, y, FALSE);
        if (new_marker != priv->marker_end)
        {
            priv->marker_end = new_marker;
            gtk_widget_queue_draw (GTK_WIDGET (w));
        }
    }
}


static void text_render_h_adjustment_update (TextRender *obj)
{
    g_return_if_fail (IS_TEXT_RENDER (obj));
    text_render_notify_status_changed (obj);
    gtk_widget_queue_draw (GTK_WIDGET (obj));
}


static void text_render_h_adjustment_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    text_render_h_adjustment_update (obj);
}


static void text_render_h_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    text_render_h_adjustment_update (obj);
}


static void text_render_v_adjustment_update (TextRender *obj)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (IS_TEXT_RENDER (obj));
    auto priv = text_render_priv (obj);

    gfloat new_value = gtk_adjustment_get_value (priv->v_adjustment);

    if (priv->dp)
        new_value = gv_align_offset_to_line_start(priv->dp, (offset_type) new_value);

    if (new_value != gtk_adjustment_get_value (priv->v_adjustment))
    {
        gtk_adjustment_set_value (priv->v_adjustment, new_value);
        g_signal_emit_by_name (priv->v_adjustment, "value-changed");
    }

    text_render_notify_status_changed (obj);
    gtk_widget_queue_draw (GTK_WIDGET (obj));
}


static void text_render_v_adjustment_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    text_render_v_adjustment_update (obj);
}


static void text_render_v_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data)
{
    g_return_if_fail (adjustment != NULL);
    g_return_if_fail (data != NULL);

    TextRender *obj = TEXT_RENDER (data);

    text_render_v_adjustment_update (obj);
}


static void text_render_free_data(TextRender *w)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    g_clear_pointer (&priv->dp, gv_free_data_presentation);
    g_clear_pointer (&priv->im, gv_free_input_modes);
    g_clear_pointer (&priv->fops, gv_file_free);
}


/*
  This function assumes W->PRIV->FOPS has been initialized correctly
   with whether a file name or a file descriptor
*/
static void text_render_internal_load(TextRender *w)
{
    auto priv = text_render_priv (w);

    gtk_adjustment_set_value (priv->h_adjustment, 0);
    gtk_adjustment_set_value (priv->v_adjustment, 0);

    priv->max_column = 0;

    // Setup the input mode translations
    priv->im = gv_input_modes_new();
    gv_init_input_modes(priv->im, (get_byte_proc)gv_file_get_byte, priv->fops);
    gv_set_input_mode(priv->im, priv->encoding);

    // Setup the data presentation mode
    priv->dp = gv_data_presentation_new();
    gv_init_data_presentation(priv->dp, priv->im,
        gv_file_get_max_offset(priv->fops));

    gv_set_wrap_limit(priv->dp, 50);
    gv_set_fixed_count(priv->dp, priv->fixed_limit);
    gv_set_tab_size(priv->dp, priv->tab_size);

    text_render_set_display_mode (w, DISPLAYMODE_TEXT);

    text_render_update_adjustments_limits(w);
}


void text_render_load_file(TextRender *w, const gchar *filename)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    text_render_free_data(w);

    priv->fops = gv_fileops_new();
    if (gv_file_open(priv->fops, filename)==-1)
    {
        g_warning ("Failed to load file (%s)", filename);
        return;
    }

    text_render_internal_load(w);
}


static void text_render_update_adjustments_limits(TextRender *w)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    if (!priv->fops)
        return;

    if (priv->v_adjustment)
    {
        gtk_adjustment_set_lower (priv->v_adjustment, 0);
        gtk_adjustment_set_upper (priv->v_adjustment, gv_file_get_max_offset(priv->fops)-1);
    }

    if (priv->h_adjustment)
    {
        gtk_adjustment_set_step_increment (priv->h_adjustment, 1);
        gtk_adjustment_set_page_increment (priv->h_adjustment, 5);
        gtk_adjustment_set_page_size (priv->h_adjustment, priv->chars_per_line);
        gtk_adjustment_set_lower (priv->h_adjustment, 0);
        if (gv_get_data_presentation_mode(priv->dp)==PRSNT_NO_WRAP)
            gtk_adjustment_set_upper (priv->h_adjustment, priv->max_column); // TODO: find our the real horz limit
        else
            gtk_adjustment_set_upper (priv->h_adjustment, 0);
    }
}


static void text_render_free_font(TextRender*w)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    g_clear_pointer (&priv->disp_font_metrics, pango_font_metrics_unref);
    g_clear_pointer (&priv->font_desc, pango_font_description_free);
}


static PangoFontMetrics *load_font (TextRender* w, const char *font_name)
{
    PangoFontDescription *new_desc = pango_font_description_from_string (font_name);
    PangoContext *context = gtk_widget_get_pango_context (GTK_WIDGET (w));
    PangoFont *new_font = pango_context_load_font (context, new_desc);
    PangoFontMetrics *new_metrics = pango_font_get_metrics (new_font, pango_context_get_language (context));

    pango_font_description_free (new_desc);
    g_object_unref (new_font);

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

    for (guint i=1; i<0x100; i++)
    {
        logical_rect.width = 0;
        // Check if the char is displayable. Caused trouble to pango
        if (is_displayable((guchar)i))
        {
            sprintf (str, "%c", (gchar) i);
            pango_layout_set_text(layout, str, -1);
            pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
        }
        maxwidth = MAX(maxwidth, MAX(0, logical_rect.width));
    }

    g_object_unref (layout);
    return maxwidth;
}


static guint text_render_filter_undisplayable_chars(TextRender *obj)
{
    auto priv = text_render_priv (obj);

    if (!priv->im)
        return 0;

    PangoRectangle logical_rect;

    PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (obj), "");
    pango_layout_set_font_description (layout, priv->font_desc);

    for (guint i=0; i<256; i++)
    {
        char_type value = gv_input_mode_byte_to_utf8(priv->im, (unsigned char) i);
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
            gv_input_mode_update_utf8_translation(priv->im, i, '.');
    }

    g_object_unref (layout);
    return 0;
}


static void text_render_setup_font(TextRender*w, const gchar *fontname, gint fontsize)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    g_return_if_fail (fontname!=NULL);
    g_return_if_fail (fontsize>0);
    auto priv = text_render_priv (w);

    text_render_free_font(w);

    gchar *fontlabel = g_strdup_printf ("%s %d", fontname, fontsize);

    priv->disp_font_metrics = load_font (w, fontlabel);
    priv->font_desc = pango_font_description_from_string (fontlabel);

    priv->char_width = get_max_char_width(GTK_WIDGET (w),
            priv->font_desc,
            priv->disp_font_metrics);

    priv->char_height =
       PANGO_PIXELS(pango_font_metrics_get_ascent(priv->disp_font_metrics)) +
       PANGO_PIXELS(pango_font_metrics_get_descent(priv->disp_font_metrics));

    g_free (fontlabel);
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


void  text_render_set_display_mode (TextRender *w, DISPLAYMODE mode)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);
    g_return_if_fail (priv->fops!=NULL);
    g_return_if_fail (priv->im!=NULL);
    g_return_if_fail (priv->dp!=NULL);

    if (mode == priv->dispmode)
        return;

    gtk_adjustment_set_value (priv->h_adjustment, 0);

    switch (mode)
    {
    case DISPLAYMODE_TEXT:
        gv_set_data_presentation_mode(priv->dp, priv->wrapmode ? PRSNT_WRAP : PRSNT_NO_WRAP);

        priv->display_line = text_mode_display_line;
        priv->pixel_to_offset = text_mode_pixel_to_offset;
        priv->copy_to_clipboard = text_mode_copy_to_clipboard;
        break;

    case DISPLAYMODE_BINARY:

        // Binary display mode doesn't support UTF8
        // TODO: switch back to the previous encoding, not just ASCII
        //        gv_set_input_mode(w->priv->im, "ASCII");

        gv_set_fixed_count(priv->dp, priv->fixed_limit);
        gv_set_data_presentation_mode(priv->dp, PRSNT_BIN_FIXED);

        priv->display_line = binary_mode_display_line;
        priv->pixel_to_offset = text_mode_pixel_to_offset;
        priv->copy_to_clipboard = text_mode_copy_to_clipboard;
        break;

    case DISPLAYMODE_HEXDUMP:

        // HEX display mode doesn't support UTF8
        // TODO: switch back to the previous encoding, not just ASCII
        //        gv_set_input_mode(w->priv->im, "ASCII");

        gv_set_fixed_count(priv->dp, HEXDUMP_FIXED_LIMIT);
        gv_set_data_presentation_mode(priv->dp, PRSNT_BIN_FIXED);

        priv->display_line = hex_mode_display_line;
        priv->pixel_to_offset = hex_mode_pixel_to_offset;
        priv->copy_to_clipboard = hex_mode_copy_to_clipboard;
        break;

    default:
        break;
    }

    text_render_setup_font (w, priv->fixed_font_name, priv->font_size);
    priv->dispmode = mode;
    auto current_offset = text_render_get_current_offset (w);
    current_offset = gv_align_offset_to_line_start (priv->dp, current_offset);
    gtk_adjustment_set_value (priv->v_adjustment, current_offset);

    gtk_widget_queue_draw (GTK_WIDGET (w));
}


DISPLAYMODE text_render_get_display_mode(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), DISPLAYMODE_TEXT);
    auto priv = text_render_priv (w);

    return priv->dispmode;
}


ViewerFileOps *text_render_get_file_ops(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), NULL);
    auto priv = text_render_priv (w);
    g_return_val_if_fail (priv->fops!=NULL, NULL);

    return priv->fops;
}


GVInputModesData *text_render_get_input_mode_data(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), NULL);
    auto priv = text_render_priv (w);
    g_return_val_if_fail (priv->im!=NULL, NULL);

    return priv->im;
}


GVDataPresentation *text_render_get_data_presentation(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), NULL);
    auto priv = text_render_priv (w);
    g_return_val_if_fail (priv->dp!=NULL, NULL);

    return priv->dp;
}


void text_render_set_tab_size(TextRender *w, int tab_size)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    if (priv->dp==NULL)
        return;

    if (tab_size<=0)
        return;

    priv->tab_size = tab_size;
    gv_set_tab_size(priv->dp, tab_size);

    gtk_widget_queue_draw (GTK_WIDGET (w));
}


int text_render_get_tab_size(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);
    auto priv = text_render_priv (w);

    return priv->tab_size;
}


void text_render_set_wrap_mode(TextRender *w, gboolean ACTIVE)
{
    g_return_if_fail (w!=NULL);
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    if (priv->dp==NULL)
        return;

    priv->wrapmode = ACTIVE;
    if (priv->dispmode==DISPLAYMODE_TEXT)
    {
        gtk_adjustment_set_value (priv->h_adjustment, 0);
        gv_set_data_presentation_mode(priv->dp, priv->wrapmode ? PRSNT_WRAP : PRSNT_NO_WRAP);
        text_render_update_adjustments_limits(w);
    }
    gtk_widget_queue_draw (GTK_WIDGET (w));
}


gboolean text_render_get_wrap_mode(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), FALSE);
    auto priv = text_render_priv (w);

    return priv->wrapmode;
}


void text_render_set_fixed_limit(TextRender *w, int fixed_limit)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    // this is saved later, for binary display mode. Hex display mode always use 16 bytes
    priv->fixed_limit = fixed_limit;

    // always 16 bytes in hex dump
    if (priv->dispmode==DISPLAYMODE_HEXDUMP)
        fixed_limit = HEXDUMP_FIXED_LIMIT;

    if (priv->dp)
        gv_set_fixed_count(priv->dp, fixed_limit);
    gtk_widget_queue_draw (GTK_WIDGET (w));
}


int text_render_get_fixed_limit(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);
    auto priv = text_render_priv (w);

    return priv->fixed_limit;
}


offset_type text_render_get_current_offset(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);
    auto priv = text_render_priv (w);

    return priv->v_adjustment ? (offset_type) gtk_adjustment_get_value (priv->v_adjustment) : 0;
}


offset_type text_render_get_size(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);
    auto priv = text_render_priv (w);

    return priv->fops ? gv_file_get_max_offset(priv->fops) : 0;
}


int text_render_get_column(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);
    auto priv = text_render_priv (w);

    return priv->h_adjustment ? (int) gtk_adjustment_get_value (priv->h_adjustment) : 0;
}


offset_type text_render_get_last_displayed_offset(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);
    auto priv = text_render_priv (w);

    return priv->last_displayed_offset;
}


void text_render_ensure_offset_visible(TextRender *w, offset_type offset)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    if (offset < text_render_get_current_offset (w) || offset > priv->last_displayed_offset)
    {
        offset = gv_align_offset_to_line_start(priv->dp, offset);
        offset = gv_scroll_lines (priv->dp, offset, -priv->lines_displayed/2);

        gtk_adjustment_set_value (priv->v_adjustment, offset);
        gtk_widget_queue_draw (GTK_WIDGET (w));
        text_render_position_changed(w);
    }
}


void text_render_set_marker(TextRender *w, offset_type start, offset_type end)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    priv->marker_start = start;
    priv->marker_end = end;
    gtk_widget_queue_draw (GTK_WIDGET (w));
}


void text_render_set_encoding(TextRender *w, const char *encoding)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    // Ugly hack: UTF-8 is not acceptable encoding in Binary/Hexdump modes
    if (g_ascii_strcasecmp (encoding, "UTF8")==0 && (
        priv->dispmode==DISPLAYMODE_BINARY || priv->dispmode==DISPLAYMODE_HEXDUMP))
        {
            g_warning ("Can't set UTF8 encoding when in Binary or HexDump display mode");
            return;
        }

    g_free (priv->encoding);
    priv->encoding = g_strdup (encoding);
    if (priv->im)
    {
        gv_set_input_mode(priv->im, encoding);
        text_render_filter_undisplayable_chars(w);
        gtk_widget_queue_draw (GTK_WIDGET (w));
    }
}


const gchar *text_render_get_encoding(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), NULL);
    auto priv = text_render_priv (w);

    return priv->encoding;
}


void text_render_set_hex_offset_display(TextRender *w, gboolean HEX_OFFSET)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);

    priv->hex_offset_display = HEX_OFFSET;
    gtk_widget_queue_draw (GTK_WIDGET (w));
}


gboolean text_render_get_hex_offset_display(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), FALSE);
    auto priv = text_render_priv (w);

    return priv->hex_offset_display;
}


void text_render_set_font_size(TextRender *w, int font_size)
{
    g_return_if_fail (IS_TEXT_RENDER (w));
    auto priv = text_render_priv (w);
    g_return_if_fail (font_size>=4);

    priv->font_size = font_size;
    gchar *font = priv->fixed_font_name;

    text_render_setup_font(w, font, font_size);
    gtk_widget_queue_draw (GTK_WIDGET (w));
}


int  text_render_get_font_size(TextRender *w)
{
    g_return_val_if_fail (IS_TEXT_RENDER (w), 0);
    auto priv = text_render_priv (w);

    return priv->font_size;
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


static offset_type text_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker)
{
    g_return_val_if_fail (obj!=NULL, 0);
    auto priv = text_render_priv (obj);
    g_return_val_if_fail (priv->dp!=NULL, 0);

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

    if (priv->char_height<=0)
        return current_offset;

    if (priv->char_width<=0)
        return current_offset;

    line = y / priv->char_height;
    column = x / priv->char_width + text_render_get_column (obj);

    // Determine offset corresponding to start of line, the character at this offset and the last column occupied by character
    offset = gv_scroll_lines (priv->dp, current_offset, line);
    choff = gv_input_mode_get_utf8_char(priv->im, offset);
    choffcol = (choff=='\t') ? priv->tab_size-1 : 0;

    next_line_offset = gv_scroll_lines (priv->dp, offset, 1);

    // While the current character does not occupy column 'column', check next character
    while (column>choffcol && offset<next_line_offset)
    {
        offset = gv_input_get_next_char_offset(priv->im, offset);
        choff = gv_input_mode_get_utf8_char(priv->im, offset);
        choffcol += (choff=='\t') ? priv->tab_size : 1;
    }

    // Increment offset if doing end-marker
    if (!start_marker)
        offset++;

    return offset;
}


static void text_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (start_offset!=end_offset);
    auto priv = text_render_priv (obj);
    g_return_if_fail (priv->dp!=NULL);
    g_return_if_fail (priv->im!=NULL);

    GdkClipboard *clip = gtk_widget_get_clipboard (GTK_WIDGET (obj));
    g_return_if_fail (clip!=NULL);

    text_render_utf8_clear_buf(obj);
    offset_type current = start_offset;
    while (current < end_offset && priv->utf8buf_length<MAX_CLIPBOARD_COPY_LENGTH)
    {
        char_type value = gv_input_mode_get_utf8_char(priv->im, current);
        if (value==INVALID_CHAR)
            break;

        current = gv_input_get_next_char_offset(priv->im, current);
        text_render_utf8_print_char(obj, value);
    }

    gdk_clipboard_set_text (clip, (const gchar *) priv->utf8buf);
}


void text_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line)
{
    auto priv = text_render_priv (w);

    offset_type current;
    char_type value;
    int char_count = 0;
    offset_type marker_start;
    offset_type marker_end;
    gboolean show_marker;
    gboolean marker_shown = FALSE;

    marker_start = priv->marker_start;
    marker_end = priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    show_marker = marker_start!=marker_end;

    if (priv->wrapmode)
        column = 0;

    text_render_utf8_clear_buf(w);

    current = start_of_line;
    while (current < end_of_line)
    {
        if (show_marker)
            marker_shown = marker_helper(w, marker_shown, current, marker_start, marker_end);

        // Read a UTF8 character from the input file. The "inputmode" module is responsible for converting the file into UTF8
        value = gv_input_mode_get_utf8_char(priv->im, current);
        if (value==INVALID_CHAR)
            break;

        // move to the next character's offset
        current = gv_input_get_next_char_offset(priv->im, current);

        if (value=='\r' || value=='\n')
            continue;

        if (value=='\t')
        {
            for (int i=0; i<priv->tab_size; i++)
                text_render_utf8_print_char(w, ' ');
            char_count += priv->tab_size;
            continue;
        }

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf (w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);

        char_count++;
    }

    if (char_count > priv->max_column)
    {
        priv->max_column = char_count;
        text_render_update_adjustments_limits(w);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_font_description (priv->layout, priv->font_desc);
    pango_layout_set_markup (priv->layout, (gchar *) priv->utf8buf, priv->utf8buf_length);
    graphene_point_t pt = { .x = -(priv->char_width*column), .y = 0 };
    gtk_snapshot_translate (snapshot, &pt);
    GdkRGBA color = { .red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0 };
    gtk_snapshot_append_layout (snapshot, priv->layout, &color);
}


void binary_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line)
{
    auto priv = text_render_priv (w);

    offset_type current;
    char_type value;
    offset_type marker_start;
    offset_type marker_end;
    gboolean show_marker;
    gboolean marker_shown = FALSE;

    marker_start = priv->marker_start;
    marker_end = priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    show_marker = marker_start!=marker_end;
    text_render_utf8_clear_buf(w);

    current = start_of_line;
    while (current < end_of_line)
    {

        if (show_marker)
            marker_shown = marker_helper(w, marker_shown, current, marker_start, marker_end);

        /* Read a UTF8 character from the input file.
           The "inputmode" module is responsible for converting the file into UTF8 */
        value = gv_input_mode_get_utf8_char(priv->im, current);
        if (value==INVALID_CHAR)
            break;

        // move to the next character's offset
        current = gv_input_get_next_char_offset(priv->im, current);

        if (value=='\r' || value=='\n' || value=='\t')
            value = gv_input_mode_byte_to_utf8(priv->im, (unsigned char)value);

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf (w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_font_description (priv->layout, priv->font_desc);
    pango_layout_set_markup (priv->layout, (gchar *) priv->utf8buf, priv->utf8buf_length);
    graphene_point_t pt = { .x = -(priv->char_width*column), .y = 0 };
    gtk_snapshot_translate (snapshot, &pt);
    GdkRGBA color = { .red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0 };
    gtk_snapshot_append_layout (snapshot, priv->layout, &color);
}


static offset_type hex_mode_pixel_to_offset(TextRender *obj, int x, int y, gboolean start_marker)
{
    g_return_val_if_fail (obj!=NULL, 0);
    auto priv = text_render_priv (obj);
    g_return_val_if_fail (priv->dp!=NULL, 0);

    int line = 0;
    int column = 0;
    offset_type current_offset = text_render_get_current_offset (obj);
    offset_type offset;
    offset_type next_line_offset;

    if (x<0)
        x = 0;
    if (y<0)
        return current_offset;

    if (priv->char_height<=0)
        return current_offset;

    if (priv->char_width<=0)
        return current_offset;

    line = y / priv->char_height;
    column = x / priv->char_width;

    offset = gv_scroll_lines (priv->dp, current_offset, line);
    next_line_offset = gv_scroll_lines (priv->dp, offset, 1);

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
        offset = gv_input_get_next_char_offset(priv->im, offset);
        column--;
    }

    return offset;
}


static void hex_mode_copy_to_clipboard(TextRender *obj, offset_type start_offset, offset_type end_offset)
{
    g_return_if_fail (obj!=NULL);
    g_return_if_fail (start_offset!=end_offset);
    auto priv = text_render_priv (obj);
    g_return_if_fail (priv->dp!=NULL);
    g_return_if_fail (priv->im!=NULL);

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
        char_type value = gv_input_mode_get_raw_byte(priv->im, current);
        if (value==INVALID_CHAR)
            break;
        text_render_utf8_printf (obj, "%02x ", (unsigned char) value);
    }

    gdk_clipboard_set_text (clip, (const gchar *) priv->utf8buf);
}


void hex_mode_display_line(TextRender *w, GtkSnapshot *snapshot, int column, offset_type start_of_line, offset_type end_of_line)
{
    auto priv = text_render_priv (w);

    offset_type marker_start = priv->marker_start;
    offset_type marker_end = priv->marker_end;

    if (marker_start > marker_end)
    {
        offset_type temp = marker_end;
        marker_end = marker_start;
        marker_start = temp;
    }

    gboolean marker_shown = FALSE;
    gboolean show_marker = marker_start!=marker_end;
    text_render_utf8_clear_buf(w);

    if (priv->hex_offset_display)
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

        int byte_value = gv_input_mode_get_raw_byte(priv->im, current);

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

        int byte_value = gv_input_mode_get_raw_byte(priv->im, current);

        if (byte_value==-1)
            break;

        char_type value = gv_input_mode_byte_to_utf8(priv->im, (unsigned char) byte_value);

        if (NEED_PANGO_ESCAPING(value))
            text_render_utf8_printf (w, escape_pango_char(value));
        else
            text_render_utf8_print_char(w, value);
    }

    if (show_marker)
        marker_closer(w, marker_shown);

    pango_layout_set_font_description (priv->layout, priv->font_desc);
    pango_layout_set_markup (priv->layout, (gchar *) priv->utf8buf, priv->utf8buf_length);
    GdkRGBA color = { .red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0 };
    gtk_snapshot_append_layout (snapshot, priv->layout, &color);
}
