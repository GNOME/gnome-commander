/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2014 Uwe Scholz

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

#ifndef __GNOME_CMD_PLUGIN_H__
#define __GNOME_CMD_PLUGIN_H__

#define GNOME_CMD_TYPE_PLUGIN              (gnome_cmd_plugin_get_type ())
#define GNOME_CMD_PLUGIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_PLUGIN, GnomeCmdPlugin))
#define GNOME_CMD_PLUGIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_PLUGIN, GnomeCmdPluginClass))
#define GNOME_CMD_IS_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_PLUGIN))
#define GNOME_CMD_IS_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_PLUGIN))
#define GNOME_CMD_PLUGIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_PLUGIN, GnomeCmdPluginClass))


typedef struct _GnomeCmdPlugin GnomeCmdPlugin;
typedef struct _GnomeCmdPluginClass GnomeCmdPluginClass;
typedef struct _GnomeCmdPluginPrivate GnomeCmdPluginPrivate;

typedef GnomeCmdPlugin *(*PluginConstructorFunc)(void);

struct _GnomeCmdPlugin
{
    GtkObject parent;
};

struct _GnomeCmdPluginClass
{
    GtkObjectClass parent_class;

    GtkWidget *(* create_main_menu) (GnomeCmdPlugin *plugin, GnomeCmdState *state);
    GList *(* create_popup_menu_items) (GnomeCmdPlugin *plugin, GnomeCmdState *state);
    void (* update_main_menu_state) (GnomeCmdPlugin *plugin, GnomeCmdState *state);
    void (* configure) (GnomeCmdPlugin *plugin);
};


GtkType gnome_cmd_plugin_get_type ();

GtkWidget *gnome_cmd_plugin_create_main_menu (GnomeCmdPlugin *plugin, GnomeCmdState *state);

GList *gnome_cmd_plugin_create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state);

void gnome_cmd_plugin_update_main_menu_state (GnomeCmdPlugin *plugin, GnomeCmdState *state);

void gnome_cmd_plugin_configure (GnomeCmdPlugin *plugin);

#endif //__GNOME_CMD_PLUGIN_H__
