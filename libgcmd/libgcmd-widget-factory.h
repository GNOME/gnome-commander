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

#ifndef __LIBGCMD_WIDGET_FACTORY_H__
#define __LIBGCMD_WIDGET_FACTORY_H__

G_BEGIN_DECLS

GtkWidget *lookup_widget (GtkWidget *widget, const gchar *widget_name);

GtkWidget *create_frame (GtkWidget *parent, const gchar *text, gint spacing);

GtkWidget *create_tabframe (GtkWidget *parent);

GtkWidget *create_space_frame (GtkWidget *parent, gint space);

GtkWidget *create_table (GtkWidget *parent, gint rows, gint cols);

GtkWidget *create_vbox (GtkWidget *parent, gboolean h, gint s);

GtkWidget *create_hbox (GtkWidget *parent, gboolean h, gint s);

GtkWidget *create_tabvbox (GtkWidget *parent);

GtkWidget *create_tabhbox (GtkWidget *parent);

GtkWidget *create_label (GtkWidget *parent, const gchar *text);

GtkWidget *create_bold_label (GtkWidget *parent, const gchar *text);

GtkWidget *create_hsep (GtkWidget *parent);

GtkWidget *create_vsep (GtkWidget *parent);

GtkWidget *create_space_hbox (GtkWidget *parent, GtkWidget *content);

GtkWidget *create_category (GtkWidget *parent, GtkWidget *content, gchar *title);

GtkWidget *create_button (GtkWidget *parent, gchar *label, GtkSignalFunc func);

GtkWidget *create_named_button (GtkWidget *parent, gchar *label, gchar *name, GtkSignalFunc func);

GtkWidget *create_button_with_data (GtkWidget *parent, const gchar *label, GtkSignalFunc func, gpointer data);

GtkWidget *create_named_button_with_data (GtkWidget *parent, const gchar *label, const gchar *name, GtkSignalFunc func, gpointer data);

GtkWidget *create_stock_button (GtkWidget *parent, gconstpointer stock, GtkSignalFunc func);

GtkWidget *create_named_stock_button (GtkWidget *parent, gconstpointer stock, gchar *name, GtkSignalFunc func);

GtkWidget *create_stock_button_with_data (GtkWidget *parent, gconstpointer stock, GtkSignalFunc func, gpointer data);

GtkWidget *create_named_stock_button_with_data (GtkWidget *parent, gconstpointer stock, gchar *name, GtkSignalFunc func, gpointer data);

GtkWidget *create_entry (GtkWidget *parent, const gchar *name, const gchar *value);

GtkWidget *create_check (GtkWidget *parent, gchar *text, gchar *name);

GtkWidget *create_radio (GtkWidget *parent, GSList *group, const gchar *text, const gchar *name);

GtkWidget *create_spin (GtkWidget *parent, gchar *name, gint min, gint max, gint value);

GtkWidget *create_color_button (GtkWidget *parent, gchar *name);

GtkWidget *create_icon_entry (GtkWidget *parent, gchar *name, const gchar *icon_path);

GtkWidget *create_scale (GtkWidget *parent, gchar *name, gint value, gint min, gint max);

GtkWidget *create_file_entry (GtkWidget *parent, gchar *name, const gchar *value);

GtkWidget *create_clist (GtkWidget *parent, gchar *name, gint cols, gint rowh, GtkSignalFunc on_row_selected, GtkSignalFunc on_row_moved);

void create_clist_column (GtkWidget *sw, gint col, gint width, gchar *label);

GtkWidget *create_vbuttonbox (GtkWidget *parent);

GtkWidget *create_hbuttonbox (GtkWidget *parent);

GtkWidget *create_combo (GtkWidget *parent);

GtkWidget *create_option_menu (GtkWidget *parent, gchar **items);

const gchar *get_combo_text (GtkWidget *combo);

GSList *get_radio_group (GtkWidget *radio);

void table_add (GtkWidget *table, GtkWidget *w, gint x, gint y, GtkAttachOptions x_opts);

void table_add_y (GtkWidget *table, GtkWidget *w, gint x, gint y, GtkAttachOptions x_opts, GtkAttachOptions y_opts);

GtkWidget *create_pixmap (GtkWidget *parent, GdkPixmap *pm, GdkBitmap *mask);

GtkWidget *create_progress_bar (GtkWidget *parent);

GtkWidget *create_sw (GtkWidget *parent);

void progress_bar_update (GtkWidget *pbar, gint max);

const char *get_entry_text (GtkWidget *parent, gchar *entry_name);

void create_error_dialog (const gchar *msg, ...);

void create_warning_dialog (const gchar *msg, ...);

G_END_DECLS

#endif //__LIBGCMD_WIDGET_FACTORY_H__
