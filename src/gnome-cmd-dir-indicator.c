/*
    GNOME Commander - A GNOME based file manager 
    Copyright (C) 2001-2004 Marcus Bjurman

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
#include "gnome-cmd-file-selector.h"
#include "utils.h"

struct _GnomeCmdDirIndicatorPrivate
{
	GtkWidget *event_box;
	GtkWidget *label;
	int *slashCharPosition;
	int *slashPixelPosition;
	int numPositions;
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
	if (dir_indicator->priv->slashCharPosition)
		free (dir_indicator->priv->slashCharPosition);
	if (dir_indicator->priv->slashPixelPosition)
		free (dir_indicator->priv->slashPixelPosition);

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

/******************
   event handlers 
*******************/
static gboolean
on_dir_indicator_clicked (GtkWidget * widget, GdkEventButton * event, gpointer cb_data)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (widget), FALSE);
	GnomeCmdDirIndicator *di = (GnomeCmdDirIndicator *) widget;

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
		/* left click - work out the path */
		gchar *chTo;
		const gchar *labelText = gtk_label_get_label ((GtkLabel *) di->priv->label);

		chTo = malloc (strlen (labelText) + 1);
		strcpy (chTo, labelText);
		gint x = (gint)event->x;
		gint i;
		GnomeCmdDirIndicator *di = (GnomeCmdDirIndicator *) widget;
		  
		for (i = 0; i < di->priv->numPositions; i++) {
			if (x < di->priv->slashPixelPosition[i]) {
				strncpy (chTo, labelText, di->priv->slashCharPosition[i]);
				chTo[di->priv->slashCharPosition[i]] = 0x0;
				gnome_cmd_file_selector_goto_directory (
					(GnomeCmdFileSelector*)cb_data, chTo);
				free (chTo);
				return TRUE;
			}
		}
		  
		/* pointer is after directory name - just return */
		return TRUE;
	}
	return FALSE;
}

static gint
on_dir_indicator_motion (GtkWidget * widget, GdkEventMotion * event, gpointer * ptr)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (widget), FALSE);
	gint i, iX, iY;
	GnomeCmdDirIndicator *di = (GnomeCmdDirIndicator *) widget;
	
	if (di->priv->slashCharPosition == NULL)
		return FALSE;
	if (di->priv->slashPixelPosition == NULL)
		return FALSE;
	
	/* find out where in the label the pointer is at */
	iX = (gint)event->x;
	iY = (gint)event->y;
	
	for (i = 0; i < di->priv->numPositions; i++) {
		if (iX < di->priv->slashPixelPosition[i]) {
			/* underline the part that is selected */
			gchar *patternbuf = malloc (di->priv->slashPixelPosition[i] + 1);
			gint j;
			
			for (j = 0; j < di->priv->slashCharPosition[i]; j++) {
				patternbuf[j] = '_';
			}
			
			patternbuf[di->priv->slashCharPosition[i]] = 0x0;
			gtk_label_set_pattern ((GtkLabel *) di->priv->label, patternbuf);
			free (patternbuf);
		    GdkCursor* cursor;
		    cursor = gdk_cursor_new(GDK_HAND2);
		    gdk_window_set_cursor(widget->window, cursor);
		    gdk_cursor_destroy(cursor);
			return TRUE;
		}
		  
		/* clear underline, cursor=pointer */
		gtk_label_set_pattern ((GtkLabel *) di->priv->label, "");
		gdk_window_set_cursor(widget->window, NULL);
	}
	
	return TRUE;
}

static gint
on_dir_indicator_leave (GtkWidget * widget, GdkEventMotion * event, gpointer * ptr)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (widget), FALSE);
	
	/* clear underline, cursor=pointer */
	GnomeCmdDirIndicator *di = (GnomeCmdDirIndicator *) widget;
	gtk_label_set_pattern ((GtkLabel *) di->priv->label, "");
	gdk_window_set_cursor(widget->window, NULL);
	
	return TRUE;
}

static int
getPixelSize (const char *s, int size)
{
	/* find the size, in pixels, of the given string */
	gchar *buf;
	gchar *utf8buf;
	buf = malloc (size + 1);
	buf[0] = 0x0;
	strncpy (buf, s, size);
	buf[size] = 0x0;
	utf8buf = get_utf8 (buf);
	GtkLabel *label = (GtkLabel *) gtk_label_new (utf8buf);
	g_object_ref (label);
	PangoLayout *layout = gtk_label_get_layout (label);
	gint xSize, ySize;
	pango_layout_get_pixel_size (layout, &xSize, &ySize);
	
	/* we're finished with the label */
	gtk_object_sink ((GtkObject *) label);
	free (utf8buf);
	free (buf);
	
	return xSize;
}

static void
init (GnomeCmdDirIndicator *indicator)
{
	indicator->priv = g_new (GnomeCmdDirIndicatorPrivate, 1);
	indicator->priv->slashCharPosition = NULL;
	indicator->priv->slashPixelPosition = NULL;
	indicator->priv->numPositions = 0;

	indicator->priv->event_box = gtk_event_box_new ();
	gtk_widget_ref (indicator->priv->event_box);
	gtk_signal_connect_object (GTK_OBJECT (indicator->priv->event_box), "motion_notify_event",
				   GTK_SIGNAL_FUNC (on_dir_indicator_motion), indicator);
	gtk_signal_connect_object (GTK_OBJECT (indicator->priv->event_box), "leave_notify_event",
				   GTK_SIGNAL_FUNC (on_dir_indicator_leave), indicator);
	gtk_widget_set_events (indicator->priv->event_box, GDK_POINTER_MOTION_MASK);

	gtk_widget_show (indicator->priv->event_box);
	gtk_container_add (GTK_CONTAINER (indicator), indicator->priv->event_box);

	indicator->priv->label = create_label (GTK_WIDGET (indicator), "not initialized");
	gtk_container_add (GTK_CONTAINER (indicator->priv->event_box), indicator->priv->label);

}


/***********************************
 * Public functions
 ***********************************/
GtkType
gnome_cmd_dir_indicator_get_type	(void)
{
	static GtkType type = 0;

	if (type == 0) {
		GtkTypeInfo info = {
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
gnome_cmd_dir_indicator_new (GnomeCmdFileSelector *fs)
{
	GnomeCmdDirIndicator *dir_indicator;

	dir_indicator = gtk_type_new (gnome_cmd_dir_indicator_get_type ());

	gtk_signal_connect (
		GTK_OBJECT (dir_indicator),
		"button_press_event",
		G_CALLBACK (on_dir_indicator_clicked),
		fs);
	
	return GTK_WIDGET (dir_indicator);
}


void
gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, const gchar *path)
{
	gint i, numSlashes;
	gchar *s = get_utf8 (path);
	
	gtk_label_set_text (GTK_LABEL (indicator->priv->label), s);

	/* Count the number of slashes in the string */
	numSlashes = 0;
	for (i = 0; i < strlen (path); i++) {
		if (*(path + i) == '/')
			numSlashes++;
	}
	
	/* there will now be numSlashes+1 entries */
	/* first entry is just slash (root) */
	/* 1..numSlashes-1 is /dir/dir etc. */
	/* last entry will be whole string */
	if (indicator->priv->slashCharPosition) {
		free (indicator->priv->slashCharPosition);
	}
	
	if (indicator->priv->slashPixelPosition) {
		free (indicator->priv->slashPixelPosition);
	}
	
	if (numSlashes > 0) {
		/* if there are any slashes then allocate some memory
		   for storing their positions (both in char positions through
		   the string, and pixel positions on the display */
		indicator->priv->slashCharPosition = malloc ((numSlashes + 1) * sizeof (int));
		indicator->priv->slashPixelPosition = malloc ((numSlashes + 1) * sizeof (int));
		indicator->priv->slashCharPosition[0] = 1;
		indicator->priv->slashPixelPosition[0] = getPixelSize (path, 1);
		numSlashes = 1;
		
		for (i = 1; i < strlen (path); i++) {
			if (*(path + i) == '/') {
				/* underline everything up to but not including slash
				 * but include the slash when calcluating pixel size,
				 * as /directory/ = /directory when doing chdir */
				indicator->priv->slashCharPosition[numSlashes] = i;
				indicator->priv->slashPixelPosition[numSlashes] =
					getPixelSize (path, i + 1);
				numSlashes++;
			}
		}
		
		/* make an entry that is the whole size of the path */
		indicator->priv->slashCharPosition[numSlashes] = strlen (path);
		indicator->priv->slashPixelPosition[numSlashes] =
			getPixelSize (path, strlen (path));
		numSlashes++;
		indicator->priv->numPositions = numSlashes;
	}
	
	g_free (s);
}


void
gnome_cmd_dir_indicator_set_active (GnomeCmdDirIndicator *indicator, gboolean value)
{
	//FIXME: Do something creative here
}
