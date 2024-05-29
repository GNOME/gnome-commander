/** 
 * @file libgcmd-utils.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
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
 */

#pragma once

gchar *get_utf8 (const gchar *unknown);

inline gchar *get_bold_text (const gchar *in)
{
    gchar *escaped_text = g_markup_escape_text (in, -1);
    gchar *result = g_strdup_printf("<span weight=\"bold\">%s</span>", escaped_text);
    g_free (escaped_text);
    return result;
}

inline gchar *get_mono_text (const gchar *in)
{
    gchar *escaped_text = g_markup_escape_text (in, -1);
    gchar *result = g_strdup_printf("<span font_family=\"monospace\">%s</span>", escaped_text);
    g_free (escaped_text);
    return result;
}

inline gchar *get_bold_mono_text (const gchar *in)
{
    gchar *escaped_text = g_markup_escape_text (in, -1);
    gchar *result = g_strdup_printf("<span font_family=\"monospace\" weight=\"bold\">%s</span>", escaped_text);
    g_free (escaped_text);
    return result;
}

#if !GTK_CHECK_VERSION(4, 0, 0)
inline void gtk_window_destroy (GtkWindow* window)
{
    gtk_widget_destroy (GTK_WIDGET (window));
}

gboolean hide_on_close_handler (GtkWidget *widget, GdkEvent *event, gpointer unused);

inline void gtk_window_set_hide_on_close (GtkWindow* window, gboolean unused_assume_always_true)
{
    g_signal_connect (window, "delete-event", G_CALLBACK (hide_on_close_handler), nullptr);
}

inline void gtk_box_append (GtkBox* box, GtkWidget* child)
{
    gtk_container_add (GTK_CONTAINER (box), child);
}

inline gboolean gdk_rectangle_contains_point (const GdkRectangle *rect, int x, int y)
{
    g_return_val_if_fail (rect != NULL, FALSE);
    return x >= rect->x && x < rect->x + rect->width && y >= rect->y && y < rect->y + rect->height;
}
#endif

