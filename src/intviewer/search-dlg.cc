/**
 * @file search-dlg.cc
 * @brief Part of GNOME Commander - A GNOME based file manager
 *
 * @copyright (C) 2006 Assaf Gordon\n
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
 *
 */

#include <config.h>
#include <string.h>

#include "libgviewer.h"
#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"

using namespace std;


static void entry_changed(GtkComboBox *combo, gpointer user_data);
static void search_dlg_action_response(GtkDialog *dlg, gint arg1, GViewerSearchDlg *sdlg);

struct GViewerSearchDlgPrivate
{
    GtkWidget  *grid;
    GtkWidget  *label;
    GtkWidget  *entry;
    GtkWidget  *text_mode, *hex_mode;
    GtkWidget  *case_sensitive_checkbox;
};


G_DEFINE_TYPE_WITH_PRIVATE (GViewerSearchDlg, gviewer_search_dlg, GTK_TYPE_DIALOG)


guint8 *gviewer_search_dlg_get_search_hex_buffer (GViewerSearchDlg *sdlg, /*out*/ guint &buflen)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), nullptr);

    gchar *text = gviewer_search_dlg_get_search_text_string (sdlg);
    guint8 *buf = text2hex (text, buflen);
    g_free (text);
    return buf;
}


gchar *gviewer_search_dlg_get_search_text_string (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), nullptr);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    return gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->entry));
}


SEARCHMODE gviewer_search_dlg_get_search_mode (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), SEARCH_MODE_TEXT);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->text_mode)) ? SEARCH_MODE_TEXT : SEARCH_MODE_HEX;
}


gboolean gviewer_search_dlg_get_case_sensitive (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), TRUE);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_sensitive_checkbox));
}


inline void set_history (GViewerSearchDlg *sdlg, GList *history)
{
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (priv->entry));
    for (GList *i = history; i; i = i->next)
    {
        const gchar *item = (const gchar *) i->data;
        if (item && *item)
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->entry), item);
    }
}


inline void set_text_mode (GViewerSearchDlg *sdlg)
{
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    gtk_widget_grab_focus (priv->entry);
    gtk_widget_set_sensitive (priv->case_sensitive_checkbox, TRUE);

    entry_changed (GTK_COMBO_BOX (priv->entry), (gpointer) sdlg);
    set_history (sdlg, gnome_cmd_data.intviewer_defaults.text_patterns.ents);
}


static void set_hex_mode (GViewerSearchDlg *sdlg)
{
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    gtk_widget_grab_focus (priv->entry);
    gtk_widget_set_sensitive (priv->case_sensitive_checkbox, FALSE);

    entry_changed (GTK_COMBO_BOX (priv->entry), (gpointer) sdlg);
    set_history (sdlg, gnome_cmd_data.intviewer_defaults.hex_patterns.ents);
}


static void search_mode_text (GtkToggleButton *btn, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (btn != nullptr);
    g_return_if_fail (sdlg != nullptr);

    if (gtk_toggle_button_get_active (btn))
        set_text_mode (sdlg);
}


static void search_mode_hex (GtkToggleButton *btn, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (btn != nullptr);
    g_return_if_fail (sdlg != nullptr);

    if (gtk_toggle_button_get_active (btn))
        set_hex_mode(sdlg);
}


static void search_dlg_action_response (GtkDialog *dlg, gint response_id, GViewerSearchDlg *sdlg)
{
    if (response_id != GTK_RESPONSE_OK)
        return;

    SEARCHMODE searchmode = gviewer_search_dlg_get_search_mode (sdlg);
    gchar *pattern = gviewer_search_dlg_get_search_text_string (sdlg);

    gnome_cmd_data.intviewer_defaults.search_mode = searchmode;

    if (searchmode == SEARCH_MODE_TEXT)
    {
        gnome_cmd_data.intviewer_defaults.text_patterns.add(pattern);
        gnome_cmd_data.intviewer_defaults.case_sensitive = gviewer_search_dlg_get_case_sensitive (sdlg);
    }
    else
    {
        gnome_cmd_data.intviewer_defaults.hex_patterns.add(pattern);
    }
    g_free (pattern);
}


static void gviewer_search_dlg_class_init (GViewerSearchDlgClass *klass)
{
}


static void entry_changed (GtkComboBox *combo, gpointer user_data)
{
    g_return_if_fail (IS_GVIEWER_SEARCH_DLG (user_data));
    GViewerSearchDlg *sdlg = GVIEWER_SEARCH_DLG (user_data);

    SEARCHMODE searchmode = gviewer_search_dlg_get_search_mode (sdlg);

    gboolean enable = FALSE;
    if (searchmode == SEARCH_MODE_HEX)
    {
        // Only if the use entered a valid hex string, enable the "find" button
        guint len;
        guint8 *buf = gviewer_search_dlg_get_search_hex_buffer (sdlg, len);

        enable = buf != nullptr && len>0;
        g_free (buf);
    }
    else
    {
        // SEARCH_MODE_TEXT
        gchar *text = gviewer_search_dlg_get_search_text_string (sdlg);
        enable = strlen (text) > 0;
        g_free (text);
    }
    gtk_dialog_set_response_sensitive (GTK_DIALOG (sdlg), GTK_RESPONSE_OK, enable);
}


static void gviewer_search_dlg_init (GViewerSearchDlg *sdlg)
{
    GtkDialog *dlg = GTK_DIALOG (sdlg);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    GtkGrid *grid;

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (sdlg));
    gtk_widget_set_margin_top (content_area, 10);
    gtk_widget_set_margin_bottom (content_area, 10);
    gtk_widget_set_margin_start (content_area, 10);
    gtk_widget_set_margin_end (content_area, 10);
    gtk_box_set_spacing (GTK_BOX (content_area), 12);

    gtk_dialog_add_button (dlg, _("_Cancel"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button (dlg, _("_OK"), GTK_RESPONSE_OK);
    gtk_dialog_set_default_response (dlg, GTK_RESPONSE_OK);

    g_signal_connect_swapped (GTK_WIDGET (sdlg), "response", G_CALLBACK (search_dlg_action_response), sdlg);

    // 2x4 Table
    grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
    gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (dlg)), GTK_WIDGET (grid));
    priv->grid = GTK_WIDGET (grid);

    // Label
    priv->label = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic (GTK_LABEL (priv->label), "_Search for:");
    gtk_grid_attach(grid, priv->label, 0, 0, 1, 1);

    // Entry Box
    priv->entry = gtk_combo_box_text_new_with_entry ();
    g_object_set (gtk_bin_get_child (GTK_BIN (priv->entry)), "activates-default", TRUE, nullptr);
    g_signal_connect (priv->entry, "changed", G_CALLBACK (entry_changed), sdlg);
    gtk_grid_attach(grid, priv->entry, 1, 0, 2, 1);

    // Search mode radio buttons
    priv->text_mode = gtk_radio_button_new_with_mnemonic(nullptr, _("_Text"));
    priv->hex_mode = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON (priv->text_mode), _("_Hexadecimal"));

    g_signal_connect (priv->text_mode, "toggled", G_CALLBACK (search_mode_text), sdlg);
    g_signal_connect (priv->hex_mode, "toggled", G_CALLBACK (search_mode_hex), sdlg);

    gtk_grid_attach(grid, priv->text_mode, 1, 1, 1, 1);
    gtk_grid_attach(grid, priv->hex_mode, 1, 2, 1, 1);

    // Case-Sensitive Checkbox
    priv->case_sensitive_checkbox = gtk_check_button_new_with_mnemonic(_("_Match case"));
    gtk_grid_attach(grid, priv->case_sensitive_checkbox, 2, 1, 1, 1);

    gtk_widget_show_all(priv->grid);
    gtk_widget_show (GTK_WIDGET (dlg));

    // Restore the previously saved state (loaded with "load_search_dlg_state")
    SEARCHMODE searchmode = (SEARCHMODE) gnome_cmd_data.intviewer_defaults.search_mode;
    if (searchmode == SEARCH_MODE_HEX)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->hex_mode), TRUE);
        set_hex_mode (sdlg);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->text_mode), TRUE);
        set_text_mode (sdlg);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->case_sensitive_checkbox), gnome_cmd_data.intviewer_defaults.case_sensitive);
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->entry), 0);

    gtk_widget_grab_focus (priv->entry);
}


GtkWidget* gviewer_search_dlg_new (GtkWindow *parent)
{
    auto dlg = static_cast<GViewerSearchDlg*> (g_object_new (gviewer_search_dlg_get_type(),
        "title", _("Find"),
        "modal", TRUE,
        "transient-for", parent,
        "resizable", FALSE,
        nullptr));

    return GTK_WIDGET (dlg);
}
