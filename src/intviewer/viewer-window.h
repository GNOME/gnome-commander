/**
 * @file viewer-window.h
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#pragma once

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file.h"

#define INTERNAL_VIEWER_SETTINGS (iv_settings_get_type ())
G_DECLARE_FINAL_TYPE (InternalViewerSettings, iv_settings, GCMD_IV, SETTINGS, GObject)
InternalViewerSettings *iv_settings_new (void);

#define GVIEWER_WINDOW(obj) \
    GTK_CHECK_CAST (obj, gviewer_window_get_type (), GViewerWindow)
#define GVIEWER_WINDOW_CLASS(clss) \
    GTK_CHECK_CLASS_CAST (clss, gviewer_window_get_type (), GViewerWindowClass)
#define IS_GVIEWER_WINDOW(obj) \
    GTK_CHECK_TYPE (obj, gviewer_window_get_type ())

#define UTF8      "UTF8"           // UTF-8
#define ASCII     "ASCII"          // English (US-ASCII)
#define CP437     "CP437"          // Terminal (CP437)
#define ISO88596  "ISO-8859-6"     // Arabic (ISO-8859-6)
#define ARABIC    "ARABIC"         // Arabic (Windows, CP1256)
#define CP864     "CP864"          // Arabic (Dos, CP864)
#define ISO88594  "ISO-8859-4"     // Baltic (ISO-8859-4)
#define ISO88592  "ISO-8859-2"     // Central European (ISO-8859-2)
#define CP1250    "CP1250"         // Central European (CP1250)
#define ISO88595  "ISO-8859-5"     // Cyrillic (ISO-8859-5)
#define CP1251    "CP1251"         // Cyrillic (CP1251)
#define ISO88597  "ISO-8859-7"     // Greek (ISO-8859-7)
#define CP1253    "CP1253"         // Greek (CP1253)
#define HEBREW    "HEBREW"         // Hebrew (Windows, CP1255)
#define CP862     "CP862"          // Hebrew (Dos, CP862)
#define ISO88598  "ISO-8859-8"     // Hebrew (ISO-8859-8)
#define ISO885915 "ISO-8859-15"    // Latin 9 (ISO-8859-15))
#define ISO88593  "ISO-8859-3"     // Maltese (ISO-8859-3)
#define ISO88599  "ISO-8859-9"     // Turkish (ISO-8859-9)
#define CP1254    "CP1254"         // Turkish (CP1254)
#define CP1252    "CP1252"         // Western (CP1252)
#define ISO88591  "ISO-8859-1"     // Western (ISO-8859-1)

struct GViewerWindowSettings
{
    GdkRectangle rect;

    gchar fixed_font_name[256];
    gchar variable_font_name[256];
    gchar charset[256];

    guint font_size;
    guint tab_size;
    guint binary_bytes_per_line;

    gboolean wrap_mode;
    gboolean hex_decimal_offset;
    gboolean metadata_visible;
};


struct GViewerWindowPrivate;


struct GViewerWindow
{
    GtkWindow parent;

    GViewerWindowPrivate *priv;
};


struct GViewerWindowClass
{
    GtkWindowClass parent_class;
};


GtkType gviewer_window_get_type ();

inline GtkWidget *gviewer_window_new ()
{
    return (GtkWidget *) g_object_new (gviewer_window_get_type (), NULL);
}

void gviewer_window_load_file (GViewerWindow *gViewerWindow, GnomeCmdFile *f);

GtkWidget *gviewer_window_file_view (GnomeCmdFile *f, GViewerWindowSettings *initial_settings=NULL);

void gviewer_window_get_current_settings(GViewerWindow *obj, /* out */ GViewerWindowSettings *settings);

GViewerWindowSettings *gviewer_window_get_settings();
void gviewer_window_load_settings(GViewerWindow *obj, /*in*/ GViewerWindowSettings *settings);

void gviewer_window_load_settings(/* out */ GViewerWindowSettings *settings);
