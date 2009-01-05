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

#include <config.h>
#include "libgcmd-deps.h"
#include "plugin-info.h"
#include "gnome-cmd-state.h"
#include "gnome-cmd-plugin.h"


static GtkObjectClass *parent_class = NULL;



/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    //GnomeCmdPlugin *plugin = GNOME_CMD_PLUGIN (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GnomeCmdPluginClass *klass)
{
    GtkObjectClass *object_class;
    GnomeCmdPluginClass *plugin_class;

    object_class = GTK_OBJECT_CLASS (klass);
    plugin_class = GNOME_CMD_PLUGIN_CLASS (klass);
    parent_class = gtk_type_class (gtk_object_get_type ());

    object_class->destroy = destroy;

    plugin_class->create_main_menu = NULL;
    plugin_class->create_popup_menu_items = NULL;
    plugin_class->update_main_menu_state = NULL;
    plugin_class->configure = NULL;
}


static void
init (GnomeCmdPlugin *plugin)
{
}



/***********************************
 * Public functions
 ***********************************/

GtkType
gnome_cmd_plugin_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "GnomeCmdPlugin",
            sizeof (GnomeCmdPlugin),
            sizeof (GnomeCmdPluginClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gtk_object_get_type (), &info);
    }
    return type;
}


GtkWidget *
gnome_cmd_plugin_create_main_menu (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GnomeCmdPluginClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_PLUGIN (plugin), NULL);

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    return klass->create_main_menu (plugin, state);
}


GList *
gnome_cmd_plugin_create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GnomeCmdPluginClass *klass;

    g_return_val_if_fail (GNOME_CMD_IS_PLUGIN (plugin), NULL);

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    return klass->create_popup_menu_items (plugin, state);
}


void
gnome_cmd_plugin_update_main_menu_state (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GnomeCmdPluginClass *klass;

    g_return_if_fail (GNOME_CMD_IS_PLUGIN (plugin));

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    klass->update_main_menu_state (plugin, state);
}


void
gnome_cmd_plugin_configure (GnomeCmdPlugin *plugin)
{
    GnomeCmdPluginClass *klass;

    g_return_if_fail (GNOME_CMD_IS_PLUGIN (plugin));

    klass = GNOME_CMD_PLUGIN_GET_CLASS (plugin);

    klass->configure (plugin);
}

