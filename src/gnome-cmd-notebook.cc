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

using namespace std;


gboolean gtk_notebook_ext_header_allocation (GtkNotebook *notebook, GtkAllocation *allocation)
{
    if (gtk_notebook_get_n_pages (notebook) == 0)
        return FALSE;

    GtkPositionType tab_pos = gtk_notebook_get_tab_pos (notebook);
    GtkWidget *the_page;

    allocation->x = INT_MAX;
    allocation->y = INT_MAX;
    allocation->width = 0;
    allocation->height = 0;

    for (int page_num=0; (the_page = gtk_notebook_get_nth_page (notebook, page_num)); ++page_num)
    {
        GtkWidget *tab = gtk_notebook_get_tab_label (notebook, the_page);

        g_return_val_if_fail (tab != nullptr, false);

        if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
            continue;

        GtkAllocation tab_allocation;
        gtk_widget_get_allocation (tab, &tab_allocation);

        int x1 = MIN (allocation->x, tab_allocation.x);
        int y1 = MIN (allocation->y, tab_allocation.y);
        int x2 = MAX (allocation->x + allocation->width, tab_allocation.x + tab_allocation.width);
        int y2 = MAX (allocation->y + allocation->height, tab_allocation.y + tab_allocation.height);

        allocation->x = x1;
        allocation->y = y1;
        allocation->width = x2 - x1;
        allocation->height = y2 - y1;
    }

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


gint gtk_notebook_ext_find_tab_num_at_pos(GtkNotebook *notebook, gint screen_x, gint screen_y)
{
    if (gtk_notebook_get_n_pages (notebook) == 0)
        return -1;

    GtkPositionType tab_pos = gtk_notebook_get_tab_pos (notebook);
    GtkWidget *the_page;

    for (int page_num=0; (the_page = gtk_notebook_get_nth_page (notebook, page_num)); ++page_num)
    {
        GtkWidget *tab = gtk_notebook_get_tab_label (notebook, the_page);

        g_return_val_if_fail (tab!=NULL, -1);

        if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
            continue;

        gint x_root, y_root;

        gdk_window_get_origin (gtk_widget_get_window (tab), &x_root, &y_root);

        GtkAllocation tab_allocation;
        gtk_widget_get_allocation (tab, &tab_allocation);

        switch (tab_pos)
        {
            case GTK_POS_TOP:
            case GTK_POS_BOTTOM:
                {
                    gint y = screen_y - y_root - tab_allocation.y;

                    if (y < 0 || y > tab_allocation.height)
                        return -1;

                    if (screen_x <= x_root + tab_allocation.x + tab_allocation.width)
                        return page_num;
                }
                break;

            case GTK_POS_LEFT:
            case GTK_POS_RIGHT:
                {
                    gint x = screen_x - x_root - tab_allocation.x;

                    if (x < 0 || x > tab_allocation.width)
                        return -1;

                    if (screen_y <= y_root + tab_allocation.y + tab_allocation.height)
                        return page_num;
                }

                break;
            default:
                break;
        }
    }

    GtkAllocation head_allocation;
    if (gtk_notebook_ext_header_allocation (notebook, &head_allocation))
    {
        gint x_root, y_root;

        gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (notebook)), &x_root, &y_root);

        gint x = screen_x - x_root - head_allocation.x;
        gint y = screen_y - y_root - head_allocation.y;

        if (x > 0 && x < head_allocation.width && y > 0 && y < head_allocation.height)
            return -2;
    }

    return -1;
}
