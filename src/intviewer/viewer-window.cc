/**
 * @file viewer-window.cc
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

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

#include <gtk/gtk.h>

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

GtkWidget *gviewer_window_new ()
{
    return (GtkWidget *) g_object_new (gviewer_window_get_type (), NULL);
}

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

static double image_scale_factors[] = {0.1, 0.2, 0.33, 0.5, 0.67, 1, 1.25, 1.50, 2, 3, 4, 5, 6, 7, 8};

const static int MAX_SCALE_FACTOR_INDEX = G_N_ELEMENTS(image_scale_factors);

struct GViewerWindowPrivate
{
    // Gtk User Interface
    GtkWidget *vbox;
    GViewer *viewer;
    GtkWidget *menubar;
    GtkWidget *statusbar;

    GSimpleActionGroup *action_group;
    GtkAccelGroup *accel_group;
    std::vector<const char*> encodingCharsets;
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
static void gviewer_window_dispose(GObject *widget);

static void gviewer_window_status_line_changed(GViewer *gViewer, const gchar *status_line, GViewerWindow *gViewerWindow);

static gboolean gviewer_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

static GtkWidget *gviewer_window_create_menus(GViewerWindow *gViewerWindow);

void gviewer_window_show_metadata(GViewerWindow *gViewerWindow);
void gviewer_window_hide_metadata(GViewerWindow *gViewerWindow);

gboolean gviewerwindow_get_metadata_visble(GViewerWindow *gViewerWindow);

// Event Handlers
static void menu_file_close (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void menu_toggle_view_exif_information(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void menu_edit_copy(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_edit_find(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_edit_find_next(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_edit_find_prev(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void menu_view_wrap(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_view_set_display_mode(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_view_set_charset(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_view_zoom_in(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_view_zoom_out(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_view_zoom_normal(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_view_zoom_best_fit(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void menu_image_operation(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void menu_settings_binary_bytes_per_line(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_settings_hex_decimal_offset(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_settings_save_settings(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void menu_help_quick_help(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void menu_help_keyboard(GSimpleAction *action, GVariant *parameter, gpointer user_data);

GtkTreeModel *create_model ();
void fill_model (GtkTreeStore *treestore, GnomeCmdFile *f);
GtkWidget *create_view ();


G_DEFINE_TYPE_WITH_PRIVATE (GViewerWindow, gviewer_window, GTK_TYPE_WINDOW)


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
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (GVIEWER_WINDOW (w)));

    gviewer_window_load_file (GVIEWER_WINDOW (w), f);

    gviewer_window_load_settings(GVIEWER_WINDOW (w), gViewerWindowSettings);

    priv->gViewerWindowSettings = gViewerWindowSettings;

    return w;
}


void gviewer_window_load_file (GViewerWindow *gViewerWindow, GnomeCmdFile *f)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (f != nullptr);

    g_free (priv->filename);

    priv->f = f;
    priv->filename = f->get_real_path();
    gviewer_load_file (priv->viewer, priv->filename);

    gtk_window_set_title (GTK_WINDOW (gViewerWindow), priv->filename);
}


static void gviewer_window_class_init (GViewerWindowClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = gviewer_window_dispose;
}


static void gviewer_window_init (GViewerWindow *w)
{
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (w));

    // priv->status_bar_msg = FALSE;
    // priv->filename = nullptr;
    priv->current_scale_index = 5;

    static const GActionEntry actionEntries[] =
    {
        { "close",                  menu_file_close,                nullptr,    nullptr,            nullptr },
        { "view-mode",              nullptr,                        "i",        "@i 0",             menu_view_set_display_mode },
        { "zoom-in",                menu_view_zoom_in,              nullptr,    nullptr,            nullptr },
        { "zoom-out",               menu_view_zoom_out,             nullptr,    nullptr,            nullptr },
        { "normal-size",            menu_view_zoom_normal,          nullptr,    nullptr,            nullptr },
        { "best-fit",               menu_view_zoom_best_fit,        nullptr,    nullptr,            nullptr },
        { "copy-text-selection",    menu_edit_copy,                 nullptr,    nullptr,            nullptr },
        { "find",                   menu_edit_find,                 nullptr,    nullptr,            nullptr },
        { "find-next",              menu_edit_find_next,            nullptr,    nullptr,            nullptr },
        { "find-previous",          menu_edit_find_prev,            nullptr,    nullptr,            nullptr },
        { "wrap-lines",             nullptr,                        nullptr,    "boolean true",     menu_view_wrap },
        { "encoding",               nullptr,                        "i",        "int32 0",          menu_view_set_charset },
        { "imageop",                menu_image_operation,           "i",        nullptr,            nullptr },
        { "chars-per-line",         nullptr,                        "i",        "int32 20",         menu_settings_binary_bytes_per_line },
        { "show-metadata-tags",     nullptr,                        nullptr,    "boolean false",    menu_toggle_view_exif_information },
        { "hexadecimal-offset",     nullptr,                        nullptr,    "boolean true",     menu_settings_hex_decimal_offset },
        { "save-current-settings",  menu_settings_save_settings,    nullptr,    nullptr,            nullptr },
        { "quick-help",             menu_help_quick_help,           nullptr,    nullptr,            nullptr },
        { "keyboard-shortcuts",     menu_help_keyboard,             nullptr,    nullptr,            nullptr },
    };

    priv->action_group = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (priv->action_group), actionEntries, G_N_ELEMENTS (actionEntries), w);
    gtk_widget_insert_action_group (GTK_WIDGET (w), "viewer", G_ACTION_GROUP (priv->action_group));

    GtkWindow *win = GTK_WINDOW (w);
    gtk_window_set_title (win, "GViewer");

    g_signal_connect (w, "key-press-event", G_CALLBACK (gviewer_window_key_pressed), nullptr);

    priv->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_show (priv->vbox);

    priv->menubar = gviewer_window_create_menus(w);
    gtk_widget_show (priv->menubar);
    gtk_box_pack_start (GTK_BOX (priv->vbox), priv->menubar, FALSE, FALSE, 0);

    priv->viewer = GVIEWER (gviewer_new());
    g_object_ref (priv->viewer);
    gtk_widget_show (GTK_WIDGET (priv->viewer));
    gtk_box_pack_start (GTK_BOX (priv->vbox), GTK_WIDGET (priv->viewer), TRUE, TRUE, 0);

    g_signal_connect (priv->viewer, "status-line-changed", G_CALLBACK (gviewer_window_status_line_changed), w);

    priv->statusbar = gtk_statusbar_new ();
    gtk_widget_show (priv->statusbar);
    gtk_box_pack_start (GTK_BOX (priv->vbox), priv->statusbar, FALSE, FALSE, 0);

    priv->statusbar_ctx_id  = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "info");

    gtk_widget_grab_focus (GTK_WIDGET (priv->viewer));

    gtk_container_add (GTK_CONTAINER (w), priv->vbox);

    // This list must be in the same order as the elements in priv->encodingMenuItems
    priv->encodingCharsets = { UTF8, ASCII, CP437,
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
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (w));

    if (priv->status_bar_msg)
        gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), priv->statusbar_ctx_id);

    if (status_line)
        gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar), priv->statusbar_ctx_id, status_line);

    priv->status_bar_msg = status_line != nullptr;
}


static GAction *gviewer_window_lookup_action (GViewerWindow *gViewerWindow, const gchar *action_name)
{
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));

    return g_action_map_lookup_action (G_ACTION_MAP (priv->action_group), action_name);
}


static void gviewer_window_action_change_state (GViewerWindow *gViewerWindow, const gchar *action_name, GVariant *value)
{
    auto action = gviewer_window_lookup_action (gViewerWindow, action_name);
    g_return_if_fail (action != nullptr);
    g_action_change_state (action, value);
}


void gviewer_window_load_settings(GViewerWindow *gViewerWindow, GViewerWindowSettings *settings)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    g_return_if_fail (settings != nullptr);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (priv->viewer != nullptr);

    gviewer_set_font_size(priv->viewer, settings->font_size);
    gviewer_set_tab_size(priv->viewer, settings->tab_size);

    gviewer_set_fixed_limit(priv->viewer, settings->binary_bytes_per_line);
    switch (settings->binary_bytes_per_line)
    {
        case 20:
            gviewer_window_action_change_state (gViewerWindow, "chars-per-line", g_variant_new_int32 (20));
            break;
        case 40:
            gviewer_window_action_change_state (gViewerWindow, "chars-per-line", g_variant_new_int32 (40));
            break;
        case 80:
            gviewer_window_action_change_state (gViewerWindow, "chars-per-line", g_variant_new_int32 (80));
            break;
        default:
            break;
    }

    gviewer_set_wrap_mode(priv->viewer, settings->wrap_mode);
    gviewer_window_action_change_state (gViewerWindow, "wrap-lines", g_variant_new_boolean (settings->wrap_mode));

    gviewer_set_hex_offset_display(priv->viewer, settings->hex_decimal_offset);
    gviewer_window_action_change_state (gViewerWindow, "hexadecimal-offset", g_variant_new_boolean (settings->hex_decimal_offset));

    if (settings->metadata_visible)
    {
        gviewer_window_show_metadata(gViewerWindow);
    }
    gviewer_window_action_change_state (gViewerWindow, "show-metadata-tags", g_variant_new_boolean (settings->metadata_visible));

    gviewer_set_encoding(priv->viewer, settings->charset);

    // activate the radio menu item for the selected charset in the settings
    auto availableCharsets = priv->encodingCharsets;
    auto index = 0;
    for(auto ii = availableCharsets.begin(); ii != availableCharsets.end(); ii++)
    {
        if (!g_strcmp0(availableCharsets.at(index), settings->charset))
            break;
        index++;
    }
    gviewer_window_action_change_state (gViewerWindow, "encoding", g_variant_new_int32 (index));

    gtk_window_resize(GTK_WINDOW (gViewerWindow),
        settings->rect.width, settings->rect.height);

#if 0
    // This doesn't work because the window is not shown yet
    if (gtk_widget_get_window (GTK_WIDGET (obj)))
        gdk_window_move (gtk_widget_get_window (GTK_WIDGET (obj)), settings->rect.x, settings->rect.y);
#endif
    gtk_window_set_position (GTK_WINDOW (gViewerWindow), GTK_WIN_POS_CENTER);
}


void gviewer_window_get_current_settings(GViewerWindow *obj, /* out */ GViewerWindowSettings *settings)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (obj));
    g_return_if_fail (settings != nullptr);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (obj));
    g_return_if_fail (priv->viewer != nullptr);

    memset(settings, 0, sizeof(GViewerWindowSettings));

    if (gtk_widget_get_window (GTK_WIDGET (obj)))
    {
        GtkAllocation obj_allocation;
        gtk_widget_get_allocation (GTK_WIDGET (obj), &obj_allocation);

        settings->rect.width = obj_allocation.width;
        settings->rect.height = obj_allocation.height;
        gdk_window_get_position(gtk_widget_get_window (GTK_WIDGET (obj)), &settings->rect.x, &settings->rect.y);
    }
    else
    {
        settings->rect.x = 0;
        settings->rect.y = 0;
        settings->rect.width = 100;
        settings->rect.height = 100;
    }
    settings->font_size = gviewer_get_font_size(priv->viewer);
    settings->wrap_mode = gviewer_get_wrap_mode(priv->viewer);
    settings->binary_bytes_per_line = gviewer_get_fixed_limit(priv->viewer);
    strncpy(settings->charset, gviewer_get_encoding(priv->viewer), sizeof(settings->charset) - 1);
    settings->hex_decimal_offset = gviewer_get_hex_offset_display(priv->viewer);
    settings->tab_size = gviewer_get_tab_size(priv->viewer);
    settings->metadata_visible = gviewerwindow_get_metadata_visble(obj);
}

gboolean gviewerwindow_get_metadata_visble(GViewerWindow *gViewerWindow)
{
    g_return_val_if_fail (IS_GVIEWER_WINDOW (gViewerWindow), FALSE);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));

    return priv->metadata_visible;
}


static void gviewer_window_dispose (GObject *object)
{
    g_return_if_fail (IS_GVIEWER_WINDOW (object));

    GViewerWindow *w = GVIEWER_WINDOW (object);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (w));

    g_clear_object (&priv->viewer);
    g_clear_pointer (&priv->filename, g_free);
    delete priv->gViewerWindowSettings;
    priv->gViewerWindowSettings = nullptr;

    G_OBJECT_CLASS (gviewer_window_parent_class)->dispose (object);
}


static gboolean gviewer_window_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    g_return_val_if_fail (IS_GVIEWER_WINDOW (widget), FALSE);

    GViewerWindow *w = GVIEWER_WINDOW (widget);

    switch (event->keyval)
    {
        case GDK_KEY_plus:
        case GDK_KEY_KP_Add:
        case GDK_KEY_equal:
           menu_view_zoom_in(nullptr, nullptr, w);
           return TRUE;

        case GDK_KEY_minus:
        case GDK_KEY_KP_Subtract:
           menu_view_zoom_out(nullptr, nullptr, w);
           return TRUE;

        default:
           break;
    }

    if (state_is_ctrl(event->state))
        switch (event->keyval)
        {
            case GDK_KEY_q:
            case GDK_KEY_Q:
                gtk_window_destroy (GTK_WINDOW (w));
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
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));

    auto menu = MenuBuilder()
        .with_action_group(G_ACTION_GROUP (priv->action_group))
        .submenu(_("_File"))
            .item(_("_Close"),                              "viewer.close",                    "Escape")
        .endsubmenu()
        .submenu(_("_View"))
            .item(_("_Text"),                               "viewer.view-mode(0)",             "1")
            .item(_("_Binary"),                             "viewer.view-mode(1)",             "2")
            .item(_("_Hexadecimal"),                        "viewer.view-mode(2)",             "3")
            .item(_("_Image"),                              "viewer.view-mode(3)",             "4")
            .section()
                .item(_("_Zoom In"),                        "viewer.zoom-in",                  "<Control>plus",        "zoom-in")
                .item(_("_Zoom Out"),                       "viewer.zoom-out",                 "<Control>minus",       "zoom-out")
                .item(_("_Normal Size"),                    "viewer.normal-size",              "<Control>0",           "zoom-original")
                .item(_("_Best Fit"),                       "viewer.best-fit",                 "<Control>period",      "zoom-fit-best")
            .endsection()
        .endsubmenu()
        .submenu(_("_Text"))
            .item(_("_Copy Text Selection"),                "viewer.copy-text-selection",      "<Control>C")
            .item(_("Find…"),                               "viewer.find",                     "<Control>F")
            .item(_("Find Next"),                           "viewer.find-next",                "F3")
            .item(_("Find Previous"),                       "viewer.find-previous",            "<Shift>F3")
            .section()
                .item(_("_Wrap lines"),                     "viewer.wrap-lines",               "<Control>W")
            .endsection()
            .submenu(_("_Encoding"))
                .item(_("_UTF-8"),                          "viewer.encoding(0)",              "U")
                .item(_("English (US-_ASCII)"),             "viewer.encoding(1)",              "A")
                .item(_("Terminal (CP437)"),                "viewer.encoding(2)",              "Q")
                .item(_("Arabic (ISO-8859-6)"),             "viewer.encoding(3)")
                .item(_("Arabic (Windows, CP1256)"),        "viewer.encoding(4)")
                .item(_("Arabic (Dos, CP864)"),             "viewer.encoding(5)")
                .item(_("Baltic (ISO-8859-4)"),             "viewer.encoding(6)")
                .item(_("Central European (ISO-8859-2)"),   "viewer.encoding(7)")
                .item(_("Central European (CP1250)"),       "viewer.encoding(8)")
                .item(_("Cyrillic (ISO-8859-5)"),           "viewer.encoding(9)")
                .item(_("Cyrillic (CP1251)"),               "viewer.encoding(10)")
                .item(_("Greek (ISO-8859-7)"),              "viewer.encoding(11)")
                .item(_("Greek (CP1253)"),                  "viewer.encoding(12)")
                .item(_("Hebrew (Windows, CP1255)"),        "viewer.encoding(13)")
                .item(_("Hebrew (Dos, CP862)"),             "viewer.encoding(14)")
                .item(_("Hebrew (ISO-8859-8)"),             "viewer.encoding(15)")
                .item(_("Latin 9 (ISO-8859-15)"),           "viewer.encoding(16)")
                .item(_("Maltese (ISO-8859-3)"),            "viewer.encoding(17)")
                .item(_("Turkish (ISO-8859-9)"),            "viewer.encoding(18)")
                .item(_("Turkish (CP1254)"),                "viewer.encoding(19)")
                .item(_("Western (CP1252)"),                "viewer.encoding(20)")
                .item(_("Western (ISO-8859-1)"),            "viewer.encoding(21)")
            .endsubmenu()
        .endsubmenu()
        .submenu(_("_Image"))
            .item(_("Rotate Clockwise"),                    "viewer.imageop(0)",               "<Control>R",           ROTATE_90_STOCKID)
            .item(_("Rotate Counter Clockwis_e"),           "viewer.imageop(1)",               nullptr,                ROTATE_270_STOCKID)
            .item(_("Rotate 180\xC2\xB0"),                  "viewer.imageop(2)",               "<Control><Shift>R",    ROTATE_180_STOCKID)
            .item(_("Flip _Vertical"),                      "viewer.imageop(3)",               nullptr,                FLIP_VERTICAL_STOCKID)
            .item(_("Flip _Horizontal"),                    "viewer.imageop(4)",               nullptr,                FLIP_HORIZONTAL_STOCKID)
        .endsubmenu()
        .submenu(_("_Settings"))
            .submenu(_("_Binary Mode"))
                .item(_("_20 chars/line"),                  "viewer.chars-per-line(20)",       "<Control>2")
                .item(_("_40 chars/line"),                  "viewer.chars-per-line(40)",       "<Control>4")
                .item(_("_80 chars/line"),                  "viewer.chars-per-line(80)",       "<Control>8")
            .endsubmenu()
            .section()
                .item( _("Show Metadata _Tags"),            "viewer.show-metadata-tags",       "T")
                .item( _("_Hexadecimal Offset"),            "viewer.hexadecimal-offset",       "<Control>D")
            .endsection()
            .item(_("_Save Current Settings"),              "viewer.save-current-settings",    "<Control>S")
        .endsubmenu()
        .submenu(_("_Help"))
            .item(_("Quick _Help"),                         "viewer.quick-help",               "F1")
            .item(_("_Keyboard Shortcuts"),                 "viewer.keyboard-shortcuts")
        .endsubmenu()
        .build();

    gtk_window_add_accel_group (GTK_WINDOW (gViewerWindow), menu.accel_group);

    return gtk_menu_bar_new_from_model (G_MENU_MODEL (menu.menu));
}


// Event Handlers
static void menu_file_close (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    gtk_window_destroy (GTK_WINDOW (gViewerWindow));
}


static void menu_toggle_view_exif_information(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    if (g_variant_get_boolean (state))
        gviewer_window_show_metadata(gViewerWindow);
    else
        gviewer_window_hide_metadata(gViewerWindow);

    g_simple_action_set_state (action, state);
}


static void menu_view_set_display_mode(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    auto dispmode = (VIEWERDISPLAYMODE) g_variant_get_int32 (state);

    if (dispmode == DISP_MODE_IMAGE)
    {
        GVariant *var = g_action_get_state(gviewer_window_lookup_action (gViewerWindow, "show-metadata-tags"));
        bool showMetadataTags = g_variant_get_boolean (var);
        g_variant_unref (var);

        if (showMetadataTags)
            gviewer_window_show_metadata(gViewerWindow);
        else
            gviewer_window_hide_metadata(gViewerWindow);
    }
    else
        gviewer_window_hide_metadata(gViewerWindow);

    gviewer_set_display_mode(priv->viewer, dispmode);
    gtk_widget_grab_focus (GTK_WIDGET (priv->viewer));

    gtk_widget_queue_draw (GTK_WIDGET (priv->viewer));

    g_simple_action_set_state (action, state);
}


static void menu_view_set_charset(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    auto charsetIndex = g_variant_get_int32 (state);
    auto charset = priv->encodingCharsets.at(charsetIndex);
    g_return_if_fail (charset != nullptr);

    gviewer_set_encoding(priv->viewer, charset);
    gtk_widget_queue_draw (GTK_WIDGET (priv->viewer));

    g_simple_action_set_state (action, state);
}


static void menu_image_operation(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    auto imageop = (ImageRender::DISPLAYMODE) g_variant_get_int32 (parameter);

    gviewer_image_operation(priv->viewer, imageop);

    gtk_widget_queue_draw (GTK_WIDGET (priv->viewer));
}


static void menu_view_zoom_in(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    switch (gviewer_get_display_mode(priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
            {
               int size = gviewer_get_font_size(priv->viewer);

               if (size==0 || size>32)  return;

               size++;
               gviewer_set_font_size(priv->viewer, size);
            }
            break;

        case DISP_MODE_IMAGE:
           {
               gviewer_set_best_fit(priv->viewer, FALSE);

               if (priv->current_scale_index<MAX_SCALE_FACTOR_INDEX-1)
                  priv->current_scale_index++;

               if (gviewer_get_scale_factor(priv->viewer) == image_scale_factors[priv->current_scale_index])
                  return;

               gviewer_set_scale_factor(priv->viewer, image_scale_factors[priv->current_scale_index]);
           }
           break;

        default:
            break;
        }
}


static void menu_view_zoom_out(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    switch (gviewer_get_display_mode(priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
            {
               int size = gviewer_get_font_size(priv->viewer);

               if (size < 4)  return;

               size--;
               gviewer_set_font_size(priv->viewer, size);
            }
            break;

        case DISP_MODE_IMAGE:
           gviewer_set_best_fit(priv->viewer, FALSE);

           if (priv->current_scale_index>0)
               priv->current_scale_index--;

           if (gviewer_get_scale_factor(priv->viewer) == image_scale_factors[priv->current_scale_index])
              return;

           gviewer_set_scale_factor(priv->viewer, image_scale_factors[priv->current_scale_index]);
           break;

        default:
            break;
        }
}


static void menu_view_zoom_normal(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    switch (gviewer_get_display_mode (priv->viewer))
    {
        case DISP_MODE_TEXT_FIXED:
        case DISP_MODE_BINARY:
        case DISP_MODE_HEXDUMP:
           // needs to completed with resetting to default font size
           break;

        case DISP_MODE_IMAGE:
           gviewer_set_best_fit (priv->viewer, FALSE);
           gviewer_set_scale_factor(priv->viewer, 1);
           priv->current_scale_index = 5;
           break;

        default:
            break;
    }
}


static void menu_view_zoom_best_fit(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    if (gviewer_get_display_mode(priv->viewer)==DISP_MODE_IMAGE)
        gviewer_set_best_fit(priv->viewer, TRUE);
}


static void menu_settings_binary_bytes_per_line(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    auto bytes_per_line = g_variant_get_int32 (state);

    gviewer_set_fixed_limit(priv->viewer, bytes_per_line);
    gtk_widget_queue_draw (GTK_WIDGET (gViewerWindow));

    g_simple_action_set_state (action, state);
}


static void menu_edit_copy(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (IS_GVIEWER_WINDOW (gViewerWindow));
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (IS_GVIEWER (priv->viewer));

    gviewer_copy_selection(priv->viewer);
}


static void start_find_thread(GViewerWindow *obj, gboolean forward)
{
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (obj));

    g_viewer_searcher_start_search(priv->srchr, forward);
    gviewer_show_search_progress_dlg(GTK_WINDOW (obj),
                                     priv->search_pattern,
                                     g_viewer_searcher_get_abort_indicator(priv->srchr),
                                     g_viewer_searcher_get_complete_indicator(priv->srchr),
                                     g_viewer_searcher_get_progress_indicator(priv->srchr));

    g_viewer_searcher_join(priv->srchr);

    if (g_viewer_searcher_get_end_of_search(priv->srchr))
    {
        GtkWidget *w;

        w = gtk_message_dialog_new(GTK_WINDOW (obj), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, _("Pattern “%s” was not found"), priv->search_pattern);
        gtk_dialog_run (GTK_DIALOG (w));
        gtk_window_destroy (GTK_WINDOW (w));
    }
    else
    {
        offset_type result;

        result = g_viewer_searcher_get_search_result(priv->srchr);
        text_render_set_marker(gviewer_get_text_render(priv->viewer),
                result,
                result + (forward?1:-1) * priv->search_pattern_len);
        text_render_ensure_offset_visible(gviewer_get_text_render(priv->viewer), result);
    }
}


static void menu_edit_find(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (gViewerWindow);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));

    // Show the Search Dialog
    GtkWidget *w = gviewer_search_dlg_new (GTK_WINDOW (gViewerWindow));
    if (gtk_dialog_run (GTK_DIALOG (w))!=GTK_RESPONSE_OK)
    {
        gtk_window_destroy (GTK_WINDOW (w));
        return;
    }

    // If a previous search is active, delete it
    if (priv->srchr != nullptr)
    {
        g_object_unref (priv->srchr);
        priv->srchr = nullptr;

        g_free (priv->search_pattern);
        priv->search_pattern = nullptr;
    }

    // Get the search information from the search dialog
    GViewerSearchDlg *srch_dlg = GVIEWER_SEARCH_DLG (w);
    priv->search_pattern = gviewer_search_dlg_get_search_text_string (srch_dlg);

    // Create & prepare the search object
    priv->srchr = g_viewer_searcher_new ();

    if (gviewer_search_dlg_get_search_mode (srch_dlg)==SEARCH_MODE_TEXT)
    {
        // Text search
        g_viewer_searcher_setup_new_text_search(priv->srchr,
            text_render_get_input_mode_data(gviewer_get_text_render(priv->viewer)),
            text_render_get_current_offset(gviewer_get_text_render(priv->viewer)),
            gv_file_get_max_offset (text_render_get_file_ops (gviewer_get_text_render(priv->viewer))),
            priv->search_pattern,
            gviewer_search_dlg_get_case_sensitive(srch_dlg));
        priv->search_pattern_len = strlen(priv->search_pattern);
    }
    else
    {
        // Hex Search
        guint buflen;
        guint8 *buffer = gviewer_search_dlg_get_search_hex_buffer (srch_dlg, buflen);

        g_return_if_fail (buffer != nullptr);

        priv->search_pattern_len = buflen;
        g_viewer_searcher_setup_new_hex_search(priv->srchr,
            text_render_get_input_mode_data(gviewer_get_text_render(priv->viewer)),
            text_render_get_current_offset(gviewer_get_text_render(priv->viewer)),
            gv_file_get_max_offset (text_render_get_file_ops(gviewer_get_text_render(priv->viewer))),
            buffer, buflen);

        g_free (buffer);
    }

    gtk_window_destroy (GTK_WINDOW (w));


    // call  "find_next" to actually do the search
    start_find_thread(gViewerWindow, TRUE);
}


static void menu_edit_find_next(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (gViewerWindow);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));

    if (!priv->srchr)
    {
        /*
            if no search is active, call "menu_edit_find".
            (which will call "menu_edit_find_next" again */
        menu_edit_find(nullptr, nullptr, gViewerWindow);
        return;
    }

    start_find_thread(gViewerWindow, TRUE);
}


static void menu_edit_find_prev(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (gViewerWindow);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));

    if (!priv->srchr)
        return;

    start_find_thread(gViewerWindow, FALSE);
}


static void menu_view_wrap(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (gViewerWindow);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (priv->viewer);

    gboolean wrap = g_variant_get_boolean (state);

    gviewer_set_wrap_mode(priv->viewer, wrap);
    gtk_widget_queue_draw (GTK_WIDGET (priv->viewer));

    g_simple_action_set_state (action, state);
}


static void menu_settings_hex_decimal_offset(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (gViewerWindow);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (priv->viewer);

    gboolean hex = g_variant_get_boolean (state);
    gviewer_set_hex_offset_display(priv->viewer, hex);

    g_simple_action_set_state (action, state);
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


static void menu_settings_save_settings(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto gViewerWindow = static_cast<GViewerWindow *>(user_data);
    g_return_if_fail (gViewerWindow);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (priv->viewer);

    GViewerWindowSettings settings;

    InternalViewerSettings *iv_settings;
    iv_settings = iv_settings_new();

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


static void menu_help_quick_help(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-internal-viewer");
}


static void menu_help_keyboard(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-internal-viewer-keyboard");
}


void gviewer_window_show_metadata(GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow != nullptr);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));
    g_return_if_fail (priv->f != nullptr);

    if (priv->metadata_visible)
        return;

    if (!priv->metadata_view)
    {
        GtkWidget *scrolledwindow = gtk_scrolled_window_new (nullptr, nullptr);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 10);
        gtk_container_add (GTK_CONTAINER (scrolledwindow), create_view ());
        gtk_box_pack_start (GTK_BOX (priv->vbox), scrolledwindow, TRUE, TRUE, 0);
        priv->metadata_view = scrolledwindow;
    }

    GtkWidget *view = gtk_bin_get_child (GTK_BIN (priv->metadata_view));
    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

    if (!model)
    {
        model = create_model ();
        fill_model (GTK_TREE_STORE (model), priv->f);
        gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);
        g_object_unref (model);          // destroy model automatically with view
        gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)), GTK_SELECTION_NONE);
    }

    priv->metadata_visible = TRUE;

    gtk_widget_show_all (priv->metadata_view);
}


void gviewer_window_hide_metadata(GViewerWindow *gViewerWindow)
{
    g_return_if_fail (gViewerWindow);
    auto priv = static_cast<GViewerWindowPrivate*>(gviewer_window_get_instance_private (gViewerWindow));

    if (!priv->metadata_view)
        return;

    if (!priv->metadata_visible)
        return;

    priv->metadata_visible = FALSE;
    // gtk_container_remove (GTK_CONTAINER (obj->priv->vbox), obj->priv->metadata_view);
    gtk_widget_hide (priv->metadata_view);
    gtk_widget_grab_focus (GTK_WIDGET (priv->viewer));
}


enum
{
    COL_TAG,
    COL_TYPE,
    COL_NAME,
    COL_VALUE,
    COL_DESC,
    NUM_COLS
};


GtkTreeModel *create_model ()
{
    GtkTreeStore *tree = gtk_tree_store_new (NUM_COLS,
                                             G_TYPE_UINT,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING);
    return GTK_TREE_MODEL (tree);
}


void fill_model (GtkTreeStore *tree, GnomeCmdFile *f)
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
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Metadata namespace"));

    g_object_set (renderer,
                  "weight-set", TRUE,
                  "weight", PANGO_WEIGHT_BOLD,
                  nullptr);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_NAME, _("Name"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Tag name"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), COL_VALUE, _("Value"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Tag value"));

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, COL_DESC, _("Description"));
    gtk_widget_set_tooltip_text (gtk_tree_view_column_get_button (col), _("Metadata tag description"));

    g_object_set (renderer,
                  "foreground-set", TRUE,
                  "foreground", "DarkGray",
                  "ellipsize-set", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  nullptr);

    return view;
}
