/** 
 * @file gnome-cmd-notebook.h
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

#include <gtk/gtk.h>

/**
 * Computes the allocation of a header area.
 */
gboolean gtk_notebook_ext_header_allocation (GtkNotebook *notebook, GtkAllocation *allocation);

/**
 * Find index of a tab by screen coordinates (0-based).
 * Returns -2 when coordinates point to a header area but not to any of the pages.
 * Returns -1 when coordinates do not point to any page.
 */
gint gtk_notebook_ext_find_tab_num_at_pos(GtkNotebook *notebook, gint screen_x, gint screen_y);

