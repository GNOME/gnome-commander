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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-profile-component.h"
#include "gnome-cmd-edit-profile-dialog.h"
#include "utils.h"

using namespace std;


enum {GCMD_RESPONSE_RESET=123};


static void response_callback (GtkDialog *dialog, int response_id, GnomeCmdProfileComponent *profile)
{
    switch (response_id)
    {
        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-advanced-rename");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GCMD_RESPONSE_RESET:
            profile->profile.reset();
            profile->update();
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GTK_RESPONSE_OK:
            profile->profile.name = gtk_entry_get_text (GTK_ENTRY (lookup_widget (GTK_WIDGET (dialog), "name")));
            profile->copy();
            break;

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
            break;

        default :
            g_assert_not_reached ();
    }
}


gboolean gnome_cmd_edit_profile_dialog_new (GtkWindow *parent, GnomeCmdData::AdvrenameConfig::Profile &profile)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Edit Profile"), parent,
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                     _("Reset"), GCMD_RESPONSE_RESET,
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

    GtkWidget *vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

    gchar *str = g_strdup_printf ("<b>%s</b>", _("_Name"));
    GtkWidget *label = gtk_label_new_with_mnemonic (str);
    g_free (str);

    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    GtkWidget *align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 6, 12, 0);
    gtk_container_add (GTK_CONTAINER (vbox), align);

    GtkWidget *entry = gtk_entry_new ();
    g_object_set_data (G_OBJECT (dialog), "name", entry);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_entry_set_text (GTK_ENTRY (entry), profile.name.c_str());
    gtk_container_add (GTK_CONTAINER (align), entry);

    GnomeCmdProfileComponent *profile_component = new GnomeCmdProfileComponent(profile);
    gtk_container_add (GTK_CONTAINER (vbox), *profile_component);

    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (response_callback), profile_component);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return result==GTK_RESPONSE_OK;
}
