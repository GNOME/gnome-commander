/** 
 * @file gnome-cmd-selection-profile-component.cc
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

#include <vector>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-selection-profile-component.h"
#include "gnome-cmd-menu-button.h"
#include "utils.h"

using namespace std;


struct GnomeCmdSelectionProfileComponentClass
{
    GtkBoxClass parent_class;
};


struct GnomeCmdSelectionProfileComponent::Private
{
    GtkWidget *grid;
    GtkWidget *filter_type_combo;
    GtkWidget *pattern_combo;
    GtkWidget *recurse_combo;
    GtkWidget *find_text_combo;
    GtkWidget *find_text_check;
    GtkWidget *case_check;

    static void on_filter_type_changed (GtkComboBox *combo, GnomeCmdSelectionProfileComponent *component);
    static void on_find_text_toggled (GtkToggleButton *togglebutton, GnomeCmdSelectionProfileComponent *component);

    Private();
    ~Private();
};


inline GnomeCmdSelectionProfileComponent::Private::Private()
{
    grid = NULL;
    filter_type_combo = NULL;
    pattern_combo = NULL;
    recurse_combo = NULL;
    find_text_combo = NULL;
    find_text_check = NULL;
    case_check = NULL;
}


inline GnomeCmdSelectionProfileComponent::Private::~Private()
{
    // g_object_unref (template_entry);

    // clear_regex_model(regex_model);

    // if (regex_model)  g_object_unref (regex_model);

    // g_free (sample_fname);
}


void GnomeCmdSelectionProfileComponent::Private::on_filter_type_changed(GtkComboBox *combo, GnomeCmdSelectionProfileComponent *component)
{
    gtk_widget_grab_focus (component->priv->pattern_combo);
}


void GnomeCmdSelectionProfileComponent::Private::on_find_text_toggled(GtkToggleButton *togglebutton, GnomeCmdSelectionProfileComponent *component)
{
    if (gtk_toggle_button_get_active (togglebutton))
    {
        gtk_widget_set_sensitive (component->priv->find_text_combo, TRUE);
        gtk_widget_set_sensitive (component->priv->case_check, TRUE);
        gtk_widget_grab_focus (component->priv->find_text_combo);
    }
    else
    {
        gtk_widget_set_sensitive (component->priv->find_text_combo, FALSE);
        gtk_widget_set_sensitive (component->priv->case_check, FALSE);
    }
}


static void combo_box_insert_text (const gchar *text, GtkComboBox *widget)
{
    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

    GtkTreeIter iter;
    gtk_list_store_append (GTK_LIST_STORE (store), &iter);
    gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, text, -1);
}


G_DEFINE_TYPE (GnomeCmdSelectionProfileComponent, gnome_cmd_selection_profile_component, GTK_TYPE_BOX)


static void gnome_cmd_selection_profile_component_init (GnomeCmdSelectionProfileComponent *component)
{
    g_object_set (component, "orientation", GTK_ORIENTATION_VERTICAL, NULL);

    component->priv = new GnomeCmdSelectionProfileComponent::Private;

    component->priv->grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (component->priv->grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (component->priv->grid), 6);
    gtk_box_pack_start (GTK_BOX (component), component->priv->grid, FALSE, TRUE, 0);


    // search for
    component->priv->filter_type_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (component->priv->filter_type_combo), _("Path matches regex:"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (component->priv->filter_type_combo), _("Name contains:"));
    component->priv->pattern_combo = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING)));
    gtk_grid_attach (GTK_GRID (component->priv->grid), component->priv->filter_type_combo, 0, 0, 1, 1);
    gtk_widget_set_hexpand (component->priv->pattern_combo, TRUE);
    gtk_grid_attach (GTK_GRID (component->priv->grid), component->priv->pattern_combo, 1, 0, 1, 1);


    // recurse check
    component->priv->recurse_combo = gtk_combo_box_text_new ();

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (component->priv->recurse_combo), _("Unlimited depth"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (component->priv->recurse_combo), _("Current directory only"));
    for (int i=1; i<=40; ++i)
    {
       gchar *item = g_strdup_printf (ngettext("%i level", "%i levels", i), i);
       combo_box_insert_text (item, GTK_COMBO_BOX (component->priv->recurse_combo));
       g_free (item);
    }

    gtk_grid_attach (GTK_GRID (component->priv->grid), create_label_with_mnemonic (*component, _("Search _recursively:"), component->priv->recurse_combo), 0, 2, 1, 1);
    gtk_widget_set_hexpand (component->priv->recurse_combo, TRUE);
    gtk_grid_attach (GTK_GRID (component->priv->grid), component->priv->recurse_combo, 1, 2, 1, 1);


    // find text
    component->priv->find_text_check = create_check_with_mnemonic (*component, _("Contains _text:"), "find_text");
    gtk_grid_attach (GTK_GRID (component->priv->grid), component->priv->find_text_check, 0, 3, 1, 1);

    component->priv->find_text_combo = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING)));
    gtk_widget_set_hexpand (component->priv->find_text_combo, TRUE);
    gtk_grid_attach (GTK_GRID (component->priv->grid), component->priv->find_text_combo, 1, 3, 1, 1);
    gtk_widget_set_sensitive (component->priv->find_text_combo, FALSE);

    gtk_combo_box_set_active (GTK_COMBO_BOX (component->priv->find_text_combo), 0);


    // case check
    component->priv->case_check = create_check_with_mnemonic (*component, _("Case sensiti_ve"), "case_check");
    gtk_grid_attach (GTK_GRID (component->priv->grid), component->priv->case_check, 1, 4, 1, 1);
    gtk_widget_set_sensitive (component->priv->case_check, FALSE);
}


static void gnome_cmd_selection_profile_component_finalize (GObject *object)
{
    GnomeCmdSelectionProfileComponent *component = GNOME_CMD_SELECTION_PROFILE_COMPONENT (object);

    delete component->priv;

    G_OBJECT_CLASS (gnome_cmd_selection_profile_component_parent_class)->finalize (object);
}


static void gnome_cmd_selection_profile_component_class_init (GnomeCmdSelectionProfileComponentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_selection_profile_component_finalize;
}


GnomeCmdSelectionProfileComponent::GnomeCmdSelectionProfileComponent(GnomeCmdData::SearchProfile &p, GtkWidget *widget, gchar *label): profile(p)
{
    if (widget)
    {
        if (label)
            gtk_grid_attach (GTK_GRID (priv->grid), create_label_with_mnemonic (*this, label, widget), 0, 1, 1, 1);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_grid_attach (GTK_GRID (priv->grid), widget, 1, 1, 1, 1);
    }

    // if (!profile.name_patterns.empty())
    // {
        // g_list_foreach (profile.name_patterns.ents, (GFunc) combo_box_insert_text, priv->pattern_combo);
        // gtk_combo_box_set_active (GTK_COMBO_BOX (priv->pattern_combo), 0);
    // }

    // if (!profile.content_patterns.empty())
        // g_list_foreach (profile.content_patterns.ents, (GFunc) combo_box_insert_text, priv->find_text_combo);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->case_check), profile.match_case);

    gtk_widget_grab_focus (priv->pattern_combo);

    gtk_widget_show_all (priv->grid);

    g_signal_connect (priv->filter_type_combo, "changed", G_CALLBACK (Private::on_filter_type_changed), this);
    g_signal_connect (priv->find_text_check, "toggled", G_CALLBACK (Private::on_find_text_toggled), this);
}


void GnomeCmdSelectionProfileComponent::update()
{
    set_name_patterns_history(gnome_cmd_data.search_defaults.name_patterns.ents);
    set_content_patterns_history(gnome_cmd_data.search_defaults.content_patterns.ents);

    gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->pattern_combo))), profile.filename_pattern.c_str());
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->filter_type_combo), (int) profile.syntax);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->recurse_combo), profile.max_depth+1);
    gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->find_text_combo))), profile.text_pattern.c_str());
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->find_text_check), profile.content_search);
}


void GnomeCmdSelectionProfileComponent::copy()
{
    const char *pattern_text = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->pattern_combo))));
    stringify(profile.filename_pattern, pattern_text);
    profile.syntax = (Filter::Type) gtk_combo_box_get_active (GTK_COMBO_BOX (priv->filter_type_combo));
    profile.max_depth = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->recurse_combo)) - 1;
    const char *find_text = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->find_text_combo))));
    stringify(profile.text_pattern, find_text);
    profile.content_search = !profile.text_pattern.empty() && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->find_text_check));
    profile.match_case = profile.content_search && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_check));
}


void GnomeCmdSelectionProfileComponent::copy(GnomeCmdData::SearchProfile &profile_in)
{
    const char *pattern_text = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->pattern_combo))));
    stringify(profile_in.filename_pattern, pattern_text);
    profile_in.syntax = (Filter::Type) gtk_combo_box_get_active (GTK_COMBO_BOX (priv->filter_type_combo));
    profile_in.max_depth = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->recurse_combo)) - 1;
    const char *find_text = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->find_text_combo))));
    stringify(profile_in.text_pattern, find_text);
    profile_in.content_search = !profile_in.text_pattern.empty() && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->find_text_check));
    profile_in.match_case = profile_in.content_search && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_check));
}

void GnomeCmdSelectionProfileComponent::set_focus()
{
    gtk_widget_grab_focus (priv->pattern_combo);
}


void GnomeCmdSelectionProfileComponent::set_name_patterns_history(GList *history)
{
    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->pattern_combo));
    gtk_list_store_clear (GTK_LIST_STORE (store));

    g_list_foreach (history, (GFunc) combo_box_insert_text, priv->pattern_combo);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->pattern_combo), 0);
}


void GnomeCmdSelectionProfileComponent::set_content_patterns_history(GList *history)
{
    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->find_text_combo));
    gtk_list_store_clear (GTK_LIST_STORE (store));

    g_list_foreach (history, (GFunc) combo_box_insert_text, priv->find_text_combo);
}


void GnomeCmdSelectionProfileComponent::set_default_activation(GtkWindow *w)
{
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->pattern_combo)), "activate", G_CALLBACK (gtk_window_activate_default), w);
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->find_text_combo)), "activate", G_CALLBACK (gtk_window_activate_default), w);
}
