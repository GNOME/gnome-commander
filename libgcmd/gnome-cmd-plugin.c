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

#include "gnome-cmd-plugin-info.h"
#include "gnome-cmd-plugin.h"

G_DEFINE_TYPE (GnomeCmdPlugin, gnome_cmd_plugin, G_TYPE_OBJECT)

/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_plugin_class_init (GnomeCmdPluginClass *klass)
{
    GnomeCmdPluginClass *plugin_class;

    plugin_class = GNOME_CMD_PLUGIN_CLASS (klass);

    plugin_class->create_actions = NULL;
    plugin_class->create_main_menu = NULL;
    plugin_class->create_popup_menu_items = NULL;
    plugin_class->configure = NULL;
}


static void gnome_cmd_plugin_init (GnomeCmdPlugin *plugin)
{
}

/***********************************
 * Public functions
 ***********************************/

GSimpleActionGroup *gnome_cmd_plugin_create_actions (GnomeCmdPlugin *plugin, const gchar *name)
{
    GnomeCmdPluginClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_PLUGIN (plugin), NULL);

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    return klass->create_actions (plugin, name);
}


GMenuModel *gnome_cmd_plugin_create_main_menu (GnomeCmdPlugin *plugin)
{
    GnomeCmdPluginClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_PLUGIN (plugin), NULL);

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    return klass->create_main_menu (plugin);
}


GMenuModel *gnome_cmd_plugin_create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GnomeCmdPluginClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_PLUGIN (plugin), NULL);

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    return klass->create_popup_menu_items (plugin, state);
}


void gnome_cmd_plugin_configure (GnomeCmdPlugin *plugin, GtkWindow *parent_window)
{
    GnomeCmdPluginClass *klass;

    g_return_if_fail (GNOME_CMD_IS_PLUGIN (plugin));

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    klass->configure (plugin, parent_window);
}
