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
#include <gtk/gtktable.h>
#include <gtk/gtkmarshal.h>

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

static GtkTableClass *parent_class = NULL;

enum
{
  STATUS_LINE_CHANGED,
  LAST_SIGNAL
};

static guint gviewer_signals[LAST_SIGNAL] = { 0 };

/* Gtk class related static functions */
static void gviewer_init (GViewer *w);
static void gviewer_class_init (GViewerClass *klass);
static void gviewer_destroy (GtkObject *object);

static void gviewer_text_status_update(TextRender *obj, TextRender::Status *status, GViewer *viewer);
static void gviewer_image_status_update(ImageRender *obj, ImageRender::Status *status, GViewer *viewer);
static gboolean on_text_viewer_button_pressed (GtkWidget *treeview, GdkEventButton *event, GViewer *viewer);

static VIEWERDISPLAYMODE guess_display_mode(const char *filename, int len);
static void gviewer_auto_detect_display_mode(GViewer *obj);

/*****************************************
    public functions
    (defined in the header file)
*****************************************/
GtkType gviewer_get_type ()
{
    static GtkType type = 0;
    if (type == 0)
    {
        GTypeInfo info =
        {
            sizeof (GViewerClass),
            NULL,
            NULL,
            (GClassInitFunc) gviewer_class_init,
            NULL,
            NULL,
            sizeof(GViewer),
            0,
            (GInstanceInitFunc) gviewer_init
        };
        type = g_type_register_static (GTK_TYPE_TABLE, "gviewerwidget", &info, (GTypeFlags) 0);
    }

    return type;
}


GtkWidget *gviewer_new ()
{
    auto w = static_cast<GViewer*> (g_object_new (gviewer_get_type (), NULL));

    return GTK_WIDGET (w);
}


static void gviewer_class_init (GViewerClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    parent_class = (GtkTableClass *) gtk_type_class (gtk_table_get_type ());

    object_class->destroy = gviewer_destroy;

    gviewer_signals[STATUS_LINE_CHANGED] =
        gtk_signal_new ("status-line-changed",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GViewerClass, status_line_changed),
            gtk_marshal_NONE__STRING,
            GTK_TYPE_NONE,
            1, GTK_TYPE_STRING);
}


static void gviewer_init (GViewer *w)
{
    w->priv = g_new0 (GViewerPrivate, 1);

    gtk_table_resize (GTK_TABLE (w), 1, 1);
    gtk_table_set_homogeneous(GTK_TABLE (w), FALSE);

    w->priv->img_initialized = FALSE;
    w->priv->dispmode = DISP_MODE_TEXT_FIXED;

    w->priv->textr = reinterpret_cast<TextRender*> (text_render_new());

    gviewer_set_tab_size(w, DEFAULT_TAB_SIZE);
    gviewer_set_wrap_mode(w, DEFAULT_WRAP_MODE);
    gviewer_set_fixed_limit(w, DEFAULT_FIXED_LIMIT);
    gviewer_set_encoding(w, DEFAULT_ENCODING);

    w->priv->tscrollbox = scroll_box_new();
    text_render_set_v_adjustment(w->priv->textr, scroll_box_get_v_adjustment(SCROLL_BOX(w->priv->tscrollbox)));
    text_render_set_h_adjustment(w->priv->textr, scroll_box_get_h_adjustment(SCROLL_BOX(w->priv->tscrollbox)));
    text_render_attach_external_v_range(w->priv->textr, scroll_box_get_v_range(SCROLL_BOX(w->priv->tscrollbox)));
    scroll_box_set_client (SCROLL_BOX(w->priv->tscrollbox), GTK_WIDGET (w->priv->textr));
    gtk_widget_show (GTK_WIDGET (w->priv->textr));
    gtk_widget_show (w->priv->tscrollbox);
    g_object_ref (w->priv->tscrollbox);

    w->priv->imgr = reinterpret_cast<ImageRender*> (image_render_new());
    gviewer_set_best_fit(w, DEFAULT_BEST_FIT);
    gviewer_set_scale_factor(w, DEFAULT_SCALE_FACTOR);
    w->priv->iscrollbox = scroll_box_new();
    image_render_set_v_adjustment (w->priv->imgr, scroll_box_get_v_adjustment (SCROLL_BOX (w->priv->iscrollbox)));
    image_render_set_h_adjustment (w->priv->imgr, scroll_box_get_h_adjustment (SCROLL_BOX (w->priv->iscrollbox)));
    image_render_set_best_fit (w->priv->imgr, TRUE);
    image_render_set_scale_factor (w->priv->imgr, 1);
    scroll_box_set_client (SCROLL_BOX(w->priv->iscrollbox), GTK_WIDGET (w->priv->imgr));
    gtk_widget_show (GTK_WIDGET (w->priv->imgr));
    gtk_widget_show (w->priv->iscrollbox);
    g_object_ref (w->priv->iscrollbox);

    w->priv->last_client = w->priv->tscrollbox;
    gtk_table_attach (GTK_TABLE (w), GTK_WIDGET (w->priv->tscrollbox), 0, 1, 0, 1,
                      (GtkAttachOptions)(GTK_FILL|GTK_EXPAND),
                      (GtkAttachOptions)(GTK_FILL|GTK_EXPAND), 0, 0);

    g_signal_connect (w, "destroy-event", G_CALLBACK (gviewer_destroy), w);

    g_signal_connect (w->priv->textr, "text-status-changed", G_CALLBACK (gviewer_text_status_update), w);
    g_signal_connect (w->priv->imgr, "image-status-changed", G_CALLBACK (gviewer_image_status_update), w);

    g_signal_connect (w->priv->textr, "button-press-event", G_CALLBACK (on_text_viewer_button_pressed), w);
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

    gtk_signal_emit (GTK_OBJECT (viewer), gviewer_signals[STATUS_LINE_CHANGED], temp);
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

    gtk_signal_emit (GTK_OBJECT (viewer), gviewer_signals[STATUS_LINE_CHANGED], temp);
}



static gboolean on_text_viewer_button_pressed (GtkWidget *treeview, GdkEventButton *event, GViewer *viewer)
{
    if (event->type==GDK_BUTTON_PRESS && event->button==3)
    {
        GtkWidget *menu = gtk_menu_new ();
        GtkWidget *menuitem;

        menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Copy selection"));
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));
        g_signal_connect (menuitem, "activate", G_CALLBACK (gviewer_copy_selection), viewer);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

        gtk_widget_show_all (menu);
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);

        return TRUE;
    }

    return FALSE;
}


static void gviewer_destroy (GtkObject *widget)
{
    g_return_if_fail (IS_GVIEWER (widget));

    GViewer *w = GVIEWER (widget);

    if (w->priv)
    {
        g_object_unref (w->priv->iscrollbox);
        g_object_unref (w->priv->tscrollbox);

        g_free (w->priv);
        w->priv = NULL;
    }

    if (GTK_OBJECT_CLASS(parent_class)->destroy)
    {
        (*GTK_OBJECT_CLASS(parent_class)->destroy) (widget);
    }
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
    g_return_if_fail (obj != nullptr);

    const unsigned DETECTION_BUF_LEN = 100;

    obj->priv->dispmode = DISP_MODE_TEXT_FIXED;

    if (!obj->priv->textr)
        return;

    ViewerFileOps *fops = text_render_get_file_ops(obj->priv->textr);

    if (!fops)
        return;

    int count = MIN(DETECTION_BUF_LEN, gv_file_get_max_offset(fops));

    obj->priv->dispmode = guess_display_mode(fops->filename, count);

}


void gviewer_set_display_mode(GViewer *obj, VIEWERDISPLAYMODE mode)
{
    g_return_if_fail (IS_GVIEWER (obj));

    if (mode==DISP_MODE_IMAGE && !obj->priv->img_initialized)
    {
        // do lazy-initialization of the image render, only when the user first asks to display the file as image

        obj->priv->img_initialized = TRUE;
        image_render_load_file(obj->priv->imgr, obj->priv->filename);
    }

    GtkWidget *client = NULL;

    obj->priv->dispmode = mode;
    switch (mode)
    {
        case DISP_MODE_TEXT_FIXED:
            client = obj->priv->tscrollbox;
            text_render_set_display_mode (obj->priv->textr, TextRender::DISPLAYMODE_TEXT);
            break;

        case DISP_MODE_BINARY:
            client = obj->priv->tscrollbox;
            text_render_set_display_mode (obj->priv->textr, TextRender::DISPLAYMODE_BINARY);
            break;

        case DISP_MODE_HEXDUMP:
            client = obj->priv->tscrollbox;
            text_render_set_display_mode (obj->priv->textr, TextRender::DISPLAYMODE_HEXDUMP);
            break;

        case DISP_MODE_IMAGE:
            client = obj->priv->iscrollbox;
            break;

        default:
            break;
    }

    if (client != obj->priv->last_client)
    {
        if (obj->priv->last_client)
            gtk_container_remove (GTK_CONTAINER(obj), obj->priv->last_client);

        gtk_widget_grab_focus (GTK_WIDGET (client));
        gtk_table_attach (GTK_TABLE (obj), client , 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

        switch (mode)
        {
            case DISP_MODE_TEXT_FIXED:
            case DISP_MODE_BINARY:
            case DISP_MODE_HEXDUMP:
                text_render_notify_status_changed(obj->priv->textr);
                break;

            case DISP_MODE_IMAGE:
                image_render_notify_status_changed(obj->priv->imgr);
                break;

            default:
                break;
        }

        gtk_widget_show (client);
        obj->priv->last_client = client;
    }
}


VIEWERDISPLAYMODE gviewer_get_display_mode(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), DISP_MODE_TEXT_FIXED);

    return obj->priv->dispmode;
}


void gviewer_load_file(GViewer *obj, const gchar*filename)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (filename);

    g_free (obj->priv->filename);

    obj->priv->filename = g_strdup (filename);

    text_render_load_file(obj->priv->textr, obj->priv->filename);

    gviewer_auto_detect_display_mode(obj);

    gviewer_set_display_mode(obj, obj->priv->dispmode);
}


void gviewer_set_tab_size(GViewer *obj, int tab_size)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->textr);

    text_render_set_tab_size(obj->priv->textr, tab_size);
}


int gviewer_get_tab_size(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    g_return_val_if_fail (obj->priv->textr, 0);

    return text_render_get_tab_size(obj->priv->textr);
}


void gviewer_set_wrap_mode(GViewer *obj, gboolean ACTIVE)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->textr);

    text_render_set_wrap_mode(obj->priv->textr, ACTIVE);
}


gboolean gviewer_get_wrap_mode(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), FALSE);
    g_return_val_if_fail (obj->priv->textr, FALSE);

    return text_render_get_wrap_mode(obj->priv->textr);
}


void gviewer_set_fixed_limit(GViewer *obj, int fixed_limit)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->textr);

    text_render_set_fixed_limit(obj->priv->textr, fixed_limit);
}


int gviewer_get_fixed_limit(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    g_return_val_if_fail (obj->priv->textr, 0);

    return text_render_get_fixed_limit(obj->priv->textr);
}


void gviewer_set_encoding(GViewer *obj, const char *encoding)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->textr);

    text_render_set_encoding(obj->priv->textr, encoding);
}


const gchar *gviewer_get_encoding(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), NULL);
    g_return_val_if_fail (obj->priv->textr, NULL);

    return text_render_get_encoding(obj->priv->textr);
}


void gviewer_set_font_size(GViewer *obj, int font_size)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->textr);

    text_render_set_font_size(obj->priv->textr, font_size);
}


int gviewer_get_font_size(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    g_return_val_if_fail (obj->priv->textr, 0);

    return text_render_get_font_size(obj->priv->textr);
}


void gviewer_set_hex_offset_display(GViewer *obj, gboolean HEX_OFFSET)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->textr);

    text_render_set_hex_offset_display(obj->priv->textr, HEX_OFFSET);
}


gboolean gviewer_get_hex_offset_display(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), FALSE);
    g_return_val_if_fail (obj->priv->textr, FALSE);

    return text_render_get_hex_offset_display(obj->priv->textr);
}


void gviewer_set_best_fit(GViewer *obj, gboolean active)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->imgr);

    image_render_set_best_fit(obj->priv->imgr, active);
}


gboolean gviewer_get_best_fit(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), FALSE);
    g_return_val_if_fail (obj->priv->textr, FALSE);

    return image_render_get_best_fit(obj->priv->imgr);
}


void gviewer_set_scale_factor(GViewer *obj, double scalefactor)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->imgr);

    image_render_set_scale_factor(obj->priv->imgr, scalefactor);
}


double gviewer_get_scale_factor(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    g_return_val_if_fail (obj->priv->imgr, 0);

    return image_render_get_scale_factor(obj->priv->imgr);
}


TextRender *gviewer_get_text_render(GViewer *obj)
{
    g_return_val_if_fail (IS_GVIEWER (obj), 0);
    g_return_val_if_fail (obj->priv->textr, 0);

    return obj->priv->textr;
}


void gviewer_image_operation(GViewer *obj, ImageRender::DISPLAYMODE op)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->imgr);

    image_render_operation(obj->priv->imgr, op);
}


void gviewer_copy_selection(GtkMenuItem *item, GViewer *obj)
{
    g_return_if_fail (IS_GVIEWER (obj));
    g_return_if_fail (obj->priv->textr);

    if (obj->priv->dispmode!=DISP_MODE_IMAGE)
        text_render_copy_selection(obj->priv->textr);
}
