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

#include <config.h>
#include <gtk/gtk.h>
#include <libgcmd/libgcmd.h>
#include "test-plugin.h"

#define NAME "Example"
#define COPYRIGHT "Copyright \xc2\xa9 2003-2006 Marcus Bjurman\n\xc2\xa9 2013-2024 Uwe Scholz"
#define AUTHOR "Marcus Bjurman <marbj499@student.liu.se>"
#define WEBPAGE "https://gcmd.github.io"


static PluginInfo plugin_nfo = {
    GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
    NAME,
    PACKAGE_VERSION,
    COPYRIGHT,
    NULL,
    NULL,
    NULL,
    NULL,
    WEBPAGE
};

struct TestPlugin
{
    GnomeCmdPlugin parent;

    gchar *action_group_name;
};


static void show_dummy_dialog(GtkWindow *parent_window)
{
    GtkWidget *dialog;
    GtkDialogFlags flags = GTK_DIALOG_MODAL;
    dialog = gtk_message_dialog_new (parent_window,
                                     flags,
                                     GTK_MESSAGE_OTHER,
                                     GTK_BUTTONS_OK,
                                     "Test plugin dummy operation");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_window_destroy (GTK_WINDOW (dialog));
}


static void on_dummy (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    show_dummy_dialog(nullptr);
}


static GSimpleActionGroup *create_actions (GnomeCmdPlugin *plugin, const gchar *name)
{
    TestPlugin *priv = (TestPlugin *) plugin;

    priv->action_group_name = g_strdup (name);

    GSimpleActionGroup *group = g_simple_action_group_new ();
    static const GActionEntry entries[] = {
        { "dummy", on_dummy }
    };
    g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), plugin);
    return group;
}


static GMenuModel *create_main_menu (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    TestPlugin *priv = (TestPlugin *) plugin;

    GMenu *menu = g_menu_new ();
    gchar *action_name = g_strdup_printf ("%s.dummy", priv->action_group_name);
    g_menu_append (menu, "Test plugin dummy operation", action_name);
    g_free (action_name);
    return G_MENU_MODEL (menu);
}


static GMenuModel *create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    TestPlugin *priv = (TestPlugin *) plugin;

    GMenu *menu = g_menu_new ();
    gchar *action_name = g_strdup_printf ("%s.dummy", priv->action_group_name);
    g_menu_append (menu, "Test plugin dummy operation", action_name);
    g_free (action_name);
    return G_MENU_MODEL (menu);
}


static void configure (GnomeCmdPlugin *plugin, GtkWindow *parent_window)
{
    show_dummy_dialog(parent_window);
}


static void dispose (GnomeCmdPlugin *plugin)
{
    TestPlugin *priv = (TestPlugin *) plugin;

    g_clear_pointer (&priv->action_group_name, g_free);

    g_free (plugin);
}


static GnomeCmdPlugin *test_plugin_new ()
{
    TestPlugin *plugin = g_new0 (TestPlugin, 1);

    plugin->parent.free = dispose;
    plugin->parent.create_actions = create_actions;
    plugin->parent.create_main_menu = create_main_menu;
    plugin->parent.create_popup_menu_items = create_popup_menu_items;
    plugin->parent.configure = configure;

    return (GnomeCmdPlugin *) plugin;
}


/***********************************
 * Public functions
 ***********************************/

extern "C"
{
    GnomeCmdPlugin *create_plugin ()
    {
        return test_plugin_new ();
    }
}

extern "C"
{
    PluginInfo *get_plugin_info ()
    {
        if (!plugin_nfo.authors)
        {
            plugin_nfo.authors = g_new0 (gchar *, 2);
            plugin_nfo.authors[0] = (gchar*) AUTHOR;
            plugin_nfo.authors[1] = NULL;
            plugin_nfo.comments = g_strdup (_("This is an example plugin that is mostly useful as a "
                                            "simple example for aspiring plugin hackers"));
        }
        return &plugin_nfo;
    }
}

