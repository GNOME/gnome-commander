/**
 * @file viewer-window.h
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

void gviewer_window_load_file (GViewerWindow *obj, GnomeCmdFile *f);

GtkWidget *gviewer_window_file_view (GnomeCmdFile *f, GViewerWindowSettings *initial_settings=NULL);

void gviewer_window_get_current_settings(GViewerWindow *obj, /* out */ GViewerWindowSettings *settings);
void gviewer_window_set_settings(GViewerWindow *obj, /*in*/ GViewerWindowSettings *settings);

void gviewer_window_load_settings(/* out */ GViewerWindowSettings *settings);
