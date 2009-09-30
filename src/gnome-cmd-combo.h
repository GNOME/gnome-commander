/* gtkcombo - combo widget for gtk+
 * Copyright 1997 Paolo Molaro
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

#ifndef __GNOME_CMD_COMBO_H__
#define __GNOME_CMD_COMBO_H__

#include "imageloader.h"
#include "gnome-cmd-pixmap.h"

#define GNOME_CMD_COMBO(obj)            GTK_CHECK_CAST (obj, gnome_cmd_combo_get_type (), GnomeCmdCombo)
#define GNOME_CMD_COMBO_CLASS(klass)    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_combo_get_type (), GnomeCmdComboClass)
#define GNOME_CMD_IS_COMBO(obj)         GTK_CHECK_TYPE (obj, gnome_cmd_combo_get_type ())


GtkType gnome_cmd_combo_get_type ();


struct GnomeCmdCombo
{
    GtkHBox hbox;

  public:

    operator GtkObject * ()             {  return GTK_OBJECT (this);       }

    GtkWidget *entry;
    GtkWidget *list;

  public:       //  FIXME:  change to private

    GtkWidget *button;
    GtkWidget *popup;
    GtkWidget *popwin;
    gboolean is_popped;

    guint entry_change_id;
    guint list_change_id;

    guint value_in_list:1;
    guint ok_if_empty:1;

    guint16 current_button;
    guint activate_id;

    gpointer sel_data;
    gchar *sel_text;

    gint widest_pixmap;
    gint highest_pixmap;
    gint text_col;

  public:

    void clear()                                                {  gtk_clist_clear (GTK_CLIST (list));  }
    gint append(gchar **text, gpointer data);
    gint insert(gchar **text, gpointer data);

    void set_pixmap(gint row, gint col, GnomeCmdPixmap *pixmap);

    void popup_list();

    void select_text(const gchar *text);
    void select_data(gpointer data);

    void update_style();

    gpointer get_selected_data()                                {  return sel_data;  }
    const gchar *get_selected_text()                            {  return sel_text;  }
};

GtkWidget *gnome_cmd_combo_new (gint num_cols, gint text_col, gchar **col_titles);

#endif // __GNOME_CMD_COMBO_H__
