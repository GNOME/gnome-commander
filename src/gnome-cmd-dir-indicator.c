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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-style.h"
#include "utils.h"

struct _GnomeCmdDirIndicatorPrivate
{
	GtkWidget *event_box;
	GtkWidget *label;
};


static GtkFrameClass *parent_class = NULL;




/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdDirIndicator *dir_indicator = GNOME_CMD_DIR_INDICATOR (object);

	g_free (dir_indicator->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdDirIndicatorClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_frame_get_type ());

	object_class->destroy = destroy;
}


static void
init (GnomeCmdDirIndicator *indicator)
{
	indicator->priv = g_new (GnomeCmdDirIndicatorPrivate, 1);


	indicator->priv->event_box = gtk_event_box_new ();
	gtk_widget_ref (indicator->priv->event_box);
	gtk_widget_show (indicator->priv->event_box);
	gtk_container_add (GTK_CONTAINER (indicator), indicator->priv->event_box);

	indicator->priv->label = create_label (GTK_WIDGET (indicator), "not initialized");
	gtk_container_add (GTK_CONTAINER (indicator->priv->event_box), indicator->priv->label);
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_dir_indicator_get_type         (void)
{
	static GtkType type = 0;

	if (type == 0)
	{
		GtkTypeInfo info =
		{
			"GnomeCmdDirIndicator",
			sizeof (GnomeCmdDirIndicator),
			sizeof (GnomeCmdDirIndicatorClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_frame_get_type (), &info);
	}
	return type;
}


GtkWidget *
gnome_cmd_dir_indicator_new (void)
{
	GnomeCmdDirIndicator *dir_indicator;

	dir_indicator = gtk_type_new (gnome_cmd_dir_indicator_get_type ());

	return GTK_WIDGET (dir_indicator);
}


void
gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, const gchar *path)
{
	gchar *s = get_utf8 (path);
	gtk_label_set_text (GTK_LABEL (indicator->priv->label), s);
	g_free (s);
}


void
gnome_cmd_dir_indicator_set_active (GnomeCmdDirIndicator *indicator, gboolean value)
{
	//FIXME: Do something creative here
}
