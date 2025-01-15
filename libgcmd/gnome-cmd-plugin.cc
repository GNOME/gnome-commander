/** 
 * @file gnome-cmd-plugin.cc
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

#include <config.h>
#include "libgcmd-deps.h"
#include "plugin-info.h"
#include "gnome-cmd-state.h"
#include "gnome-cmd-plugin.h"


void gnome_cmd_plugin_free (GnomeCmdPlugin *plugin)
{
    if (plugin->free)
        plugin->free (plugin);
}


GSimpleActionGroup *gnome_cmd_plugin_create_actions (GnomeCmdPlugin *plugin, const gchar *name)
{
    if (plugin->create_actions)
        return plugin->create_actions (plugin, name);
    else
        return NULL;
}


GMenuModel *gnome_cmd_plugin_create_main_menu (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    if (plugin->create_main_menu)
        return plugin->create_main_menu (plugin, state);
    else
        return NULL;
}


GMenuModel *gnome_cmd_plugin_create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    if (plugin->create_popup_menu_items)
        return plugin->create_popup_menu_items (plugin, state);
    else
        return NULL;
}


void gnome_cmd_plugin_configure (GnomeCmdPlugin *plugin, GtkWindow *parent_window)
{
    if (plugin->configure)
        plugin->configure (plugin, parent_window);
}
