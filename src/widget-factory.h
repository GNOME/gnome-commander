/** 
 * @file widget-factory.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#ifndef __WIDGET_FACTORY_H__
#define __WIDGET_FACTORY_H__

#include "libgcmd/libgcmd-deps.h"
#include "gnome-cmd-combo.h"

inline GnomeCmdCombo *create_clist_combo (GtkWidget *parent, gint num_cols, gint text_col, gchar **titles)
{
    GnomeCmdCombo *combo = new GnomeCmdCombo(num_cols, text_col, titles);
    g_object_ref (combo);
    g_object_set_data_full (G_OBJECT (parent), "combo", combo, (GDestroyNotify) g_object_unref);
    gtk_widget_show (*combo);

    return combo;
}

#endif // __WIDGET_FACTORY_H__
