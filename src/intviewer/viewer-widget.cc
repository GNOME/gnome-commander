/**
 * @file viewer-widget.cc
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

#include <iostream>

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "libgviewer.h"
#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "utils.h"

using namespace std;


#define DEFAULT_TAB_SIZE          8
#define DEFAULT_WRAP_MODE      TRUE
#define DEFAULT_FIXED_LIMIT      80
#define DEFAULT_ENCODING    "ASCII"
#define DEFAULT_BEST_FIT       TRUE
#define DEFAULT_SCALE_FACTOR    1.0


/* Class Private Data */
struct GViewerPrivate
{
    GtkWidget         *tscrollbox;
    TextRender        *textr;

    GtkWidget         *iscrollbox;
    ImageRender       *imgr;
    gboolean          img_initialized;

    GtkWidget         *last_client;

    gchar             *filename;
    VIEWERDISPLAYMODE dispmode;

};

enum
{
  STATUS_LINE_CHANGED,
  LAST_SIGNAL
};

static guint gviewer_signals[LAST_SIGNAL] = { 0 };

/* Gtk class related static functions */
static void gviewer_init (GViewer *w);
static void gviewer_class_init (GViewerClass *klass);
static void gviewer_dispose (GObject *object);

static void gviewer_text_status_update(TextRender *obj, TextRender::Status *status, GViewer *viewer);
static void gviewer_image_status_update(ImageRender *obj, ImageRender::Status *status, GViewer *viewer);
static void on_text_viewer_button_pressed (GtkGestureMultiPress *gesture, int n_press, double x, double y, gpointer user_data);

static VIEWERDISPLAYMODE guess_display_mode(const char *filename, int len);
static void gviewer_auto_detect_display_mode(GViewer *obj);
static void gviewer_copy_selection_handler(GSimpleAction *action, GVariant *parameter, gpointer user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GViewer, gviewer, GTK_TYPE_GRID)

/*****************************************
    public functions
    (defined in the header file)
*****************************************/

GtkWidget *gviewer_new ()
{
    auto w = static_cast<GViewer*> (g_object_new (gviewer_get_type (), NULL));

    return GTK_WIDGET (w);
}


static void gviewer_class_init (GViewerClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = gviewer_dispose;

    gviewer_signals[STATUS_LINE_CHANGED] =
        g_signal_new ("status-line-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GViewerClass, status_line_changed),
                      nullptr, nullptr,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1, G_TYPE_STRING);
}


static void gviewer_init (GViewer *w)
{
    static GActionEntry action_entries[] = {
        { "copy-selection", gviewer_copy_selection_handler }
    };
    GSimpleActionGroup *action_group = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), action_entries, G_N_ELEMENTS (action_entries), w);
    gtk_widget_insert_action_group (GTK_WIDGET (w), "viewer", G_ACTION_GROUP (action_group));

    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (w));

    priv->img_initialized = FALSE;
    priv->dispmode = DISP_MODE_TEXT_FIXED;

    priv->textr = reinterpret_cast<TextRender*> (text_render_new());

    gviewer_set_tab_size(w, DEFAULT_TAB_SIZE);
    gviewer_set_wrap_mode(w, DEFAULT_WRAP_MODE);
    gviewer_set_fixed_limit(w, DEFAULT_FIXED_LIMIT);
    gviewer_set_encoding(w, DEFAULT_ENCODING);

    priv->tscrollbox = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_container_add (GTK_CONTAINER (priv->tscrollbox), GTK_WIDGET (priv->textr));
    gtk_widget_set_hexpand (GTK_WIDGET (priv->tscrollbox), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (priv->tscrollbox), TRUE);
    gtk_widget_show (GTK_WIDGET (priv->textr));
    gtk_widget_show (priv->tscrollbox);
    g_object_ref (priv->tscrollbox);

    priv->imgr = reinterpret_cast<ImageRender*> (image_render_new());
    gviewer_set_best_fit(w, DEFAULT_BEST_FIT);
    gviewer_set_scale_factor(w, DEFAULT_SCALE_FACTOR);
    priv->iscrollbox = gtk_scrolled_window_new (nullptr, nullptr);
    image_render_set_best_fit (priv->imgr, TRUE);
    image_render_set_scale_factor (priv->imgr, 1);
    gtk_container_add (GTK_CONTAINER (priv->iscrollbox), GTK_WIDGET (priv->imgr));
    gtk_widget_set_hexpand (GTK_WIDGET (priv->iscrollbox), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (priv->iscrollbox), TRUE);
    gtk_widget_show (GTK_WIDGET (priv->imgr));
    gtk_widget_show (priv->iscrollbox);
    g_object_ref (priv->iscrollbox);

    priv->last_client = priv->tscrollbox;
    gtk_grid_attach (GTK_GRID (w), GTK_WIDGET (priv->tscrollbox), 0, 0, 1, 1);

    g_signal_connect (priv->textr, "text-status-changed", G_CALLBACK (gviewer_text_status_update), w);
    g_signal_connect (priv->imgr, "image-status-changed", G_CALLBACK (gviewer_image_status_update), w);

    GtkGesture *button_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (priv->textr));
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (button_gesture), 3);
    g_signal_connect (button_gesture, "pressed", G_CALLBACK (on_text_viewer_button_pressed), w);

}


#define MAX_STATUS_LENGTH 128
static void gviewer_text_status_update(TextRender *obj, TextRender::Status *status, GViewer *viewer)
{
    g_return_if_fail (IS_GVIEWER (viewer));
    g_return_if_fail (status!=NULL);

    static gchar temp[MAX_STATUS_LENGTH];

    g_snprintf(temp, sizeof (temp),
               _("Position: %lu of %lu\tColumn: %d\t%s"),
               (unsigned long) status->current_offset,
               (unsigned long) status->size,
               status->column,
               status->wrap_mode?_("Wrap"):"");

    g_signal_emit (viewer, gviewer_signals[STATUS_LINE_CHANGED], 0, temp);
}


static void gviewer_image_status_update(ImageRender *obj, ImageRender::Status *status, GViewer *viewer)
{
    g_return_if_fail (IS_GVIEWER (viewer));
    g_return_if_fail (status!=NULL);

    static gchar temp[MAX_STATUS_LENGTH];

    *temp = 0;

    if (status->image_width > 0 && status->image_height > 0)
    {
        gchar zoom[10];
        char *size_string = strdup("");

        if (!status->best_fit)
            g_snprintf(zoom, sizeof(zoom), "%i%%", (int)(status->scale_factor*100.0));

        g_snprintf (temp, sizeof(temp),
                    "%i x %i %s  %i %s  %s    %s",
                    status->image_width, status->image_height,
                    ngettext ("pixel", "pixels", status->image_height),
                    status->bits_per_sample,
                    ngettext ("bit/sample", "bits/sample", status->bits_per_sample),
                    size_string,
                    status->best_fit?_("(fit to window)"):zoom);
        free(size_string);
    }

    g_signal_emit (viewer, gviewer_signals[STATUS_LINE_CHANGED], 0, temp);
}



static void on_text_viewer_button_pressed (GtkGestureMultiPress *gesture, int n_press, double x, double y, gpointer user_data)
{
    GViewer *viewer = static_cast<GViewer*>(user_data);

    if (n_press == 1)
    {
        GMenu *menu = g_menu_new ();
        g_menu_append (menu, _("_Copy selection"), "viewer.copy-selection");

        GtkWidget *popover = gtk_popover_new_from_model (GTK_WIDGET (viewer), G_MENU_MODEL (menu));
        gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
        GdkRectangle rect = { (gint) x, (gint) y, 0, 0 };
        gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
        gtk_popover_popup (GTK_POPOVER (popover));
    }
}


static void gviewer_dispose (GObject *object)
{
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (object)));

    g_clear_object (&priv->iscrollbox);
    g_clear_object (&priv->tscrollbox);

    G_OBJECT_CLASS (gviewer_parent_class)->dispose (object);
}


static VIEWERDISPLAYMODE guess_display_mode(const char *filename, int len)
{
    auto returnValue = DISP_MODE_TEXT_FIXED;
    auto gFile = g_file_new_for_path(filename);

    GError *error;
    error = nullptr;
    auto gcmdFileInfo = g_file_query_info(gFile,
                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   nullptr,
                                   &error);
    if (error)
    {
        g_message ("guess_display_mode: retrieving file info for %s failed: %s",
                    g_file_peek_path(gFile), error->message);
        g_error_free (error);
    }

    auto gFileContentType = g_file_info_get_attribute_string (gcmdFileInfo,
                                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

    if (g_ascii_strncasecmp (gFileContentType, "text/", 5) == 0)
        returnValue = DISP_MODE_TEXT_FIXED;
    if (g_ascii_strncasecmp (gFileContentType, "image/", 6) == 0)
        returnValue = DISP_MODE_IMAGE;
    if (g_ascii_strncasecmp (gFileContentType, "application/", 12) == 0)
        returnValue = DISP_MODE_BINARY;

    g_object_unref(gcmdFileInfo);
    g_object_unref(gFile);

    return returnValue;
}


void gviewer_auto_detect_display_mode(GViewer *obj)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));

    const unsigned DETECTION_BUF_LEN = 100;

    priv->dispmode = DISP_MODE_TEXT_FIXED;

    if (!priv->textr)
        return;

    ViewerFileOps *fops = text_render_get_file_ops(priv->textr);

    if (!fops)
        return;

    int count = MIN(DETECTION_BUF_LEN, gv_file_get_max_offset(fops));

    priv->dispmode = guess_display_mode(fops->filename, count);
}


void gviewer_set_display_mode(GViewer *obj, VIEWERDISPLAYMODE mode)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));

    if (mode==DISP_MODE_IMAGE && !priv->img_initialized)
    {
        // do lazy-initialization of the image render, only when the user first asks to display the file as image

        priv->img_initialized = TRUE;
        image_render_load_file(priv->imgr, priv->filename);
    }

    GtkWidget *client = NULL;

    priv->dispmode = mode;
    switch (mode)
    {
        case DISP_MODE_TEXT_FIXED:
            client = priv->tscrollbox;
            text_render_set_display_mode (priv->textr, TextRender::DISPLAYMODE_TEXT);
            break;

        case DISP_MODE_BINARY:
            client = priv->tscrollbox;
            text_render_set_display_mode (priv->textr, TextRender::DISPLAYMODE_BINARY);
            break;

        case DISP_MODE_HEXDUMP:
            client = priv->tscrollbox;
            text_render_set_display_mode (priv->textr, TextRender::DISPLAYMODE_HEXDUMP);
            break;

        case DISP_MODE_IMAGE:
            client = priv->iscrollbox;
            break;

        default:
            break;
    }

    if (client != priv->last_client)
    {
        if (priv->last_client)
            gtk_container_remove (GTK_CONTAINER(obj), priv->last_client);

        gtk_widget_grab_focus (GTK_WIDGET (client));
        gtk_widget_set_hexpand (client, TRUE);
        gtk_widget_set_vexpand (client, TRUE);
        gtk_grid_attach (GTK_GRID (obj), client, 0, 0, 1, 1);

        switch (mode)
        {
            case DISP_MODE_TEXT_FIXED:
            case DISP_MODE_BINARY:
            case DISP_MODE_HEXDUMP:
                text_render_notify_status_changed(priv->textr);
                break;

            case DISP_MODE_IMAGE:
                image_render_notify_status_changed(priv->imgr);
                break;

            default:
                break;
        }

        gtk_widget_show (client);
        priv->last_client = client;
    }
}


VIEWERDISPLAYMODE gviewer_get_display_mode(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), DISP_MODE_TEXT_FIXED);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));

    return priv->dispmode;
}


void gviewer_load_file(GViewer *obj, const gchar*filename)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (filename);
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));

    g_free (priv->filename);
    priv->filename = g_strdup (filename);

    text_render_load_file(priv->textr, priv->filename);

    gviewer_auto_detect_display_mode(obj);

    gviewer_set_display_mode(obj, priv->dispmode);
}


void gviewer_set_tab_size(GViewer *obj, int tab_size)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->textr);

    text_render_set_tab_size(priv->textr, tab_size);
}


int gviewer_get_tab_size(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, 0);

    return text_render_get_tab_size(priv->textr);
}


void gviewer_set_wrap_mode(GViewer *obj, gboolean ACTIVE)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->textr);

    text_render_set_wrap_mode(priv->textr, ACTIVE);
}


gboolean gviewer_get_wrap_mode(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), FALSE);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, FALSE);

    return text_render_get_wrap_mode(priv->textr);
}


void gviewer_set_fixed_limit(GViewer *obj, int fixed_limit)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->textr);

    text_render_set_fixed_limit(priv->textr, fixed_limit);
}


int gviewer_get_fixed_limit(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, 0);

    return text_render_get_fixed_limit(priv->textr);
}


void gviewer_set_encoding(GViewer *obj, const char *encoding)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->textr);

    text_render_set_encoding(priv->textr, encoding);
}


const gchar *gviewer_get_encoding(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), NULL);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, NULL);

    return text_render_get_encoding(priv->textr);
}


void gviewer_set_font_size(GViewer *obj, int font_size)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->textr);

    text_render_set_font_size(priv->textr, font_size);
}


int gviewer_get_font_size(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, 0);

    return text_render_get_font_size(priv->textr);
}


void gviewer_set_hex_offset_display(GViewer *obj, gboolean HEX_OFFSET)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->textr);

    text_render_set_hex_offset_display(priv->textr, HEX_OFFSET);
}


gboolean gviewer_get_hex_offset_display(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), FALSE);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, FALSE);

    return text_render_get_hex_offset_display(priv->textr);
}


void gviewer_set_best_fit(GViewer *obj, gboolean active)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->imgr);

    image_render_set_best_fit(priv->imgr, active);
}


gboolean gviewer_get_best_fit(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), FALSE);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, FALSE);

    return image_render_get_best_fit(priv->imgr);
}


void gviewer_set_scale_factor(GViewer *obj, double scalefactor)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->imgr);

    image_render_set_scale_factor(priv->imgr, scalefactor);
}


double gviewer_get_scale_factor(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->imgr, 0);

    return image_render_get_scale_factor(priv->imgr);
}


TextRender *gviewer_get_text_render(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_val_if_fail (priv->textr, 0);

    return priv->textr;
}


void gviewer_image_operation(GViewer *obj, ImageRender::DISPLAYMODE op)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->imgr);

    image_render_operation(priv->imgr, op);
}


void gviewer_copy_selection(GViewer *obj)
{
    g_return_if_fail (IS_GVIEWER (obj));
    auto priv = static_cast<GViewerPrivate*>(gviewer_get_instance_private (GVIEWER (obj)));
    g_return_if_fail (priv->textr);

    if (priv->dispmode!=DISP_MODE_IMAGE)
        text_render_copy_selection(priv->textr);
}


static void gviewer_copy_selection_handler(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GViewer *obj = static_cast<GViewer*>(user_data);
    gviewer_copy_selection(obj);
}
