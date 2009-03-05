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
#ifndef __GNOME_CMD_MAIN_MENU_H__
#define __GNOME_CMD_MAIN_MENU_H__

#include "gnome-cmd-file.h"
#include "plugin_manager.h"

#define GNOME_CMD_MAIN_MENU(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_main_menu_get_type (), GnomeCmdMainMenu)
#define GNOME_CMD_MAIN_MENU_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_main_menu_get_type (), GnomeCmdMainMenuClass)
#define GNOME_CMD_IS_MAIN_MENU(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_main_menu_get_type ())


struct GnomeCmdMainMenuPrivate;


struct GnomeCmdMainMenu
{
    GtkMenuBar parent;

    GnomeCmdMainMenuPrivate *priv;
};


struct GnomeCmdMainMenuClass
{
    GtkMenuBarClass parent_class;
};


GtkWidget *gnome_cmd_main_menu_new ();

GtkType gnome_cmd_main_menu_get_type ();

void gnome_cmd_main_menu_update_connections (GnomeCmdMainMenu *main_menu);

void gnome_cmd_main_menu_update_bookmarks (GnomeCmdMainMenu *main_menu);

void gnome_cmd_main_menu_update_sens (GnomeCmdMainMenu *main_menu);

void gnome_cmd_main_menu_add_plugin_menu (GnomeCmdMainMenu *main_menu, PluginData *data);

#endif // __GNOME_CMD_MAIN_MENU_H__
