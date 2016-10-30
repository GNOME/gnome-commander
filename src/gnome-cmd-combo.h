/** 
 * @file gnome-cmd-combo.h
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

#define GNOME_CMD_TYPE_COMBO              (gnome_cmd_combo_get_type ())
#define GNOME_CMD_COMBO(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_COMBO, GnomeCmdCombo))
#define GNOME_CMD_COMBO_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_COMBO, GnomeCmdComboClass))
#define GNOME_CMD_IS_COMBO(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_COMBO))
#define GNOME_CMD_IS_COMBO_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_COMBO))
#define GNOME_CMD_COMBO_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_COMBO, GnomeCmdComboClass))


GtkType gnome_cmd_combo_get_type ();


struct GnomeCmdCombo
{
    GtkHBox hbox;

  public:

    operator GObject * () const         {  return G_OBJECT (this);         }
    operator GtkObject * () const       {  return GTK_OBJECT (this);       }
    operator GtkWidget * () const       {  return GTK_WIDGET (this);       }

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

    void *operator new (size_t size);
    void operator delete (void *p)      {  g_object_unref (p);  }

    GnomeCmdCombo(gint num_cols, gint text_col, gchar **col_titles=NULL);
    ~GnomeCmdCombo()                    {}

    void clear()                                                {  gtk_clist_clear (GTK_CLIST (list));  }
    gint append(gchar **text, gpointer data);
    gint insert(gchar **text, gpointer data);

    void set_pixmap(gint row, gint col, GnomeCmdPixmap *pixmap);

    void popup_list();

    void select_data(gpointer data);

    void update_style();

    gpointer get_selected_data() const                          {  return sel_data;  }
    const gchar *get_selected_text() const                      {  return sel_text;  }
};

inline void *GnomeCmdCombo::operator new (size_t size)
{
    return g_object_new (GNOME_CMD_TYPE_COMBO, NULL);
}

#endif // __GNOME_CMD_COMBO_H__
