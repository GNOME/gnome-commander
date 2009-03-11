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


static GnomeCmdDialogClass *parent_class = NULL;


static void on_ok (GtkButton *button, GnomeCmdPatternselDialog *dialog)
{
    g_return_if_fail (GNOME_CMD_IS_PATTERNSEL_DIALOG (dialog));

    GnomeCmdData::SearchConfig &defaults = gnome_cmd_data.search_defaults;

    const gchar *s = gtk_entry_get_text (GTK_ENTRY (dialog->priv->pattern_entry));
    gboolean case_sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check));

    if (dialog->priv->mode)
        dialog->priv->fl->select_pattern(s, case_sens);
    else
        dialog->priv->fl->unselect_pattern(s, case_sens);

    defaults.name_patterns.add(s);

    gtk_widget_hide (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdPatternselDialog *dialog)
{
    gtk_widget_hide (GTK_WIDGET (dialog));
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    GnomeCmdPatternselDialog *dialog = GNOME_CMD_PATTERNSEL_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdPatternselDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GnomeCmdDialogClass *) gtk_type_class (gnome_cmd_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdPatternselDialog *dialog)
{
    dialog->priv = g_new0 (GnomeCmdPatternselDialogPrivate, 1);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_patternsel_dialog_new (GnomeCmdFileList *fl, gboolean mode)
{
    GtkWidget *hbox, *vbox, *label;
    GnomeCmdData::SearchConfig &defaults = gnome_cmd_data.search_defaults;
    GnomeCmdPatternselDialog *dialog = (GnomeCmdPatternselDialog *) gtk_type_new (gnome_cmd_patternsel_dialog_get_type ());
    dialog->priv->mode = mode;
    dialog->priv->fl = fl;

    gnome_cmd_dialog_setup ( GNOME_CMD_DIALOG (dialog), mode ? _("Select Using Pattern") : _("Unselect Using Pattern"));

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_OK, GTK_SIGNAL_FUNC (on_ok), dialog);

    vbox = create_vbox (GTK_WIDGET (dialog), FALSE, 6);
    hbox = create_hbox (GTK_WIDGET (dialog), FALSE, 6);
    label = create_label (GTK_WIDGET (dialog), _("Pattern:"));

    dialog->priv->pattern_combo = create_combo (GTK_WIDGET (dialog));
    gtk_combo_disable_activate (GTK_COMBO (dialog->priv->pattern_combo));
    if (!defaults.name_patterns.empty())
        gtk_combo_set_popdown_strings (GTK_COMBO (dialog->priv->pattern_combo), defaults.name_patterns.ents);
    dialog->priv->pattern_entry = GTK_COMBO (dialog->priv->pattern_combo)->entry;
    gnome_cmd_dialog_editable_enters (GNOME_CMD_DIALOG (dialog), GTK_EDITABLE (dialog->priv->pattern_entry));
    gtk_entry_select_region (GTK_ENTRY (dialog->priv->pattern_entry), 0, -1);

    dialog->priv->case_check = create_check (GTK_WIDGET (dialog), _("Case sensitive"), "case_sens");

    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->priv->pattern_combo, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->case_check, TRUE, FALSE, 0);

    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), vbox);

    gtk_widget_grab_focus (dialog->priv->pattern_entry);

    return GTK_WIDGET (dialog);
}


GtkType gnome_cmd_patternsel_dialog_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdPatternselDialog",
            sizeof (GnomeCmdPatternselDialog),
            sizeof (GnomeCmdPatternselDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}
