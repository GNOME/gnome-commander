/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2012 Piotr Eljasiak
    Copyright (C) 2013-2024 Uwe Scholz

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

#define TEST_PLUGIN(obj) \
    G_TYPE_CHECK_INSTANCE_CAST (obj, test_plugin_get_type (), TestPlugin)
#define TEST_PLUGIN_CLASS(klass) \
    G_TYPE_CHECK_CLASS_CAST (klass, test_plugin_get_type (), TestPluginClass)
#define IS_TEST_PLUGIN(obj) \
    G_TYPE_CHECK_INSTANCE_TYPE (obj, test_plugin_get_type ())


typedef struct _TestPlugin TestPlugin;
typedef struct _TestPluginClass TestPluginClass;
typedef struct _TestPluginPrivate TestPluginPrivate;

struct _TestPlugin
{
    GnomeCmdPlugin parent;

    TestPluginPrivate *priv;
};

struct _TestPluginClass
{
    GnomeCmdPluginClass parent_class;
};


GType test_plugin_get_type ();

GnomeCmdPlugin *test_plugin_new ();
extern "C"
{
    GnomeCmdPlugin *create_plugin   ();
    PluginInfo     *get_plugin_info ();
}
