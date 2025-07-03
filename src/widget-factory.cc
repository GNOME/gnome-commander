/**
 * @file libgcmd-widget-factory.cc
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

#include "gnome-cmd-includes.h"
#include "widget-factory.h"


GtkWidget *lookup_widget (GtkWidget *widget, const gchar *widget_name)
{
    GtkWidget *parent, *found_widget;

    for (;;)
    {
        parent = gtk_widget_get_parent(widget);
        if (!parent)
            break;
        widget = parent;
    }

    found_widget = (GtkWidget *) g_object_get_data (G_OBJECT (widget), widget_name);
    if (!found_widget)
        g_warning ("Widget not found: %s", widget_name);
    return found_widget;
}
