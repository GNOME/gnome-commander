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

#include "config.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgcmd.h>
#include "test-plugin.h"

#define NAME "Example"
#define COPYRIGHT "Copyright \xc2\xa9 2003-2006 Marcus Bjurman\n\xc2\xa9 2013-2024 Uwe Scholz"
#define AUTHOR "Marcus Bjurman <marbj499@student.liu.se>"
#define WEBPAGE "https://gcmd.github.io"


static GnomeCmdPluginInfo plugin_nfo = {
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

struct _GnomeCmdTestPlugin
{
    GObject parent;
};


static void gnome_cmd_configurable_init (GnomeCmdConfigurableInterface *iface);
static void gnome_cmd_file_actions_init (GnomeCmdFileActionsInterface *iface);


G_DEFINE_TYPE_WITH_CODE (GnomeCmdTestPlugin, gnome_cmd_test_plugin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_CONFIGURABLE, gnome_cmd_configurable_init)
                         G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_FILE_ACTIONS, gnome_cmd_file_actions_init))


static void show_dummy_dialog(GtkWindow *parent_window)
{
    GtkWidget *dialog;
    GtkDialogFlags flags = GTK_DIALOG_MODAL;
    dialog = gtk_message_dialog_new (parent_window,
                                     flags,
                                     GTK_MESSAGE_OTHER,
                                     GTK_BUTTONS_OK,
                                     "Test plugin dummy operation");
    g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_window_destroy), dialog);
    gtk_window_present (GTK_WINDOW (dialog));
}


static GMenuModel *create_main_menu (GnomeCmdFileActions *iface)
{
    GMenu *menu = g_menu_new ();
    g_menu_append (menu, "Test plugin dummy operation", "dummy");
    return G_MENU_MODEL (menu);
}


static GMenuModel *create_popup_menu_items (GnomeCmdFileActions *iface, GnomeCmdState *state)
{
    GMenu *menu = g_menu_new ();
    g_menu_append (menu, "Test plugin dummy operation", "dummy");
    return G_MENU_MODEL (menu);
}


static void execute (GnomeCmdFileActions *iface, const gchar *action, GVariant *parameter, GtkWindow *parent_window, GnomeCmdState *state)
{
    show_dummy_dialog(parent_window);
}


static void configure (GnomeCmdConfigurable *iface, GtkWindow *parent_window)
{
    show_dummy_dialog(parent_window);
}




/*******************************
 * Gtk class implementation
 *******************************/

static void gnome_cmd_test_plugin_class_init (GnomeCmdTestPluginClass *klass)
{
}


static void gnome_cmd_configurable_init (GnomeCmdConfigurableInterface *iface)
{
    iface->configure = configure;
}


static void gnome_cmd_file_actions_init (GnomeCmdFileActionsInterface *iface)
{
    iface->create_main_menu = create_main_menu;
    iface->create_popup_menu_items = create_popup_menu_items;
    iface->execute = execute;
}


static void gnome_cmd_test_plugin_init (GnomeCmdTestPlugin *plugin)
{
}

/***********************************
 * Public functions
 ***********************************/

GObject *create_plugin ()
{
    return G_OBJECT (g_object_new (GNOME_CMD_TYPE_TEST_PLUGIN, NULL));
}

GnomeCmdPluginInfo *get_plugin_info ()
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
