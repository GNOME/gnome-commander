/**
 * @file libgcmd-widget-factory.cc
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
        parent = GTK_IS_MENU (widget) ? gtk_menu_get_attach_widget (GTK_MENU (widget)) : gtk_widget_get_parent(widget);
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
    GtkWidget *frame = create_frame (parent, nullptr, space);
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


GtkWidget *create_space_hbox (GtkWidget *parent, GtkWidget *content)
{
    GtkWidget *hbox = create_hbox (parent, FALSE, 0);
    GtkWidget *lbl = create_label (parent, "    ");

    gtk_box_pack_start (GTK_BOX (hbox), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), content, TRUE, TRUE, 0);

    return hbox;
}


GtkWidget *create_category (GtkWidget *parent, GtkWidget *content, const gchar *title)
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

    key = gtk_label_parse_uline (GTK_LABEL (gtk_bin_get_child( GTK_BIN (w))), label);
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


GtkWidget *create_radio_with_mnemonic (GtkWidget *parent, GSList *group, gchar *text, const gchar *name)
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
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    GtkWidget *spin = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif
    g_object_ref (spin);
    g_object_set_data_full (G_OBJECT (parent), name, spin, g_object_unref);
    gtk_widget_show (spin);
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

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


static void preview_update (GtkFileChooser *fileChooser, GtkImage *preview)
{
    char *filename;
    GdkPixbuf *pixbuf;

    filename = gtk_file_chooser_get_preview_filename (fileChooser);
    if (filename)
    {
        pixbuf = gdk_pixbuf_new_from_file (filename, nullptr);

        gtk_file_chooser_set_preview_widget_active (fileChooser, pixbuf != nullptr);

        if (pixbuf)
        {
            gtk_image_set_from_pixbuf (preview, pixbuf);
            g_object_unref (pixbuf);
        }

        g_free (filename);
    }
}


static void icon_button_clicked (GtkButton *button, const gchar* iconPath)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;
    GtkWidget *preview;
    int responseValue;

    dialog = gtk_file_chooser_dialog_new (_("Select an Image File"),
                                            GTK_WINDOW (gtk_widget_get_ancestor ((GtkWidget*)button, GTK_TYPE_WINDOW)),
                                            GTK_FILE_CHOOSER_ACTION_OPEN,
                                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                            nullptr);
    if (iconPath)
    {
        auto folderPath = g_path_get_dirname(iconPath);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), folderPath);
        g_free(folderPath);
    }
    else
    {
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), PIXMAPS_DIR);
    }

    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pixbuf_formats (filter);
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    preview = gtk_image_new ();
    gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog), preview);
    g_signal_connect (dialog, "update-preview", G_CALLBACK (preview_update), preview);

    responseValue = gtk_dialog_run (GTK_DIALOG (dialog));

    if (responseValue == GTK_RESPONSE_ACCEPT)
    {
        auto icon_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        gtk_image_set_from_file (GTK_IMAGE (gtk_button_get_image (button)), icon_path);
        gtk_button_set_label (button, icon_path == nullptr ? _("Choose Icon") : nullptr);
        gtk_widget_set_tooltip_text(GTK_WIDGET(button), icon_path);
    }

    gtk_widget_destroy (dialog);
}


GtkWidget *create_icon_button_widget (GtkWidget *parent, const gchar *name, const gchar *iconPath)
{
    auto image = gtk_image_new ();
    auto gtkButton = gtk_button_new ();
    if (iconPath && *iconPath != '\0')
    {
        gtk_image_set_from_file (GTK_IMAGE (image), iconPath);
        gtk_widget_set_tooltip_text(gtkButton, iconPath);
    }
    else
    {
        gtk_button_set_label (GTK_BUTTON(gtkButton), _("Choose Icon"));
    }
    gtk_button_set_image (GTK_BUTTON (gtkButton), image);
    g_signal_connect (gtkButton, "clicked", G_CALLBACK (icon_button_clicked), (gpointer) iconPath);
    g_object_ref (gtkButton);
    g_object_set_data_full (G_OBJECT (parent), name, gtkButton, g_object_unref);
    gtk_widget_show (gtkButton);

    return gtkButton;
}


GtkWidget *create_scale (GtkWidget *parent, const gchar *name, gint value, gint min, gint max)
{
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
    GtkWidget *scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (value, min, max, 0, 0, 0)));
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    g_object_ref (scale);
    g_object_set_data_full (G_OBJECT (parent), name, scale, g_object_unref);
    gtk_widget_show (scale);
    gtk_scale_set_digits (GTK_SCALE (scale), 0);

    return scale;
}


GtkWidget *create_directory_chooser_button (GtkWidget *parent, const gchar *name, const gchar *value)
{
    GtkWidget *chooser;
    chooser = gtk_file_chooser_button_new (_("Folder selection"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    if (value == nullptr)
    {
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), "");
    }
    else
    {
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), value);
    }
    g_object_ref (chooser);
    g_object_set_data_full (G_OBJECT (parent), name, chooser, g_object_unref);
    gtk_widget_show (chooser);

    return chooser;
}


GtkWidget *create_file_chooser_button (GtkWidget *parent, const gchar *name, const gchar *value)
{
    GtkWidget *chooser;
    chooser = gtk_file_chooser_button_new (_("File selection"), GTK_FILE_CHOOSER_ACTION_OPEN);
    if (value == nullptr)
    {
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), "");
    }
    else
    {
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), value);
    }

    g_object_ref (chooser);
    g_object_set_data_full (G_OBJECT (parent), name, chooser, g_object_unref);
    gtk_widget_show (chooser);

    return chooser;
}


GtkWidget *create_treeview (GtkWidget *parent, const gchar *name, GtkTreeModel *model, gint rowh, GtkSignalFunc on_selection_changed, GtkSignalFunc on_rows_reordered)
{
    GtkWidget *sw, *view;

    sw = gtk_scrolled_window_new (nullptr, nullptr);
    g_object_ref (sw);
    g_object_set_data_full (G_OBJECT (parent), "sw", sw, g_object_unref);
    gtk_widget_show (sw);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    view = gtk_tree_view_new_with_model (model);
    g_object_ref (view);
    g_object_set_data (G_OBJECT (sw), "view", view);
    g_object_set_data (G_OBJECT (sw), "rowh", GINT_TO_POINTER (rowh));
    g_object_set_data_full (G_OBJECT (parent), name, view, g_object_unref);
    gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (view), TRUE);
    gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (view), FALSE);
    gtk_widget_show (view);
    gtk_container_add (GTK_CONTAINER (sw), view);

    g_object_unref (model);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

    if (on_selection_changed)
        g_signal_connect (selection, "changed", G_CALLBACK (on_selection_changed), parent);
    if (on_rows_reordered)
        g_signal_connect (model, "rows-reordered", G_CALLBACK (on_rows_reordered), parent);
    return sw;
}


void create_treeview_column (GtkWidget *sw, gint col, gint width, const gchar *label)
{
    GtkTreeView *view = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (sw), "view"));
    gint rowh = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (sw), "rowh"));
    GtkTreeModel *model = gtk_tree_view_get_model (view);

    GtkCellRenderer *renderer;
    const gchar *attribute;
    GType type = gtk_tree_model_get_column_type (model, col);
    switch (type)
    {
        case G_TYPE_STRING:
            renderer = gtk_cell_renderer_text_new ();
            attribute = "text";
            break;
        default:
            if (type == GDK_TYPE_PIXBUF)
            {
                renderer = gtk_cell_renderer_pixbuf_new ();
                attribute = "pixbuf";
                break;
            }
            return;
    }

    gtk_cell_renderer_set_fixed_size (renderer, -1, rowh);

    GtkTreeViewColumn *column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width (column, width);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_title (column, label);
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_add_attribute (column, renderer, attribute, col);
    gtk_tree_view_insert_column (view, column, col);
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

GtkWidget *create_combo_box_text_with_entry (GtkWidget *parent)
{
    GtkWidget *combo = gtk_combo_box_text_new_with_entry ();
    g_object_ref (combo);
    g_object_set_data_full (G_OBJECT (parent), "combo", combo, g_object_unref);
    gtk_widget_show (combo);
    return combo;
}

GtkWidget *create_combo_box_text (GtkWidget *parent, const gchar **items)
{
    GtkWidget *combo = gtk_combo_box_text_new ();
    g_object_ref (combo);
    g_object_set_data_full (G_OBJECT (parent), "combo", combo, g_object_unref);
    gtk_widget_show (combo);

    for (gint i = 0; items[i]; i++)
        gtk_combo_box_text_append_text ((GtkComboBoxText*) combo, items[i]);

    return combo;
}


GtkWidget *create_progress_bar (GtkWidget *parent)
{
    GtkWidget *w = gtk_progress_bar_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), "progress_bar", w, g_object_unref);
    gtk_widget_show (w);
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (w), TRUE);
    return w;
}


GtkWidget *create_sw (GtkWidget *parent)
{
    GtkWidget *scrolledwindow = gtk_scrolled_window_new (nullptr, nullptr);
    g_object_ref (scrolledwindow);
    g_object_set_data_full (G_OBJECT (parent), "scrolledwindow", scrolledwindow, g_object_unref);
    gtk_widget_show (scrolledwindow);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    return scrolledwindow;
}


const char *get_entry_text (GtkWidget *parent, const gchar *entry_name)
{
    GtkWidget *entry = lookup_widget (parent, entry_name);
    if (!entry) return nullptr;
    if (!GTK_IS_ENTRY (entry)) return nullptr;

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

    dialog = gtk_message_dialog_new (GTK_WINDOW (main_win_widget), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", string);

    g_signal_connect (dialog, "response", G_CALLBACK (on_response), dialog);

    gtk_widget_show (dialog);
}
