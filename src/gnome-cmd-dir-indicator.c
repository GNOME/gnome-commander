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
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "utils.h"

struct _GnomeCmdDirIndicatorPrivate
{
	GtkWidget *event_box;
	GtkWidget *label;
	GtkWidget *history_button;
	GtkWidget *dir_history_popup;
	gboolean is_popped;
	GnomeCmdFileSelector *fs;
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


/*******************************
 * Event handlers 
 *******************************/
static gboolean
on_dir_indicator_clicked (GnomeCmdDirIndicator *indicator,
						  GdkEventButton *event,
						  GnomeCmdFileSelector *fs)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator), FALSE);

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
		/* left click - work out the path */
		gchar *chTo;
		const gchar *labelText = gtk_label_get_text (GTK_LABEL (indicator->priv->label));
		chTo = malloc (strlen (labelText) + 1);
		strcpy (chTo, labelText);
		gint x = (gint)event->x;
		gint i;
		  
		for (i = 0; i < indicator->priv->numPositions; i++) {
			if (x < indicator->priv->slashPixelPosition[i]) {
				strncpy (chTo, labelText, indicator->priv->slashCharPosition[i]);
				chTo[indicator->priv->slashCharPosition[i]] = 0x0;
				gnome_cmd_main_win_switch_fs (main_win, fs);
				gnome_cmd_file_selector_goto_directory (fs, chTo);
				free (chTo);
				return TRUE;
			}
		}
		  
		/* pointer is after directory name - just return */
		return TRUE;
	}
	return FALSE;
}


static void
update_markup (GnomeCmdDirIndicator *indicator, gint i)
{
	gchar *s, *m;

	if (indicator->priv->slashCharPosition == NULL)
		return;

	s = g_strdup (gtk_label_get_text (GTK_LABEL (indicator->priv->label)));
	if (i > 0) {
		gchar *mt, *ms;
		gchar *t = g_strdup (&s[indicator->priv->slashCharPosition[i]]);
		s[indicator->priv->slashCharPosition[i]] = '\0';

		mt = get_mono_text (t);
		ms = get_bold_mono_text (s);
		m = g_strdup_printf ("%s%s", ms, mt);
		g_free (t);
		g_free (mt);
		g_free (ms);
	}
	else {
		m = get_mono_text (s);
	}
	
	gtk_label_set_markup (GTK_LABEL (indicator->priv->label), m);
	g_free (s);
	g_free (m);
}


static gint
on_dir_indicator_motion (GnomeCmdDirIndicator *indicator,
						 GdkEventMotion *event,
						 gpointer user_data)
{
	gint i, iX, iY;

	g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator), FALSE);
	
	if (indicator->priv->slashCharPosition == NULL)
		return FALSE;
	if (indicator->priv->slashPixelPosition == NULL)
		return FALSE;
	
	/* find out where in the label the pointer is at */
	iX = (gint)event->x;
	iY = (gint)event->y;
	
	for (i = 0; i < indicator->priv->numPositions; i++) {
		if (iX < indicator->priv->slashPixelPosition[i]) {
			/* underline the part that is selected */
		    GdkCursor* cursor;

			/*
			patternbuf = malloc (indicator->priv->slashPixelPosition[i] + 1);			
			for (j = 0; j < indicator->priv->slashCharPosition[i]; j++)
				patternbuf[j] = '_';			
			patternbuf[indicator->priv->slashCharPosition[i]] = 0x0;
			gtk_label_set_pattern ((GtkLabel *) indicator->priv->label, patternbuf);
			free (patternbuf);
			*/
			cursor = gdk_cursor_new(GDK_HAND2);
		    gdk_window_set_cursor(GTK_WIDGET(indicator)->window, cursor);
		    gdk_cursor_destroy(cursor);

			update_markup (indicator, i);
			
			return TRUE;
		}
		  
		/* clear underline, cursor=pointer */
		//gtk_label_set_pattern ((GtkLabel *) indicator->priv->label, "");
		update_markup (indicator, 0);
		gdk_window_set_cursor(GTK_WIDGET (indicator)->window, NULL);
	}
	
	return TRUE;
}

static gint
on_dir_indicator_leave (GnomeCmdDirIndicator *indicator,
						GdkEventMotion *event,
						gpointer user_data)
{
	g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator), FALSE);
	
	/* clear underline, cursor=pointer */
	//gtk_label_set_pattern ((GtkLabel *) indicator->priv->label, "");
	update_markup (indicator, 0);
	gdk_window_set_cursor(GTK_WIDGET (indicator)->window, NULL);
	
	return TRUE;
}

static int
getPixelSize (const char *s, int size)
{
	/* find the size, in pixels, of the given string */
	gchar *buf, *ms;
	gchar *utf8buf;
	gint xSize, ySize;
	GtkLabel *label;
	PangoLayout *layout;
	
	buf = malloc (size + 1);
	buf[0] = 0x0;
	strncpy (buf, s, size);
	buf[size] = 0x0;
	utf8buf = get_utf8 (buf);
	
	label = GTK_LABEL (gtk_label_new (utf8buf));
	ms = get_mono_text (utf8buf);
	gtk_label_set_markup (label, ms);
	g_free (ms);
	g_object_ref (label);
	layout = gtk_label_get_layout (label);
	pango_layout_get_pixel_size (layout, &xSize, &ySize);
	
	/* we're finished with the label */
	gtk_object_sink (GTK_OBJECT (label));
	free (utf8buf);
	free (buf);
	
	return xSize;
}

static gboolean
on_history_button_clicked (GtkWidget *button, GnomeCmdDirIndicator *indicator)
{
	if (indicator->priv->is_popped) {
		gtk_widget_hide (indicator->priv->dir_history_popup);
		indicator->priv->is_popped = FALSE;
	}
	else {
		gnome_cmd_dir_indicator_show_history (indicator);
		indicator->priv->is_popped = TRUE;
	}

	return TRUE;
}


static void
on_dir_history_popup_hide (GtkMenu *menu, GnomeCmdDirIndicator *indicator)
{
	indicator->priv->dir_history_popup = NULL;
	indicator->priv->is_popped = FALSE;
}


static void
on_dir_history_item_selected (GtkMenuItem *item, const gchar *path)
{
	GnomeCmdDirIndicator *indicator = gtk_object_get_data (GTK_OBJECT (item), "indicator");

	g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));
	g_return_if_fail (path != NULL);
	
	gnome_cmd_main_win_switch_fs (main_win, indicator->priv->fs);
	gnome_cmd_file_selector_goto_directory (indicator->priv->fs, path);
}


static void
get_dir_history_popup_pos (GtkMenu *menu,
						   gint *x, gint *y, gboolean push_in,
						   GnomeCmdDirIndicator *indicator)
{
	GtkWidget *w;

	g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));

	w = GTK_WIDGET (indicator->priv->fs->list);

	gdk_window_get_origin (w->window, x, y);
}


static void
popup_dir_history (GnomeCmdDirIndicator *indicator)
{
	GList *l;
	GnomeCmdCon *con;
	History *history;
	gint w = -1;

	if (indicator->priv->dir_history_popup) return;
	
	indicator->priv->dir_history_popup = gtk_menu_new ();
	gtk_widget_ref (indicator->priv->dir_history_popup);
	gtk_object_set_data_full (
		GTK_OBJECT (indicator), "dir_history_popup",
		indicator->priv->dir_history_popup, (GtkDestroyNotify) gtk_widget_unref);
	gtk_signal_connect (
		GTK_OBJECT (indicator->priv->dir_history_popup), "hide",
		GTK_SIGNAL_FUNC (on_dir_history_popup_hide), indicator);

	con = gnome_cmd_file_selector_get_connection (indicator->priv->fs);
	history = gnome_cmd_con_get_dir_history (con);
	l = history->ents;
	while (l) {
		gchar *path = (gchar*)l->data;
		GtkWidget *item = gtk_menu_item_new_with_label (path);
		gtk_widget_ref (item);
		gtk_object_set_data (GTK_OBJECT (item), "indicator", indicator);			
		gtk_object_set_data_full (
			GTK_OBJECT (indicator->priv->dir_history_popup),
			"menu_item", item, (GtkDestroyNotify) gtk_widget_unref);		
		gtk_signal_connect (
			GTK_OBJECT (item), "activate",
			GTK_SIGNAL_FUNC (on_dir_history_item_selected), path);
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (indicator->priv->dir_history_popup), item);
		l = l->next;
	}

	gnome_popup_menu_do_popup (
		indicator->priv->dir_history_popup,
		(GtkMenuPositionFunc)get_dir_history_popup_pos,	indicator,
		NULL, NULL, NULL);

	if (GTK_WIDGET (indicator)->allocation.width > 100)
		w = GTK_WIDGET (indicator)->allocation.width;
	
	gtk_widget_set_usize (indicator->priv->dir_history_popup, w, -1);
}


static void
init (GnomeCmdDirIndicator *indicator)
{
	GtkWidget *hbox, *arrow;
	
	indicator->priv = g_new (GnomeCmdDirIndicatorPrivate, 1);
	indicator->priv->dir_history_popup = NULL;
	indicator->priv->is_popped = FALSE;
	indicator->priv->slashCharPosition = NULL;
	indicator->priv->slashPixelPosition = NULL;
	indicator->priv->numPositions = 0;

	/* create the directory label and it's event box */
	indicator->priv->event_box = gtk_event_box_new ();
	gtk_widget_ref (indicator->priv->event_box);
	gtk_signal_connect_object (GTK_OBJECT (indicator->priv->event_box), "motion_notify_event",
							   GTK_SIGNAL_FUNC (on_dir_indicator_motion), indicator);
	gtk_signal_connect_object (GTK_OBJECT (indicator->priv->event_box), "leave_notify_event",
							   GTK_SIGNAL_FUNC (on_dir_indicator_leave), indicator);
	gtk_widget_set_events (indicator->priv->event_box, GDK_POINTER_MOTION_MASK);

	gtk_widget_show (indicator->priv->event_box);

	indicator->priv->label = create_label (GTK_WIDGET (indicator), "not initialized");
	gtk_container_add (GTK_CONTAINER (indicator->priv->event_box), indicator->priv->label);

	/* create the history popup button */
	indicator->priv->history_button = gtk_button_new ();
	GTK_WIDGET_UNSET_FLAGS (indicator->priv->history_button, GTK_CAN_FOCUS);
	gtk_widget_ref (indicator->priv->history_button);
	gtk_button_set_relief (GTK_BUTTON (indicator->priv->history_button), gnome_cmd_data_get_button_relief ());
	gtk_object_set_data_full (GTK_OBJECT (indicator),
							  "button", indicator->priv->history_button,
							  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (indicator->priv->history_button);
	
	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_ref (arrow);
	gtk_object_set_data_full (GTK_OBJECT (indicator),
							  "arrow", arrow,
							  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (arrow);
	gtk_container_add (GTK_CONTAINER (indicator->priv->history_button), arrow);

	/* pack */
	hbox = create_hbox (GTK_WIDGET (indicator), FALSE, 10);
	gtk_container_add (GTK_CONTAINER (indicator), hbox);
	gtk_box_pack_start (GTK_BOX (hbox), indicator->priv->event_box, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), indicator->priv->history_button, FALSE, FALSE, 0);


	gtk_signal_connect (GTK_OBJECT (indicator->priv->history_button), "clicked",
						GTK_SIGNAL_FUNC (on_history_button_clicked), indicator);
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

	dir_indicator->priv->fs = fs;
	
	return GTK_WIDGET (dir_indicator);
}


void
gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, const gchar *path)
{
	gint i, numSlashes;
	gchar *s = get_utf8 (path);
	
	gtk_label_set_text (GTK_LABEL (indicator->priv->label), s);
	update_markup (indicator, 0);

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


void
gnome_cmd_dir_indicator_show_history (GnomeCmdDirIndicator *indicator)
{
	popup_dir_history (indicator);
}


