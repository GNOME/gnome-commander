/** 
 * @file libgcmd-widget-factory.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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

#ifndef __LIBGCMD_WIDGET_FACTORY_H__
#define __LIBGCMD_WIDGET_FACTORY_H__

GtkWidget *lookup_widget (GtkWidget *widget, const gchar *widget_name);

GtkWidget *create_frame (GtkWidget *parent, const gchar *text, gint spacing);

GtkWidget *create_tabframe (GtkWidget *parent);

GtkWidget *create_space_frame (GtkWidget *parent, gint space);

GtkWidget *create_table (GtkWidget *parent, gint rows, gint cols);

GtkWidget *create_vbox (GtkWidget *parent, gboolean h, gint s);

GtkWidget *create_hbox (GtkWidget *parent, gboolean h, gint s);

inline GtkWidget *create_tabvbox (GtkWidget *parent)
{
    return create_vbox (parent, FALSE, 6);
}

inline GtkWidget *create_tabhbox (GtkWidget *parent)
{
    return create_hbox (parent, FALSE, 6);
}

GtkWidget *create_label (GtkWidget *parent, const gchar *text);

GtkWidget *create_label_with_mnemonic (GtkWidget *parent, const gchar *text, GtkWidget *for_widget);

GtkWidget *create_bold_label (GtkWidget *parent, const gchar *text);

GtkWidget *create_hsep (GtkWidget *parent);

GtkWidget *create_vsep (GtkWidget *parent);

GtkWidget *create_space_hbox (GtkWidget *parent, GtkWidget *content);

GtkWidget *create_category (GtkWidget *parent, GtkWidget *content, const gchar *title);

GtkWidget *create_named_button_with_data (GtkWidget *parent, const gchar *label, const gchar *name, GCallback func, gpointer data);

inline GtkWidget *create_button_with_data (GtkWidget *parent, const gchar *label, GCallback func, gpointer data)
{
    return create_named_button_with_data (parent, label, "button", func, data);
}

inline GtkWidget *create_button (GtkWidget *parent, const gchar *label, GCallback func)
{
    return create_button_with_data (parent, label, func, parent);
}

inline GtkWidget *create_named_button (GtkWidget *parent, const gchar *label, const gchar *name, GCallback func)
{
    return create_named_button_with_data (parent, label, name, func, parent);
}

GtkWidget *create_named_stock_button_with_data (GtkWidget *parent, gconstpointer stock, const gchar *name, GCallback func, gpointer data);

inline GtkWidget *create_stock_button_with_data (GtkWidget *parent, gconstpointer stock, GCallback func, gpointer data)
{
    return create_named_stock_button_with_data (parent, stock, "button", func, data);
}

inline GtkWidget *create_named_stock_button (GtkWidget *parent, gconstpointer stock, const gchar *name, GCallback func)
{
    return create_named_stock_button_with_data (parent, stock, name, func, parent);
}

inline GtkWidget *create_stock_button (GtkWidget *parent, gconstpointer stock, GCallback func)
{
    return create_stock_button_with_data (parent, stock, func, parent);
}

GtkWidget *create_entry (GtkWidget *parent, const gchar *name, const gchar *value);

GtkWidget *create_check (GtkWidget *parent, const gchar *text, const gchar *name);

GtkWidget *create_check_with_mnemonic (GtkWidget *parent, const gchar *text, const gchar *name);

GtkWidget *create_radio (GtkWidget *parent, GSList *group, const gchar *text, const gchar *name);

GtkWidget *create_radio_with_mnemonic (GtkWidget *parent, GSList *group, gchar *text, gchar *name);

GtkWidget *create_spin (GtkWidget *parent, const gchar *name, gint min, gint max, gint value);

GtkWidget *create_color_button (GtkWidget *parent, const gchar *name);

GtkWidget *create_icon_entry (GtkWidget *parent, const gchar *name, const gchar *icon_path);

GtkWidget *create_scale (GtkWidget *parent, const gchar *name, gint value, gint min, gint max);

GtkWidget *create_file_entry (GtkWidget *parent, const gchar *name, const gchar *value);

GtkWidget *create_clist (GtkWidget *parent, const gchar *name, gint cols, gint rowh, GtkSignalFunc on_row_selected, GtkSignalFunc on_row_moved);

void create_clist_column (GtkWidget *sw, gint col, gint width, const gchar *label);

GtkWidget *create_vbuttonbox (GtkWidget *parent);

GtkWidget *create_hbuttonbox (GtkWidget *parent);

GtkWidget *create_combo (GtkWidget *parent);

GtkWidget *create_combo_new (GtkWidget *parent);

GtkWidget *create_option_menu (GtkWidget *parent, const gchar **items);

inline const gchar *get_combo_text (GtkWidget *combo)
{
    return gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (combo)->entry));
}

inline GSList *get_radio_group (GtkWidget *radio)
{
    return gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio));
}

/**
 * Wrapper function for <a href="https://developer.gnome.org/gtk3/stable/GtkTable.html#gtk-table-attach">gtk_table_attach</a> to easily add a widget to a GtkTable table.
 * \param *table A table widget.
 * \param *w The widget which should be added to the table.
 * \param x The column number to attach the left side of a child widget to.
 * \param y The row number to attach the top of a child widget to.
 * \param x_opts Used to specify the properties of the child widget when the table is resized.
 */
inline void table_add (GtkWidget *table, GtkWidget *w, gint x, gint y, GtkAttachOptions x_opts)
{
    gtk_table_attach (GTK_TABLE (table), w, x, x+1, y, y+1, x_opts, (GtkAttachOptions)0, 0, 0);
}

inline void table_add_y (GtkWidget *table, GtkWidget *w, gint x, gint y, GtkAttachOptions x_opts, GtkAttachOptions y_opts)
{
    gtk_table_attach (GTK_TABLE (table), w, x, x+1, y, y+1, x_opts, y_opts, 0, 0);
}

GtkWidget *create_pixmap (GtkWidget *parent, GdkPixmap *pm, GdkBitmap *mask);

GtkWidget *create_progress_bar (GtkWidget *parent);

GtkWidget *create_sw (GtkWidget *parent);

void progress_bar_update (GtkWidget *pbar, gint max);

const char *get_entry_text (GtkWidget *parent, const gchar *entry_name);

void create_error_dialog (const gchar *msg, ...);

#endif //__LIBGCMD_WIDGET_FACTORY_H__
