/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2023 Uwe Scholz

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

#pragma once

#define PLUGIN_TYPE_SETTINGS (plugin_settings_get_type ())
G_DECLARE_FINAL_TYPE (PluginSettings, plugin_settings, GCMD, SETTINGS, GObject)
PluginSettings *plugin_settings_new (void);

#define FILE_ROLLER_PLUGIN(obj) \
    G_TYPE_CHECK_INSTANCE_CAST (obj, file_roller_plugin_get_type (), FileRollerPlugin)
#define FILE_ROLLER_PLUGIN_CLASS(klass) \
    G_TYPE_CHECK_CLASS_CAST (klass, file_roller_plugin_get_type (), FileRollerPluginClass)
#define IS_FILE_ROLLER_PLUGIN(obj) \
    G_TYPE_CHECK_INSTANCE_TYPE (obj, file_roller_plugin_get_type ())


typedef struct _FileRollerPlugin FileRollerPlugin;
typedef struct _FileRollerPluginClass FileRollerPluginClass;
typedef struct _FileRollerPluginPrivate FileRollerPluginPrivate;

struct _FileRollerPlugin
{
    GnomeCmdPlugin parent;

    FileRollerPluginPrivate *priv;
};

struct _FileRollerPluginClass
{
    GnomeCmdPluginClass parent_class;
};

GType file_roller_plugin_get_type ();

GnomeCmdPlugin *file_roller_plugin_new ();
extern "C"
{
    GnomeCmdPlugin *create_plugin ();
    PluginInfo     *get_plugin_info ();
}
