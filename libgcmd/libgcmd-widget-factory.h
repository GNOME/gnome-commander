/**
 * @file libgcmd-widget-factory.h
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

#pragma once

GtkWidget *lookup_widget (GtkWidget *widget, const gchar *widget_name);

GtkWidget *create_frame (GtkWidget *parent, const gchar *text, gint spacing);

GtkWidget *create_tabframe (GtkWidget *parent);

GtkWidget *create_space_frame (GtkWidget *parent, gint space);

GtkWidget *create_grid (GtkWidget *parent);

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

GtkWidget *create_entry (GtkWidget *parent, const gchar *name, const gchar *value);

GtkWidget *create_check (GtkWidget *parent, const gchar *text, const gchar *name);

GtkWidget *create_check_with_mnemonic (GtkWidget *parent, const gchar *text, const gchar *name);

GtkWidget *create_radio (GtkWidget *parent, GSList *group, const gchar *text, const gchar *name);

GtkWidget *create_radio_with_mnemonic (GtkWidget *parent, GSList *group, gchar *text, const gchar *name);

GtkWidget *create_spin (GtkWidget *parent, const gchar *name, gint min, gint max, gint value);

GtkWidget *create_color_button (GtkWidget *parent, const gchar *name);

GtkWidget *create_icon_button_widget (GtkWidget *parent, const gchar *name, const gchar *icon_path);

GtkWidget *create_scale (GtkWidget *parent, const gchar *name, gint value, gint min, gint max);

GtkWidget *create_directory_chooser_button (GtkWidget *parent, const gchar *name, const gchar *value);

GtkWidget *create_file_chooser_button (GtkWidget *parent, const gchar *name, const gchar *value);

GtkWidget *create_treeview (GtkWidget *parent, const gchar *name, GtkTreeModel *model, gint rowh, GCallback on_selection_changed, GCallback on_rows_reordered);

void create_treeview_column (GtkWidget *sw, gint col, gint width, const gchar *label);

GtkWidget *create_vbuttonbox (GtkWidget *parent);

GtkWidget *create_hbuttonbox (GtkWidget *parent);

GtkWidget *create_combo_box_text_with_entry (GtkWidget *parent);

GtkWidget *create_combo_box_text (GtkWidget *parent, const gchar **items);

inline const gchar *get_combo_box_entry_text (GtkWidget *combo)
{
    return gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo))));
}

inline GSList *get_radio_group (GtkWidget *radio)
{
    return gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio));
}

GtkWidget *create_progress_bar (GtkWidget *parent);

GtkWidget *create_sw (GtkWidget *parent);

const char *get_entry_text (GtkWidget *parent, const gchar *entry_name);

#ifdef __GNUC__
    void create_error_dialog (const gchar *msg, ...) __attribute__ ((format (gnu_printf, 1, 2)));
#else
    void create_error_dialog (const gchar *msg, ...);
#endif

#if !GTK_CHECK_VERSION(3,0,0)
inline void gtk_progress_bar_set_show_text (GtkProgressBar *pbar, gboolean show_text)
{
    gtk_progress_bar_set_text (pbar, nullptr);
    pbar->progress.show_text = show_text;
}

inline GtkWidget *gtk_tree_view_column_get_button (GtkTreeViewColumn *tree_column)
{
    return tree_column->button;
}

inline gboolean gtk_tree_model_iter_previous (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    GtkTreePath *path = gtk_tree_model_get_path (tree_model, iter);
    gboolean result = gtk_tree_path_prev (path);
    if (result)
        result = gtk_tree_model_get_iter (tree_model, iter, path);
    gtk_tree_path_free (path);
    return result;
}
#endif


class MenuBuilder
{
public:
    MenuBuilder()
        : menu(g_menu_new()),
          accel_group(gtk_accel_group_new()),
          parent(nullptr),
          label(nullptr),
          action_group(nullptr),
          action_widget(nullptr)
    {
    }

    MenuBuilder with_action_group(GActionGroup *action_group)
    {
        this->action_group = action_group;
        return *this;
    }

    MenuBuilder item(const gchar *label,
                     const gchar *detailed_action,
                     const gchar *accelerator = nullptr,
                     const gchar *icon = nullptr) &&;

    MenuBuilder item(GMenuItem *item) &&;

    MenuBuilder submenu(const gchar *label) &&
    {
        return MenuBuilder(this, label);
    }

    MenuBuilder endsubmenu() &&
    {
        g_menu_append_submenu (parent->menu, label, G_MENU_MODEL (menu));
        return *parent;
    }

    MenuBuilder section() &&
    {
        return MenuBuilder(this, nullptr);
    }

    MenuBuilder endsection() &&
    {
        g_menu_append_section (parent->menu, nullptr, G_MENU_MODEL (menu));
        return *parent;
    }

    MenuBuilder section(GMenuModel *section) &&
    {
        g_menu_append_section (menu, nullptr, section);
        return *this;
    }

    MenuBuilder section(GMenu *section) &&
    {
        g_menu_append_section (menu, nullptr, G_MENU_MODEL (section));
        return *this;
    }

    struct Result
    {
        GMenu *menu;
        GtkAccelGroup *accel_group;
    };

    Result build() &&
    {
        return Result { menu, accel_group };
    }
private:
    GMenu *menu;
    GtkAccelGroup *accel_group;
    MenuBuilder *parent = nullptr;
    const gchar *label = nullptr;
    GActionGroup *action_group;
    GtkWidget *action_widget;

    MenuBuilder(MenuBuilder *parent, const gchar *label = nullptr)
        : menu(g_menu_new()),
          accel_group(parent->accel_group),
          parent(parent),
          label(label),
          action_group(parent->action_group),
          action_widget(parent->action_widget)
    {
    }
};

