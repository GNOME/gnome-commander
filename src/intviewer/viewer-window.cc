/**
 * @file viewer-window.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2020 Uwe Scholz\n
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

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gtk/gtktable.h>

#include "libgviewer.h"
#include "search-dlg.h"

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-treeview.h"
#include "utils.h"
#include "tags/gnome-cmd-tags.h"
#include "gnome-cmd-data.h"

using namespace std;


#define G_OBJ_CHARSET_KEY        "charset"
#define G_OBJ_DISPMODE_KEY       "dispmode"
#define G_OBJ_BYTES_PER_LINE_KEY "bytesperline"
#define G_OBJ_IMAGE_OP_KEY       "imageop"
#define G_OBJ_EXTERNAL_TOOL_KEY  "exttool"

#define GCMD_INTERNAL_VIEWER               "org.gnome.gnome-commander.preferences.internal-viewer"
#define GCMD_GSETTINGS_IV_CHARSET          "charset"

#define NUMBER_OF_CHARSETS       22

/***********************************
 * Functions for using GSettings
 ***********************************/

struct _InternalViewerSettings
{
    GObject parent;
    GSettings *internalviewer;
};

G_DEFINE_TYPE (InternalViewerSettings, iv_settings, G_TYPE_OBJECT)

static void iv_settings_finalize (GObject *object)
{
    G_OBJECT_CLASS (iv_settings_parent_class)->finalize (object);
}

static void iv_settings_dispose (GObject *object)
{
    InternalViewerSettings *gs = GCMD_IV_SETTINGS (object);

    g_clear_object (&gs->internalviewer);

    G_OBJECT_CLASS (iv_settings_parent_class)->dispose (object);
}

static void iv_settings_class_init (InternalViewerSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = iv_settings_finalize;
    object_class->dispose = iv_settings_dispose;
}

InternalViewerSettings *iv_settings_new ()
{
    return (InternalViewerSettings *) g_object_new (INTERNAL_VIEWER_SETTINGS, nullptr);
}

static void iv_settings_init (InternalViewerSettings *gs)
{
    gs->internalviewer = g_settings_new (GCMD_INTERNAL_VIEWER);
}

/***********************************/

static GtkWindowClass *parent_class = nullptr;

static double image_scale_factors[] = {0.1, 0.2, 0.33, 0.5, 0.67, 1, 1.25, 1.50, 2, 3, 4, 5, 6, 7, 8};

const static int MAX_SCALE_FACTOR_INDEX = G_N_ELEMENTS(image_scale_factors);

struct GViewerWindowPrivate
{
    // Gtk User Interface
    GtkWidget *vbox;
    GViewer *viewer;
    GtkWidget *menubar;
    GtkWidget *statusbar;

    GtkAccelGroup *accel_group;
    GtkWidget *encoding_menu_item[NUMBER_OF_CHARSETS];
    GtkWidget *wrap_mode_menu_item;
    GtkWidget *hex_offset_menu_item;
    GtkWidget *show_exif_menu_item;
    GtkWidget *fixed_limit_menu_items[3];
    GViewerWindowSettings state;

    GtkWidget *metadata_view;
    gboolean metadata_visible;

    int current_scale_index;

    GnomeCmdFile *f;
    gchar *filename;
    guint statusbar_ctx_id;
    gboolean status_bar_msg;

    GViewerSearcher *srchr;
    gchar *search_pattern;
    gint  search_pattern_len;
};

static void gviewer_window_init(GViewerWindow *w);
static void gviewer_window_class_init (GViewerWindowClass *klass);
static void gviewer_window_destroy(GtkObject *widget);

static void gviewer_window_status_line_changed(GViewer *obj, const gchar *status_line, GViewerWindow *wnd);

static gboolean gviewer_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

static GtkWidget *gviewer_window_create_menus(GViewerWindow *obj);

inline void gviewer_window_show_metadata(GViewerWindow *obj);
inline void gviewer_window_hide_metadata(GViewerWindow *obj);

// Event Handlers
static void menu_file_close (GtkMenuItem *item, GViewerWindow *obj);

static void menu_view_exif_information(GtkMenuItem *item, GViewerWindow *obj);

static void menu_edit_copy(GtkMenuItem *item, GViewerWindow *obj);
static void menu_edit_find(GtkMenuItem *item, GViewerWindow *obj);
static void menu_edit_find_next(GtkMenuItem *item, GViewerWindow *obj);
static void menu_edit_find_prev(GtkMenuItem *item, GViewerWindow *obj);

static void menu_view_wrap(GtkMenuItem *item, GViewerWindow *obj);
static void menu_view_display_mode(GtkMenuItem *item, GViewerWindow *obj);
static void menu_view_set_charset(GtkMenuItem *item, GViewerWindow *obj);
static void menu_view_zoom_in(GtkMenuItem *item, GViewerWindow *obj);
static void menu_view_zoom_out(GtkMenuItem *item, GViewerWindow *obj);
static void menu_view_zoom_normal(GtkMenuItem *item, GViewerWindow *obj);
static void menu_view_zoom_best_fit(GtkMenuItem *item, GViewerWindow *obj);

static void menu_image_operation(GtkMenuItem *item, GViewerWindow *obj);

static void menu_settings_binary_bytes_per_line(GtkMenuItem *item, GViewerWindow *obj);
static void menu_settings_hex_decimal_offset(GtkMenuItem *item, GViewerWindow *obj);
static void menu_settings_save_settings(GtkMenuItem *item, GViewerWindow *obj);

static void menu_help_quick_help(GtkMenuItem *item, GViewerWindow *obj);
static void menu_help_keyboard(GtkMenuItem *item, GViewerWindow *obj);

inline GtkTreeModel *create_model ();
inline void fill_model (GtkTreeStore *treestore, GnomeCmdFile *f);
GtkWidget *create_view ();

/*****************************************
    public functions
    (defined in the header file)
*****************************************/

GtkWidget *gviewer_window_file_view (GnomeCmdFile *f, GViewerWindowSettings *initial_settings)
{
    GViewerWindowSettings set;

    if (!initial_settings)
    {
        gviewer_window_load_settings(&set);
        initial_settings = &set;
    }

    GtkWidget *w = gviewer_window_new ();

    gviewer_window_load_file (GVIEWER_WINDOW (w), f);

    if (initial_settings)
        gviewer_window_set_settings(GVIEWER_WINDOW (w), initial_settings);

    return w;
}


void gviewer_window_load_file (GViewerWindow *obj, GnomeCmdFile *f)
{
    g_return_if_fail (obj != nullptr);
    g_return_if_fail (f != nullptr);

    g_free (obj->priv->filename);

    obj->priv->f = f;
    obj->priv->filename = f->get_real_path();
    gviewer_load_file (obj->priv->viewer, obj->priv->filename);

    gtk_window_set_title (GTK_WINDOW (obj), obj->priv->filename);
}


GtkType gviewer_window_get_type ()
{
    static GtkType type = 0;
    if (type == 0)
    {
        GTypeInfo info =
        {
            sizeof (GViewerWindowClass),
            nullptr,
            nullptr,
            (GClassInitFunc) gviewer_window_class_init,
            nullptr,
            nullptr,
            sizeof(GViewerWindow),
            0,
            (GInstanceInitFunc) gviewer_window_init
        };
        type = g_type_register_static (GTK_TYPE_WINDOW, "gviewerwindow", &info, (GTypeFlags) 0);
    }
    return type;
}


static void gviewer_window_map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != nullptr)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void gviewer_window_class_init (GViewerWindowClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkWindowClass *) gtk_type_class (gtk_window_get_type ());

    object_class->destroy = gviewer_window_destroy;
    widget_class->map = gviewer_window_map;
}


static void gviewer_window_init (GViewerWindow *w)
{
    w->priv = g_new0 (GViewerWindowPrivate, 1);

    // w->priv->status_bar_msg = FALSE;
    // w->priv->filename = nullptr;
    // w->priv->metadata_view = nullptr;
    // w->priv->metadata_visible = FALSE;
    w->priv->current_scale_index = 5;

    GtkWindow *win = GTK_WINDOW (w);
    gtk_window_set_title (win, "GViewer");

    g_signal_connect (w, "key-press-event", G_CALLBACK (gviewer_window_key_pressed), nullptr);

    w->priv->vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (w->priv->vbox);

    w->priv->menubar = gviewer_window_create_menus(w);
    gtk_widget_show (w->priv->menubar);
    gtk_box_pack_start (GTK_BOX (w->priv->vbox), w->priv->menubar, FALSE, FALSE, 0);

    w->priv->viewer = reinterpret_cast<GViewer*> (gviewer_new());
    g_object_ref (w->priv->viewer);
    gtk_widget_show (GTK_WIDGET (w->priv->viewer));
    gtk_box_pack_start (GTK_BOX (w->priv->vbox), GTK_WIDGET (w->priv->viewer), TRUE, TRUE, 0);

    g_signal_connect (w->priv->viewer, "status-line-changed", G_CALLBACK (gviewer_window_status_line_changed), w);

    w->priv->statusbar = gtk_statusbar_new ();
    gtk_widget_show (w->priv->statusbar);
    gtk_box_pack_start (GTK_BOX (w->priv->vbox), w->priv->statusbar, FALSE, FALSE, 0);

    w->priv->statusbar_ctx_id  = gtk_statusbar_get_context_id (GTK_STATUSBAR (w->priv->statusbar), "info");

    gtk_widget_grab_focus (GTK_WIDGET (w->priv->viewer));

    gtk_container_add (GTK_CONTAINER (w), w->priv->vbox);
}


static void gviewer_window_status_line_changed(GViewer *obj, const gchar *status_line, GViewerWindow *wnd)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (wnd));

    GViewerWindow *w = GVIEWER_WINDOW (wnd);

    if (w->priv->status_bar_msg)
        gtk_statusbar_pop (GTK_STATUSBAR (w->priv->statusbar), w->priv->statusbar_ctx_id);

    if (status_line)
        gtk_statusbar_push (GTK_STATUSBAR (w->priv->statusbar), w->priv->statusbar_ctx_id, status_line);

    w->priv->status_bar_msg = status_line != nullptr;
}


void gviewer_window_set_settings(GViewerWindow *obj, /*in*/ GViewerWindowSettings *settings)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (obj));
    g_return_if_fail (settings != nullptr);
    g_return_if_fail (obj->priv->viewer != nullptr);

    gviewer_set_font_size(obj->priv->viewer, settings->font_size);
    gviewer_set_tab_size(obj->priv->viewer, settings->tab_size);

    gviewer_set_fixed_limit(obj->priv->viewer, settings->binary_bytes_per_line);
    switch (settings->binary_bytes_per_line)
    {
        case 20:
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->fixed_limit_menu_items[0]), TRUE);
            break;
        case 40:
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->fixed_limit_menu_items[1]), TRUE);
            break;
        case 80:
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->fixed_limit_menu_items[2]), TRUE);
            break;
        default:
            break;
    }

    gviewer_set_wrap_mode(obj->priv->viewer, settings->wrap_mode);
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM (obj->priv->wrap_mode_menu_item),
            settings->wrap_mode);

    gviewer_set_hex_offset_display(obj->priv->viewer, settings->hex_decimal_offset);
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM (obj->priv->hex_offset_menu_item),
            settings->hex_decimal_offset);

    gviewer_set_encoding(obj->priv->viewer, settings->charset);
    if (strcmp(settings->charset, (gchar*) "UTF8") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[0]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ASCII") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[1]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP437") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[2]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-6") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[3]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ARABIC") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[4]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP864") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[5]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-4") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[6]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-2") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[7]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP1250") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[8]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-5") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[9]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP1251") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[10]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-7") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[11]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP1253") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[12]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "HEBREW") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[13]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP862") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[14]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-8") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[15]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-15") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[16]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-3") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[17]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-9") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[18]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP1254") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[19]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "CP1252") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[20]), TRUE);
    }
    else if (strcmp(settings->charset, (gchar*) "ISO-8859-1") == 0)
    {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj->priv->encoding_menu_item[21]), TRUE);
    }

    gtk_window_resize(GTK_WINDOW (obj),
        settings->rect.width, settings->rect.height);

#if 0
    // This doesn't work because the window is not shown yet
    if (GTK_WIDGET (obj)->window)
        gdk_window_move (GTK_WIDGET (obj)->window, settings->rect.x, settings->rect.y);
#endif
    gtk_window_set_position (GTK_WINDOW (obj), GTK_WIN_POS_CENTER);
}


void gviewer_window_get_current_settings(GViewerWindow *obj, /* out */ GViewerWindowSettings *settings)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (obj));
    g_return_if_fail (settings != nullptr);
    g_return_if_fail (obj->priv->viewer != nullptr);

    memset(settings, 0, sizeof(GViewerWindowSettings));

    if (GTK_WIDGET (obj)->window)
    {
        settings->rect.width = GTK_WIDGET (obj)->allocation.width;
        settings->rect.height = GTK_WIDGET (obj)->allocation.height;
        gdk_window_get_position(GTK_WIDGET (obj)->window, &settings->rect.x, &settings->rect.y);
    }
    else
    {
        settings->rect.x = 0;
        settings->rect.y = 0;
        settings->rect.width = 100;
        settings->rect.height = 100;
    }
    settings->font_size = gviewer_get_font_size(obj->priv->viewer);
    settings->wrap_mode = gviewer_get_wrap_mode(obj->priv->viewer);
    settings->binary_bytes_per_line = gviewer_get_fixed_limit(obj->priv->viewer);
    strncpy(settings->charset, gviewer_get_encoding(obj->priv->viewer), sizeof(settings->charset) - 1);
    settings->hex_decimal_offset = gviewer_get_hex_offset_display(obj->priv->viewer);
    settings->tab_size = gviewer_get_tab_size(obj->priv->viewer);
}


static void gviewer_window_destroy (GtkObject *widget)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (widget));

    GViewerWindow *w = GVIEWER_WINDOW (widget);

    if (w->priv)
    {
        g_object_unref (w->priv->viewer);

        g_free (w->priv->filename);
        w->priv->filename = nullptr;

        g_free (w->priv);
        w->priv = nullptr;
    }

    if (GTK_OBJECT_CLASS(parent_class)->destroy)
        (*GTK_OBJECT_CLASS(parent_class)->destroy)(widget);
}


static gboolean gviewer_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    g_return_val_if_fail (IS_GVIEWER_WINDOW (widget), FALSE);

    GViewerWindow *w = GVIEWER_WINDOW (widget);

    switch (event->keyval)
    {
        case GDK_plus:
        case GDK_KP_Add:
        case GDK_equal:
           menu_view_zoom_in(nullptr, w);
           return TRUE;

        case GDK_minus:
        case GDK_KP_Subtract:
           menu_view_zoom_out(nullptr, w);
           return TRUE;

        default:
           break;
    }

    if (state_is_ctrl(event->state))
        switch (event->keyval)
        {
            case GDK_t:
            case GDK_T:
                if (w->priv->metadata_visible)
                    gviewer_window_hide_metadata(w);
                else
                    gviewer_window_show_metadata(w);
                return TRUE;

            case GDK_w:
            case GDK_W:
                gtk_widget_destroy (GTK_WIDGET (w));
                return TRUE;

            default:
                break;
        }

    if (state_is_shift(event->state))
        switch (event->keyval)
        {
            case GDK_F7:
               menu_edit_find_next(nullptr, w);
               return TRUE;
            default:
                break;
        }

    if (state_is_alt(event->state))
        switch (event->keyval)
        {
            case GDK_Return:
            case GDK_KP_Enter:
                gviewer_window_show_metadata(w);
                return TRUE;
            default:
                break;
        }

    switch (state_is_blank(event->keyval))
    {
        case GDK_t:
        case GDK_T:
            if (w->priv->metadata_visible)
                gviewer_window_hide_metadata(w);
            else
                gviewer_window_show_metadata(w);
            return TRUE;

        case GDK_F7:
           menu_edit_find(nullptr, w);
           return TRUE;

        default:
           break;
    }

    return FALSE;
}


static GtkWidget *create_menu_seperator(GtkWidget *container)
{
    GtkWidget *separatormenuitem1 = gtk_separator_menu_item_new ();

    gtk_widget_show (separatormenuitem1);
    gtk_container_add (GTK_CONTAINER (container), separatormenuitem1);
    gtk_widget_set_sensitive (separatormenuitem1, FALSE);

    return separatormenuitem1;
}


enum MENUITEMTYPE
{
    MI_NONE,
    MI_NORMAL,
    MI_CHECK,
    MI_RADIO,
    MI_SEPERATOR,
    MI_SUBMENU
} ;


struct MENU_ITEM_DATA
{
    MENUITEMTYPE menutype;
    const gchar *label;

    guint keyval;
    guint modifier;

    GCallback callback;

    GnomeUIPixmapType pixmap_type;
    gconstpointer pixmap_info;

    const gchar *gobj_key;
    gpointer *gobj_val;

    GtkWidget **menu_item_widget;

    GSList **radio_list;
};

static GtkWidget *create_menu_item (MENUITEMTYPE type,
                                    const gchar *name,
                                    GtkWidget *container,
                                    GtkAccelGroup *accel,
                                    guint keyval,
                                    guint modifier,
                                    GCallback callback,
                                    GnomeUIPixmapType pixmap_type,
                                    gconstpointer pixmap_info,
                                    gpointer userdata)
{
    GtkWidget *menuitem;

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (type)
    {
        case MI_CHECK:
            menuitem = gtk_check_menu_item_new_with_mnemonic (_(name));
            break;

        case MI_NORMAL:
        default:
            menuitem = gtk_image_menu_item_new_with_mnemonic (_(name));
            break;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    if (pixmap_type != GNOME_APP_PIXMAP_NONE && pixmap_info != nullptr)
    {
        GtkWidget *pixmap = create_ui_pixmap (nullptr, pixmap_type, pixmap_info, GTK_ICON_SIZE_MENU);
        if (pixmap)
        {
            gtk_widget_show (pixmap);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), pixmap);
        }

    }

    gtk_widget_show (menuitem);
    gtk_container_add (GTK_CONTAINER (container), menuitem);

    if (accel && keyval)
        gtk_widget_add_accelerator (menuitem, "activate", accel, keyval, (GdkModifierType) modifier, GTK_ACCEL_VISIBLE);

    g_signal_connect (menuitem, "activate", callback, userdata);

    return menuitem;
}

static GtkWidget *create_radio_menu_item (GSList **group,
                                          const gchar *name,
                                          GtkWidget *container,
                                          GtkAccelGroup *accel,
                                          guint keyval,
                                          guint modifier,
                                          GCallback callback,
                                          gpointer userdata)
{
    GtkWidget *menuitem = gtk_radio_menu_item_new_with_mnemonic (*group, _(name));

    *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));

    if (accel && keyval)
        gtk_widget_add_accelerator (menuitem, "activate", accel, keyval, (GdkModifierType) modifier, GTK_ACCEL_VISIBLE);

    g_signal_connect (menuitem, "activate", callback, userdata);

    gtk_widget_show (menuitem);
    gtk_container_add (GTK_CONTAINER (container), menuitem);

    return menuitem;
}

inline GtkWidget *create_sub_menu(const gchar *name, GtkWidget *container)
{
    GtkWidget *menuitem4 = gtk_menu_item_new_with_mnemonic (_(name));
    gtk_widget_show (menuitem4);
    gtk_container_add (GTK_CONTAINER (container), menuitem4);

    GtkWidget *menu4 = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem4), menu4);

    return menu4;
}

static void create_menu_items (GtkWidget *container, GtkAccelGroup *accel, gpointer user_data, MENU_ITEM_DATA *menudata)
{
    g_return_if_fail (menudata != nullptr);
    g_return_if_fail (container != nullptr);

    while (menudata && menudata->menutype!=MI_NONE)
    {
        GtkWidget *item = nullptr;

        switch (menudata->menutype)
        {
            case MI_NONE:
                break;

            case MI_SUBMENU:
                item = create_sub_menu(menudata->label, container);
                break;

            case MI_SEPERATOR:
                item = create_menu_seperator(container);
                break;

            case MI_NORMAL:
            case MI_CHECK:
                item = create_menu_item(menudata->menutype,
                                        menudata->label, container,
                                        menudata->keyval ? accel : nullptr,
                                        menudata->keyval,
                                        menudata->modifier,
                                        menudata->callback,
                                        menudata->pixmap_type, menudata->pixmap_info, user_data);
                break;

            case MI_RADIO:
                if (!menudata->radio_list)
                    g_warning ("radio_list field is nullptr in \"%s\" menu item", menudata->label);
                else
                {
                    item = create_radio_menu_item(menudata->radio_list,
                                                  menudata->label, container,
                                                  menudata->keyval ? accel : nullptr,
                                                  menudata->keyval,
                                                  menudata->modifier,
                                                  menudata->callback, user_data);
                }
                break;

            default:
                break;
        }

        if (menudata->gobj_key)
            g_object_set_data (G_OBJECT (item), menudata->gobj_key, menudata->gobj_val);

        if (menudata->menu_item_widget)
            *menudata->menu_item_widget = item;

        menudata++;
    }
}

#define NO_KEYVAL       0
#define NO_MODIFIER     0
#define NO_GOBJ_KEY     nullptr
#define NO_GOBJ_VAL     nullptr
#define NO_MENU_ITEM    nullptr
#define NO_GSLIST       nullptr
#define NO_PIXMAP_INFO  0

 static GtkWidget *gviewer_window_create_menus(GViewerWindow *obj)
{
    GtkWidget *int_viewer_menu;
    GtkWidget *submenu;

    MENU_ITEM_DATA file_menu_items[] = {
        {MI_NORMAL, _("_Close"), GDK_Escape, NO_MODIFIER, G_CALLBACK (menu_file_close),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_CLOSE,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NONE}
    };

    GSList *view_mode_list = nullptr;

    MENU_ITEM_DATA view_menu_items[] = {
        {MI_RADIO, _("_Text"), GDK_1, NO_MODIFIER, G_CALLBACK (menu_view_display_mode),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                G_OBJ_DISPMODE_KEY, (gpointer *) GUINT_TO_POINTER (DISP_MODE_TEXT_FIXED),
                NO_MENU_ITEM, &view_mode_list},
        {MI_RADIO, _("_Binary"), GDK_2, NO_MODIFIER, G_CALLBACK (menu_view_display_mode),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                G_OBJ_DISPMODE_KEY, (gpointer *) GUINT_TO_POINTER (DISP_MODE_BINARY),
                NO_MENU_ITEM, &view_mode_list},
        {MI_RADIO, _("_Hexadecimal"), GDK_3, NO_MODIFIER, G_CALLBACK (menu_view_display_mode),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                G_OBJ_DISPMODE_KEY, (gpointer *) GUINT_TO_POINTER (DISP_MODE_HEXDUMP),
                NO_MENU_ITEM, &view_mode_list},
        {MI_RADIO, _("_Image"), GDK_4, NO_MODIFIER, G_CALLBACK (menu_view_display_mode),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                G_OBJ_DISPMODE_KEY, (gpointer *) GUINT_TO_POINTER (DISP_MODE_IMAGE),
                NO_MENU_ITEM, &view_mode_list},
        {MI_SEPERATOR},
        {MI_NORMAL, _("_Zoom In"), GDK_plus, GDK_CONTROL_MASK, G_CALLBACK (menu_view_zoom_in),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_IN,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Zoom _Out"), GDK_minus, GDK_CONTROL_MASK, G_CALLBACK (menu_view_zoom_out),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_OUT,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("_Normal Size"), GDK_0, GDK_CONTROL_MASK, G_CALLBACK (menu_view_zoom_normal),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_100,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Best _Fit"), NO_KEYVAL, NO_MODIFIER, G_CALLBACK (menu_view_zoom_best_fit),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ZOOM_FIT,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NONE}
    };


    GtkWidget *encoding_submenu = nullptr;
    MENU_ITEM_DATA text_menu_items[] = {
        {MI_NORMAL, _("_Copy Text Selection"), GDK_C, GDK_CONTROL_MASK, G_CALLBACK (menu_edit_copy),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_COPY,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Find…"), GDK_F, GDK_CONTROL_MASK, G_CALLBACK (menu_edit_find),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_FIND,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Find Next"), GDK_F3, NO_MODIFIER, G_CALLBACK (menu_edit_find_next),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Find Previous"), GDK_F3, GDK_SHIFT_MASK, G_CALLBACK (menu_edit_find_prev),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_SEPERATOR},
        {MI_CHECK, _("_Wrap lines"), GDK_W, NO_MODIFIER, G_CALLBACK (menu_view_wrap),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                &obj->priv->wrap_mode_menu_item, NO_GSLIST},
        {MI_SEPERATOR},
        {MI_SUBMENU, _("_Encoding"), NO_KEYVAL, NO_MODIFIER, G_CALLBACK (nullptr),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                &encoding_submenu, NO_GSLIST},
        {MI_NONE}
    };

#define ENCODING_MENU_ITEM(label, keyval, value, item_number) {MI_RADIO, _(label), \
            keyval, NO_MODIFIER, G_CALLBACK (menu_view_set_charset), \
            GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO, \
            G_OBJ_CHARSET_KEY, (gpointer *) GUINT_TO_POINTER(value), \
            &obj->priv->encoding_menu_item[item_number], &text_encoding_list}

    GSList *text_encoding_list = nullptr;
    MENU_ITEM_DATA encoding_menu_items[] = {
        ENCODING_MENU_ITEM("_UTF-8", GDK_u, "UTF8", 0),
        ENCODING_MENU_ITEM("English (US-_ASCII)", GDK_a, "ASCII", 1),
        ENCODING_MENU_ITEM("Terminal (CP437)", GDK_q, "CP437", 2),
        ENCODING_MENU_ITEM("Arabic (ISO-8859-6)", NO_KEYVAL, "ISO-8859-6", 3),
        ENCODING_MENU_ITEM("Arabic (Windows, CP1256)", NO_KEYVAL, "ARABIC", 4),
        ENCODING_MENU_ITEM("Arabic (Dos, CP864)", NO_KEYVAL, "CP864",5 ),
        ENCODING_MENU_ITEM("Baltic (ISO-8859-4)", NO_KEYVAL, "ISO-8859-4",6 ),
        ENCODING_MENU_ITEM("Central European (ISO-8859-2)", NO_KEYVAL, "ISO-8859-2", 7),
        ENCODING_MENU_ITEM("Central European (CP1250)", NO_KEYVAL, "CP1250", 8),
        ENCODING_MENU_ITEM("Cyrillic (ISO-8859-5)", NO_KEYVAL, "ISO-8859-5", 9),
        ENCODING_MENU_ITEM("Cyrillic (CP1251)", NO_KEYVAL, "CP1251", 10),
        ENCODING_MENU_ITEM("Greek (ISO-8859-7)", NO_KEYVAL, "ISO-8859-7", 11),
        ENCODING_MENU_ITEM("Greek (CP1253)", NO_KEYVAL, "CP1253", 12),
        ENCODING_MENU_ITEM("Hebrew (Windows, CP1255)", NO_KEYVAL, "HEBREW", 13),
        ENCODING_MENU_ITEM("Hebrew (Dos, CP862)", NO_KEYVAL, "CP862", 14),
        ENCODING_MENU_ITEM("Hebrew (ISO-8859-8)", NO_KEYVAL, "ISO-8859-8", 15),
        ENCODING_MENU_ITEM("Latin 9 (ISO-8859-15))", NO_KEYVAL, "ISO-8859-15", 16),
        ENCODING_MENU_ITEM("Maltese (ISO-8859-3)", NO_KEYVAL, "ISO-8859-3", 17),
        ENCODING_MENU_ITEM("Turkish (ISO-8859-9)", NO_KEYVAL, "ISO-8859-9", 18),
        ENCODING_MENU_ITEM("Turkish (CP1254)", NO_KEYVAL, "CP1254", 19),
        ENCODING_MENU_ITEM("Western (CP1252)", NO_KEYVAL, "CP1252", 20),
        ENCODING_MENU_ITEM("Western (ISO-8859-1)", NO_KEYVAL, "ISO-8859-1", 21),
        {MI_NONE}
    };

    MENU_ITEM_DATA image_menu_items[] = {
        {MI_CHECK, _("Show Metadata _Tags"), GDK_t, NO_MODIFIER,
                G_CALLBACK (menu_view_exif_information),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_SEPERATOR},
        {MI_NORMAL, _("_Rotate Clockwise"), GDK_R, GDK_CONTROL_MASK,
                G_CALLBACK (menu_image_operation),
                GNOME_APP_PIXMAP_FILENAME, "gnome-commander/rotate-90-16.xpm",
                G_OBJ_IMAGE_OP_KEY, (gpointer *) GUINT_TO_POINTER (ImageRender::ROTATE_COUNTERCLOCKWISE),
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Rotate Counter Clockwis_e"), NO_KEYVAL, NO_MODIFIER,
                G_CALLBACK (menu_image_operation),
                GNOME_APP_PIXMAP_FILENAME, "gnome-commander/rotate-270-16.xpm",
                G_OBJ_IMAGE_OP_KEY, (gpointer *) GUINT_TO_POINTER (ImageRender::ROTATE_COUNTERCLOCKWISE),
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("_Rotate 180\xC2\xB0"), GDK_r, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                G_CALLBACK (menu_image_operation),
                GNOME_APP_PIXMAP_FILENAME, "gnome-commander/rotate-180-16.xpm",
                G_OBJ_IMAGE_OP_KEY, (gpointer *) GUINT_TO_POINTER (ImageRender::ROTATE_UPSIDEDOWN),
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Flip _Vertical"), NO_KEYVAL, NO_MODIFIER,
                G_CALLBACK (menu_image_operation),
                GNOME_APP_PIXMAP_FILENAME, "gnome-commander/flip-vertical-16.xpm",
                G_OBJ_IMAGE_OP_KEY, (gpointer *) GUINT_TO_POINTER (ImageRender::FLIP_VERTICAL),
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("Flip _Horizontal"), NO_KEYVAL, NO_MODIFIER,
                G_CALLBACK (menu_image_operation),
                GNOME_APP_PIXMAP_FILENAME, "gnome-commander/flip-horizontal-16.xpm",
                G_OBJ_IMAGE_OP_KEY, (gpointer *) GUINT_TO_POINTER (ImageRender::FLIP_HORIZONTAL),
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NONE}
    };

    GtkWidget *binary_mode_settings_submenu = nullptr;
    MENU_ITEM_DATA settings_menu_items[] = {
        {MI_SUBMENU, _("_Binary Mode"), NO_KEYVAL, NO_MODIFIER,
                G_CALLBACK (nullptr),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                &binary_mode_settings_submenu, NO_GSLIST},

        {MI_CHECK, _("_Hexadecimal Offset"), GDK_d, GDK_CONTROL_MASK,
                G_CALLBACK (menu_settings_hex_decimal_offset),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                &obj->priv->hex_offset_menu_item, NO_GSLIST},
        {MI_SEPERATOR},
        {MI_NORMAL, _("_Save Current Settings"), GDK_s, GDK_CONTROL_MASK,
                G_CALLBACK (menu_settings_save_settings),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NONE}
    };

    GSList *binmode_chars_per_line_list = nullptr;
    MENU_ITEM_DATA binmode_settings_menu_items[] = {
        {MI_RADIO, _("_20 chars/line"), GDK_2, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                G_CALLBACK (menu_settings_binary_bytes_per_line),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                G_OBJ_BYTES_PER_LINE_KEY, (gpointer *) GUINT_TO_POINTER(20),
                &obj->priv->fixed_limit_menu_items[0], &binmode_chars_per_line_list},
        {MI_RADIO, _("_40 chars/line"), GDK_4, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                G_CALLBACK (menu_settings_binary_bytes_per_line),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                G_OBJ_BYTES_PER_LINE_KEY, (gpointer *) GUINT_TO_POINTER(40),
                &obj->priv->fixed_limit_menu_items[1], &binmode_chars_per_line_list},
        {MI_RADIO, _("_80 chars/line"), GDK_8, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                G_CALLBACK (menu_settings_binary_bytes_per_line),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                G_OBJ_BYTES_PER_LINE_KEY, (gpointer *) GUINT_TO_POINTER(80),
                &obj->priv->fixed_limit_menu_items[2], &binmode_chars_per_line_list},
        {MI_NONE}
    };

    MENU_ITEM_DATA help_menu_items[] = {
        {MI_NORMAL, _("Quick _Help"), GDK_F1, NO_MODIFIER,
                G_CALLBACK (menu_help_quick_help),
                GNOME_APP_PIXMAP_STOCK, GTK_STOCK_HELP,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NORMAL, _("_Keyboard Shortcuts"), NO_KEYVAL, NO_MODIFIER,
                G_CALLBACK (menu_help_keyboard),
                GNOME_APP_PIXMAP_NONE, NO_PIXMAP_INFO,
                NO_GOBJ_KEY, NO_GOBJ_VAL,
                NO_MENU_ITEM, NO_GSLIST},
        {MI_NONE}
    };

    int_viewer_menu = gtk_menu_bar_new ();
    obj->priv->accel_group = gtk_accel_group_new ();

    // File Menu
    submenu = create_sub_menu(_("_File"), int_viewer_menu);
    create_menu_items(submenu, obj->priv->accel_group, obj, file_menu_items);

    submenu = create_sub_menu(_("_View"), int_viewer_menu);
    create_menu_items(submenu, obj->priv->accel_group, obj, view_menu_items);

    submenu = create_sub_menu(_("_Text"), int_viewer_menu);
    create_menu_items(submenu, obj->priv->accel_group, obj, text_menu_items);
    // "encoding_submenu" was initialized when "text_menu_items" is usd to create the "text" menu
    create_menu_items(encoding_submenu, obj->priv->accel_group, obj, encoding_menu_items);

    submenu = create_sub_menu(_("_Image"), int_viewer_menu);
    create_menu_items(submenu, obj->priv->accel_group, obj, image_menu_items);

    submenu = create_sub_menu(_("_Settings"), int_viewer_menu);
    create_menu_items(submenu, obj->priv->accel_group, obj, settings_menu_items);

    create_menu_items(binary_mode_settings_submenu, obj->priv->accel_group, obj, binmode_settings_menu_items);

    submenu = create_sub_menu(_("_Help"), int_viewer_menu);
    create_menu_items(submenu, obj->priv->accel_group, obj, help_menu_items);

    gtk_window_add_accel_group (GTK_WINDOW (obj), obj->priv->accel_group);
    return int_viewer_menu;
}


// Event Handlers
static void menu_file_close (GtkMenuItem *item, GViewerWindow *obj)
{
    gtk_widget_destroy (GTK_WIDGET (obj));
}


static void menu_view_exif_information(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj != nullptr);
    g_return_if_fail (obj->priv->viewer != nullptr);

    if (gviewer_get_display_mode(obj->priv->viewer) != DISP_MODE_IMAGE)
        return;

    if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
        gviewer_window_show_metadata(obj);
    else
        gviewer_window_hide_metadata(obj);
}


static void menu_view_display_mode(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
        return;

    VIEWERDISPLAYMODE dispmode = (VIEWERDISPLAYMODE) GPOINTER_TO_UINT (g_object_get_data(G_OBJECT (item), G_OBJ_DISPMODE_KEY));

    if (dispmode==DISP_MODE_IMAGE)
    {
        if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (obj->priv->show_exif_menu_item)))
            gviewer_window_show_metadata(obj);
        else
            gviewer_window_hide_metadata(obj);
    }
    else
        gviewer_window_hide_metadata(obj);

    gviewer_set_display_mode(obj->priv->viewer, dispmode);
    gtk_widget_grab_focus (GTK_WIDGET (obj->priv->viewer));

    gtk_widget_draw (GTK_WIDGET (obj->priv->viewer), nullptr);
}


static void menu_view_set_charset(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
        return;

    gchar *charset = (gchar *) g_object_get_data (G_OBJECT (item), G_OBJ_CHARSET_KEY);
    g_return_if_fail (charset != nullptr);

    gviewer_set_encoding(obj->priv->viewer, charset);
    gtk_widget_draw (GTK_WIDGET (obj->priv->viewer), nullptr);
}


static void menu_image_operation(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    ImageRender::DISPLAYMODE imageop = (ImageRender::DISPLAYMODE) GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), G_OBJ_IMAGE_OP_KEY));

    gviewer_image_operation(obj->priv->viewer, imageop);

    gtk_widget_draw (GTK_WIDGET (obj->priv->viewer), nullptr);
}


static void menu_view_zoom_in(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    switch (gviewer_get_display_mode(obj->priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
            {
               int size = gviewer_get_font_size(obj->priv->viewer);

               if (size==0 || size>32)  return;

               size++;
               gviewer_set_font_size(obj->priv->viewer, size);
            }
            break;

        case DISP_MODE_IMAGE:
           {
               gviewer_set_best_fit(obj->priv->viewer, FALSE);

               if (obj->priv->current_scale_index<MAX_SCALE_FACTOR_INDEX-1)
                  obj->priv->current_scale_index++;

               if (gviewer_get_scale_factor(obj->priv->viewer) == image_scale_factors[obj->priv->current_scale_index])
                  return;

               gviewer_set_scale_factor(obj->priv->viewer, image_scale_factors[obj->priv->current_scale_index]);
           }
           break;

        default:
            break;
        }
}


static void menu_view_zoom_out(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    switch (gviewer_get_display_mode(obj->priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
            {
               int size = gviewer_get_font_size(obj->priv->viewer);

               if (size < 4)  return;

               size--;
               gviewer_set_font_size(obj->priv->viewer, size);
            }
            break;

        case DISP_MODE_IMAGE:
           gviewer_set_best_fit(obj->priv->viewer, FALSE);

           if (obj->priv->current_scale_index>0)
               obj->priv->current_scale_index--;

           if (gviewer_get_scale_factor(obj->priv->viewer) == image_scale_factors[obj->priv->current_scale_index])
              return;

           gviewer_set_scale_factor(obj->priv->viewer, image_scale_factors[obj->priv->current_scale_index]);
           break;

        default:
            break;
        }
}


static void menu_view_zoom_normal(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    switch (gviewer_get_display_mode (obj->priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
           // needs to completed with resetting to default font size
           break;

        case DISP_MODE_IMAGE:
           gviewer_set_best_fit (obj->priv->viewer, FALSE);
           gviewer_set_scale_factor(obj->priv->viewer, 1);
           obj->priv->current_scale_index = 5;
           break;

        default:
            break;
    }
}


static void menu_view_zoom_best_fit(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    if (gviewer_get_display_mode(obj->priv->viewer)==DISP_MODE_IMAGE)
        gviewer_set_best_fit(obj->priv->viewer, TRUE);
}


static void menu_settings_binary_bytes_per_line(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
        return;

    int bytes_per_line = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), G_OBJ_BYTES_PER_LINE_KEY));

    gviewer_set_fixed_limit(obj->priv->viewer, bytes_per_line);
    gtk_widget_draw (GTK_WIDGET (obj->priv->viewer), nullptr);
}


static void menu_edit_copy(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    gviewer_copy_selection(item, obj->priv->viewer);
}


static void start_find_thread(GViewerWindow *obj, gboolean forward)
{
    g_viewer_searcher_start_search(obj->priv->srchr, forward);
    gviewer_show_search_progress_dlg(GTK_WINDOW (obj),
                                     obj->priv->search_pattern,
                                     g_viewer_searcher_get_abort_indicator(obj->priv->srchr),
                                     g_viewer_searcher_get_complete_indicator(obj->priv->srchr),
                                     g_viewer_searcher_get_progress_indicator(obj->priv->srchr));

    g_viewer_searcher_join(obj->priv->srchr);

    if (g_viewer_searcher_get_end_of_search(obj->priv->srchr))
    {
        GtkWidget *w;

        w = gtk_message_dialog_new(GTK_WINDOW (obj), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, _("Pattern “%s” was not found"), obj->priv->search_pattern);
        gtk_dialog_run (GTK_DIALOG (w));
        gtk_widget_destroy (w);
    }
    else
    {
        offset_type result;

        result = g_viewer_searcher_get_search_result(obj->priv->srchr);
        text_render_set_marker(gviewer_get_text_render(obj->priv->viewer),
                result,
                result + (forward?1:-1) * obj->priv->search_pattern_len);
        text_render_ensure_offset_visible(gviewer_get_text_render(obj->priv->viewer), result);
    }
}


static void menu_edit_find(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);

    // Show the Search Dialog
    GtkWidget *w = gviewer_search_dlg_new (GTK_WINDOW (obj));
    if (gtk_dialog_run (GTK_DIALOG (w))!=GTK_RESPONSE_OK)
    {
        gtk_widget_destroy (w);
        return;
    }

    // If a previous search is active, delete it
    if (obj->priv->srchr != nullptr)
    {
        g_object_unref (obj->priv->srchr);
        obj->priv->srchr = nullptr;

        g_free (obj->priv->search_pattern);
        obj->priv->search_pattern = nullptr;
    }

    // Get the search information from the search dialog
    GViewerSearchDlg *srch_dlg = GVIEWER_SEARCH_DLG (w);
    obj->priv->search_pattern = gviewer_search_dlg_get_search_text_string (srch_dlg);

    // Create & prepare the search object
    obj->priv->srchr = g_viewer_searcher_new ();

    if (gviewer_search_dlg_get_search_mode (srch_dlg)==SEARCH_MODE_TEXT)
    {
        // Text search
        g_viewer_searcher_setup_new_text_search(obj->priv->srchr,
            text_render_get_input_mode_data(gviewer_get_text_render(obj->priv->viewer)),
            text_render_get_current_offset(gviewer_get_text_render(obj->priv->viewer)),
            gv_file_get_max_offset (text_render_get_file_ops (gviewer_get_text_render(obj->priv->viewer))),
            obj->priv->search_pattern,
            gviewer_search_dlg_get_case_sensitive(srch_dlg));
        obj->priv->search_pattern_len = strlen(obj->priv->search_pattern);
    }
    else
    {
        // Hex Search
        guint buflen;
        guint8 *buffer = gviewer_search_dlg_get_search_hex_buffer (srch_dlg, buflen);

        g_return_if_fail (buffer != nullptr);

        obj->priv->search_pattern_len = buflen;
        g_viewer_searcher_setup_new_hex_search(obj->priv->srchr,
            text_render_get_input_mode_data(gviewer_get_text_render(obj->priv->viewer)),
            text_render_get_current_offset(gviewer_get_text_render(obj->priv->viewer)),
            gv_file_get_max_offset (text_render_get_file_ops(gviewer_get_text_render(obj->priv->viewer))),
            buffer, buflen);

        g_free (buffer);
    }

    gtk_widget_destroy (w);


    // call  "find_next" to actually do the search
    start_find_thread(obj, TRUE);
}


static void menu_edit_find_next(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);

    if (!obj->priv->srchr)
    {
        /*
            if no search is active, call "menu_edit_find".
            (which will call "menu_edit_find_next" again */
        menu_edit_find(item, obj);
        return;
    }

    start_find_thread(obj, TRUE);
}


static void menu_edit_find_prev(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);

    if (!obj->priv->srchr)
        return;

    start_find_thread(obj, FALSE);
}


static void menu_view_wrap(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    gboolean wrap = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));

    gviewer_set_wrap_mode(obj->priv->viewer, wrap);
    gtk_widget_draw (GTK_WIDGET (obj->priv->viewer), nullptr);
}


static void menu_settings_hex_decimal_offset(GtkMenuItem *item, GViewerWindow *obj)
{
    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    gboolean hex = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));
    gviewer_set_hex_offset_display(obj->priv->viewer, hex);
}


void gviewer_window_load_settings(/* out */ GViewerWindowSettings *settings)
{
    g_return_if_fail (settings != nullptr);

    InternalViewerSettings *iv_settings;
    iv_settings = iv_settings_new();

    gchar *temp = g_settings_get_string (iv_settings->internalviewer, GCMD_GSETTINGS_IV_CHARSET);
    strncpy(settings->charset, temp, sizeof(settings->charset) - 1);
    g_free (temp);

    temp = g_settings_get_string (iv_settings->internalviewer, GCMD_SETTINGS_IV_FIXED_FONT_NAME);
    strncpy(settings->fixed_font_name, temp, sizeof(settings->fixed_font_name) - 1);
    g_free (temp);

    temp = g_settings_get_string (iv_settings->internalviewer, GCMD_SETTINGS_IV_VARIABLE_FONT_NAME);
    strncpy(settings->variable_font_name, temp, sizeof(settings->variable_font_name) - 1);
    g_free (temp);

    settings->hex_decimal_offset = g_settings_get_boolean (iv_settings->internalviewer, GCMD_SETTINGS_IV_DISPLAY_HEX_OFFSET);
    settings->wrap_mode = g_settings_get_boolean (iv_settings->internalviewer, GCMD_SETTINGS_IV_WRAP_MODE);

    settings->font_size = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_FONT_SIZE);
    settings->tab_size = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_TAB_SIZE);
    settings->binary_bytes_per_line = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_BINARY_BYTES_PER_LINE);

    settings->rect.x = g_settings_get_int (iv_settings->internalviewer, GCMD_SETTINGS_IV_X_OFFSET);
    settings->rect.y = g_settings_get_int (iv_settings->internalviewer, GCMD_SETTINGS_IV_Y_OFFSET);
    settings->rect.width = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_WIDTH);
    settings->rect.height = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_HEIGHT);
}


static void menu_settings_save_settings(GtkMenuItem *item, GViewerWindow *obj)
{
    GViewerWindowSettings settings;

    InternalViewerSettings *iv_settings;
    iv_settings = iv_settings_new();

    g_return_if_fail (obj);
    g_return_if_fail (obj->priv->viewer);

    gviewer_window_get_current_settings(obj, &settings);

    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_GSETTINGS_IV_CHARSET, settings.charset);
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_FIXED_FONT_NAME, settings.fixed_font_name);
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_VARIABLE_FONT_NAME, settings.variable_font_name);

    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_DISPLAY_HEX_OFFSET, &(settings.hex_decimal_offset));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_WRAP_MODE, &(settings.wrap_mode));

    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_FONT_SIZE, &(settings.font_size));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_TAB_SIZE, &(settings.tab_size));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_BINARY_BYTES_PER_LINE, &(settings.binary_bytes_per_line));
 
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_X_OFFSET, &(settings.rect.x));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_Y_OFFSET, &(settings.rect.y));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_WIDTH, &(settings.rect.width));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_HEIGHT, &(settings.rect.height));
}


static void menu_help_quick_help(GtkMenuItem *item, GViewerWindow *obj)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-internal-viewer");
}


static void menu_help_keyboard(GtkMenuItem *item, GViewerWindow *obj)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-internal-viewer-keyboard");
}


inline void gviewer_window_show_metadata(GViewerWindow *w)
{
    g_return_if_fail (w != nullptr);
    g_return_if_fail (w->priv->f != nullptr);

    if (w->priv->metadata_visible)
        return;

    if (!w->priv->metadata_view)
    {
        GtkWidget *scrolledwindow = gtk_scrolled_window_new (nullptr, nullptr);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 10);
        gtk_container_add (GTK_CONTAINER (scrolledwindow), create_view ());
        gtk_box_pack_start (GTK_BOX (w->priv->vbox), scrolledwindow, TRUE, TRUE, 0);
        w->priv->metadata_view = scrolledwindow;
    }

    GtkWidget *view = gtk_bin_get_child (GTK_BIN (w->priv->metadata_view));
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

    if (!model)
    {
        model = create_model ();
        fill_model (GTK_TREE_STORE (model), w->priv->f);
        gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);
        g_object_unref (model);          // destroy model automatically with view
        gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)), GTK_SELECTION_NONE);
    }

    w->priv->metadata_visible = TRUE;

    gtk_widget_show_all (w->priv->metadata_view);
}


inline void gviewer_window_hide_metadata(GViewerWindow *obj)
{
    g_return_if_fail (obj != nullptr);
    g_return_if_fail (obj->priv->metadata_view != nullptr);

    if (!obj->priv->metadata_visible)
        return;

    obj->priv->metadata_visible = FALSE;
    // gtk_container_remove (GTK_CONTAINER (obj->priv->vbox), obj->priv->metadata_view);
    gtk_widget_hide_all (obj->priv->metadata_view);
    gtk_widget_grab_focus (GTK_WIDGET (obj->priv->viewer));
}


enum
{
    COL_TAG,
    COL_TYPE,
    COL_NAME,
    COL_VALUE,
    COL_DESC,
    NUM_COLS
} ;


inline GtkTreeModel *create_model ()
{
    GtkTreeStore *tree = gtk_tree_store_new (NUM_COLS,
                                             G_TYPE_UINT,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING);
    return GTK_TREE_MODEL (tree);
}


inline void fill_model (GtkTreeStore *tree, GnomeCmdFile *f)
{
    if (!gcmd_tags_bulk_load (f))
        return;

    GnomeCmdTagClass prev_tagclass = TAG_NONE_CLASS;

    GtkTreeIter toplevel;

    for (GnomeCmdFileMetadata::METADATA_COLL::const_iterator i=f->metadata->begin(); i!=f->metadata->end(); ++i)
    {
        const GnomeCmdTag t = i->first;
        GnomeCmdTagClass curr_tagclass = gcmd_tags_get_class(t);

        if (curr_tagclass==TAG_NONE_CLASS)
            continue;

        if (prev_tagclass!=curr_tagclass)
        {
            gtk_tree_store_append (tree, &toplevel, nullptr);
            gtk_tree_store_set (tree, &toplevel,
                                COL_TAG, TAG_NONE,
                                COL_TYPE, gcmd_tags_get_class_name(t),
                                -1);
        }

        for (set<std::string>::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
        {
            GtkTreeIter child;

            gtk_tree_store_append (tree, &child, &toplevel);
            gtk_tree_store_set (tree, &child,
                                COL_TAG, t,
                                COL_NAME, gcmd_tags_get_title(t),
                                COL_VALUE, j->c_str(),
                                COL_DESC, gcmd_tags_get_description(t),
                                -1);
        }

        prev_tagclass = curr_tagclass;
    }

}


GtkWidget *create_view ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "enable-search", TRUE,
                  "search-column", COL_VALUE,
                  nullptr);

    GtkCellRenderer *renderer = nullptr;
    GtkTreeViewColumn *col = nullptr;

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_TYPE, _("Type"));
    gtk_widget_set_tooltip_text (col->button, _("Metadata namespace"));

    g_object_set (renderer,
                  "weight-set", TRUE,
                  "weight", PANGO_WEIGHT_BOLD,
                  nullptr);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_NAME, _("Name"));
    gtk_widget_set_tooltip_text (col->button, _("Tag name"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_VALUE, _("Value"));
    gtk_widget_set_tooltip_text (col->button, _("Tag value"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_DESC, _("Description"));
    gtk_widget_set_tooltip_text (col->button, _("Metadata tag description"));

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  nullptr);

    return view;
}
