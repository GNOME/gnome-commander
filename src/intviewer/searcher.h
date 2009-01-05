/*
    LibGViewer - GTK+ File Viewer library
    Copyright (C) 2006 Assaf Gordon

    Part of
        GNOME Commander - A GNOME based file manager
        Copyright (C) 2001-2006 Marcus Bjurman
        Copyright (C) 2007-2009 Piotr Eljasiak

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

#ifndef SEARCHER_H
#define SEARCHER_H

#include <glib.h>
#include <glib-object.h>

#define G_TYPE_VIEWERSEARCHER         (g_viewer_searcher_get_type ())
#define G_VIEWERSEARCHER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_VIEWERSEARCHER, GViewerSearcher))
#define G_VIEWERSEARCHER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_VIEWERSEARCHER, GViewerSearcherClass))
#define G_IS_VIEWERSEARCHER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_VIEWERSEARCHER))
#define G_IS_VIEWERSEARCHER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_VIEWERSEARCHER))
#define G_VIEWERSEARCHER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_VIEWERSEARCHER, GViewerSearcherClass))

struct GViewerSearcherPrivate;

struct GViewerSearcher
{
    GObject parent;
    GViewerSearcherPrivate *priv;
};

struct GViewerSearcherClass
{
    GObjectClass parent_class;
    /* Add Signal Functions Here */
};

GType g_viewer_searcher_get_type();
GViewerSearcher *g_viewer_searcher_new();


void g_viewer_searcher_setup_new_text_search(GViewerSearcher *srchr,
                 GVInputModesData *imd,
                 offset_type start_offset,
                 offset_type max_offset,
                 const gchar *text,
                 gboolean case_sensitive);

void g_viewer_searcher_setup_new_hex_search(GViewerSearcher *srchr,
                 GVInputModesData *imd,
                 offset_type start_offset,
                 offset_type max_offset,
                 const guint8 *buffer, guint buflen);

/*
    call "g_viewer_searcher_start_search" to start the search thread.
    Make sure the search parameters have been configured BEFORE starting the thread.
    Remember to 'join' the search thread wih "g_viewer_searcher_join"
*/
void g_viewer_searcher_start_search(GViewerSearcher *src, gboolean forward);

void g_viewer_searcher_join(GViewerSearcher *src);


/*
    returns TRUE if the search has reached the last (or first if searching backwards) offset.
    should be called ONLY after the search thread is done (ensure it with g_viewer_searcher_join)
*/
gboolean g_viewer_searcher_get_end_of_search(GViewerSearcher *src);

/*
    returns the found at which the search found the string
    (only valid if "g_viewer_searcher_get_end_of_search" returned FALSE).
    should be called ONLY after the search thread is done (ensure it with g_viewer_searcher_join)
*/
offset_type g_viewer_searcher_get_search_result(GViewerSearcher *src);


/* Search Progress, in 0.1% increments.
   0 = 0%,  1000 = 100%.
   READ ONLY (value is set by search thread).
   Returns pointer to an atomic gint.

   To read the value, use "g_atomic_int_get"
   (read glib's "atomic operations").
   */
gint *g_viewer_searcher_get_progress_indicator (GViewerSearcher *src);

/* Abort Indicator.
   While the search thread is active, set this value to non-zero
   (using "g_atomic_int_inc" or "g_atomic_int_add") to request the
   thread to abort the search.

   Returns pointer to an atomic gint.

   The search thread only READs this value, never WRITEs it.
   */
gint * g_viewer_searcher_get_abort_indicator(GViewerSearcher *src);

/* Search Completed Indicator.
   If the search thread is stopped (either because a match is found or
   the search reached the end of the file), this gint will be set to non-zero.

   Use this value in your UI, while the search thread is active.

   Returns pointer to an atomic gint.
   To read the value, use "g_atomic_int_get"
   (read glib's "atomic operations").
   */
gint * g_viewer_searcher_get_complete_indicator(GViewerSearcher *src);

#endif /* SEARCHER_H */
