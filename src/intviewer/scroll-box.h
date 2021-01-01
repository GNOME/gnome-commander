/**
 * @file scroll-box.h
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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

#define SCROLL_BOX(obj)          GTK_CHECK_CAST (obj, scroll_box_get_type (), ScrollBox)
#define SCROLL_BOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, scroll_box_get_type (), ScrollBoxClass)
#define IS_SCROLL_BOX(obj)       GTK_CHECK_TYPE (obj, scroll_box_get_type ())

struct ScrollBoxPrivate;

struct ScrollBox
{
    GtkTable table;
    ScrollBoxPrivate *priv;
};

struct ScrollBoxClass
{
    GtkTableClass parent_class;
};

GtkWidget     *scroll_box_new ();
GtkType        scroll_box_get_type ();
void           scroll_box_set_client (ScrollBox *obj, GtkWidget *client);
GtkWidget     *scroll_box_get_client (ScrollBox *obj);

GtkAdjustment *scroll_box_get_h_adjustment (ScrollBox *obj);
void           scroll_box_set_h_adjustment (ScrollBox *obj, GtkAdjustment *adjustment);
GtkAdjustment *scroll_box_get_v_adjustment (ScrollBox *obj);
void           scroll_box_set_v_adjustment (ScrollBox *obj, GtkAdjustment *adjustment);

GtkRange      *scroll_box_get_v_range(ScrollBox *obj);
