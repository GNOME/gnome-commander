/**
 * @file gnome-cmd-notebook.cc
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-notebook.h"
#include "utils.h"
#include "libgcmd/libgcmd-utils.h"

using namespace std;


gboolean gtk_notebook_ext_header_allocation (GtkNotebook *notebook, GtkAllocation *allocation)
{
    if (gtk_notebook_get_n_pages (notebook) == 0)
        return FALSE;

    GtkPositionType tab_pos = gtk_notebook_get_tab_pos (notebook);
    GtkWidget *the_page;

    int x1 = INT_MAX;
    int y1 = INT_MAX;
    int x2 = 0;
    int y2 = 0;
    for (int page_num=0; (the_page = gtk_notebook_get_nth_page (notebook, page_num)); ++page_num)
    {
        GtkWidget *tab = gtk_notebook_get_tab_label (notebook, the_page);

        g_return_val_if_fail (tab != nullptr, false);

        if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
            continue;

        GtkAllocation tab_allocation;
        gtk_widget_get_allocation (tab, &tab_allocation);
        gtk_widget_translate_coordinates (tab, GTK_WIDGET (notebook), 0, 0, &tab_allocation.x, &tab_allocation.y);

        x1 = MIN (x1, tab_allocation.x);
        y1 = MIN (y1, tab_allocation.y);
        x2 = MAX (x2, tab_allocation.x + tab_allocation.width);
        y2 = MAX (y2, tab_allocation.y + tab_allocation.height);
    }
    allocation->x = x1;
    allocation->y = y1;
    allocation->width = x2 - x1;
    allocation->height = y2 - y1;

    GtkAllocation notebook_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (notebook), &notebook_allocation);

    switch (tab_pos)
    {
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
            allocation->x = 0;
            allocation->width = notebook_allocation.width;
            break;

        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
            allocation->y = 0;
            allocation->height = notebook_allocation.height;
            break;
        default:
            break;
    }

    return TRUE;
}


gint gtk_notebook_ext_find_tab_num_at_pos(GtkNotebook *notebook, gint x, gint y)
{
    if (gtk_notebook_get_n_pages (notebook) == 0)
        return -1;

    GtkWidget *the_page;

    for (int page_num=0; (the_page = gtk_notebook_get_nth_page (notebook, page_num)); ++page_num)
    {
        GtkWidget *tab = gtk_notebook_get_tab_label (notebook, the_page);

        GtkAllocation tab_allocation;
        gtk_widget_get_allocation (tab, &tab_allocation);
        gtk_widget_translate_coordinates (tab, GTK_WIDGET (notebook), 0, 0, &tab_allocation.x, &tab_allocation.y);

        if (gdk_rectangle_contains_point (&tab_allocation, x, y))
            return page_num;
    }

    GtkAllocation head_allocation;
    if (gtk_notebook_ext_header_allocation (notebook, &head_allocation))
        if (gdk_rectangle_contains_point (&head_allocation, x, y))
            return -2;

    return -1;
}
