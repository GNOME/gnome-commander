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

#ifndef __PLUGIN_MANAGER_H__
#define __PLUGIN_MANAGER_H__

#include <gmodule.h>

struct PluginData
{
    gboolean active;
    gboolean loaded;
    gboolean autoload;
    gchar *fname;
    gchar *fpath;
    GnomeCmdPlugin *plugin;
    PluginInfo *info;
    GtkWidget *menu;
    GModule *module;
};


void plugin_manager_init ();
void plugin_manager_shutdown ();
GList *plugin_manager_get_all ();
void plugin_manager_show ();

#endif // __PLUGIN_MANAGER_H__
