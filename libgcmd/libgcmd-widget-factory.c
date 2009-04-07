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

#include <config.h>
#include "libgcmd-deps.h"
#include "libgcmd-utils.h"
#include "libgcmd-widget-factory.h"
#include "gnome-cmd-dialog.h"


GtkWidget*
lookup_widget                          (GtkWidget       *widget,
                                        const gchar     *widget_name)
{
    GtkWidget *parent, *found_widget;

    for (;;)
    {
        if (GTK_IS_MENU (widget))
            parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
        else
            parent = widget->parent;
        if (parent == NULL)
            break;
        widget = parent;
    }

    found_widget = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (widget), widget_name);
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
GtkWidget *
create_frame (GtkWidget *parent, const gchar *text, gint spacing)
{
    GtkWidget *frame = gtk_frame_new (text);
    gtk_widget_ref (frame);
    gtk_object_set_data_full (GTK_OBJECT (parent), "spaced_frame", frame,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_container_set_border_width (GTK_CONTAINER (frame), spacing);
    gtk_widget_show (frame);
    return frame;
}


GtkWidget *
create_tabframe (GtkWidget *parent)
{
    GtkWidget *frame = create_frame (parent, "", 6);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    return frame;
}


GtkWidget *
create_space_frame (GtkWidget *parent, gint space)
{
    GtkWidget *frame = create_frame (parent, NULL, space);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    return frame;
}


GtkWidget *
create_table (GtkWidget *parent, gint rows, gint cols)
{
    GtkWidget *table = gtk_table_new (rows, cols, FALSE);
    gtk_widget_ref (table);
    gtk_object_set_data_full (GTK_OBJECT (parent), "table", table,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (table);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);

    return table;
}


GtkWidget *
create_vbox (GtkWidget *parent, gboolean h, gint s)
{
    GtkWidget *vbox = gtk_vbox_new (h, s);
    gtk_widget_ref (vbox);
    gtk_object_set_data_full (GTK_OBJECT (parent), "vbox", vbox,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (vbox);

    return vbox;
}


GtkWidget *
create_hbox (GtkWidget *parent, gboolean h, gint s)
{
    GtkWidget *hbox = gtk_hbox_new (h, s);
    gtk_widget_ref (hbox);
    gtk_object_set_data_full (GTK_OBJECT (parent), "hbox", hbox,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (hbox);

    return hbox;
}


GtkWidget *
create_tabvbox (GtkWidget *parent)
{
    return create_vbox (parent, FALSE, 6);
}


GtkWidget *
create_tabhbox (GtkWidget *parent)
{
    return create_hbox (parent, FALSE, 6);
}


GtkWidget *
create_label (GtkWidget *parent, const gchar *text)
{
    GtkWidget *label;

    label = gtk_label_new (text);
    gtk_widget_ref (label);
    gtk_object_set_data_full (GTK_OBJECT (parent), "label", label,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (label);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    return label;
}


GtkWidget *
create_bold_label (GtkWidget *parent, const gchar *text)
{
    GtkWidget *label = create_label (parent, text);

    gchar *s = get_bold_text (text);
    gtk_label_set_markup (GTK_LABEL (label), s);
    g_free (s);

    return label;
}


GtkWidget *
create_hsep (GtkWidget *parent)
{
    GtkWidget *sep = gtk_hseparator_new ();
    gtk_widget_ref (sep);
    gtk_object_set_data_full (GTK_OBJECT (parent), "sep", sep,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (sep);
    return sep;
}


GtkWidget *
create_vsep (GtkWidget *parent)
{
    GtkWidget *sep = gtk_vseparator_new ();
    gtk_widget_ref (sep);
    gtk_object_set_data_full (GTK_OBJECT (parent), "sep", sep,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (sep);
    return sep;
}


GtkWidget *
create_space_hbox (GtkWidget *parent, GtkWidget *content)
{
    GtkWidget *hbox = create_hbox (parent, FALSE, 0);
    GtkWidget *lbl = create_label (parent, "    ");

    gtk_box_pack_start (GTK_BOX (hbox), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), content, TRUE, TRUE, 0);

    return hbox;
}


GtkWidget *
create_category (GtkWidget *parent, GtkWidget *content, gchar *title)
{
    GtkWidget *frame = create_vbox (parent, FALSE, 0);
    GtkWidget *label = create_bold_label (parent, title);
    GtkWidget *hbox = create_space_hbox (parent, content);
    GtkWidget *inner_frame = create_space_frame (parent, 3);

    gtk_object_set_data (GTK_OBJECT (frame), "label", label);

    gtk_box_pack_start (GTK_BOX (frame), label, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (frame), inner_frame, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (inner_frame), hbox);

    return frame;
}


GtkWidget *
create_named_button_with_data (GtkWidget *parent, const gchar *label, const gchar *name, GtkSignalFunc func, gpointer data)
{
    guint key;
    GtkAccelGroup *accel_group = gtk_accel_group_new ();
    GtkWidget *w = gtk_button_new_with_label ("");

    key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (w)->child), label);
    gtk_widget_add_accelerator (w, "clicked", accel_group,
                                key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
    gtk_window_add_accel_group (GTK_WINDOW (parent), accel_group);
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, w,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);
    if (func)
        gtk_signal_connect (GTK_OBJECT (w), "clicked", func, data);

    return w;
}


GtkWidget *
create_button_with_data (GtkWidget *parent, const gchar *label,
                         GtkSignalFunc func, gpointer data)
{
    return create_named_button_with_data (parent, label, "button", func, data);
}


GtkWidget *
create_button (GtkWidget *parent, gchar *label, GtkSignalFunc func)
{
    return create_button_with_data (parent, label, func, parent);
}


GtkWidget *
create_named_button (GtkWidget *parent, gchar *label, gchar *name, GtkSignalFunc func)
{
    return create_named_button_with_data (parent, label, name, func, parent);
}


GtkWidget *
create_named_stock_button_with_data (GtkWidget *parent, gconstpointer stock, gchar *name, GtkSignalFunc func, gpointer data)
{
    GtkWidget *w = gtk_button_new_from_stock (stock);
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, w,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);
    if (func)
        gtk_signal_connect (GTK_OBJECT (w), "clicked", func, data);
    return w;
}


GtkWidget *
create_stock_button_with_data (GtkWidget *parent, gconstpointer stock, GtkSignalFunc func, gpointer data)
{
    return create_named_stock_button_with_data (parent, stock, "button", func, data);
}


GtkWidget *
create_named_stock_button (GtkWidget *parent, gconstpointer stock, gchar *name, GtkSignalFunc func)
{
    return create_named_stock_button_with_data (parent, stock, name, func, parent);
}


GtkWidget *
create_stock_button (GtkWidget *parent, gconstpointer stock, GtkSignalFunc func)
{
    return create_stock_button_with_data (parent, stock, func, parent);
}


GtkWidget *
create_entry (GtkWidget *parent, const gchar *name, const gchar *value)
{
    GtkWidget *w = gtk_entry_new ();
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, w,
                              (GtkDestroyNotify) gtk_widget_unref);
    if (value)
        gtk_entry_set_text (GTK_ENTRY (w), value);
    gtk_widget_show (w);
    return w;
}


GtkWidget *
create_check (GtkWidget *parent, gchar *text, gchar *name)
{
    GtkWidget *btn;

    btn = gtk_check_button_new_with_label (text);
    gtk_widget_ref (btn);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, btn,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (btn);

    return btn;
}


GtkWidget *
create_radio (GtkWidget *parent, GSList *group, const gchar *text, const gchar *name)
{
    GtkWidget *btn;

    btn = gtk_radio_button_new_with_label (group, text);
    gtk_widget_ref (btn);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, btn,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (btn);

    return btn;
}


GtkWidget *
create_spin (GtkWidget *parent, gchar *name, gint min, gint max, gint value)
{
    GtkObject *adj;
    GtkWidget *spin;

    adj = gtk_adjustment_new (value, min, max, 1, 10, 10);
    spin = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
    gtk_widget_ref (spin);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, spin,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (spin);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);

    return spin;
}


GtkWidget *
create_color_button (GtkWidget *parent, gchar *name)
{
    GtkWidget *w = gtk_color_button_new ();
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, w,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);
    return w;
}


GtkWidget *
create_icon_entry (GtkWidget *parent, gchar *name, const gchar *icon_path)
{
    GtkWidget *icon_entry = gnome_icon_entry_new (NULL, NULL);
    gtk_widget_ref (icon_entry);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, icon_entry,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (icon_entry);
    if (icon_path)
        gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (icon_entry), icon_path);
    return icon_entry;
}


GtkWidget *
create_scale (GtkWidget *parent, gchar *name, gint value, gint min, gint max)
{
    GtkWidget *scale = gtk_hscale_new (
        GTK_ADJUSTMENT (gtk_adjustment_new (value, min, max, 0, 0, 0)));

    gtk_widget_ref (scale);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, scale,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (scale);
    gtk_scale_set_digits (GTK_SCALE (scale), 0);

    return scale;
}


GtkWidget *
create_file_entry (GtkWidget *parent, gchar *name, const gchar *value)
{
    GtkWidget *fentry, *entry;

    fentry = gnome_file_entry_new (NULL, NULL);
    gtk_widget_ref (fentry);
    gtk_object_set_data_full (GTK_OBJECT (parent), "fileentry", fentry,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (fentry);

    entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fentry));
    gtk_widget_ref (entry);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, entry,
                              (GtkDestroyNotify) gtk_widget_unref);
    if (value)
        gtk_entry_set_text (GTK_ENTRY (entry), value);
    gtk_widget_show (entry);

    return fentry;
}


GtkWidget *
create_clist (GtkWidget *parent, gchar *name, gint cols, gint rowh,
              GtkSignalFunc on_row_selected, GtkSignalFunc on_row_moved)
{
    GtkWidget *sw, *clist;

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (sw);
    gtk_object_set_data_full (GTK_OBJECT (parent), "sw", sw,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (sw);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    clist = gtk_clist_new (cols);
    gtk_widget_ref (clist);
    gtk_object_set_data (GTK_OBJECT (sw), "clist", clist);
    gtk_object_set_data_full (GTK_OBJECT (parent), name, clist,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (clist);
    gtk_clist_set_row_height (GTK_CLIST (clist), rowh);
    gtk_container_add (GTK_CONTAINER (sw), clist);
    gtk_clist_column_titles_show (GTK_CLIST (clist));

    if (on_row_selected)
        gtk_signal_connect (GTK_OBJECT (clist), "select-row",
                            GTK_SIGNAL_FUNC (on_row_selected), parent);
    if (on_row_moved)
        gtk_signal_connect (GTK_OBJECT (clist), "row-move",
                            GTK_SIGNAL_FUNC (on_row_moved), parent);
    return sw;
}


void
create_clist_column (GtkWidget *sw, gint col, gint width, gchar *label)
{
    GtkWidget *clist = gtk_object_get_data (GTK_OBJECT (sw), "clist");
    gtk_clist_set_column_width (GTK_CLIST (clist), col, width);
    gtk_clist_set_column_title (GTK_CLIST (clist), col, label);
}


GtkWidget *
create_vbuttonbox (GtkWidget *parent)
{
    GtkWidget *w = gtk_vbutton_box_new ();
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), "vbuttonbox", w,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (w), GTK_BUTTONBOX_START);
    gtk_box_set_spacing (GTK_BOX (w), 12);
    return w;
}


GtkWidget *
create_hbuttonbox (GtkWidget *parent)
{
    GtkWidget *w = gtk_hbutton_box_new ();
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), "hbuttonbox", w,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);
    gtk_box_set_spacing (GTK_BOX (w), 12);
    return w;
}


GtkWidget *
create_combo (GtkWidget *parent)
{
    GtkWidget *combo = gtk_combo_new ();
    gtk_widget_ref (combo);
    gtk_object_set_data_full (GTK_OBJECT (parent), "combo", combo,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (combo);
    return combo;
}


GtkWidget *
create_option_menu (GtkWidget *parent, gchar **items)
{
    gint i = 0;

    GtkWidget *optmenu = gtk_option_menu_new ();
    gtk_widget_ref (optmenu);
    gtk_object_set_data_full (GTK_OBJECT (parent), "optmenu", optmenu,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (optmenu);

    GtkWidget *menu = gtk_menu_new ();
    gtk_widget_show (menu);

    for (i = 0; items[i]; i++)
    {
        GtkWidget *item = gtk_menu_item_new_with_label (items[i]);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu), menu);

    return optmenu;
}


const gchar *
get_combo_text (GtkWidget *combo)
{
    return gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (combo)->entry));
}


GtkWidget *
create_progress_bar (GtkWidget *parent)
{
    GtkWidget *w = gtk_progress_bar_new ();
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), "progress_bar", w,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);
    gtk_progress_set_show_text (GTK_PROGRESS (w), TRUE);
    return w;
}


GSList *
get_radio_group (GtkWidget *radio)
{
    return gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
}


void
table_add (GtkWidget *table, GtkWidget *w, gint x, gint y, GtkAttachOptions x_opts)
{
    gtk_table_attach (GTK_TABLE (table), w, x, x+1, y, y+1, x_opts, (GtkAttachOptions)0, 0, 0);
}


void
table_add_y (GtkWidget *table, GtkWidget *w, gint x, gint y,
             GtkAttachOptions x_opts, GtkAttachOptions y_opts)
{
    gtk_table_attach (GTK_TABLE (table), w, x, x+1, y, y+1, x_opts, y_opts, 0, 0);
}


GtkWidget *
create_pixmap (GtkWidget *parent, GdkPixmap *pm, GdkBitmap *mask)
{
    GtkWidget *w = gtk_pixmap_new (pm, mask);
    gtk_widget_ref (w);
    gtk_object_set_data_full (GTK_OBJECT (parent), "pixmap", w,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (w);
    return w;
}


GtkWidget *
create_sw (GtkWidget *parent)
{
    GtkWidget *scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (scrolledwindow);
    gtk_object_set_data_full (GTK_OBJECT (parent), "scrolledwindow", scrolledwindow,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (scrolledwindow);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    return scrolledwindow;
}


void
progress_bar_update (GtkWidget *pbar, gint max)
{
    gint value = (gint)gtk_progress_get_value (GTK_PROGRESS (pbar)) + 1;

    if (value >= max) value = 0;

    gtk_progress_set_value (GTK_PROGRESS (pbar), (gfloat)value);
}


const char *
get_entry_text (GtkWidget *parent, gchar *entry_name)
{
    GtkWidget *entry = lookup_widget (parent, entry_name);
    if (!entry) return NULL;
    if (!GTK_IS_ENTRY (entry)) return NULL;

    return gtk_entry_get_text (GTK_ENTRY (entry));
}


static void
on_response (GtkDialog *dialog, gint id, gpointer data)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


void
create_error_dialog (const gchar *msg, ...)
{
    va_list      argptr;
    gchar        string[1024];
    GtkWidget    *dialog;

    va_start (argptr, msg);
    vsprintf (string, msg, argptr);
    va_end (argptr);

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_win_widget), GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, string);

    gtk_signal_connect (GTK_OBJECT (dialog), "response", GTK_SIGNAL_FUNC (on_response), dialog);

    gtk_widget_show (dialog);
}


void
create_warning_dialog (const gchar *msg, ...)
{
    va_list      argptr;
    gchar        string[1024];
    GtkWidget    *dialog;

    va_start (argptr, msg);
    vsprintf (string, msg, argptr);
    va_end (argptr);

    dialog = gtk_message_dialog_new (
        GTK_WINDOW (main_win_widget), GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, msg);

    gtk_signal_connect (GTK_OBJECT (dialog), "response",
                        GTK_SIGNAL_FUNC (on_response), dialog);

    gtk_widget_show (dialog);
}
