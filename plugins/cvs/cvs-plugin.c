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
#include <gnome.h>
#include <libgcmd/libgcmd.h>
#include "cvs-plugin.h"
#include "cvs-plugin.xpm"
#include "interface.h"
#include "parser.h"

#define NAME "CVS"
#define COMMENTS "A plugin that eventually will be a simple CVS client"
#define COPYRIGHT "Copyright 2003 Marcus Bjurman"
#define AUTHOR "Marcus Bjurman <marbj499@student.liu.se>"


static PluginInfo plugin_nfo = {
	GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
	NAME,
	VERSION,
	COPYRIGHT,
	COMMENTS,
	NULL,
	NULL,
	NULL,
	NULL
};


struct _CvsPluginPrivate
{
	GtkWidget *update;
	GtkWidget *log;
	GtkWidget *last_log;
	GtkWidget *last_change;
};


static GnomeCmdPluginClass *parent_class = NULL;


static void
on_dummy (GtkMenuItem *item, gpointer data)
{
	gnome_ok_dialog ("Cvs plugin dummy operation");
}


static void
create_log_window (CvsPlugin *plugin, const gchar *fpath)
{
	GList *revs;
	GtkCList *rev_list;
	GtkWidget *win = create_main_win (plugin);

	rev_list = GTK_CLIST (lookup_widget (win, "rev_list"));

	g_return_if_fail (GTK_IS_CLIST (rev_list));

	plugin->log_history = log_create (fpath);
	if (!plugin->log_history) return;
		
	revs = plugin->log_history->revisions;
	while (revs) {
		Revision *rev = (Revision *)revs->data;
		gint row;
		gchar *text[2];
		text[0] = rev->number;
		text[1] = NULL;
		// Add this rev. to the list
		row = gtk_clist_append (rev_list, text);
		gtk_clist_set_row_data (rev_list, row, rev);
		revs = revs->next;
	}

	gtk_widget_show (win);
}


static void
on_log (GtkMenuItem *item, GnomeCmdState *state)
{
	GList *files = state->active_dir_selected_files;
	CvsPlugin *plugin = gtk_object_get_data (GTK_OBJECT (item), "plugin");

	while (files) {
		GnomeVFSFileInfo *info = (GnomeVFSFileInfo*)files->data;
		GnomeVFSURI *uri = gnome_vfs_uri_append_file_name (
			state->active_dir_uri, info->name);		
		const gchar *fpath = gnome_vfs_uri_get_path (uri);
		create_log_window (plugin, fpath);
		files = files->next;
	}
}


static GtkWidget *
create_menu_item (const gchar *name, gboolean show_pixmap,
				  GtkSignalFunc callback, gpointer data,
				  CvsPlugin *plugin)
{
	GtkWidget *item, *label;
	GtkWidget *pixmap = NULL;

	if (show_pixmap) {
		item = gtk_image_menu_item_new ();
		pixmap = gnome_pixmap_new_from_xpm_d ((const gchar**)cvs_plugin_xpm);
		if (pixmap) {
			gtk_widget_show (pixmap);
			gtk_image_menu_item_set_image (
				GTK_IMAGE_MENU_ITEM (item), pixmap);
		}
	}
	else
		item = gtk_menu_item_new ();

	gtk_widget_show (item);
	
	/* Create the contents of the menu item */
	label = gtk_label_new (name);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (item), label);

	/* Connect to the signal and set user data */	
	gtk_object_set_data (GTK_OBJECT (item),
						 GNOMEUIINFO_KEY_UIDATA,
						 data);

	if (callback)
		gtk_signal_connect (GTK_OBJECT (item), "activate", callback, data);

	gtk_object_set_data (GTK_OBJECT (item), "plugin", plugin);

	return item;
}


static GtkWidget * 
create_main_menu (GnomeCmdPlugin *p, GnomeCmdState *state)
{
	GtkWidget *item, *child;
	GtkMenu *submenu;
	CvsPlugin *plugin = CVS_PLUGIN (p);

	submenu = GTK_MENU (gtk_menu_new ());
	item = create_menu_item ("CVS", FALSE, NULL, NULL, plugin);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
							   GTK_WIDGET (submenu));

	plugin->priv->update = child =
		create_menu_item ("Update", FALSE, GTK_SIGNAL_FUNC (on_dummy), state, plugin);
	gtk_menu_append (submenu, child);
	plugin->priv->log = child =
		create_menu_item ("Log", FALSE, GTK_SIGNAL_FUNC (on_log), state, plugin);
	gtk_menu_append (submenu, child);
	plugin->priv->last_log = child =
		create_menu_item ("Last Log", FALSE, GTK_SIGNAL_FUNC (on_dummy), state, plugin);
	gtk_menu_append (submenu, child);
	plugin->priv->last_change = child =
		create_menu_item ("Last Change", FALSE, GTK_SIGNAL_FUNC (on_dummy), state, plugin);
	gtk_menu_append (submenu, child);

	return item;
}


static GList * 
create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
	GtkWidget *item = create_menu_item (
		"CVS plugin dummy operation", TRUE, GTK_SIGNAL_FUNC (on_dummy), state,
		CVS_PLUGIN (plugin));
	return g_list_append (NULL, item);
}


static gboolean
cvs_dir_exists (GList *files)
{
	while (files) {
		GnomeVFSFileInfo *info = (GnomeVFSFileInfo*)files->data;
		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY &&
			strcmp (info->name, "CVS") == 0) {
			return TRUE;
		}
		files = files->next;
	}

	return FALSE;
}


static void  
update_main_menu_state (GnomeCmdPlugin *p, GnomeCmdState *state)
{
	CvsPlugin *plugin = CVS_PLUGIN (p);
	
	if (cvs_dir_exists (state->active_dir_files)) {
		gtk_widget_set_sensitive (plugin->priv->update, TRUE);
		gtk_widget_set_sensitive (plugin->priv->log, TRUE);
		gtk_widget_set_sensitive (plugin->priv->last_log, TRUE);
		gtk_widget_set_sensitive (plugin->priv->last_change, TRUE);
	}
	else {
		gtk_widget_set_sensitive (plugin->priv->update, FALSE);
		gtk_widget_set_sensitive (plugin->priv->log, FALSE);
		gtk_widget_set_sensitive (plugin->priv->last_log, FALSE);
		gtk_widget_set_sensitive (plugin->priv->last_change, FALSE);
	}
}


static void
configure (GnomeCmdPlugin *plugin)
{
	g_printerr ("configure...\n");
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	//CvsPlugin *plugin = CVS_PLUGIN (object);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (CvsPluginClass *klass)
{
	GtkObjectClass *object_class;
	GnomeCmdPluginClass *plugin_class;

	object_class = GTK_OBJECT_CLASS (klass);
	plugin_class = GNOME_CMD_PLUGIN_CLASS (klass);
	parent_class = gtk_type_class (gnome_cmd_plugin_get_type ());

	object_class->destroy = destroy;

	plugin_class->create_main_menu = create_main_menu;
	plugin_class->create_popup_menu_items = create_popup_menu_items;
	plugin_class->update_main_menu_state = update_main_menu_state;
	plugin_class->configure = configure;
}


static void
init (CvsPlugin *plugin)
{
	plugin->priv = g_new (CvsPluginPrivate, 1);
}



/***********************************
 * Public functions
 ***********************************/

GtkType
cvs_plugin_get_type         (void)
{
	static GtkType type = 0;

	if (type == 0)
	{
		GtkTypeInfo info =
		{
			"CvsPlugin",
			sizeof (CvsPlugin),
			sizeof (CvsPluginClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gnome_cmd_plugin_get_type (), &info);
	}
	return type;
}


GnomeCmdPlugin *
cvs_plugin_new (void)
{
	CvsPlugin *plugin;

	plugin = gtk_type_new (cvs_plugin_get_type ());

	return GNOME_CMD_PLUGIN (plugin);
}


GnomeCmdPlugin *create_plugin (void)
{
	return cvs_plugin_new ();
}


PluginInfo *get_plugin_info (void)
{
	if (!plugin_nfo.authors) {
		plugin_nfo.authors = g_new (gchar*, 2);
		plugin_nfo.authors[0] = AUTHOR;
		plugin_nfo.authors[1] = NULL;
	}
	return &plugin_nfo;
}
