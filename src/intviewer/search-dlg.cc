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


// HEX history doesn't work yet
#undef HEX_HISTORY

void entry_changed(GtkEntry *entry, gpointer  user_data);
static void search_dlg_destroy (GtkWidget *object);
static void search_dlg_action_response(GtkDialog *dlg, gint arg1, GViewerSearchDlg *sdlg);

struct GViewerSearchDlgPrivate
{
    GtkWidget  *grid;
    GtkWidget  *label;
    GtkWidget  *entry;
    GtkWidget  *text_mode, *hex_mode;
    GtkWidget  *case_sensitive_checkbox;

    SEARCHMODE searchmode;

    gchar      *search_text_string;
    guint8     *search_hex_buffer;
    guint      search_hex_buflen;
};


G_DEFINE_TYPE_WITH_PRIVATE (GViewerSearchDlg, gviewer_search_dlg, GTK_TYPE_DIALOG)


guint8 *gviewer_search_dlg_get_search_hex_buffer (GViewerSearchDlg *sdlg, /*out*/ guint &buflen)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), nullptr);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));
    g_return_val_if_fail (priv->search_hex_buffer != nullptr, nullptr);
    g_return_val_if_fail (priv->search_hex_buflen>0, nullptr);

    guint8 *result = g_new0 (guint8, priv->search_hex_buflen);
    memcpy (result, priv->search_hex_buffer, priv->search_hex_buflen);

    buflen = priv->search_hex_buflen;

    return result;
}


gchar *gviewer_search_dlg_get_search_text_string (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), nullptr);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));
    g_return_val_if_fail (priv->search_text_string != nullptr, nullptr);

    return g_strdup (priv->search_text_string);
}


SEARCHMODE gviewer_search_dlg_get_search_mode (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), SEARCH_MODE_TEXT);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    return priv->searchmode;
}


gboolean gviewer_search_dlg_get_case_sensitive (GViewerSearchDlg *sdlg)
{
    g_return_val_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg), TRUE);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_sensitive_checkbox));
}


inline void set_text_history (GViewerSearchDlg *sdlg)
{
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->entry));
    for (GList *i=gnome_cmd_data.intviewer_defaults.text_patterns.ents; i; i=i->next)
        if (i->data)
        {
            GtkTreeIter iter;
            gtk_list_store_append (GTK_LIST_STORE (store), &iter);
            gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, (const gchar *) i->data, -1);
        }
}


inline void set_text_mode (GViewerSearchDlg *sdlg)
{
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    gtk_widget_grab_focus (priv->entry);
    priv->searchmode = SEARCH_MODE_TEXT;
    gtk_widget_set_sensitive(priv->case_sensitive_checkbox, TRUE);

    GtkEntry *w = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->entry)));
    entry_changed (w, (gpointer) sdlg);
}


static void set_hex_mode (GViewerSearchDlg *sdlg)
{
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

#if defined(HEX_HISTORY)
    for (GList *i=gnome_cmd_data.intviewer_defaults.hex_patterns.ents; i; i=i->next)
        if (i->data)
            gtk_combo_box_prepend_text (GTK_COMBO_BOX (sdlg->priv->entry), (gchar *) i->data);
#endif
    gtk_widget_grab_focus (priv->entry);

    priv->searchmode = SEARCH_MODE_HEX;

    gtk_widget_set_sensitive(priv->case_sensitive_checkbox, FALSE);

    // Check if the text in the GtkEntryBox has hex value, otherwise disable the "Find" button
    GtkEntry *w = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->entry)));
    entry_changed (w, (gpointer) sdlg);
}


static void search_mode_text (GtkToggleButton *btn, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (btn != nullptr);
    g_return_if_fail (sdlg != nullptr);

    if (!gtk_toggle_button_get_active(btn))
        return;

    set_text_mode (sdlg);
}


static void search_mode_hex (GtkToggleButton *btn, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (btn != nullptr);
    g_return_if_fail (sdlg != nullptr);

    if (gtk_toggle_button_get_active(btn))
        set_hex_mode(sdlg);
}


static void search_dlg_action_response (GtkDialog *dlg, gint arg1, GViewerSearchDlg *sdlg)
{
    g_return_if_fail (IS_GVIEWER_SEARCH_DLG (sdlg));
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    if (arg1 != GTK_RESPONSE_OK)
        return;

    g_return_if_fail (priv->search_text_string==nullptr);
    g_return_if_fail (priv->search_hex_buffer==nullptr);

    const gchar *pattern = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->entry))));

    priv->search_text_string = g_strdup (pattern);

    if (priv->searchmode==SEARCH_MODE_TEXT)   // text mode search
    {
        gnome_cmd_data.intviewer_defaults.text_patterns.add(pattern);

        gnome_cmd_data.intviewer_defaults.case_sensitive =
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_sensitive_checkbox));
    }
    else    // hex mode search
    {
        priv->search_hex_buffer = text2hex (pattern, priv->search_hex_buflen);
        g_return_if_fail (priv->search_hex_buffer != nullptr);

        gnome_cmd_data.intviewer_defaults.hex_patterns.add(pattern);
    }
}


static void gviewer_search_dlg_class_init (GViewerSearchDlgClass *klass)
{
    GTK_WIDGET_CLASS(klass)->destroy = search_dlg_destroy;
}


void entry_changed (GtkEntry *entry, gpointer  user_data)
{
    g_return_if_fail (IS_GVIEWER_SEARCH_DLG (user_data));

    gboolean enable = FALSE;

    GViewerSearchDlg *sdlg = GVIEWER_SEARCH_DLG (user_data);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    if (priv->searchmode==SEARCH_MODE_HEX)
    {
        // Only if the use entered a valid hex string, enable the "find" button
        guint len;
        guint8 *buf = text2hex (gtk_entry_get_text (entry), len);

        enable = buf != nullptr && len>0;
        g_free (buf);
    }
    else
    {
        // SEARCH_MODE_TEXT
        enable = strlen (gtk_entry_get_text (entry))>0;
    }
    gtk_dialog_set_response_sensitive (GTK_DIALOG (user_data), GTK_RESPONSE_OK, enable);
}


static void gviewer_search_dlg_init (GViewerSearchDlg *sdlg)
{
    GtkDialog *dlg = GTK_DIALOG (sdlg);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (sdlg));

    GtkGrid *grid;
    GtkWidget *entry;

    priv->searchmode = (SEARCHMODE) gnome_cmd_data.intviewer_defaults.search_mode;

    gtk_window_set_title (GTK_WINDOW (dlg), _("Find"));
    gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
    gtk_dialog_add_button (dlg, _("_Cancel"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button (dlg, _("_OK"), GTK_RESPONSE_OK);
    gtk_dialog_set_default_response (dlg, GTK_RESPONSE_OK);

    g_signal_connect_swapped (GTK_WIDGET (dlg), "response", G_CALLBACK (search_dlg_action_response), sdlg);

    // 2x4 Table
    grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (dlg)), GTK_WIDGET (grid), FALSE, TRUE, 0);
    priv->grid = GTK_WIDGET (grid);

    // Label
    priv->label = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic (GTK_LABEL (priv->label), "_Search for:");
    gtk_grid_attach(grid, priv->label, 0, 0, 1, 1);

    // Entry Box
    priv->entry = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING)));
    entry = gtk_bin_get_child (GTK_BIN (priv->entry));
    g_object_set(entry, "activates-default", TRUE, nullptr);
    g_signal_connect (entry, "changed", G_CALLBACK (entry_changed), sdlg);
    gtk_grid_attach(grid, priv->entry, 1, 0, 2, 1);

    set_text_history (sdlg);

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
    if (priv->searchmode==SEARCH_MODE_HEX)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->hex_mode), TRUE);
        set_hex_mode (sdlg);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->text_mode), TRUE);
        set_text_mode (sdlg);
    }

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->case_sensitive_checkbox), gnome_cmd_data.intviewer_defaults.case_sensitive);

    if (!gnome_cmd_data.intviewer_defaults.text_patterns.empty())
        gtk_entry_set_text(GTK_ENTRY(entry), gnome_cmd_data.intviewer_defaults.text_patterns.front());

    gtk_widget_grab_focus (priv->entry);
}


static void search_dlg_destroy (GtkWidget *object)
{
    g_return_if_fail (IS_GVIEWER_SEARCH_DLG (object));

    GViewerSearchDlg *w = GVIEWER_SEARCH_DLG (object);
    auto priv = static_cast<GViewerSearchDlgPrivate*>(gviewer_search_dlg_get_instance_private (w));

    gnome_cmd_data.intviewer_defaults.search_mode = priv->searchmode;

    g_clear_pointer (&priv->search_text_string, g_free);

    GTK_WIDGET_CLASS (gviewer_search_dlg_parent_class)->destroy (object);
}


GtkWidget* gviewer_search_dlg_new (GtkWindow *parent)
{
    auto dlg = static_cast<GViewerSearchDlg*> (g_object_new (gviewer_search_dlg_get_type(), nullptr));

    return GTK_WIDGET (dlg);
}
