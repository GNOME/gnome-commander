/**
 * @file gnome-cmd-patternsel-dialog.cc
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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-file-list.h"
#include "utils.h"
#include "dialogs/gnome-cmd-patternsel-dialog.h"

using namespace std;


struct GnomeCmdPatternselDialogPrivate
{
    GnomeCmdFileList *fl;

    GtkWidget *case_check;
    GtkWidget *pattern_combo;
    GtkWidget *pattern_entry;
    GtkWidget *regex_check;

    gboolean mode;
};


G_DEFINE_TYPE (GnomeCmdPatternselDialog, gnome_cmd_patternsel_dialog, GNOME_CMD_TYPE_DIALOG)


static void on_ok (GtkButton *button, GnomeCmdPatternselDialog *dialog)
{
    g_return_if_fail (GNOME_CMD_IS_PATTERNSEL_DIALOG (dialog));

    gboolean case_sens = gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->priv->case_check));
    gnome_cmd_data.search_defaults.default_profile.syntax = gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->priv->regex_check)) ? Filter::TYPE_REGEX : Filter::TYPE_FNMATCH;

    const gchar *s = gtk_editable_get_text (GTK_EDITABLE (dialog->priv->pattern_entry));

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
    GtkWidget *hbox, *vbox, *label;

    dialog->priv = g_new0 (GnomeCmdPatternselDialogPrivate, 1);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_Cancel"), G_CALLBACK(on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_OK"), G_CALLBACK(on_ok), dialog);

    vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 6);
    hbox = create_hbox (GTK_WIDGET (dialog), FALSE, 6);

    dialog->priv->pattern_combo = create_combo_box_text_with_entry (GTK_WIDGET (dialog));
    for (auto list = defaults.name_patterns.ents; list; list = list->next)
        gtk_combo_box_text_append_text ((GtkComboBoxText*) dialog->priv->pattern_combo, (const gchar*) list->data);
    if (defaults.name_patterns.ents)
        gtk_combo_box_set_active ((GtkComboBox*) dialog->priv->pattern_combo, 0);
    dialog->priv->pattern_entry = gtk_combo_box_get_child (GTK_COMBO_BOX (dialog->priv->pattern_combo));
    gtk_entry_set_activates_default (GTK_ENTRY (dialog->priv->pattern_entry), TRUE);
    gtk_editable_select_region (GTK_EDITABLE (dialog->priv->pattern_entry), 0, -1);

    label = create_label_with_mnemonic (GTK_WIDGET (dialog), _("_Pattern:"), dialog->priv->pattern_entry);

    dialog->priv->case_check = create_check_with_mnemonic (GTK_WIDGET (dialog), _("Case _sensitive"), "case_sens");

    gtk_box_append (GTK_BOX (vbox), hbox);
    gtk_box_append (GTK_BOX (hbox), label);
    gtk_widget_set_hexpand (dialog->priv->pattern_combo, TRUE);
    gtk_box_append (GTK_BOX (hbox), dialog->priv->pattern_combo);

    hbox = create_hbox (GTK_WIDGET (dialog), FALSE, 6);
    gtk_box_append (GTK_BOX (vbox), hbox);
    gtk_box_append (GTK_BOX (hbox), dialog->priv->case_check);

    GtkWidget *shell_check = gtk_check_button_new_with_mnemonic (_("She_ll syntax"));
    gtk_box_append (GTK_BOX (hbox), shell_check);
    if (gnome_cmd_data.search_defaults.default_profile.syntax == Filter::TYPE_FNMATCH)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (shell_check), TRUE);
    dialog->priv->regex_check = gtk_check_button_new_with_mnemonic (_("Rege_x syntax"));
    gtk_check_button_set_group (GTK_CHECK_BUTTON (dialog->priv->regex_check), GTK_CHECK_BUTTON (shell_check));
    gtk_box_append (GTK_BOX (hbox), dialog->priv->regex_check);
    if (gnome_cmd_data.search_defaults.default_profile.syntax == Filter::TYPE_REGEX)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (dialog->priv->regex_check), TRUE);

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
