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
#include "gnome-cmd-list-popmenu.h"
#include "gnome-cmd-file-selector.h"
#include "utils.h"
#include "cap.h"

#include "../pixmaps/file-type-icons/file_type_dir.xpm"
#include "../pixmaps/file-type-icons/file_type_regular.xpm"


static GtkMenuClass *parent_class = NULL;

struct _GnomeCmdListPopmenuPrivate
{
};


static void
on_new_directory (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
	gnome_cmd_file_selector_show_mkdir_dialog (fs);
}


static void
on_new_textfile (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
	gnome_cmd_file_selector_show_new_textfile_dialog (fs);
}


static void
on_refresh (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
	gnome_cmd_file_selector_reload (fs);
}


static void
on_paste (GtkMenuItem *item, GnomeCmdFileSelector *fs)
{
	gnome_cmd_file_selector_cap_paste (fs);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
	GnomeCmdListPopmenu *menu = GNOME_CMD_LIST_POPMENU (object);
	
	g_free (menu->priv);
	
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
class_init (GnomeCmdListPopmenuClass *class)
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
init (GnomeCmdListPopmenu *menu)
{
	menu->priv = g_new (GnomeCmdListPopmenuPrivate, 1);
}




/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_list_popmenu_new (GnomeCmdFileSelector *fs)
{	
	int i;
	GnomeCmdListPopmenu *menu;
	
	static GnomeUIInfo new_uiinfo[] =
	{
		{
			GNOME_APP_UI_ITEM, N_("_Directory"),
			NULL,
			on_new_directory, NULL, NULL,
			GNOME_APP_PIXMAP_DATA, file_type_dir_xpm,
			0, 0, NULL
		},
		{
			GNOME_APP_UI_ITEM, N_("_Text file"),
			NULL,
			on_new_textfile, NULL, NULL,
			GNOME_APP_PIXMAP_DATA, file_type_regular_xpm,
			0, 0, NULL
		},
		GNOMEUIINFO_END
	};

	static GnomeUIInfo popmenu_uiinfo[] =
	{
		{
			GNOME_APP_UI_SUBTREE, N_("_New..."),
			NULL,
			new_uiinfo, NULL, NULL,
			GNOME_APP_PIXMAP_NONE, NULL,
			0, 0, NULL
		},
		{
			GNOME_APP_UI_ITEM, N_("_Paste"),
			NULL,
			on_paste, NULL, NULL,
			GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_PASTE,
			0, 0, NULL
		},
		{
			GNOME_APP_UI_ITEM, N_("_Refresh"),
			NULL,
			on_refresh, NULL, NULL,
			GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_REFRESH,
			0, 0, NULL
		},
		GNOMEUIINFO_END
	};

	g_return_val_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs), NULL);

	menu = gtk_type_new (gnome_cmd_list_popmenu_get_type ());
	
	
	/* Set default callback data 
	 */
	i = 0;
	while (new_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO) {
		if (new_uiinfo[i].type == GNOME_APP_UI_ITEM) {
			new_uiinfo[i].user_data = fs;
		}
		i++;
	}
	i = 0;
	while (popmenu_uiinfo[i].type != GNOME_APP_UI_ENDOFINFO) {
		if (popmenu_uiinfo[i].type == GNOME_APP_UI_ITEM) {
			popmenu_uiinfo[i].user_data = fs;
		}
		i++;
	}

	
	/* Fill the menu
	 */
	gnome_app_fill_menu (GTK_MENU_SHELL (menu), popmenu_uiinfo,
						 NULL, FALSE, 0);

	return GTK_WIDGET (menu);
}



GtkType
gnome_cmd_list_popmenu_get_type         (void)
{
	static GtkType type = 0;

	if (type == 0)
	{
		GtkTypeInfo info =
		{
			"GnomeCmdListPopmenu",
			sizeof (GnomeCmdListPopmenu),
			sizeof (GnomeCmdListPopmenuClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_menu_get_type (), &info);
	}
	return type;
}

