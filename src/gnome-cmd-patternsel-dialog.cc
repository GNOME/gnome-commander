/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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
#include "gnome-cmd-data.h"
#include "gnome-cmd-patternsel-dialog.h"
#include "gnome-cmd-file-list.h"
#include "utils.h"

using namespace std;


struct GnomeCmdPatternselDialogPrivate
{
    GnomeCmdFileList *fl;

    GtkWidget *case_check;
    GtkWidget *pattern_combo;
    GtkWidget *pattern_entry;

    gboolean mode;
};


G_DEFINE_TYPE (GnomeCmdPatternselDialog, gnome_cmd_patternsel_dialog, GNOME_CMD_TYPE_DIALOG)


static void on_ok (GtkButton *button, GnomeCmdPatternselDialog *dialog)
{
    g_return_if_fail (GNOME_CMD_IS_PATTERNSEL_DIALOG (dialog));

    gboolean case_sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check));
    gnome_cmd_data.search_defaults.default_profile.syntax = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lookup_widget (GTK_WIDGET (dialog), "regex_radio"))) ? Filter::TYPE_REGEX : Filter::TYPE_FNMATCH;

    const gchar *s = gtk_entry_get_text (GTK_ENTRY (dialog->priv->pattern_entry));

    Filter pattern(s, case_sens, gnome_cmd_data.search_defaults.default_profile.syntax);
    
    if (dialog->priv->mode)
        dialog->priv->fl->select(pattern);
    else
        dialog->priv->fl->unselect(pattern);

    gnome_cmd_data.search_defaults.name_patterns.add(s);

    gtk_widget_hide (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdPatternselDialog *dialog)
{
    gtk_widget_hide (GTK_WIDGET (dialog));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_patternsel_dialog_finalize (GObject *object)
{
    GnomeCmdPatternselDialog *dialog = GNOME_CMD_PATTERNSEL_DIALOG (object);

    g_free (dialog->priv);

    G_OBJECT_CLASS (gnome_cmd_patternsel_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_patternsel_dialog_init (GnomeCmdPatternselDialog *dialog)
{
    GnomeCmdData::SearchConfig &defaults = gnome_cmd_data.search_defaults;
    GtkWidget *hbox, *vbox, *label, *radio;

    dialog->priv = g_new0 (GnomeCmdPatternselDialogPrivate, 1);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK, GTK_SIGNAL_FUNC (on_ok), dialog);

    vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 6);
    hbox = create_hbox (GTK_WIDGET (dialog), FALSE, 6);

    dialog->priv->pattern_combo = create_combo (GTK_WIDGET (dialog));
    gtk_combo_disable_activate (GTK_COMBO (dialog->priv->pattern_combo));
    if (!defaults.name_patterns.empty())
        gtk_combo_set_popdown_strings (GTK_COMBO (dialog->priv->pattern_combo), defaults.name_patterns.ents);
    dialog->priv->pattern_entry = GTK_COMBO (dialog->priv->pattern_combo)->entry;
    g_signal_connect_swapped (dialog->priv->pattern_entry, "activate", G_CALLBACK (gtk_window_activate_default), dialog);
    gtk_editable_select_region (GTK_EDITABLE (dialog->priv->pattern_entry), 0, -1);

    label = create_label_with_mnemonic (GTK_WIDGET (dialog), _("_Pattern:"), dialog->priv->pattern_entry);

    dialog->priv->case_check = create_check_with_mnemonic (GTK_WIDGET (dialog), _("Case _sensitive"), "case_sens");

    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->priv->pattern_combo, TRUE, TRUE, 0);

    hbox = create_hbox (GTK_WIDGET (dialog), FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->priv->case_check, TRUE, FALSE, 0);

    radio = create_radio_with_mnemonic (GTK_WIDGET (dialog), NULL, _("She_ll syntax"), "shell_radio");
    gtk_box_pack_end (GTK_BOX (hbox), radio, TRUE, FALSE, 0);
    if (gnome_cmd_data.search_defaults.default_profile.syntax == Filter::TYPE_FNMATCH)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
    radio = create_radio_with_mnemonic (GTK_WIDGET (dialog), get_radio_group (radio), _("Rege_x syntax"), "regex_radio");
    gtk_box_pack_end (GTK_BOX (hbox), radio, TRUE, FALSE, 0);
    if (gnome_cmd_data.search_defaults.default_profile.syntax == Filter::TYPE_REGEX)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), vbox);

    gtk_widget_grab_focus (dialog->priv->pattern_entry);
}


static void gnome_cmd_patternsel_dialog_class_init (GnomeCmdPatternselDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_patternsel_dialog_finalize;
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_patternsel_dialog_new (GnomeCmdFileList *fl, gboolean mode)
{
    GnomeCmdPatternselDialog *dialog = (GnomeCmdPatternselDialog *) g_object_new (GNOME_CMD_TYPE_PATTERNSEL_DIALOG, NULL);

    dialog->priv->mode = mode;
    dialog->priv->fl = fl;

    gtk_window_set_title (GTK_WINDOW (dialog), mode ? _("Select Using Pattern") : _("Unselect Using Pattern"));

    return GTK_WIDGET (dialog);
}
