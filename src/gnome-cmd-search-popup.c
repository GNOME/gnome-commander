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
#include <config.h>
#include "gnome-cmd-includes.h"

struct _GnomeCmdSearchPopmenuPrivate
{
};

static GtkMenuClass *parent_class = NULL;

static GnomeCmdFile *
get_selected_file (GtkCList *clist)
{
	gint row = clist->focus_row;
	GnomeCmdFile *finfo = (GnomeCmdFile*)gtk_clist_get_row_data (clist, row);
	return finfo;
}


static GList *
get_selected_files (GtkCList *clist)
{
	return g_list_append (NULL, get_selected_file (clist));
}


static GnomeCmdDir *
get_dir (GtkCList *clist)
{
	GnomeCmdFile *finfo = get_selected_file (clist);
	return finfo->dir;
}


static void
search_cap_cut (GtkMenuItem     *menuitem,
				GtkCList *clist)
{
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	cap_cut_files (get_selected_files (clist), get_dir (clist));
}


static void
search_cap_copy (GtkMenuItem     *menuitem,
				 GtkCList *clist)
{
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	cap_cut_files (get_selected_files (clist), get_dir (clist));
}


static void
search_delete                         (GtkMenuItem     *menuitem,
									   GtkCList *clist)
{
	GList *files;
	
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	files = get_selected_files (clist);
	if (files)
		gnome_cmd_file_delete (files);
}


static void
search_view                           (GtkMenuItem     *menuitem,
									   GtkCList *clist)
{
	GnomeCmdFile *finfo;
	
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	finfo = get_selected_file (clist);
	if (finfo)
		gnome_cmd_file_view (finfo);
}


static void
search_edit                           (GtkMenuItem     *menuitem,
									   GtkCList *clist)
{
	GnomeCmdFile *finfo;
	
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	finfo = get_selected_file (clist);
	if (finfo)
		gnome_cmd_file_edit (finfo);
}


static void
search_chmod                          (GtkMenuItem     *menuitem,
									   GtkCList *clist)
{
	GList *files;
	
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	files = get_selected_files (clist);
	if (files)
		gnome_cmd_file_show_chmod_dialog (files);
}


static void
search_chown                          (GtkMenuItem     *menuitem,
									   GtkCList *clist)
{
	GList *files;
	
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	files = get_selected_files (clist);
	if (files)
		gnome_cmd_file_show_chown_dialog (files);
}


static void
search_properties                     (GtkMenuItem     *menuitem,
									   GtkCList *clist)
{
	GnomeCmdFile *finfo;
	
	g_return_if_fail (GTK_IS_CLIST (clist));
	
	finfo = get_selected_file (clist);	
	if (finfo)
		gnome_cmd_file_show_properties (finfo);
}




/*******************************
 * Gtk class implementation
 *******************************/

static GnomeUIInfo popmenu_uiinfo[] =
{
	GNOMEUIINFO_MENU_CUT_ITEM (search_cap_cut, NULL),
	GNOMEUIINFO_MENU_COPY_ITEM (search_cap_copy, NULL),
	{
		GNOME_APP_UI_ITEM, N_("_Delete"),
		NULL,
		search_delete, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TRASH,
		0, 0, NULL
	},
	GNOMEUIINFO_SEPARATOR,
	{
		GNOME_APP_UI_ITEM, N_("_View"),
		NULL,
		search_view, NULL, NULL,
		GNOME_APP_PIXMAP_NONE, NULL,
		0, 0, NULL
	},
	{
		GNOME_APP_UI_ITEM, N_("_Edit"),
		NULL,
		search_edit, NULL, NULL,
		GNOME_APP_PIXMAP_NONE, NULL,
		0, 0, NULL
	},
	GNOMEUIINFO_SEPARATOR,
	{
		GNOME_APP_UI_ITEM, N_("Chmod"),
		NULL,
		search_chmod, NULL, NULL,
		GNOME_APP_PIXMAP_NONE, NULL,
		0, 0, NULL
	},
	{
		GNOME_APP_UI_ITEM, N_("Chown"),
		NULL,
		search_chown, NULL, NULL,
		GNOME_APP_PIXMAP_NONE, NULL,
		0, 0, NULL
	},
	{
		GNOME_APP_UI_ITEM, N_("Properties"),
		NULL,
		search_properties, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PROP,
		0, 0, NULL
	},
	GNOMEUIINFO_END
};


static void
destroy (GtkObject *object)
{
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
class_init (GnomeCmdSearchPopmenuClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	parent_class = gtk_type_class (gtk_menu_get_type ());
	object_class->destroy = destroy;
	widget_class->map = map;
}


static void
init (GnomeCmdSearchPopmenu *menu)
{	
	gnome_app_fill_menu (GTK_MENU_SHELL (menu), popmenu_uiinfo,
						 NULL, FALSE, 0);

	gtk_widget_ref (popmenu_uiinfo[0].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "cut1",
							  popmenu_uiinfo[0].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[1].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "copy1",
							  popmenu_uiinfo[1].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[2].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "delete1",
							  popmenu_uiinfo[2].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[3].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "separator1",
							  popmenu_uiinfo[3].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[4].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "view1",
							  popmenu_uiinfo[4].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[5].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "edit1",
							  popmenu_uiinfo[5].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[6].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "separator2",
							  popmenu_uiinfo[6].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[7].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "chmod1",
							  popmenu_uiinfo[7].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[8].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "chown1",
							  popmenu_uiinfo[8].widget,
							  (GtkDestroyNotify) gtk_widget_unref);

	gtk_widget_ref (popmenu_uiinfo[9].widget);
	gtk_object_set_data_full (GTK_OBJECT (menu), "properties1",
							  popmenu_uiinfo[9].widget,
							  (GtkDestroyNotify) gtk_widget_unref);
}




/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_search_popmenu_new (GtkCList *clist)
{	
	int i = 0;
	GnomeCmdSearchPopmenu *menu;

	while (popmenu_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO) {
		if (popmenu_uiinfo[i].type == GNOME_APP_UI_ITEM
			|| popmenu_uiinfo[i].type == GNOME_APP_UI_ITEM_CONFIGURABLE)
			popmenu_uiinfo[i].user_data = clist;
		i++;
	}
	
	menu = gtk_type_new (gnome_cmd_search_popmenu_get_type ());

	return GTK_WIDGET (menu);
}



GtkType
gnome_cmd_search_popmenu_get_type         (void)
{
	static GtkType dlg_type = 0;

	if (dlg_type == 0)
	{
		GtkTypeInfo dlg_info =
		{
			"GnomeCmdSearchPopmenu",
			sizeof (GnomeCmdSearchPopmenu),
			sizeof (GnomeCmdSearchPopmenuClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		dlg_type = gtk_type_unique (gtk_menu_get_type (), &dlg_info);
	}
	return dlg_type;
}

