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

/* you should access only the entry and list fields directly */
struct GnomeCmdCombo
{
    GtkHBox hbox;

    GtkWidget *entry;
    GtkWidget *button;
    GtkWidget *popup;
    GtkWidget *popwin;
    GtkWidget *list;
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
};

struct GnomeCmdComboClass
{
    GtkHBoxClass parent_class;

    void (* item_selected)     (GnomeCmdCombo *combo, gpointer data);
    void (* popwin_hidden)     (GnomeCmdCombo *combo);
};

guint gnome_cmd_combo_get_type ();

GtkWidget *gnome_cmd_combo_new (gint num_cols, gint text_col, gchar **col_titles);

void gnome_cmd_combo_clear (GnomeCmdCombo *combo);
gint gnome_cmd_combo_append (GnomeCmdCombo *combo, gchar **text, gpointer data);
gint gnome_cmd_combo_insert (GnomeCmdCombo *combo, gchar **text, gpointer data);

void gnome_cmd_combo_set_pixmap (GnomeCmdCombo *combo, gint row, gint col, GnomeCmdPixmap *pixmap);

void gnome_cmd_combo_popup_list  (GnomeCmdCombo *combo);

void gnome_cmd_combo_select_text (GnomeCmdCombo *combo, const gchar *text);
void gnome_cmd_combo_select_data (GnomeCmdCombo *combo, gpointer data);

void gnome_cmd_combo_update_style (GnomeCmdCombo *combo);

gpointer gnome_cmd_combo_get_selected_data (GnomeCmdCombo *combo);
const gchar *gnome_cmd_combo_get_selected_text (GnomeCmdCombo *combo);

#endif // __GNOME_CMD_COMBO_H__
