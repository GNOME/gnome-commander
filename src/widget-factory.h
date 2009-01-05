/*
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

#ifndef __WIDGET_FACTORY_H__
#define __WIDGET_FACTORY_H__

#include "libgcmd/libgcmd-deps.h"
#include "gnome-cmd-combo.h"

inline GtkWidget *create_clist_combo (GtkWidget *parent, gint num_cols, gint text_col, gchar **titles)
{
    GtkWidget *combo = gnome_cmd_combo_new (num_cols, text_col, titles);
    gtk_widget_ref (combo);
    gtk_object_set_data_full (GTK_OBJECT (parent), "combo", combo, (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (combo);

    return combo;
}

#endif // __WIDGET_FACTORY_H__
