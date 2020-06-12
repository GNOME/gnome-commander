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
#include <vector>

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
#include "imageloader.h"

using namespace std;


#define G_OBJ_CHARSET_KEY        "charset"
#define G_OBJ_DISPMODE_KEY       "dispmode"
#define G_OBJ_BYTES_PER_LINE_KEY "bytesperline"
#define G_OBJ_IMAGE_OP_KEY       "imageop"
#define G_OBJ_EXTERNAL_TOOL_KEY  "exttool"

#define GCMD_INTERNAL_VIEWER               "org.gnome.gnome-commander.preferences.internal-viewer"
#define GCMD_GSETTINGS_IV_CHARSET          "charset"

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
    std::vector<GtkWidget *> encodingMenuItems;
    std::vector<const char*> encodingCharsets;
    GtkWidget *wrap_mode_menu_item;
    GtkWidget *hex_offset_menu_item;
    GtkWidget *show_exif_menu_item;
    GtkWidget *fixed_limit_menu_items[3];
    GViewerWindowSettings *gViewerWindowSettings;

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

static void gviewer_window_status_line_changed(GViewer *gViewer, const gchar *status_line, GViewerWindow *gViewerWindow);

static gboolean gviewer_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

static GtkWidget *gviewer_window_create_menus(GViewerWindow *gViewerWindow);

inline void gviewer_window_show_metadata(GViewerWindow *gViewerWindow);
inline void gviewer_window_hide_metadata(GViewerWindow *gViewerWindow);

gboolean gviewerwindow_get_metadata_visble(GViewerWindow *gViewerWindow);

// Event Handlers
static void menu_file_close (GtkMenuItem *item, GViewerWindow *gViewerWindow);

static void menu_toggle_view_exif_information(GtkToggleAction *item, GViewerWindow *gViewerWindow);

static void menu_edit_copy(GtkMenuItem *item, GViewerWindow *gViewerWindow);
static void menu_edit_find(GtkMenuItem *item, GViewerWindow *gViewerWindow);
static void menu_edit_find_next(GtkMenuItem *item, GViewerWindow *gViewerWindow);
static void menu_edit_find_prev(GtkMenuItem *item, GViewerWindow *gViewerWindow);

static void menu_view_wrap(GtkToggleAction *item, GViewerWindow *gViewerWindow);
static void menu_view_set_display_mode(GtkAction *notUsed, GtkRadioAction *radioAction, GViewerWindow *gViewerWindow);
static void menu_view_set_charset(GtkAction *notUsed, GtkRadioAction *radioAction, GViewerWindow *gViewerWindow);
static void menu_view_zoom_in(GtkMenuItem *item, GViewerWindow *gViewerWindow);
static void menu_view_zoom_out(GtkMenuItem *item, GViewerWindow *gViewerWindow);
static void menu_view_zoom_normal(GtkMenuItem *item, GViewerWindow *gViewerWindow);
static void menu_view_zoom_best_fit(GtkMenuItem *item, GViewerWindow *gViewerWindow);

static void menu_image_operation(GtkMenuItem *item, GViewerWindow *gViewerWindow);

static void menu_settings_binary_bytes_per_line(GtkAction *notUsed, GtkRadioAction *radioAction, GViewerWindow *gViewerWindow);
static void menu_settings_hex_decimal_offset(GtkToggleAction *item, GViewerWindow *gViewerWindow);
static void menu_settings_save_settings(GtkMenuItem *item, GViewerWindow *gViewerWindow);

static void menu_help_quick_help(GtkMenuItem *item, GViewerWindow *gViewerWindow);
static void menu_help_keyboard(GtkMenuItem *item, GViewerWindow *gViewerWindow);

inline GtkTreeModel *create_model ();
inline void fill_model (GtkTreeStore *treestore, GnomeCmdFile *f);
GtkWidget *create_view ();

/*****************************************
    public functions
    (defined in the header file)
*****************************************/

GtkWidget *gviewer_window_file_view (GnomeCmdFile *f, GViewerWindowSettings *gViewerWindowSettings)
{
    if (!gViewerWindowSettings)
    {
        gViewerWindowSettings = gviewer_window_get_settings();
    }

    GtkWidget *w = gviewer_window_new ();

    gviewer_window_load_file (GVIEWER_WINDOW (w), f);

    gviewer_window_load_settings(GVIEWER_WINDOW (w), gViewerWindowSettings);

    GVIEWER_WINDOW(w)->priv->gViewerWindowSettings = gViewerWindowSettings;

    return w;
}


void gviewer_window_load_file (GViewerWindow *gViewerWindow, GnomeCmdFile *f)
{
    g_return_if_fail (gViewerWindow != nullptr);
    g_return_if_fail (f != nullptr);

    g_free (gViewerWindow->priv->filename);

    gViewerWindow->priv->f = f;
    gViewerWindow->priv->filename = f->get_real_path();
    gviewer_load_file (gViewerWindow->priv->viewer, gViewerWindow->priv->filename);

    gtk_window_set_title (GTK_WINDOW (gViewerWindow), gViewerWindow->priv->filename);
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

    // This list must be in the same order as the elements in w->priv->encodingMenuItems
    w->priv->encodingCharsets = { UTF8, ASCII, CP437,
                          ISO88596, ARABIC, CP864,
                          ISO88594, ISO88592, CP1250,
                          ISO88595, CP1251, ISO88597,
                          CP1253, HEBREW, CP862,
                          ISO88598, ISO885915, ISO88593,
                          ISO88599, CP1254, CP1252,
                          ISO88591 };
}

static void gviewer_window_status_line_changed(GViewer *gViewer, const gchar *status_line, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));

    GViewerWindow *w = GVIEWER_WINDOW (gViewerWindow);

    if (w->priv->status_bar_msg)
        gtk_statusbar_pop (GTK_STATUSBAR (w->priv->statusbar), w->priv->statusbar_ctx_id);

    if (status_line)
        gtk_statusbar_push (GTK_STATUSBAR (w->priv->statusbar), w->priv->statusbar_ctx_id, status_line);

    w->priv->status_bar_msg = status_line != nullptr;
}


void gviewer_window_load_settings(GViewerWindow *gViewerWindow, GViewerWindowSettings *settings)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    g_return_if_fail (settings != nullptr);
    g_return_if_fail (gViewerWindow->priv->viewer != nullptr);

    gviewer_set_font_size(gViewerWindow->priv->viewer, settings->font_size);
    gviewer_set_tab_size(gViewerWindow->priv->viewer, settings->tab_size);

    gviewer_set_fixed_limit(gViewerWindow->priv->viewer, settings->binary_bytes_per_line);
    switch (settings->binary_bytes_per_line)
    {
        case 20:
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gViewerWindow->priv->fixed_limit_menu_items[0]), TRUE);
            break;
        case 40:
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gViewerWindow->priv->fixed_limit_menu_items[1]), TRUE);
            break;
        case 80:
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gViewerWindow->priv->fixed_limit_menu_items[2]), TRUE);
            break;
        default:
            break;
    }

    gviewer_set_wrap_mode(gViewerWindow->priv->viewer, settings->wrap_mode);
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM (gViewerWindow->priv->wrap_mode_menu_item),settings->wrap_mode);

    gviewer_set_hex_offset_display(gViewerWindow->priv->viewer, settings->hex_decimal_offset);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM (gViewerWindow->priv->hex_offset_menu_item), settings->hex_decimal_offset);

    if (settings->metadata_visible)
    {
        gviewer_window_show_metadata(gViewerWindow);
    }
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gViewerWindow->priv->show_exif_menu_item), settings->metadata_visible);

    gviewer_set_encoding(gViewerWindow->priv->viewer, settings->charset);

    // activate the radio menu item for the selected charset in the settings
    auto availableCharsets = gViewerWindow->priv->encodingCharsets;
    auto index = 0;
    for(auto ii = availableCharsets.begin(); ii != availableCharsets.end(); ii++)
    {
        if (!g_strcmp0(availableCharsets.at(index), settings->charset))
            break;
        index++;
    }
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gViewerWindow->priv->encodingMenuItems[index]), TRUE);

    gtk_window_resize(GTK_WINDOW (gViewerWindow),
        settings->rect.width, settings->rect.height);

#if 0
    // This doesn't work because the window is not shown yet
    if (GTK_WIDGET (obj)->window)
        gdk_window_move (GTK_WIDGET (obj)->window, settings->rect.x, settings->rect.y);
#endif
    gtk_window_set_position (GTK_WINDOW (gViewerWindow), GTK_WIN_POS_CENTER);
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
    settings->metadata_visible = gviewerwindow_get_metadata_visble(obj);
}

gboolean gviewerwindow_get_metadata_visble(GViewerWindow *gViewerWindow)
{
    g_return_val_if_fail (IS_GVIEWER_WINDOW (gViewerWindow), FALSE);

    return (gViewerWindow->priv->metadata_visible);
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
        delete(w->priv->gViewerWindowSettings);

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
            case GDK_q:
            case GDK_Q:
                gtk_widget_destroy (GTK_WIDGET (w));
                return TRUE;

            default:
                break;
        }

    if (state_is_shift(event->state))
        switch (event->keyval)
        {
            default:
                break;
        }

    if (state_is_alt(event->state))
        switch (event->keyval)
        {
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


static GtkWidget *gviewer_window_create_menus(GViewerWindow *gViewerWindow)
{
    static const char *uiMenuBarDescription =
    "<ui>"
    "  <menubar name='InternalViewerMenuBar'>"
    "    <menu action='FileMenu'>"
    "      <menuitem action='Close'/>"
    "    </menu>"
    "    <menu action='ViewMenu'>"
    "      <menuitem action='Text'/>"
    "      <menuitem action='Binary'/>"
    "      <menuitem action='Hexadecimal'/>"
    "      <menuitem action='Image'/>"
    "      <separator/>"
    "      <menuitem action='ZoomIn'/>"
    "      <menuitem action='ZoomOut'/>"
    "      <menuitem action='NormalSize'/>"
    "      <menuitem action='BestFit'/>"
    "    </menu>"
    "    <menu action='TextMenu'>"
    "      <menuitem action='CopyTextSelection'/>"
    "      <menuitem action='Find'/>"
    "      <menuitem action='FindNext'/>"
    "      <menuitem action='FindPrevious'/>"
    "      <separator/>"
    "      <menuitem action='WrapLines'/>"
    "      <separator/>"
    "      <menu action='Encoding'>"
    "        <menuitem action='UTF8'/>"
    "        <menuitem action='EnglishUsAscii'/>"
    "        <menuitem action='TerminalCP437'/>"
    "        <menuitem action='ArabicISO88596'/>"
    "        <menuitem action='ArabicWindows'/>"
    "        <menuitem action='ArabicDosCP864'/>"
    "        <menuitem action='BalticISO88594'/>"
    "        <menuitem action='CentrEuISO88592'/>"
    "        <menuitem action='CentrEuCP1250'/>"
    "        <menuitem action='CyrISO88595'/>"
    "        <menuitem action='CyrCP1251'/>"
    "        <menuitem action='GreekISO88597'/>"
    "        <menuitem action='GreekCP1253'/>"
    "        <menuitem action='HebrewWin'/>"
    "        <menuitem action='HebrewDos'/>"
    "        <menuitem action='HebrewISO88598'/>"
    "        <menuitem action='Latin9'/>"
    "        <menuitem action='MalteseISO88593'/>"
    "        <menuitem action='TurkishISO88599'/>"
    "        <menuitem action='TurkishCP1254'/>"
    "        <menuitem action='WesternCP1252'/>"
    "        <menuitem action='WesternISO88591'/>"
    "      </menu>"
    "    </menu>"
    "    <menu action='ImageMenu'>"
    "      <menuitem action='ShowMetadataTags'/>"
    "      <separator/>"
    "      <menuitem action='RotateClockwise'/>"
    "      <menuitem action='RotateCounterClockwise'/>"
    "      <menuitem action='Rotate180'/>"
    "      <menuitem action='FlipVertical'/>"
    "      <menuitem action='FlipHorizontal'/>"
    "    </menu>"
    "    <menu action='SettingsMenu'>"
    "      <menu action='BinaryMode'>"
    "        <menuitem action='20CharsPerLine'/>"
    "        <menuitem action='40CharsPerLine'/>"
    "        <menuitem action='80CharsPerLine'/>"
    "      </menu>"
    "      <menuitem action='HexadecimalOffset'/>"
    "      <separator/>"
    "      <menuitem action='SaveCurrentSettings'/>"
    "    </menu>"
    "    <menu action='HelpMenu'>"
    "      <menuitem action='QuickHelp'/>"
    "      <menuitem action='KeyboardShortcuts'/>"
    "    </menu>"
    "  </menubar>"
    "</ui>";


    static const GtkActionEntry fileMenuEntries[] =
    {
        { "FileMenu", nullptr, _("_File") },
        { "Close", GTK_STOCK_CLOSE,  _("_Close"), "Escape", nullptr, (GCallback) menu_file_close }
    };

    static const GtkRadioActionEntry viewMenuRadioEntries[] =
    {
        { "Text",        nullptr, _("_Text"),        "1", nullptr, DISP_MODE_TEXT_FIXED},
        { "Binary",      nullptr, _("_Binary"),      "2", nullptr, DISP_MODE_BINARY},
        { "Hexadecimal", nullptr, _("_Hexadecimal"), "3", nullptr, DISP_MODE_HEXDUMP},
        { "Image",       nullptr, _("_Image"),       "4", nullptr, DISP_MODE_IMAGE},
    };

    static const GtkActionEntry viewMenuEntries[] =
    {
        { "ViewMenu",   nullptr,            _("_View") },
        { "ZoomIn",     GTK_STOCK_ZOOM_IN,  _("_Zoom In"),     "<Control>plus",  nullptr, (GCallback) menu_view_zoom_in },
        { "ZoomOut",    GTK_STOCK_ZOOM_OUT, _("_Zoom Out"),    "<Control>minus", nullptr, (GCallback) menu_view_zoom_out },
        { "NormalSize", GTK_STOCK_ZOOM_100, _("_Normal Size"), "<Control>0",        nullptr, (GCallback) menu_view_zoom_normal },
        { "BestFit",    GTK_STOCK_ZOOM_FIT, _("_Best Fit"),    "<Control>period",   nullptr, (GCallback) menu_view_zoom_best_fit }
    };

    static const GtkActionEntry textMenuEntries[] =
    {
        { "TextMenu",          nullptr,        _("_Text") },
        { "CopyTextSelection", GTK_STOCK_COPY, _("_Copy Text Selection"), "<Control>C", nullptr, (GCallback) menu_edit_copy },
        { "Find",              GTK_STOCK_FIND, _("Find…"),                "<Control>F", nullptr, (GCallback) menu_edit_find },
        { "FindNext",          nullptr,        _("Find Next"),            "F3",         nullptr, (GCallback) menu_edit_find_next },
        { "FindPrevious",      nullptr,        _("Find Previous"),        "<Shift>F3",  nullptr, (GCallback) menu_edit_find_prev },
        { "Encoding",          nullptr,        _("_Encoding") }
    };

    static const GtkToggleActionEntry textMenuToggleEntries[] =
    {
        { "WrapLines",         nullptr,        _("_Wrap lines"), "<Control>W",  nullptr, (GCallback) menu_view_wrap, true}
    };

    static const GtkRadioActionEntry encodingMenuRadioEntries[] =
    {
        { "UTF8",            nullptr, _("_UTF-8"),        "U", nullptr, 0},
        { "EnglishUsAscii",  nullptr, _("English (US-_ASCII)"), "A", nullptr, 1},
        { "TerminalCP437",   nullptr, _("Terminal (CP437)"), "Q", nullptr, 2},
        { "ArabicISO88596",  nullptr, _("Arabic (ISO-8859-6)"), nullptr, nullptr, 3},
        { "ArabicWindows",   nullptr, _("Arabic (Windows, CP1256)"), nullptr, nullptr, 4},
        { "ArabicDosCP864",  nullptr, _("Arabic (Dos, CP864)"), nullptr, nullptr, 5},
        { "BalticISO88594",  nullptr, _("Baltic (ISO-8859-4)"), nullptr, nullptr, 6},
        { "CentrEuISO88592", nullptr, _("Central European (ISO-8859-2)"), nullptr, nullptr, 7},
        { "CentrEuCP1250",   nullptr, _("Central European (CP1250)"), nullptr, nullptr, 8},
        { "CyrISO88595",     nullptr, _("Cyrillic (ISO-8859-5)"), nullptr, nullptr, 9},
        { "CyrCP1251",       nullptr, _("Cyrillic (CP1251)"), nullptr, nullptr, 10},
        { "GreekISO88597",   nullptr, _("Greek (ISO-8859-7)"), nullptr, nullptr, 11},
        { "GreekCP1253",     nullptr, _("Greek (CP1253)"), nullptr, nullptr, 12},
        { "HebrewWin",       nullptr, _("Hebrew (Windows, CP1255)"), nullptr, nullptr, 13},
        { "HebrewDos",       nullptr, _("Hebrew (Dos, CP862)"), nullptr, nullptr, 14},
        { "HebrewISO88598",  nullptr, _("Hebrew (ISO-8859-8)"), nullptr, nullptr, 15},
        { "Latin9",          nullptr, _("Latin 9 (ISO-8859-15)"), nullptr, nullptr, 16},
        { "MalteseISO88593", nullptr, _("Maltese (ISO-8859-3)"), nullptr, nullptr, 17},
        { "TurkishISO88599", nullptr, _("Turkish (ISO-8859-9)"), nullptr, nullptr, 18},
        { "TurkishCP1254",   nullptr, _("Turkish (CP1254)"), nullptr, nullptr, 19},
        { "WesternCP1252",   nullptr, _("Western (CP1252)"), nullptr, nullptr, 20},
        { "WesternISO88591", nullptr, _("Western (ISO-8859-1)"), nullptr, nullptr, 21},
    };

    static const GtkToggleActionEntry imageMenuToggleEntries[] =
    {
        { "ShowMetadataTags", nullptr, _("Show Metadata _Tags"), "T",  nullptr, (GCallback) menu_toggle_view_exif_information, false}
    };

    static const GtkActionEntry imageMenuEntries[] =
    {
        { "ImageMenu",              nullptr,                 _("_Image") },
        { "RotateClockwise",        ROTATE_90_STOCKID,       _("Rotate Clockwise"),          "<Control>R", nullptr, (GCallback) menu_image_operation },
        { "RotateCounterClockwise", ROTATE_270_STOCKID,      _("Rotate Counter Clockwis_e"), nullptr,      nullptr, (GCallback) menu_image_operation },
        { "Rotate180",              ROTATE_180_STOCKID,      _("Rotate 180\xC2\xB0"),        "<Control><Shift>R", nullptr, (GCallback) menu_image_operation },
        { "FlipVertical",           FLIP_VERTICAL_STOCKID,   _("Flip _Vertical"),            nullptr,      nullptr, (GCallback) menu_image_operation },
        { "FlipHorizontal",         FLIP_HORIZONTAL_STOCKID, _("Flip _Horizontal"),          nullptr,      nullptr, (GCallback) menu_image_operation }
    };

    static const GtkActionEntry settingsMenuEntries[] =
    {
        { "SettingsMenu",        nullptr, _("_Settings") },
        { "BinaryMode",          nullptr, _("_Binary Mode") },
        { "SaveCurrentSettings", nullptr, _("_Save Current Settings"), "<Control>S", nullptr, (GCallback) menu_settings_save_settings }
    };

    static const GtkToggleActionEntry settingsMenuToggleEntries[] =
    {
        { "HexadecimalOffset", nullptr, _("_Hexadecimal Offset"), "<Control>D", nullptr, (GCallback) menu_settings_hex_decimal_offset, true }
    };

    static const GtkRadioActionEntry settingsMenuRadioEntries[] =
    {
        { "20CharsPerLine", nullptr, _("_20 chars/line"), "<Control>2", nullptr, 20},
        { "40CharsPerLine", nullptr, _("_40 chars/line"), "<Control>4", nullptr, 40},
        { "80CharsPerLine", nullptr, _("_80 chars/line"), "<Control>8", nullptr, 80}
    };

    static const GtkActionEntry helpMenuEntries[] =
    {
        { "HelpMenu",          nullptr,          _("_Help") },
        { "QuickHelp",         GTK_STOCK_HELP,   _("Quick _Help"),        "F1",     nullptr, (GCallback) menu_help_quick_help },
        { "KeyboardShortcuts", GTK_STOCK_ITALIC, _("_Keyboard Shortcuts"), nullptr, nullptr, (GCallback) menu_help_keyboard }
    };

    //gViewerWindow->priv->accel_group = gtk_accel_group_new ();

    GtkActionGroup *actionGroup;
    GtkAccelGroup *accelGroup;
    GtkUIManager *uiManager;
    GError *error;

    actionGroup = gtk_action_group_new ("InternalViewerMenuBarActions");
    gtk_action_group_add_actions (actionGroup, fileMenuEntries, G_N_ELEMENTS (fileMenuEntries), gViewerWindow);
    gtk_action_group_add_actions (actionGroup, viewMenuEntries, G_N_ELEMENTS (viewMenuEntries), gViewerWindow);
    gtk_action_group_add_actions (actionGroup, textMenuEntries, G_N_ELEMENTS (textMenuEntries), gViewerWindow);
    gtk_action_group_add_actions (actionGroup, imageMenuEntries, G_N_ELEMENTS (imageMenuEntries), gViewerWindow);
    gtk_action_group_add_actions (actionGroup, settingsMenuEntries, G_N_ELEMENTS (settingsMenuEntries), gViewerWindow);
    gtk_action_group_add_actions (actionGroup, helpMenuEntries, G_N_ELEMENTS (helpMenuEntries), gViewerWindow);

    gtk_action_group_add_toggle_actions (actionGroup, textMenuToggleEntries, G_N_ELEMENTS (textMenuToggleEntries), gViewerWindow);
    gtk_action_group_add_toggle_actions (actionGroup, imageMenuToggleEntries, G_N_ELEMENTS (imageMenuToggleEntries), gViewerWindow);
    gtk_action_group_add_toggle_actions (actionGroup, settingsMenuToggleEntries, G_N_ELEMENTS (settingsMenuToggleEntries), gViewerWindow);

    gtk_action_group_add_radio_actions (actionGroup, viewMenuRadioEntries,
                                        G_N_ELEMENTS (viewMenuRadioEntries), 0, (GCallback) menu_view_set_display_mode, gViewerWindow);
    gtk_action_group_add_radio_actions (actionGroup, encodingMenuRadioEntries,
                                        G_N_ELEMENTS (encodingMenuRadioEntries), 0, (GCallback) menu_view_set_charset, gViewerWindow);
    gtk_action_group_add_radio_actions (actionGroup, settingsMenuRadioEntries,
                                        G_N_ELEMENTS (settingsMenuRadioEntries), 0, (GCallback) menu_settings_binary_bytes_per_line, gViewerWindow);

    uiManager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (uiManager, actionGroup, 0);

    accelGroup = gtk_ui_manager_get_accel_group (uiManager);
    gtk_window_add_accel_group (GTK_WINDOW (gViewerWindow), accelGroup);

    error = nullptr;
    if (!gtk_ui_manager_add_ui_from_string (uiManager, uiMenuBarDescription, -1, &error))
    {
        g_message ("building main menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
    }

    gtk_ui_manager_ensure_update (uiManager);

    // Reference toggle items: SettingsMenu->BinaryModes
    gViewerWindow->priv->fixed_limit_menu_items[0]
        = gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/SettingsMenu/BinaryMode/20CharsPerLine");
    gViewerWindow->priv->fixed_limit_menu_items[1]
        = gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/SettingsMenu/BinaryMode/40CharsPerLine");
    gViewerWindow->priv->fixed_limit_menu_items[2]
        = gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/SettingsMenu/BinaryMode/80CharsPerLine");

    // Reference wrap item: TextMenu->WrapLines
    gViewerWindow->priv->wrap_mode_menu_item = gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/WrapLines");

    // Reference toggle items: TextMenu->Charsets
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/UTF8"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/EnglishUsAscii"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/TerminalCP437"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/ArabicISO88596"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/ArabicWindows"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/ArabicDosCP864"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/BalticISO88594"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/CentrEuISO88592"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/CentrEuCP1250"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/CyrISO88595"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/CyrCP1251"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/GreekISO88597"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/GreekCP1253"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/HebrewWin"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/HebrewDos"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/HebrewISO88598"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/Latin9"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/MalteseISO88593"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/TurkishISO88599"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/TurkishCP1254"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/WesternCP1252"));
    gViewerWindow->priv->encodingMenuItems.push_back(gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/TextMenu/Encoding/WesternISO88591"));

    // Reference toggle item SettingsMenu->HexadecimalOffset
    gViewerWindow->priv->hex_offset_menu_item = gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/SettingsMenu/HexadecimalOffset");

    // Reference toggle item ImageMenu->ShowMetadataTags
    gViewerWindow->priv->show_exif_menu_item = gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar/ImageMenu/ShowMetadataTags");

    return gtk_ui_manager_get_widget (uiManager, "/InternalViewerMenuBar");
}


// Event Handlers
static void menu_file_close (GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    gtk_widget_destroy (GTK_WIDGET (gViewerWindow));
}


static void menu_toggle_view_exif_information(GtkToggleAction *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    gtk_toggle_action_get_active (item)
        ? gviewer_window_show_metadata(gViewerWindow)
        : gviewer_window_hide_metadata(gViewerWindow);
}


static void menu_view_set_display_mode(GtkAction *notUsed, GtkRadioAction *radioAction, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (gViewerWindow->priv->viewer));

    auto dispmode = (VIEWERDISPLAYMODE) gtk_radio_action_get_current_value (radioAction);

    if (dispmode == DISP_MODE_IMAGE)
    {
        if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (gViewerWindow->priv->show_exif_menu_item)))
            gviewer_window_show_metadata(gViewerWindow);
        else
            gviewer_window_hide_metadata(gViewerWindow);
    }
    else
        gviewer_window_hide_metadata(gViewerWindow);

    gviewer_set_display_mode(gViewerWindow->priv->viewer, dispmode);
    gtk_widget_grab_focus (GTK_WIDGET (gViewerWindow->priv->viewer));

    gtk_widget_draw (GTK_WIDGET (gViewerWindow->priv->viewer), nullptr);
}


static void menu_view_set_charset(GtkAction *notUsed, GtkRadioAction *radioAction, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (gViewerWindow->priv->viewer));

    auto charsetIndex = gtk_radio_action_get_current_value (radioAction);
    auto charset = gViewerWindow->priv->encodingCharsets.at(charsetIndex);
    g_return_if_fail (charset != nullptr);

    gviewer_set_encoding(gViewerWindow->priv->viewer, charset);
    gtk_widget_draw (GTK_WIDGET (gViewerWindow->priv->viewer), nullptr);
}


static void menu_image_operation(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    ImageRender::DISPLAYMODE imageop = (ImageRender::DISPLAYMODE) GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), G_OBJ_IMAGE_OP_KEY));

    gviewer_image_operation(gViewerWindow->priv->viewer, imageop);

    gtk_widget_draw (GTK_WIDGET (gViewerWindow->priv->viewer), nullptr);
}


static void menu_view_zoom_in(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    switch (gviewer_get_display_mode(gViewerWindow->priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
            {
               int size = gviewer_get_font_size(gViewerWindow->priv->viewer);

               if (size==0 || size>32)  return;

               size++;
               gviewer_set_font_size(gViewerWindow->priv->viewer, size);
            }
            break;

        case DISP_MODE_IMAGE:
           {
               gviewer_set_best_fit(gViewerWindow->priv->viewer, FALSE);

               if (gViewerWindow->priv->current_scale_index<MAX_SCALE_FACTOR_INDEX-1)
                  gViewerWindow->priv->current_scale_index++;

               if (gviewer_get_scale_factor(gViewerWindow->priv->viewer) == image_scale_factors[gViewerWindow->priv->current_scale_index])
                  return;

               gviewer_set_scale_factor(gViewerWindow->priv->viewer, image_scale_factors[gViewerWindow->priv->current_scale_index]);
           }
           break;

        default:
            break;
        }
}


static void menu_view_zoom_out(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    switch (gviewer_get_display_mode(gViewerWindow->priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
            {
               int size = gviewer_get_font_size(gViewerWindow->priv->viewer);

               if (size < 4)  return;

               size--;
               gviewer_set_font_size(gViewerWindow->priv->viewer, size);
            }
            break;

        case DISP_MODE_IMAGE:
           gviewer_set_best_fit(gViewerWindow->priv->viewer, FALSE);

           if (gViewerWindow->priv->current_scale_index>0)
               gViewerWindow->priv->current_scale_index--;

           if (gviewer_get_scale_factor(gViewerWindow->priv->viewer) == image_scale_factors[gViewerWindow->priv->current_scale_index])
              return;

           gviewer_set_scale_factor(gViewerWindow->priv->viewer, image_scale_factors[gViewerWindow->priv->current_scale_index]);
           break;

        default:
            break;
        }
}


static void menu_view_zoom_normal(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    switch (gviewer_get_display_mode (gViewerWindow->priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
           // needs to completed with resetting to default font size
           break;

        case DISP_MODE_IMAGE:
           gviewer_set_best_fit (gViewerWindow->priv->viewer, FALSE);
           gviewer_set_scale_factor(gViewerWindow->priv->viewer, 1);
           gViewerWindow->priv->current_scale_index = 5;
           break;

        default:
            break;
    }
}


static void menu_view_zoom_best_fit(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    if (gviewer_get_display_mode(gViewerWindow->priv->viewer)==DISP_MODE_IMAGE)
        gviewer_set_best_fit(gViewerWindow->priv->viewer, TRUE);
}


static void menu_settings_binary_bytes_per_line(GtkAction *notUsed, GtkRadioAction *radioAction, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (gViewerWindow->priv->viewer));

    auto bytes_per_line = gtk_radio_action_get_current_value (radioAction);

    gviewer_set_fixed_limit(gViewerWindow->priv->viewer, bytes_per_line);
    gtk_widget_draw (GTK_WIDGET (gViewerWindow), nullptr);
}


static void menu_edit_copy(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    gviewer_copy_selection(item, gViewerWindow->priv->viewer);
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


static void menu_edit_find(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);

    // Show the Search Dialog
    GtkWidget *w = gviewer_search_dlg_new (GTK_WINDOW (gViewerWindow));
    if (gtk_dialog_run (GTK_DIALOG (w))!=GTK_RESPONSE_OK)
    {
        gtk_widget_destroy (w);
        return;
    }

    // If a previous search is active, delete it
    if (gViewerWindow->priv->srchr != nullptr)
    {
        g_object_unref (gViewerWindow->priv->srchr);
        gViewerWindow->priv->srchr = nullptr;

        g_free (gViewerWindow->priv->search_pattern);
        gViewerWindow->priv->search_pattern = nullptr;
    }

    // Get the search information from the search dialog
    GViewerSearchDlg *srch_dlg = GVIEWER_SEARCH_DLG (w);
    gViewerWindow->priv->search_pattern = gviewer_search_dlg_get_search_text_string (srch_dlg);

    // Create & prepare the search object
    gViewerWindow->priv->srchr = g_viewer_searcher_new ();

    if (gviewer_search_dlg_get_search_mode (srch_dlg)==SEARCH_MODE_TEXT)
    {
        // Text search
        g_viewer_searcher_setup_new_text_search(gViewerWindow->priv->srchr,
            text_render_get_input_mode_data(gviewer_get_text_render(gViewerWindow->priv->viewer)),
            text_render_get_current_offset(gviewer_get_text_render(gViewerWindow->priv->viewer)),
            gv_file_get_max_offset (text_render_get_file_ops (gviewer_get_text_render(gViewerWindow->priv->viewer))),
            gViewerWindow->priv->search_pattern,
            gviewer_search_dlg_get_case_sensitive(srch_dlg));
        gViewerWindow->priv->search_pattern_len = strlen(gViewerWindow->priv->search_pattern);
    }
    else
    {
        // Hex Search
        guint buflen;
        guint8 *buffer = gviewer_search_dlg_get_search_hex_buffer (srch_dlg, buflen);

        g_return_if_fail (buffer != nullptr);

        gViewerWindow->priv->search_pattern_len = buflen;
        g_viewer_searcher_setup_new_hex_search(gViewerWindow->priv->srchr,
            text_render_get_input_mode_data(gviewer_get_text_render(gViewerWindow->priv->viewer)),
            text_render_get_current_offset(gviewer_get_text_render(gViewerWindow->priv->viewer)),
            gv_file_get_max_offset (text_render_get_file_ops(gviewer_get_text_render(gViewerWindow->priv->viewer))),
            buffer, buflen);

        g_free (buffer);
    }

    gtk_widget_destroy (w);


    // call  "find_next" to actually do the search
    start_find_thread(gViewerWindow, TRUE);
}


static void menu_edit_find_next(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);

    if (!gViewerWindow->priv->srchr)
    {
        /*
            if no search is active, call "menu_edit_find".
            (which will call "menu_edit_find_next" again */
        menu_edit_find(item, gViewerWindow);
        return;
    }

    start_find_thread(gViewerWindow, TRUE);
}


static void menu_edit_find_prev(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);

    if (!gViewerWindow->priv->srchr)
        return;

    start_find_thread(gViewerWindow, FALSE);
}


static void menu_view_wrap(GtkToggleAction *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    gboolean wrap = gtk_toggle_action_get_active (item);

    gviewer_set_wrap_mode(gViewerWindow->priv->viewer, wrap);
    gtk_widget_draw (GTK_WIDGET (gViewerWindow->priv->viewer), nullptr);
}


static void menu_settings_hex_decimal_offset(GtkToggleAction *item, GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    gboolean hex = gtk_toggle_action_get_active (item);
    gviewer_set_hex_offset_display(gViewerWindow->priv->viewer, hex);
}


GViewerWindowSettings *gviewer_window_get_settings()
{
    InternalViewerSettings *iv_settings;
    iv_settings = iv_settings_new();

    auto gViewerWindowSettings = new GViewerWindowSettings();

    gchar *temp = g_settings_get_string (iv_settings->internalviewer, GCMD_GSETTINGS_IV_CHARSET);
    strncpy(gViewerWindowSettings->charset, temp, sizeof(gViewerWindowSettings->charset) - 1);
    g_free (temp);

    temp = g_settings_get_string (iv_settings->internalviewer, GCMD_SETTINGS_IV_FIXED_FONT_NAME);
    strncpy(gViewerWindowSettings->fixed_font_name, temp, sizeof(gViewerWindowSettings->fixed_font_name) - 1);
    g_free (temp);

    temp = g_settings_get_string (iv_settings->internalviewer, GCMD_SETTINGS_IV_VARIABLE_FONT_NAME);
    strncpy(gViewerWindowSettings->variable_font_name, temp, sizeof(gViewerWindowSettings->variable_font_name) - 1);
    g_free (temp);

    gViewerWindowSettings->hex_decimal_offset = g_settings_get_boolean (iv_settings->internalviewer, GCMD_SETTINGS_IV_DISPLAY_HEX_OFFSET);
    gViewerWindowSettings->wrap_mode = g_settings_get_boolean (iv_settings->internalviewer, GCMD_SETTINGS_IV_WRAP_MODE);

    gViewerWindowSettings->font_size = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_FONT_SIZE);
    gViewerWindowSettings->tab_size = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_TAB_SIZE);
    gViewerWindowSettings->binary_bytes_per_line = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_BINARY_BYTES_PER_LINE);
    gViewerWindowSettings->metadata_visible = g_settings_get_boolean (iv_settings->internalviewer, GCMD_SETTINGS_IV_METADATA_VISIBLE);

    gViewerWindowSettings->rect.x = g_settings_get_int (iv_settings->internalviewer, GCMD_SETTINGS_IV_X_OFFSET);
    gViewerWindowSettings->rect.y = g_settings_get_int (iv_settings->internalviewer, GCMD_SETTINGS_IV_Y_OFFSET);
    gViewerWindowSettings->rect.width = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_WIDTH);
    gViewerWindowSettings->rect.height = g_settings_get_uint (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_HEIGHT);

    return gViewerWindowSettings;
}


static void menu_settings_save_settings(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    GViewerWindowSettings settings;

    InternalViewerSettings *iv_settings;
    iv_settings = iv_settings_new();

    g_return_if_fail (gViewerWindow);
    g_return_if_fail (gViewerWindow->priv->viewer);

    gviewer_window_get_current_settings(gViewerWindow, &settings);

    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_GSETTINGS_IV_CHARSET, settings.charset);
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_FIXED_FONT_NAME, settings.fixed_font_name);
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_VARIABLE_FONT_NAME, settings.variable_font_name);

    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_DISPLAY_HEX_OFFSET, &(settings.hex_decimal_offset));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_WRAP_MODE, &(settings.wrap_mode));

    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_FONT_SIZE, &(settings.font_size));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_TAB_SIZE, &(settings.tab_size));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_BINARY_BYTES_PER_LINE, &(settings.binary_bytes_per_line));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_METADATA_VISIBLE, &(settings.metadata_visible));

    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_X_OFFSET, &(settings.rect.x));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_Y_OFFSET, &(settings.rect.y));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_WIDTH, &(settings.rect.width));
    gnome_cmd_data.set_gsettings_when_changed (iv_settings->internalviewer, GCMD_SETTINGS_IV_WINDOW_HEIGHT, &(settings.rect.height));
}


static void menu_help_quick_help(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-internal-viewer");
}


static void menu_help_keyboard(GtkMenuItem *item, GViewerWindow *gViewerWindow)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-internal-viewer-keyboard");
}


inline void gviewer_window_show_metadata(GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow != nullptr);
    g_return_if_fail (gViewerWindow->priv->f != nullptr);

    if (gViewerWindow->priv->metadata_visible)
        return;

    if (!gViewerWindow->priv->metadata_view)
    {
        GtkWidget *scrolledwindow = gtk_scrolled_window_new (nullptr, nullptr);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 10);
        gtk_container_add (GTK_CONTAINER (scrolledwindow), create_view ());
        gtk_box_pack_start (GTK_BOX (gViewerWindow->priv->vbox), scrolledwindow, TRUE, TRUE, 0);
        gViewerWindow->priv->metadata_view = scrolledwindow;
    }

    GtkWidget *view = gtk_bin_get_child (GTK_BIN (gViewerWindow->priv->metadata_view));
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

    if (!model)
    {
        model = create_model ();
        fill_model (GTK_TREE_STORE (model), gViewerWindow->priv->f);
        gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);
        g_object_unref (model);          // destroy model automatically with view
        gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)), GTK_SELECTION_NONE);
    }

    gViewerWindow->priv->metadata_visible = TRUE;

    gtk_widget_show_all (gViewerWindow->priv->metadata_view);
}


inline void gviewer_window_hide_metadata(GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);

    if (!gViewerWindow->priv->metadata_view)
        return;

    if (!gViewerWindow->priv->metadata_visible)
        return;

    gViewerWindow->priv->metadata_visible = FALSE;
    // gtk_container_remove (GTK_CONTAINER (obj->priv->vbox), obj->priv->metadata_view);
    gtk_widget_hide_all (gViewerWindow->priv->metadata_view);
    gtk_widget_grab_focus (GTK_WIDGET (gViewerWindow->priv->viewer));
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
