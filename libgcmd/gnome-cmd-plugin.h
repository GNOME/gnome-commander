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

#ifndef __GNOME_CMD_PLUGIN_H__
#define __GNOME_CMD_PLUGIN_H__

G_BEGIN_DECLS

#define GNOME_CMD_PLUGIN(obj) \
    GTK_CHECK_CAST (obj, gnome_cmd_plugin_get_type (), GnomeCmdPlugin)
#define GNOME_CMD_PLUGIN_CLASS(klass) \
    GTK_CHECK_CLASS_CAST (klass, gnome_cmd_plugin_get_type (), GnomeCmdPluginClass)
#define GNOME_CMD_IS_PLUGIN(obj) \
    GTK_CHECK_TYPE (obj, gnome_cmd_plugin_get_type ())
#define GNOME_CMD_PLUGIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_CMD_PLUGIN, GnomeCmdPluginClass))


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

G_END_DECLS

#endif //__GNOME_CMD_PLUGIN_H__
