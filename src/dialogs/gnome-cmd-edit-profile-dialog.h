/** 
 * @FILE gnome-cmd-edit-profile-dialog.h
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

#include "gnome-cmd-data.h"

namespace GnomeCmd
{
    template <typename PROFILE, typename COMPONENT>
    class EditProfileDialog
    {
        COMPONENT *component;
        const char *help_id;
        gint result;

        enum {GCMD_RESPONSE_RESET=123};

        static void response_callback(GtkDialog *dialog, int response_id, EditProfileDialog<PROFILE,COMPONENT> *dlg);

      public:

        EditProfileDialog(GtkWindow *parent, PROFILE &profile, const char *id);

        operator gboolean () const       {  return result==GTK_RESPONSE_OK;  }
    };

    template <typename PROFILE, typename COMPONENT>
    void EditProfileDialog<PROFILE,COMPONENT>::response_callback(GtkDialog *dialog, int response_id, EditProfileDialog<PROFILE,COMPONENT> *dlg)
    {
        switch (response_id)
        {
            case GTK_RESPONSE_HELP:
                gnome_cmd_help_display ("gnome-commander.xml", dlg->help_id);
                g_signal_stop_emission_by_name (dialog, "response");
                break;

            case GCMD_RESPONSE_RESET:
                dlg->component->profile.reset();
                dlg->component->update();
                g_signal_stop_emission_by_name (dialog, "response");
                break;

            case GTK_RESPONSE_OK:
                dlg->component->profile.name = gtk_editable_get_text (GTK_EDITABLE (lookup_widget (GTK_WIDGET (dialog), "name")));
                dlg->component->copy();
                break;

            case GTK_RESPONSE_NONE:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_CANCEL:
                break;

            default :
                g_assert_not_reached ();
        }
    }

    template <typename PROFILE, typename COMPONENT>
    EditProfileDialog<PROFILE,COMPONENT>::EditProfileDialog(GtkWindow *parent, PROFILE &profile, const char *id):  help_id(id)
    {
        GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Edit Profile"), parent,
                                                         GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                         _("_Help"), GTK_RESPONSE_HELP,
                                                         _("Reset"), GCMD_RESPONSE_RESET,
                                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                                         _("_OK"), GTK_RESPONSE_OK,
                                                         NULL);

        GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

        // HIG defaults
        gtk_widget_set_margin_top (content_area, 10);
        gtk_widget_set_margin_bottom (content_area, 10);
        gtk_widget_set_margin_start (content_area, 10);
        gtk_widget_set_margin_end (content_area, 10);
        gtk_box_set_spacing (GTK_BOX (content_area), 6);

        gchar *str = g_strdup_printf ("<b>%s</b>", _("_Name"));
        GtkWidget *label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (content_area), label);

        GtkWidget *entry = gtk_entry_new ();
        g_object_set_data (G_OBJECT (dialog), "name", entry);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
        gtk_editable_set_text (GTK_EDITABLE (entry), profile.name.c_str());
        gtk_widget_set_margin_bottom (entry, 6);
        gtk_widget_set_margin_start (entry, 12);
        gtk_box_append (GTK_BOX (content_area), entry);

        component = new COMPONENT(profile);
        gtk_box_append (GTK_BOX (content_area), *component);

        component->update();

        gtk_widget_show_all (content_area);

        gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

        g_signal_connect (dialog, "response", G_CALLBACK (response_callback), this);

        result = gtk_dialog_run (GTK_DIALOG (dialog));

        gtk_window_destroy (GTK_WINDOW (dialog));
    }
}
