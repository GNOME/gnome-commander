/**
 * @file searcher.h
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

#pragma once

#include "gnome-cmd-includes.h"

#define G_TYPE_VIEWERSEARCHER         (g_viewer_searcher_get_type ())
#define G_VIEWERSEARCHER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_VIEWERSEARCHER, GViewerSearcher))
#define G_VIEWERSEARCHER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_VIEWERSEARCHER, GViewerSearcherClass))
#define G_IS_VIEWERSEARCHER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_VIEWERSEARCHER))
#define G_IS_VIEWERSEARCHER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_VIEWERSEARCHER))
#define G_VIEWERSEARCHER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_VIEWERSEARCHER, GViewerSearcherClass))

struct GViewerSearcher;

extern "C" GType g_viewer_searcher_get_type();
GViewerSearcher *g_viewer_searcher_new();


extern "C" void g_viewer_searcher_setup_new_text_search(GViewerSearcher *srchr,
                 GVInputModesData *imd,
                 offset_type start_offset,
                 offset_type max_offset,
                 const gchar *text,
                 gboolean case_sensitive);

extern "C" void g_viewer_searcher_setup_new_hex_search(GViewerSearcher *srchr,
                 GVInputModesData *imd,
                 offset_type start_offset,
                 offset_type max_offset,
                 const guint8 *buffer, guint buflen);


// Search Progress is reported via `progress` callback in 0.1% increments. 0 = 0%, 1000 = 100%.
extern "C" gboolean g_viewer_searcher_search(GViewerSearcher *src, gboolean forward, GnomeCmdCallback<guint> progress, gpointer user_data);


/*
    returns the found at which the search found the string
    (only valid if "g_viewer_searcher_search" returned TRUE).
*/
extern "C" offset_type g_viewer_searcher_get_search_result(GViewerSearcher *src);


extern "C" void g_viewer_searcher_abort(GViewerSearcher *src);
