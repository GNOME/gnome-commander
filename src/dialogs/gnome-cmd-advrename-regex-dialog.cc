/** 
 * @file gnome-cmd-advrename-regex-dialog.cc
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
#include <stdio.h>
#include <regex.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-advrename-regex-dialog.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-hintbox.h"
#include "utils.h"

using namespace std;


static void response_callback (GtkDialog *dialog, int response_id, GnomeCmd::RegexReplace *rx)
{
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            rx->assign(gtk_entry_get_text (GTK_ENTRY (lookup_widget (GTK_WIDGET (dialog), "pattern"))),
                       gtk_entry_get_text (GTK_ENTRY (lookup_widget (GTK_WIDGET (dialog), "replace"))),
                       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lookup_widget (GTK_WIDGET (dialog), "match_case"))));
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        default :
            g_assert_not_reached ();
    }
}


gboolean gnome_cmd_advrename_regex_dialog_new (const gchar *title, GtkWindow *parent, GnomeCmd::RegexReplace *rx)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons (title, parent,
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     // GTK_STOCK_HELP, GTK_RESPONSE_HELP,             //  FIXME: ???
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                     NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);
    gtk_box_set_spacing (GTK_BOX (content_area),6);

    GtkWidget *grid, *label, *entry, *check;

    grid = gtk_grid_new ();
    gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_container_add (GTK_CONTAINER (content_area), grid);

    label = gtk_label_new_with_mnemonic (_("_Search for:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    entry = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_entry_set_text (GTK_ENTRY (entry), rx ? rx->pattern.c_str() : nullptr);
    g_object_set_data (G_OBJECT (dialog), "pattern", entry);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

    label = gtk_label_new_with_mnemonic (_("_Replace with:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

    entry = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_entry_set_text (GTK_ENTRY (entry), rx ? rx->replacement.c_str() : nullptr);
    g_object_set_data (G_OBJECT (dialog), "replace", entry);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);

    check = gtk_check_button_new_with_mnemonic (_("_Match case"));
    g_object_set_data (G_OBJECT (dialog), "match_case", check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), rx ? rx->match_case : FALSE);
    gtk_widget_set_margin_top (check, 6);
    gtk_widget_set_margin_left (check, 12);
    gtk_grid_attach (GTK_GRID (grid), check, 0, 2, 2, 1);

    gtk_widget_show_all (content_area);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), rx);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return result==GTK_RESPONSE_OK;
}
