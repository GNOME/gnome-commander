/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2003 Marcus Bjurman

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 
#include <config.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-patternsel-dialog.h"
#include "gnome-cmd-file-list.h"

struct _GnomeCmdPatternselDialogPrivate
{
	GnomeCmdFileList *fl;
	GtkWidget *case_check;
	gboolean mode;
};


static GnomeCmdStringDialogClass *parent_class = NULL;



static gboolean
on_ok (GnomeCmdStringDialog *string_dialog,
	   const gchar **values,
	   GnomeCmdPatternselDialog *dialog)
{
	const gchar *string = values[0];
	gboolean case_sens;

	g_return_val_if_fail (string != NULL, TRUE);

	case_sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check));
	
	if (dialog->priv->mode)
		gnome_cmd_file_list_select_pattern (dialog->priv->fl, string, case_sens);
	else
		gnome_cmd_file_list_unselect_pattern (dialog->priv->fl, string, case_sens);

	return TRUE;
}


static void
on_cancel (GtkWidget *widget, GnomeCmdPatternselDialog *dialog)
{
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdPatternselDialog *dialog = GNOME_CMD_PATTERNSEL_DIALOG (object);

	g_free (dialog->priv);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
map (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
		GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdPatternselDialogClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	parent_class = gtk_type_class (gnome_cmd_string_dialog_get_type ());
	object_class->destroy = destroy;
	widget_class->map = map;
}


static void
init (GnomeCmdPatternselDialog *dialog)
{
	dialog->priv = g_new (GnomeCmdPatternselDialogPrivate, 1);
}




/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_patternsel_dialog_new (GnomeCmdFileList *fl, gboolean mode)
{
	const gchar *labels[] = {_("Pattern:")};
	GnomeCmdPatternselDialog *dialog = gtk_type_new (gnome_cmd_patternsel_dialog_get_type ());

	dialog->priv->mode = mode;
	dialog->priv->fl = fl;
	
	gnome_cmd_string_dialog_setup_with_cancel (
		GNOME_CMD_STRING_DIALOG (dialog),
		mode?_("Select Using Pattern"):_("Unselect Using Pattern"),
		labels,
		1,
		(GnomeCmdStringDialogCallback)on_ok,
		GTK_SIGNAL_FUNC (on_cancel),
		dialog);
	
	dialog->priv->case_check = gtk_check_button_new_with_label (_("Case sensitive"));
	gtk_widget_ref (dialog->priv->case_check);
	gtk_object_set_data_full (GTK_OBJECT (dialog),
							  "case_check", dialog->priv->case_check,
							  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (dialog->priv->case_check);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), dialog->priv->case_check, FALSE, TRUE, 0);
	
	return GTK_WIDGET (dialog);
}



GtkType
gnome_cmd_patternsel_dialog_get_type         (void)
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

		dlg_type = gtk_type_unique (gnome_cmd_string_dialog_get_type (), &dlg_info);
	}
	return dlg_type;
}



