/**
 * @file gnome-cmd-combo.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Modified by Marcus Bjurman <marbj499@student.liu.se> 2001
 * The orginal comments are left intact above
 */

#pragma once

#include "imageloader.h"

#define GNOME_CMD_TYPE_COMBO              (gnome_cmd_combo_get_type ())
#define GNOME_CMD_COMBO(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_COMBO, GnomeCmdCombo))
#define GNOME_CMD_COMBO_CLASS(vtable)     (G_TYPE_CHECK_CLASS_CAST ((vtable), GNOME_CMD_TYPE_COMBO, GnomeCmdComboClass))
#define GNOME_CMD_IS_COMBO(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_CMD_TYPE_COMBO))
#define GNOME_CMD_IS_COMBO_CLASS(vtable)  (G_TYPE_CHECK_CLASS_TYPE ((vtable), GNOME_CMD_TYPE_COMBO))
#define GNOME_CMD_COMBO_GET_CLASS(inst)   (G_TYPE_INSTANCE_GET_CLASS ((inst), GNOME_CMD_TYPE_COMBO, GnomeCmdComboClass))

struct GnomeCmdComboClass;
struct GnomeCmdComboPrivate;

struct GnomeCmdCombo
{
    GtkComboBox parent_instance;

    GnomeCmdComboPrivate *priv;

  public:

    operator GObject * () const         {  return G_OBJECT (this);         }
    operator GtkWidget * () const       {  return GTK_WIDGET (this);       }

    GtkWidget * get_entry();

    void clear();
    void append(gchar *text, gpointer data, ...); // ... = pairs of column number and value, terminated with -1

    void popup_list();

    void select_data(gpointer data);

    void update_style();
};

GType gnome_cmd_combo_get_type (void) G_GNUC_CONST;
GtkWidget *gnome_cmd_combo_new_with_store (GtkListStore *store, gint num_cols, gint text_col, gint data_col);

