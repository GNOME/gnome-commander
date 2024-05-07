/** 
 * @file plugin_manager.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gmodule.h>

struct PluginData
{
    gboolean active;
    gboolean loaded;
    gboolean autoload;
    gchar *fname;
    gchar *fpath;
    gchar *action_group_name;
    GnomeCmdPlugin *plugin;
    PluginInfo *info;
    GMenuModel *menu;
    GModule *module;
};


void plugin_manager_init ();
void plugin_manager_shutdown ();
GList *plugin_manager_get_all ();
void plugin_manager_show ();
