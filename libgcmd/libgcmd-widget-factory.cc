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
#include "libgcmd-deps.h"
#include "libgcmd-utils.h"
#include "libgcmd-widget-factory.h"
#include "gnome-cmd-dialog.h"


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


GtkWidget *create_label_with_mnemonic (GtkWidget *parent, const gchar *text, GtkWidget *for_widget)
{
    GtkWidget *label = gtk_label_new_with_mnemonic (text);

    if (for_widget)
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), for_widget);

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


GtkWidget *create_radio_with_mnemonic (GtkWidget *parent, GtkWidget *group, gchar *text, const gchar *name)
{
    GtkWidget *radio = gtk_check_button_new_with_label (text);
    gtk_check_button_set_group (GTK_CHECK_BUTTON (radio), GTK_CHECK_BUTTON (group));

    g_object_ref (radio);
    g_object_set_data_full (G_OBJECT (parent), name, radio, g_object_unref);
    gtk_widget_show (radio);

    return radio;
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


GtkWidget *create_color_button (GtkWidget *parent, const gchar *name)
{
    GtkWidget *w = gtk_color_button_new ();
    g_object_ref (w);
    g_object_set_data_full (G_OBJECT (parent), name, w, g_object_unref);
    gtk_widget_show (w);
    return w;
}


static void icon_button_set_path (GtkButton *button, const gchar* icon_path)
{
    g_object_set_data_full (G_OBJECT (button), "file", g_strdup (icon_path), g_free);

    if (icon_path && *icon_path != '\0')
    {
        auto image = gtk_image_new ();
        gtk_image_set_from_file (GTK_IMAGE (image), icon_path);
        gtk_widget_set_tooltip_text (GTK_WIDGET (button), icon_path);
        gtk_button_set_child (GTK_BUTTON (button), GTK_WIDGET (image));
    }
    else
    {
        gtk_button_set_child (GTK_BUTTON (button), gtk_label_new (_("Choose Icon")));
    }
}


static void icon_button_chooser_response (GtkDialog *dialog, int response_id, gpointer user_data)
{
    GtkButton *button = GTK_BUTTON (user_data);

    if (response_id == GTK_RESPONSE_ACCEPT)
    {
        auto icon_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
        auto icon_path = g_file_get_path (icon_file);
        icon_button_set_path (button, icon_path);
        g_free (icon_path);
        g_object_unref (icon_file);
    }

    gtk_window_destroy (GTK_WINDOW (dialog));
}


static void icon_button_clicked (GtkButton *button, const gchar* iconPath)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;

    dialog = gtk_file_chooser_dialog_new (_("Select an Image File"),
                                            GTK_WINDOW (gtk_widget_get_ancestor ((GtkWidget*)button, GTK_TYPE_WINDOW)),
                                            GTK_FILE_CHOOSER_ACTION_OPEN,
                                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                                            _("_OK"), GTK_RESPONSE_ACCEPT,
                                            nullptr);
    if (iconPath)
    {
        auto folderPath = g_path_get_dirname(iconPath);
        GFile *folder = g_file_new_for_path (folderPath);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), folder, nullptr);
        g_object_unref (folder);
        g_free(folderPath);
    }
    else
    {
        GFile *folder = g_file_new_for_path (PIXMAPS_DIR);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), folder, nullptr);
        g_object_unref (folder);
    }

    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pixbuf_formats (filter);
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    g_signal_connect (dialog, "response", G_CALLBACK (icon_button_chooser_response), button);
    gtk_window_present (GTK_WINDOW (dialog));
}


GtkWidget *create_icon_button_widget (GtkWidget *parent, const gchar *name, const gchar *iconPath)
{
    auto gtkButton = gtk_button_new ();
    icon_button_set_path (GTK_BUTTON (gtkButton), iconPath);
    g_signal_connect (gtkButton, "clicked", G_CALLBACK (icon_button_clicked), (gpointer) iconPath);
    g_object_ref (gtkButton);
    g_object_set_data_full (G_OBJECT (parent), name, gtkButton, g_object_unref);
    gtk_widget_show (gtkButton);

    return gtkButton;
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


static void directory_chooser_response (GtkNativeDialog *chooser, gint response_id, GtkWidget *button)
{
    if (response_id != GTK_RESPONSE_ACCEPT)
        return;

    GFile *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));
    if (file != nullptr)
    {
        directory_chooser_button_set_file (button, file);
        g_object_unref (file);
    }
}


static void on_directory_chooser_click (GtkButton *button, GtkWidget *parent)
{
    auto chooser = gtk_file_chooser_native_new (_("Select Directory"),
        GTK_WINDOW (parent),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        nullptr,
        nullptr);
    gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (chooser), true);
    g_signal_connect (chooser, "response", G_CALLBACK (directory_chooser_response), button);

    if (GFile *file = directory_chooser_button_get_file (GTK_WIDGET (button)))
        gtk_file_chooser_set_file (GTK_FILE_CHOOSER (chooser), file, nullptr);

    gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
}


GtkWidget *create_directory_chooser_button (GtkWidget *parent, const gchar *name)
{
    GtkWidget *button = create_named_button (parent, _("(None)"), name, G_CALLBACK (on_directory_chooser_click));
    return button;
}


GFile *directory_chooser_button_get_file (GtkWidget *button)
{
    return (GFile *) g_object_get_data (G_OBJECT (button), "file");
}


void directory_chooser_button_set_file (GtkWidget *button, GFile *file)
{
    g_object_set_data_full (G_OBJECT (button), "file", g_object_ref (file), g_object_unref);
    gchar *label = file ? g_file_get_basename (file) : _("(None)");
    gtk_button_set_label (GTK_BUTTON (button), label);
    g_free (label);
}


GtkWidget *create_treeview (GtkWidget *parent, const gchar *name, GtkTreeModel *model, gint rowh, GCallback on_selection_changed, GCallback on_rows_reordered)
{
    GtkWidget *sw, *view;

    sw = gtk_scrolled_window_new ();
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
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), view);

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
    if (type == G_TYPE_STRING)
    {
        renderer = gtk_cell_renderer_text_new ();
        attribute = "text";
    }
    else if (type == G_TYPE_ICON)
    {
        renderer = gtk_cell_renderer_pixbuf_new ();
        attribute = "gicon";
    }
    else if (type == GDK_TYPE_PIXBUF)
    {
        renderer = gtk_cell_renderer_pixbuf_new ();
        attribute = "pixbuf";
    }
    else
    {
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

    if (items)
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
    GtkWidget *scrolledwindow = gtk_scrolled_window_new ();
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

    return gtk_editable_get_text (GTK_EDITABLE (entry));
}


MenuBuilder MenuBuilder::item(GMenuItem *item) &&
{
    g_menu_append_item (menu, item);
    g_object_unref (item);
    return *this;
}


MenuBuilder MenuBuilder::item(const gchar *label,
                              const gchar *detailed_action,
                              const gchar *accelerator,
                              const gchar *icon) &&
{
    GMenuItem *item = g_menu_item_new (label, detailed_action);
    if (accelerator)
    {
        g_menu_item_set_attribute_value(item, "accel", g_variant_new_string (accelerator));

        auto trigger = gtk_shortcut_trigger_parse_string (accelerator);
        if (trigger)
        {
            if (shortcuts == nullptr)
                shortcuts = g_list_store_new (GTK_TYPE_SHORTCUT);

            g_list_store_append (shortcuts,
                gtk_shortcut_new (trigger, gtk_named_action_new (detailed_action)));
        }
        else
        {
            g_warning ("Failed to parse '%s' as an accelerator.", accelerator);
        }
    }
    if (icon)
        g_menu_item_set_icon (item, g_themed_icon_new (icon));
    g_menu_append_item (menu, item);
    g_object_unref (item);
    return *this;
}

