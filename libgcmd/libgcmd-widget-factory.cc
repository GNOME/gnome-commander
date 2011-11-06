/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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

#include <config.h>
#include "libgcmd-deps.h"
#include "libgcmd-utils.h"
#include "libgcmd-widget-factory.h"
#include "gnome-cmd-dialog.h"


GtkWidget *lookup_widget (GtkWidget *widget, const gchar *widget_name)
{
    GtkWidget *parent, *found_widget;

    for (;;)
    {
        parent = GTK_IS_MENU (widget) ? gtk_menu_get_attach_widget (GTK_MENU (widget)) : widget->parent;
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

/**
 * This type of frame is the first thing packed in each tab
 *
 */
GtkWidget *create_frame (GtkWidget *parent, const gchar *text, gint spacing)
{
    GtkWidget *frame = gtk_frame_new (text);
    g_object_ref (frame);
    g_object_set_data_full (G_OBJECT (parent), "spaced_frame", frame, g_object_unref);
    gtk_container_set_border_width (GTK_CONTAINER (frame), spacing);
    gtk_widget_show (frame);
    return frame;
}


GtkWidget *create_tabframe (GtkWidget *parent)
{
    GtkWidget *frame = create_frame (parent, "", 6);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    return frame;
}


GtkWidget *create_space_frame (GtkWidget *parent, gint space)
{
    GtkWidget *frame = create_frame (parent, NULL, space);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    return frame;
}


GtkWidget *create_table (GtkWidget *parent, gint rows, gint cols)
{
    GtkWidget *table = gtk_table_new (rows, cols, FALSE);
    g_object_ref (table);
    g_object_set_data_full (G_OBJECT (parent), "table", table, g_object_unref);
    gtk_widget_show (table);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);

    return table;
}


GtkWidget *create_vbox (GtkWidget *parent, gboolean h, gint s)
{
    GtkWidget *vbox = gtk_vbox_new (h, s);
    g_object_ref (vbox);
    g_object_set_data_full (G_OBJECT (parent), "vbox", vbox, g_object_unref);
    gtk_widget_show (vbox);

    return vbox;
}


GtkWidget *create_hbox (GtkWidget *parent, gboolean h, gint s)
{
    GtkWidget *hbox = gtk_hbox_new (h, s);
    g_object_ref (hbox);
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
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    return label;
}


GtkWidget *create_label_with_mnemonic (GtkWidget *parent, const gchar *text, GtkWidget *for_widget)
{
    GtkWidget *label = gtk_label_new_with_mnemonic (text);

    if (for_widget)
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), for_widget);

    g_object_ref (label);
    g_object_set_data_full (G_OBJECT (parent), "label", label, g_object_unref);
    gtk_widget_show (label);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

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
    GtkWidget *sep = gtk_hseparator_new ();
    g_object_ref (sep);
    g_object_set_data_full (G_OBJECT (parent), "sep", sep, g_object_unref);
    gtk_widget_show (sep);
    return sep;
}


GtkWidget *create_vsep (GtkWidget *parent)
{
    GtkWidget *sep = gtk_vseparator_new ();
    g_object_ref (sep);
    g_object_set_data_full (G_OBJECT (parent), "sep", sep, g_object_unref);
    gtk_widget_show (sep);
    return sep;
}


GtkWidget *create_space_hbox (GtkWidget *parent, GtkWidget *content)
{
    GtkWidget *hbox = create_hbox (parent, FALSE, 0);
    GtkWidget *lbl = create_label (parent, "    ");

    gtk_box_pack_start (GTK_BOX (hbox), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), content, TRUE, TRUE, 0);

    return hbox;
}


GtkWidget *create_category (GtkWidget *parent, GtkWidget *content, gchar *title)
{
    GtkWidget *frame = create_vbox (parent, FALSE, 0);
    GtkWidget *label = create_bold_label (parent, title);
    GtkWidget *hbox = create_space_hbox (parent, content);
    GtkWidget *inner_frame = create_space_frame (parent, 3);

    g_object_set_data (G_OBJECT (frame), "label", label);

    gtk_box_pack_start (GTK_BOX (frame), label, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (frame), inner_frame, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (inner_frame), hbox);

    return frame;
}


GtkWidget *create_named_button_with_data (GtkWidget *parent, const gchar *label, const gchar *name, GCallback func, gpointer data)
{
    guint key;
    GtkAccelGroup *accel_group = gtk_accel_group_new ();
    GtkWidget *w = gtk_button_new_with_label ("");

    key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (w)->child), label);
    gtk_widget_add_accelerator (w, "clicked", accel_group, key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
    gtk_window_add_accel_group (GTK_WINDOW (parent), accel_group);
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), name, w, g_object_unref);
    gtk_widget_show (w);
    if (func)
        g_signal_connect (w, "clicked", func, data);

    return w;
}


GtkWidget *create_named_stock_button_with_data (GtkWidget *parent, gconstpointer stock, const gchar *name, GCallback func, gpointer data)
{
    GtkWidget *w = gtk_button_new_from_stock ((const gchar *) stock);
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
        gtk_entry_set_text (GTK_ENTRY (w), value);
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


GtkWidget *create_radio (GtkWidget *parent, GSList *group, const gchar *text, const gchar *name)
{
    GtkWidget *btn = gtk_radio_button_new_with_label (group, text);
    g_object_ref (btn);
    g_object_set_data_full (G_OBJECT (parent), name, btn, g_object_unref);
    gtk_widget_show (btn);

    return btn;
}


GtkWidget *create_radio_with_mnemonic (GtkWidget *parent, GSList *group, gchar *text, gchar *name)
{
    GtkWidget *radio = gtk_radio_button_new_with_mnemonic (group, text);

    g_object_ref (radio);
    g_object_set_data_full (G_OBJECT (parent), name, radio, g_object_unref);
    gtk_widget_show (radio);

    return radio;
}


GtkWidget *create_spin (GtkWidget *parent, const gchar *name, gint min, gint max, gint value)
{
    GtkObject *adj = gtk_adjustment_new (value, min, max, 1, 10, 0);
    GtkWidget *spin = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
    g_object_ref (spin);
    g_object_set_data_full (G_OBJECT (parent), name, spin, g_object_unref);
    gtk_widget_show (spin);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);

    return spin;
}


GtkWidget *create_color_button (GtkWidget *parent, const gchar *name)
{
    GtkWidget *w = gtk_color_button_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), name, w, g_object_unref);
    gtk_widget_show (w);
    return w;
}


GtkWidget *create_icon_entry (GtkWidget *parent, const gchar *name, const gchar *icon_path)
{
    GtkWidget *icon_entry = gnome_icon_entry_new (NULL, NULL);
    g_object_ref (icon_entry);
    g_object_set_data_full (G_OBJECT (parent), name, icon_entry, g_object_unref);
    gtk_widget_show (icon_entry);
    if (icon_path)
        gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (icon_entry), icon_path);
    return icon_entry;
}


GtkWidget *create_scale (GtkWidget *parent, const gchar *name, gint value, gint min, gint max)
{
    GtkWidget *scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (value, min, max, 0, 0, 0)));

    g_object_ref (scale);
    g_object_set_data_full (G_OBJECT (parent), name, scale, g_object_unref);
    gtk_widget_show (scale);
    gtk_scale_set_digits (GTK_SCALE (scale), 0);

    return scale;
}


GtkWidget *create_file_entry (GtkWidget *parent, const gchar *name, const gchar *value)
{
    GtkWidget *fentry, *entry;

    fentry = gnome_file_entry_new (NULL, NULL);
    g_object_ref (fentry);
    g_object_set_data_full (G_OBJECT (parent), "fileentry", fentry, g_object_unref);
    gtk_widget_show (fentry);

    entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fentry));
    g_object_ref (entry);
    g_object_set_data_full (G_OBJECT (parent), name, entry, g_object_unref);
    if (value)
        gtk_entry_set_text (GTK_ENTRY (entry), value);
    gtk_widget_show (entry);

    return fentry;
}


GtkWidget *create_clist (GtkWidget *parent, const gchar *name, gint cols, gint rowh, GtkSignalFunc on_row_selected, GtkSignalFunc on_row_moved)
{
    GtkWidget *sw, *clist;

    sw = gtk_scrolled_window_new (NULL, NULL);
    g_object_ref (sw);
    g_object_set_data_full (G_OBJECT (parent), "sw", sw, g_object_unref);
    gtk_widget_show (sw);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    clist = gtk_clist_new (cols);
    g_object_ref (clist);
    g_object_set_data (G_OBJECT (sw), "clist", clist);
    g_object_set_data_full (G_OBJECT (parent), name, clist, g_object_unref);
    gtk_widget_show (clist);
    gtk_clist_set_row_height (GTK_CLIST (clist), rowh);
    gtk_container_add (GTK_CONTAINER (sw), clist);
    gtk_clist_column_titles_show (GTK_CLIST (clist));

    if (on_row_selected)
        g_signal_connect (clist, "select-row", G_CALLBACK (on_row_selected), parent);
    if (on_row_moved)
        g_signal_connect (clist, "row-move", G_CALLBACK (on_row_moved), parent);
    return sw;
}


void create_clist_column (GtkWidget *sw, gint col, gint width, const gchar *label)
{
    GtkWidget *clist = (GtkWidget *) g_object_get_data (G_OBJECT (sw), "clist");
    gtk_clist_set_column_width (GTK_CLIST (clist), col, width);
    gtk_clist_set_column_title (GTK_CLIST (clist), col, label);
}


GtkWidget *create_vbuttonbox (GtkWidget *parent)
{
    GtkWidget *w = gtk_vbutton_box_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), "vbuttonbox", w, g_object_unref);
    gtk_widget_show (w);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (w), GTK_BUTTONBOX_START);
    gtk_box_set_spacing (GTK_BOX (w), 12);
    return w;
}


GtkWidget *create_hbuttonbox (GtkWidget *parent)
{
    GtkWidget *w = gtk_hbutton_box_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), "hbuttonbox", w, g_object_unref);
    gtk_widget_show (w);
    gtk_box_set_spacing (GTK_BOX (w), 12);
    return w;
}


GtkWidget *create_combo (GtkWidget *parent)
{
    GtkWidget *combo = gtk_combo_new ();
    g_object_ref (combo);
    g_object_set_data_full (G_OBJECT (parent), "combo", combo, g_object_unref);
    gtk_widget_show (combo);
    return combo;
}


GtkWidget *create_option_menu (GtkWidget *parent, const gchar **items)
{
    GtkWidget *optmenu = gtk_option_menu_new ();
    g_object_ref (optmenu);
    g_object_set_data_full (G_OBJECT (parent), "optmenu", optmenu, g_object_unref);
    gtk_widget_show (optmenu);

    GtkWidget *menu = gtk_menu_new ();
    gtk_widget_show (menu);

    for (gint i = 0; items[i]; i++)
    {
        GtkWidget *item = gtk_menu_item_new_with_label (items[i]);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu), menu);

    return optmenu;
}


GtkWidget *create_progress_bar (GtkWidget *parent)
{
    GtkWidget *w = gtk_progress_bar_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), "progress_bar", w, g_object_unref);
    gtk_widget_show (w);
    gtk_progress_set_show_text (GTK_PROGRESS (w), TRUE);
    return w;
}


GtkWidget *create_pixmap (GtkWidget *parent, GdkPixmap *pm, GdkBitmap *mask)
{
    GtkWidget *w = gtk_pixmap_new (pm, mask);
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), "pixmap", w, g_object_unref);
    gtk_widget_show (w);
    return w;
}


GtkWidget *create_sw (GtkWidget *parent)
{
    GtkWidget *scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
    g_object_ref (scrolledwindow);
    g_object_set_data_full (G_OBJECT (parent), "scrolledwindow", scrolledwindow, g_object_unref);
    gtk_widget_show (scrolledwindow);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    return scrolledwindow;
}


void progress_bar_update (GtkWidget *pbar, gint max)
{
    gint value = (gint)gtk_progress_get_value (GTK_PROGRESS (pbar)) + 1;

    if (value >= max) value = 0;

    gtk_progress_set_value (GTK_PROGRESS (pbar), (gfloat)value);
}


const char *get_entry_text (GtkWidget *parent, const gchar *entry_name)
{
    GtkWidget *entry = lookup_widget (parent, entry_name);
    if (!entry) return NULL;
    if (!GTK_IS_ENTRY (entry)) return NULL;

    return gtk_entry_get_text (GTK_ENTRY (entry));
}


static void on_response (GtkDialog *dialog, gint id, gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


void create_error_dialog (const gchar *msg, ...)
{
    va_list      argptr;
    gchar        string[1024];
    GtkWidget    *dialog;

    va_start (argptr, msg);
    vsprintf (string, msg, argptr);
    va_end (argptr);

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_win_widget), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, string);

    g_signal_connect (dialog, "response", G_CALLBACK (on_response), dialog);

    gtk_widget_show (dialog);
}
