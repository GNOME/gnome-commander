/**
 * @file gnome-cmd-combo.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2022 Uwe Scholz\n
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
 * Modified by Marcus Bjurman <marbj499@student.liu.se> 2001-2006
 * The orginal comments are left intact above
 */

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-data.h"

using namespace std;


enum
{
  ITEM_SELECTED,
  POPWIN_HIDDEN,
  LAST_SIGNAL
};

struct GnomeCmdComboClass
{
    GtkComboBoxClass parent_class;

    void (* item_selected)     (GnomeCmdCombo *combo, gpointer data);
    void (* popwin_hidden)     (GnomeCmdCombo *combo);
};

struct GnomeCmdComboPrivate
{
    GtkListStore *store;
    GtkWidget *entry;

    gint num_cols;
    gint text_col;
    gint data_col;

    guint select_in_progress:1;
    guint has_pixbufs:1;
};

static guint combo_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE(GnomeCmdCombo, gnome_cmd_combo, GTK_TYPE_COMBO_BOX)


/*******************************
 * Utility functions
 *******************************/

static inline GtkWidget *gnome_cmd_combo_get_popup_widget (GnomeCmdCombo *combo)
{
    // try to get the popup widget from combobox using accessible object
    AtkObject *accesible = gtk_combo_box_get_popup_accessible (&combo->parent_instance);
    if (GTK_IS_ACCESSIBLE(accesible))
    {
        return gtk_accessible_get_widget (GTK_ACCESSIBLE (accesible));
    }

    // alternative way of getting the popup widget from combobox by searching for the popup window
    GtkWidget *popup_widget = NULL;
    GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo));
    if (GTK_IS_WINDOW (toplevel))
    {
        GList *list = gtk_window_group_list_windows (gtk_window_get_group (GTK_WINDOW (toplevel)));

        for (GList *i = list; i; i = i->next)
        {
            GtkWidget *window = (GtkWidget *) i->data;
            if (!strcmp(gtk_widget_get_name (window), "gtk-combobox-popup-window"))
            {
                if (gtk_widget_get_visible (window))
                {
                    GtkWidget *scrolled_window = gtk_bin_get_child (GTK_BIN (window));
                    popup_widget = gtk_bin_get_child (GTK_BIN (scrolled_window));
                    break;
                }
            }
        }

        g_list_free (list);
    }

    return popup_widget;
}


/*******************************
 * Callbacks
 *******************************/

static void gnome_cmd_combo_changed (GtkComboBox *widget, GnomeCmdCombo *combo)
{
    GtkTreeIter iter;
    gpointer data;

    if (!combo->priv->select_in_progress)
    {
        if (gtk_combo_box_get_active_iter (&combo->parent_instance, &iter))
        {
            gtk_tree_model_get (GTK_TREE_MODEL (combo->priv->store), &iter, combo->priv->data_col, &data, -1);
            gtk_signal_emit (*combo, combo_signals[ITEM_SELECTED], data);
        }
    }
}


static void gnome_cmd_combo_notify_popup_shown (GObject *gobject, GParamSpec *pspec, GnomeCmdCombo *combo)
{
    gboolean popup_shown;

    g_object_get (gobject, "popup-shown", &popup_shown, NULL);

    if (popup_shown)
    {
        // settings for treeview are handled here, because treeview isn't created at initialization, but later
        // and also because the popup widget may get removed and recreated
        GtkWidget *popup_widget = gnome_cmd_combo_get_popup_widget (combo);

        // disable hover selection mode in treeview
        gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (popup_widget), FALSE);

        gtk_widget_set_style (popup_widget, list_style);

        // set width to at least 200 pixels
        GtkWidget *window = gtk_widget_get_parent (gtk_widget_get_parent (popup_widget));

        gint width;
        gint height;
        gtk_window_get_size (GTK_WINDOW (window), &width, &height);
        if (width < 200)
        {
            gtk_widget_set_size_request (window, 200, height);
            gdk_window_resize (gtk_widget_get_window (window), 200, height);
        }
    }
    else
        gtk_signal_emit (*combo, combo_signals[POPWIN_HIDDEN]);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_combo_class_init (GnomeCmdComboClass *klass)
{
    GtkObjectClass *object_class = (GtkObjectClass *) klass;

    combo_signals[ITEM_SELECTED] =
        gtk_signal_new ("item-selected",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdComboClass, item_selected),
            gtk_marshal_NONE__POINTER,
            GTK_TYPE_NONE,
            1, GTK_TYPE_POINTER);

    combo_signals[POPWIN_HIDDEN] =
        gtk_signal_new ("popwin-hidden",
            GTK_RUN_LAST,
            G_OBJECT_CLASS_TYPE (object_class),
            GTK_SIGNAL_OFFSET (GnomeCmdComboClass, popwin_hidden),
            gtk_marshal_NONE__NONE,
            GTK_TYPE_NONE,
            0);

    klass->item_selected = NULL;
    klass->popwin_hidden = NULL;
}

static void gnome_cmd_combo_init (GnomeCmdCombo *combo)
{
    combo->priv = static_cast<GnomeCmdComboPrivate *> (gnome_cmd_combo_get_instance_private (combo));

    combo->priv->select_in_progress = 0;
    combo->priv->has_pixbufs = 0;
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_combo_new_with_store (GtkListStore *store, gint num_cols, gint text_col, gint data_col)
{
    g_return_val_if_fail (GTK_IS_LIST_STORE (store), NULL);

    // force value of style property "appears-as-list" to TRUE, which makes GtkComboBox use GtkTreeView instead of GtkMenu
    // based on tests/testcombochange.c in gtk sources
    static gboolean style_loaded = FALSE;
    if (!style_loaded)
    {
        gtk_rc_parse_string ("style \"GnomeCmdCombo-style\" {\n"
                             "  GtkComboBox::appears-as-list = 1\n"
                             "}\n"
                             "\n"
                             "class \"GnomeCmdCombo\" style \"GnomeCmdCombo-style\"");
        style_loaded = TRUE;
    }

    GtkTreeModel *model = GTK_TREE_MODEL (store);

    auto *combo = static_cast<GnomeCmdCombo *> (g_object_new (GNOME_CMD_TYPE_COMBO, "model", model, "has-entry", TRUE, NULL));
    GnomeCmdComboPrivate *priv = combo->priv;

    priv->store = store;
    priv->num_cols = num_cols;
    priv->text_col = text_col;
    priv->data_col = data_col;

    g_object_unref (store);

    gtk_combo_box_set_entry_text_column (&combo->parent_instance, text_col);

    GtkCellLayout *layout = GTK_CELL_LAYOUT (combo);

    for (gint col = 0; col < num_cols; ++col)
    {
        if (col == text_col)
            continue;

        GtkCellRenderer *renderer;
        const gchar *attribute = NULL;
        gboolean expand = FALSE;
        GType type = gtk_tree_model_get_column_type (model, col);
        if (type == GDK_TYPE_PIXBUF)
        {
            renderer = gtk_cell_renderer_pixbuf_new ();
            attribute = "pixbuf";

            gtk_cell_renderer_set_fixed_size (renderer, 20, 20);
            priv->has_pixbufs = 1;
        }
        else
        {
            renderer = gtk_cell_renderer_text_new ();
            if (type == G_TYPE_STRING)
            {
                attribute = "text";
                expand = TRUE;
            }
        }

        gtk_cell_layout_pack_start (layout, renderer, expand);
        if (attribute)
            gtk_cell_layout_add_attribute (layout, renderer, attribute, col);
        if (col < text_col)
            gtk_cell_layout_reorder (layout, renderer, col);
    }

    priv->entry = gtk_bin_get_child (GTK_BIN (combo));

    gtk_widget_set_size_request (priv->entry, 60, -1);
    gtk_entry_set_editable (GTK_ENTRY (priv->entry), FALSE);
    gtk_widget_set_can_focus (priv->entry, FALSE);

    // TODO: in gtk3 - set button relief to none
    // TODO: in gtk3 - set button can_focus to false

    g_signal_connect (&combo->parent_instance, "changed", G_CALLBACK (gnome_cmd_combo_changed), combo);
    g_signal_connect (&combo->parent_instance, "notify::popup-shown", G_CALLBACK (gnome_cmd_combo_notify_popup_shown), combo);

    return GTK_WIDGET (combo);
}


GtkWidget *GnomeCmdCombo::get_entry()
{
    return priv->entry;
}


void GnomeCmdCombo::clear()
{
    gtk_list_store_clear (priv->store);

    if (priv->has_pixbufs)
    {
        GList *list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (this));

        for (GList *i = list; i; i = i->next)
        {
            if (GTK_IS_CELL_RENDERER_PIXBUF (i->data))
                gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (i->data), 20, 20);
        }

        g_list_free (list);
    }
}


void GnomeCmdCombo::append(gchar *text, gpointer data, ...)
{
    g_return_if_fail (text != NULL);

    GtkTreeIter iter;
    gtk_list_store_append (priv->store, &iter);
    gtk_list_store_set (priv->store, &iter,
                        priv->text_col, text,
                        priv->data_col, data,
                        -1);

    va_list var_args;
    va_start (var_args, data);
    gtk_list_store_set_valist (priv->store, &iter, var_args);
    va_end (var_args);


    if (priv->has_pixbufs)
    {
        GtkTreeModel *model = GTK_TREE_MODEL (priv->store);
        GList *list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (this));

        gint col = 0;
        for (GList *i = list; i; i = i->next, ++col)
        {
            if (GTK_IS_CELL_RENDERER_PIXBUF (i->data))
            {
                GdkPixbuf *pixbuf;
                gtk_tree_model_get (model, &iter, col, &pixbuf, -1);

                if (pixbuf != NULL)
                {
                    gint pixbuf_width = gdk_pixbuf_get_width (pixbuf);
                    gint pixbuf_height = gdk_pixbuf_get_height (pixbuf);

                    g_object_unref(pixbuf);

                    GtkCellRenderer *renderer = GTK_CELL_RENDERER (i->data);

                    gint cell_width;
                    gint cell_height;

                    gtk_cell_renderer_get_fixed_size (renderer, &cell_width, &cell_height);

                    if (cell_width < pixbuf_width)
                        cell_width = pixbuf_width;

                    if (cell_height < pixbuf_height)
                        cell_height = pixbuf_height;

                    gtk_cell_renderer_set_fixed_size (renderer, cell_width, cell_height);
                }
            }
        }

        g_list_free (list);
    }
}


void GnomeCmdCombo::popup_list()
{
    gtk_combo_box_popup (&parent_instance);
}


void GnomeCmdCombo::select_data(gpointer data)
{
    GtkTreeModel *model = GTK_TREE_MODEL (priv->store);
    GtkTreeIter iter;
    gpointer *row_data;

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gtk_tree_model_get (model, &iter, priv->data_col, &row_data, -1);

            if (row_data == data)
            {
                priv->select_in_progress = 1;
                gtk_combo_box_set_active_iter (&parent_instance, &iter);
                priv->select_in_progress = 0;
                break;
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }
}


void GnomeCmdCombo::update_style()
{
    gtk_widget_set_style (priv->entry, list_style);
}

