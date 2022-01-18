/** 
 * @file gnome-cmd-treeview.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2022 Uwe Scholz\n
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

GtkTreeViewColumn *gnome_cmd_treeview_create_new_text_column (GtkTreeView *view, GtkCellRenderer *&renderer, gint COL_ID, const gchar *title=NULL);
GtkTreeViewColumn *gnome_cmd_treeview_create_new_pixbuf_column (GtkTreeView *view, GtkCellRenderer *&renderer, gint COL_ID, const gchar *title=NULL);
GtkTreeViewColumn *gnome_cmd_treeview_create_new_toggle_column (GtkTreeView *view, GtkCellRenderer *&renderer, gint COL_ID, const gchar *title=NULL);

inline GtkTreeViewColumn *gnome_cmd_treeview_create_new_text_column (GtkTreeView *view, gint COL_ID, const gchar *title=NULL)
{
    GtkCellRenderer *renderer = NULL;

    return gnome_cmd_treeview_create_new_text_column (view, renderer, COL_ID, title);
}
