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
#include "utils.h"
#include "widget-factory.h"

using namespace std;


struct GnomeCmdSelectionProfileComponentPrivate
{
    GtkWidget *grid;
    GtkWidget *filter_type_combo;
    GtkWidget *pattern_combo;
    GtkWidget *recurse_label;
    GtkWidget *recurse_combo;
    GtkWidget *find_text_combo;
    GtkWidget *find_text_check;
    GtkWidget *case_check;

    SearchProfile *profile;
};


static GnomeCmdSelectionProfileComponentPrivate *profile_component_private (GnomeCmdSelectionProfileComponent *component)
{
    return (GnomeCmdSelectionProfileComponentPrivate *) g_object_get_data (G_OBJECT (component), "search-profile-component-priv");
}


static void on_filter_type_changed(GtkComboBox *combo, GnomeCmdSelectionProfileComponent *component)
{
    auto priv = profile_component_private (component);
    gtk_widget_grab_focus (priv->pattern_combo);
}


static void on_find_text_toggled(GtkCheckButton *checkbutton, GnomeCmdSelectionProfileComponent *component)
{
    auto priv = profile_component_private (component);
    if (gtk_check_button_get_active (checkbutton))
    {
        gtk_widget_set_sensitive (priv->find_text_combo, TRUE);
        gtk_widget_set_sensitive (priv->case_check, TRUE);
        gtk_widget_grab_focus (priv->find_text_combo);
    }
    else
    {
        gtk_widget_set_sensitive (priv->find_text_combo, FALSE);
        gtk_widget_set_sensitive (priv->case_check, FALSE);
    }
}


static void combo_box_insert_text (const gchar *text, GtkComboBox *widget)
{
    if (text == nullptr || *text == '\0')
        return;

    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

    GtkTreeIter iter;
    gtk_list_store_append (GTK_LIST_STORE (store), &iter);
    gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, text, -1);
}


extern "C" void gnome_cmd_selection_profile_component_init (GnomeCmdSelectionProfileComponent *component)
{
    auto priv = g_new0 (GnomeCmdSelectionProfileComponentPrivate, 1);
    g_object_set_data_full (G_OBJECT (component), "search-profile-component-priv", priv, g_free);

    priv->grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (priv->grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (priv->grid), 12);
    gtk_widget_set_parent (GTK_WIDGET (priv->grid), GTK_WIDGET (component));


    // search for
    priv->filter_type_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->filter_type_combo), _("Path matches regex:"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->filter_type_combo), _("Name contains:"));
    priv->pattern_combo = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING)));
    g_object_set (priv->pattern_combo, "entry-text-column", 0, nullptr);
    gtk_grid_attach (GTK_GRID (priv->grid), priv->filter_type_combo, 0, 0, 1, 1);
    gtk_widget_set_hexpand (priv->pattern_combo, TRUE);
    gtk_grid_attach (GTK_GRID (priv->grid), priv->pattern_combo, 1, 0, 1, 1);


    // recurse check
    priv->recurse_combo = gtk_combo_box_text_new ();

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->recurse_combo), _("Unlimited depth"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->recurse_combo), _("Current directory only"));
    for (int i=1; i<=40; ++i)
    {
       gchar *item = g_strdup_printf (ngettext("%i level", "%i levels", i), i);
       combo_box_insert_text (item, GTK_COMBO_BOX (priv->recurse_combo));
       g_free (item);
    }

    priv->recurse_label = create_label_with_mnemonic (GTK_WIDGET (component), _("Search _recursively:"), priv->recurse_combo);
    gtk_label_set_xalign (GTK_LABEL (priv->recurse_label), 0.0);
    gtk_grid_attach (GTK_GRID (priv->grid), priv->recurse_label, 0, 1, 1, 1);
    gtk_widget_set_hexpand (priv->recurse_combo, TRUE);
    gtk_grid_attach (GTK_GRID (priv->grid), priv->recurse_combo, 1, 1, 1, 1);


    // find text
    priv->find_text_check = create_check_with_mnemonic (GTK_WIDGET (component), _("Contains _text:"), "find_text");
    gtk_grid_attach (GTK_GRID (priv->grid), priv->find_text_check, 0, 2, 1, 1);

    priv->find_text_combo = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING)));
    g_object_set (priv->find_text_combo, "entry-text-column", 0, nullptr);
    gtk_widget_set_hexpand (priv->find_text_combo, TRUE);
    gtk_grid_attach (GTK_GRID (priv->grid), priv->find_text_combo, 1, 2, 1, 1);
    gtk_widget_set_sensitive (priv->find_text_combo, FALSE);

    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->find_text_combo), 0);


    // case check
    priv->case_check = create_check_with_mnemonic (GTK_WIDGET (component), _("Case sensiti_ve"), "case_check");
    gtk_grid_attach (GTK_GRID (priv->grid), priv->case_check, 1, 3, 1, 1);
    gtk_widget_set_sensitive (priv->case_check, FALSE);
}


extern "C" void gnome_cmd_selection_profile_component_dispose (GnomeCmdSelectionProfileComponent *component)
{
    auto priv = profile_component_private (component);

    g_clear_object (&priv->profile);
}


GnomeCmdSelectionProfileComponent *gnome_cmd_search_profile_component_new (SearchProfile *profile, GtkSizeGroup *labels_size_group)
{
    auto component = (GnomeCmdSelectionProfileComponent *) g_object_new (gnome_cmd_selection_profile_component_get_type (), nullptr);
    auto priv = profile_component_private (component);

    g_set_object (&priv->profile, profile);

    if (labels_size_group)
    {
        gtk_size_group_add_widget (labels_size_group, priv->filter_type_combo);
        gtk_size_group_add_widget (labels_size_group, priv->recurse_label);
        gtk_size_group_add_widget (labels_size_group, priv->find_text_check);
    }

    gboolean match_case;
    g_object_get (profile, "match-case", &match_case, nullptr);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (priv->case_check), match_case);

    gtk_widget_grab_focus (priv->pattern_combo);

    g_signal_connect (priv->filter_type_combo, "changed", G_CALLBACK (on_filter_type_changed), component);
    g_signal_connect (priv->find_text_check, "toggled", G_CALLBACK (on_find_text_toggled), component);

    return g_object_ref_sink (component);
}


void gnome_cmd_search_profile_component_update (GnomeCmdSelectionProfileComponent *component)
{
    auto priv = profile_component_private (component);

    gnome_cmd_search_profile_component_set_name_patterns_history (component, gnome_cmd_data.search_defaults.name_patterns.ents);
    gnome_cmd_search_profile_component_set_content_patterns_history (component, gnome_cmd_data.search_defaults.content_patterns.ents);

    gchar *filename_pattern;
    gchar *text_pattern;
    gboolean match_case;
    gint max_depth;
    gint syntax;
    gboolean content_search;

    g_object_get (priv->profile,
        "filename-pattern", &filename_pattern,
        "text-pattern", &text_pattern,
        "match-case", &match_case,
        "max-depth", &max_depth,
        "syntax", &syntax,
        "content-search", &content_search,
        nullptr);

    gtk_editable_set_text (GTK_EDITABLE (gtk_combo_box_get_child (GTK_COMBO_BOX (priv->pattern_combo))), filename_pattern);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->filter_type_combo), syntax);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->recurse_combo), max_depth + 1);
    gtk_editable_set_text (GTK_EDITABLE (gtk_combo_box_get_child (GTK_COMBO_BOX (priv->find_text_combo))), text_pattern);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (priv->find_text_check), content_search);

    g_free (filename_pattern);
    g_free (text_pattern);
}


void gnome_cmd_search_profile_component_copy (GnomeCmdSelectionProfileComponent *component)
{
    auto priv = profile_component_private (component);

    const gchar *text_pattern = gtk_editable_get_text (GTK_EDITABLE (gtk_combo_box_get_child (GTK_COMBO_BOX (priv->find_text_combo))));
    gboolean content_search = (text_pattern != nullptr && text_pattern[0] != '\0') && gtk_check_button_get_active (GTK_CHECK_BUTTON (priv->find_text_check));

    g_object_set (priv->profile,
        "filename-pattern", gtk_editable_get_text (GTK_EDITABLE (gtk_combo_box_get_child (GTK_COMBO_BOX (priv->pattern_combo)))),
        "max-depth", gtk_combo_box_get_active (GTK_COMBO_BOX (priv->recurse_combo)) - 1,
        "syntax", gtk_combo_box_get_active (GTK_COMBO_BOX (priv->filter_type_combo)),
        "text-pattern", text_pattern,
        "content-search", content_search,
        "match-case", content_search && gtk_check_button_get_active (GTK_CHECK_BUTTON (priv->case_check)),
        nullptr);
}


void gnome_cmd_search_profile_component_set_focus(GnomeCmdSelectionProfileComponent *component)
{
    auto priv = profile_component_private (component);
    gtk_widget_grab_focus (priv->pattern_combo);
}


void gnome_cmd_search_profile_component_set_name_patterns_history (GnomeCmdSelectionProfileComponent *component, GList *history)
{
    auto priv = profile_component_private (component);

    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->pattern_combo));
    gtk_list_store_clear (GTK_LIST_STORE (store));

    g_list_foreach (history, (GFunc) combo_box_insert_text, priv->pattern_combo);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->pattern_combo), 0);
}


void gnome_cmd_search_profile_component_set_content_patterns_history (GnomeCmdSelectionProfileComponent *component, GList *history)
{
    auto priv = profile_component_private (component);

    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->find_text_combo));
    gtk_list_store_clear (GTK_LIST_STORE (store));

    g_list_foreach (history, (GFunc) combo_box_insert_text, priv->find_text_combo);
}


void gnome_cmd_search_profile_component_set_default_activation (GnomeCmdSelectionProfileComponent *component, GtkWindow *w)
{
    auto priv = profile_component_private (component);

    gtk_entry_set_activates_default (GTK_ENTRY (gtk_combo_box_get_child (GTK_COMBO_BOX (priv->pattern_combo))), TRUE);
    gtk_entry_set_activates_default (GTK_ENTRY (gtk_combo_box_get_child (GTK_COMBO_BOX (priv->find_text_combo))), TRUE);
}
