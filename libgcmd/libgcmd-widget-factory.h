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

GtkWidget *create_grid (GtkWidget *parent);

GtkWidget *create_vbox (GtkWidget *parent, gboolean h, gint s);

GtkWidget *create_hbox (GtkWidget *parent, gboolean h, gint s);

inline GtkWidget *create_tabvbox (GtkWidget *parent)
{
    return create_vbox (parent, FALSE, 6);
}

GtkWidget *create_label (GtkWidget *parent, const gchar *text);

GtkWidget *create_label_with_mnemonic (GtkWidget *parent, const gchar *text, GtkWidget *for_widget);

GtkWidget *create_bold_label (GtkWidget *parent, const gchar *text);

GtkWidget *create_hsep (GtkWidget *parent);

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

GtkWidget *create_radio (GtkWidget *parent, GtkWidget *group, const gchar *text, const gchar *name);

GtkWidget *create_radio_with_mnemonic (GtkWidget *parent, GtkWidget *group, gchar *text, const gchar *name);

GtkWidget *create_spin (GtkWidget *parent, const gchar *name, gint min, gint max, gint value);

GtkWidget *create_color_button (GtkWidget *parent, const gchar *name);

GtkWidget *create_icon_button_widget (GtkWidget *parent, const gchar *name, const gchar *icon_path);

GtkWidget *create_scale (GtkWidget *parent, const gchar *name, gint value, gint min, gint max);

GtkWidget *create_directory_chooser_button (GtkWidget *parent, const gchar *name);
GFile *directory_chooser_button_get_file (GtkWidget *button);
void directory_chooser_button_set_file (GtkWidget *button, GFile *file);

GtkWidget *create_treeview (GtkWidget *parent, const gchar *name, GtkTreeModel *model, gint rowh, GCallback on_selection_changed, GCallback on_rows_reordered);

void create_treeview_column (GtkWidget *sw, gint col, gint width, const gchar *label);

GtkWidget *create_combo_box_text_with_entry (GtkWidget *parent);

GtkWidget *create_combo_box_text (GtkWidget *parent, const gchar **items);

GtkWidget *create_progress_bar (GtkWidget *parent);

GtkWidget *create_sw (GtkWidget *parent);

const char *get_entry_text (GtkWidget *parent, const gchar *entry_name);


class MenuBuilder
{
public:
    MenuBuilder()
        : menu(g_menu_new()),
          shortcuts(nullptr),
          parent(nullptr),
          label(nullptr)
    {
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
        parent->shortcuts = shortcuts;
        return *parent;
    }

    MenuBuilder section() &&
    {
        return MenuBuilder(this, nullptr);
    }

    MenuBuilder endsection() &&
    {
        g_menu_append_section (parent->menu, nullptr, G_MENU_MODEL (menu));
        parent->shortcuts = shortcuts;
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
        GListModel *shortcuts;
    };

    Result build() &&
    {
        return Result { .menu = menu, .shortcuts = G_LIST_MODEL (shortcuts) };
    }
private:
    GMenu *menu;
    GListStore *shortcuts;
    MenuBuilder *parent = nullptr;
    const gchar *label = nullptr;

    MenuBuilder(MenuBuilder *parent, const gchar *label = nullptr)
        : menu(g_menu_new()),
          shortcuts(parent->shortcuts),
          parent(parent),
          label(label)
    {
    }
};

