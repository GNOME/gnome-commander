/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>
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

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area),6);

    GtkWidget *table, *align, *label, *entry, *check;

    table = gtk_table_new (3, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (table), 5);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), table);

    label = gtk_label_new_with_mnemonic (_("_Search for:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

    entry = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_entry_set_text (GTK_ENTRY (entry), rx->pattern.c_str());
    g_object_set_data (G_OBJECT (dialog), "pattern", entry);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 0, 1);

    label = gtk_label_new_with_mnemonic (_("_Replace with:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

    entry = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_entry_set_text (GTK_ENTRY (entry), rx->replacement.c_str());
    g_object_set_data (G_OBJECT (dialog), "replace", entry);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 1, 2);

    align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 6, 0, 12, 0);
    gtk_table_attach_defaults (GTK_TABLE (table), align, 0, 2, 2, 3);

    check = gtk_check_button_new_with_mnemonic (_("_Match case"));
    g_object_set_data (G_OBJECT (dialog), "match_case", check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), rx ? rx->match_case : FALSE);
    gtk_container_add (GTK_CONTAINER (align), check);

#if !GLIB_CHECK_VERSION (2, 14, 0)
    box = gnome_cmd_hint_box_new (_("Some regular expressions functionality is disabled. "
                                    "To enable it's necessary to build GNOME Commander with GLib ≥ 2.14. "
                                    "Please contact your package maintainer about that."));
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), box);
#endif

    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), rx);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return result==GTK_RESPONSE_OK;
}
