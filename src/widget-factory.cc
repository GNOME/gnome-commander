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

#include <config.h>
#include "gnome-cmd-includes.h"
#include "text-utils.h"
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


/***********************************************************************
 *
 *  Utility funtions for creating nicely layed out widgets
 *
 **********************************************************************/

GtkWidget *create_grid (GtkWidget *parent)
{
    GtkWidget *grid = gtk_grid_new ();
    g_object_ref (grid);
    g_object_set_data_full (G_OBJECT (parent), "grid", grid, g_object_unref);
    gtk_widget_show (grid);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

    return grid;
}


GtkWidget *create_vbox (GtkWidget *parent, gboolean h, gint s)
{
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, s);
    g_object_ref (vbox);
    gtk_box_set_homogeneous (GTK_BOX (vbox), h);
    g_object_set_data_full (G_OBJECT (parent), "vbox", vbox, g_object_unref);
    gtk_widget_show (vbox);

    return vbox;
}


GtkWidget *create_hbox (GtkWidget *parent, gboolean h, gint s)
{
    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, s);
    g_object_ref (hbox);
    gtk_box_set_homogeneous (GTK_BOX (hbox), h);
    g_object_set_data_full (G_OBJECT (parent), "hbox", hbox, g_object_unref);
    gtk_widget_show (hbox);

    return hbox;
}


GtkWidget *create_label (GtkWidget *parent, const gchar *text)
{
    GtkWidget *label;

    label = gtk_label_new (text);
    g_object_ref (label);
    g_object_set_data_full (G_OBJECT (parent), "label", label, g_object_unref);
    gtk_widget_show (label);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

    return label;
}


GtkWidget *create_bold_label (GtkWidget *parent, const gchar *text)
{
    GtkWidget *label = create_label (parent, text);

    gchar *s = get_bold_text (text);
    gtk_label_set_markup (GTK_LABEL (label), s);
    g_free (s);

    return label;
}


GtkWidget *create_hsep (GtkWidget *parent)
{
    GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    g_object_ref (sep);
    g_object_set_data_full (G_OBJECT (parent), "sep", sep, g_object_unref);
    gtk_widget_show (sep);
    return sep;
}


GtkWidget *create_category (GtkWidget *parent, GtkWidget *content, const gchar *title)
{
    GtkWidget *frame = create_vbox (parent, FALSE, 0);
    GtkWidget *label = create_bold_label (parent, title);

    g_object_set_data (G_OBJECT (frame), "label", label);

    gtk_widget_set_margin_top (content, 3);
    gtk_widget_set_margin_bottom (content, 3);
    gtk_widget_set_margin_start (content, 18);
    gtk_widget_set_margin_end (content, 18);

    gtk_box_append (GTK_BOX (frame), label);
    gtk_box_append (GTK_BOX (frame), content);

    return frame;
}


GtkWidget *create_named_button_with_data (GtkWidget *parent, const gchar *label, const gchar *name, GCallback func, gpointer data)
{
    GtkWidget *w = gtk_button_new_with_mnemonic (label);

    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), name, w, g_object_unref);
    gtk_widget_show (w);
    if (func)
        g_signal_connect (w, "clicked", func, data);

    return w;
}


GtkWidget *create_entry (GtkWidget *parent, const gchar *name, const gchar *value)
{
    GtkWidget *w = gtk_entry_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), name, w, g_object_unref);
    if (value)
        gtk_editable_set_text (GTK_EDITABLE (w), value);
    gtk_widget_show (w);
    return w;
}


GtkWidget *create_check (GtkWidget *parent, const gchar *text, const gchar *name)
{
    GtkWidget *btn = gtk_check_button_new_with_label (text);
    g_object_ref (btn);
    g_object_set_data_full (G_OBJECT (parent), name, btn, g_object_unref);
    gtk_widget_show (btn);

    return btn;
}


GtkWidget *create_check_with_mnemonic (GtkWidget *parent, const gchar *text, const gchar *name)
{
    GtkWidget *btn = gtk_check_button_new_with_mnemonic (text);

    g_object_ref (btn);
    g_object_set_data_full (G_OBJECT (parent), name, btn, g_object_unref);
    gtk_widget_show (btn);

    return btn;
}


GtkWidget *create_radio (GtkWidget *parent, GtkWidget *group, const gchar *text, const gchar *name)
{
    GtkWidget *btn = gtk_check_button_new_with_label (text);
    gtk_check_button_set_group (GTK_CHECK_BUTTON (btn), GTK_CHECK_BUTTON (group));
    g_object_ref (btn);
    g_object_set_data_full (G_OBJECT (parent), name, btn, g_object_unref);
    gtk_widget_show (btn);

    return btn;
}


GtkWidget *create_spin (GtkWidget *parent, const gchar *name, gint min, gint max, gint value)
{
    GtkAdjustment *adj = GTK_ADJUSTMENT (gtk_adjustment_new (value, min, max, 1, 10, 0));
    GtkWidget *spin = gtk_spin_button_new (adj, 1, 0);
    g_object_ref (spin);
    g_object_set_data_full (G_OBJECT (parent), name, spin, g_object_unref);
    gtk_widget_show (spin);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
    return spin;
}


GtkWidget *create_scale (GtkWidget *parent, const gchar *name, gint value, gint min, gint max)
{
    GtkWidget *scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT (gtk_adjustment_new (value, min, max, 0, 0, 0)));
    g_object_ref (scale);
    g_object_set_data_full (G_OBJECT (parent), name, scale, g_object_unref);
    gtk_widget_show (scale);
    gtk_scale_set_digits (GTK_SCALE (scale), 0);

    return scale;
}


GtkWidget *create_combo_box_text (GtkWidget *parent, const gchar **items)
{
    GtkWidget *combo = gtk_combo_box_text_new ();
    g_object_ref (combo);
    g_object_set_data_full (G_OBJECT (parent), "combo", combo, g_object_unref);
    gtk_widget_show (combo);

    if (items)
        for (gint i = 0; items[i]; i++)
            gtk_combo_box_text_append_text ((GtkComboBoxText*) combo, items[i]);

    return combo;
}
