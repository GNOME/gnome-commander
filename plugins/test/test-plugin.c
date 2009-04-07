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
#include <gnome.h>
#include <libgcmd/libgcmd.h>
#include "test-plugin.h"
#include "test-plugin.xpm"

#define NAME "Example"
#define COPYRIGHT "Copyright \xc2\xa9 2003-2006 Marcus Bjurman"
#define AUTHOR "Marcus Bjurman <marbj499@student.liu.se>"
#define WEBPAGE "http://www.nongnu.org/gcmd"


static PluginInfo plugin_nfo = {
    GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
    NAME,
    VERSION,
    COPYRIGHT,
    NULL,
    NULL,
    NULL,
    NULL,
    WEBPAGE
};

struct _TestPluginPrivate
{
#ifdef __sun
    gchar dummy;  // Sun's forte compiler does not like empty structs
#endif
};

static GnomeCmdPluginClass *parent_class = NULL;


static void on_dummy (GtkMenuItem *item, gpointer data)
{
    gnome_ok_dialog ("Test plugin dummy operation");
}


static GtkWidget *create_menu_item (const gchar *name, gboolean show_pixmap, GtkSignalFunc callback, gpointer data)
{
    GtkWidget *item, *label;

    if (show_pixmap)
    {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) test_plugin_xpm);
        GtkWidget *pixmap = gtk_image_new_from_pixbuf (pixbuf);
        g_object_unref (G_OBJECT (pixbuf));
        item = gtk_image_menu_item_new ();
        if (pixmap)
        {
            gtk_widget_show (pixmap);
            gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), pixmap);
        }
    }
    else
        item = gtk_menu_item_new ();

    gtk_widget_show (item);

    // Create the contents of the menu item
    label = gtk_label_new (name);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (item), label);

    // Connect to the signal and set user data
    gtk_object_set_data (GTK_OBJECT (item), GNOMEUIINFO_KEY_UIDATA, data);

    if (callback)
        gtk_signal_connect (GTK_OBJECT (item), "activate", callback, data);

    return item;
}


static GtkWidget *create_main_menu (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GtkWidget *item, *child;
    GtkMenu *submenu;

    submenu = GTK_MENU (gtk_menu_new ());
    item = create_menu_item ("Test", FALSE, NULL, NULL);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
                               GTK_WIDGET (submenu));

    child = create_menu_item ("Test plugin dummy operation", FALSE, GTK_SIGNAL_FUNC (on_dummy), state);
    gtk_menu_append (submenu, child);

    return item;
}


static GList *create_popup_menu_items (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
    GtkWidget *item = create_menu_item ("Test plugin dummy operation", TRUE, GTK_SIGNAL_FUNC (on_dummy), state);

    return g_list_append (NULL, item);
}


static void update_main_menu_state (GnomeCmdPlugin *plugin, GnomeCmdState *state)
{
}


static void configure (GnomeCmdPlugin *plugin)
{
    gnome_ok_dialog ("Test plugin configuration dialog");
}




/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    //TestPlugin *plugin = TEST_PLUGIN (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (TestPluginClass *klass)
{
    GtkObjectClass *object_class;
    GnomeCmdPluginClass *plugin_class;

    object_class = GTK_OBJECT_CLASS (klass);
    plugin_class = GNOME_CMD_PLUGIN_CLASS (klass);
    parent_class = gtk_type_class (gnome_cmd_plugin_get_type ());

    object_class->destroy = destroy;

    plugin_class->create_main_menu = create_main_menu;
    plugin_class->create_popup_menu_items = create_popup_menu_items;
    plugin_class->update_main_menu_state = update_main_menu_state;
    plugin_class->configure = configure;
}


static void init (TestPlugin *plugin)
{
    plugin->priv = g_new (TestPluginPrivate, 1);
}



/***********************************
 * Public functions
 ***********************************/

GtkType test_plugin_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info =
        {
            "TestPlugin",
            sizeof (TestPlugin),
            sizeof (TestPluginClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        type = gtk_type_unique (gnome_cmd_plugin_get_type (), &info);
    }
    return type;
}


GnomeCmdPlugin *test_plugin_new ()
{
    TestPlugin *plugin = gtk_type_new (test_plugin_get_type ());

    return GNOME_CMD_PLUGIN (plugin);
}


GnomeCmdPlugin *create_plugin ()
{
    return test_plugin_new ();
}


PluginInfo *get_plugin_info ()
{
    if (!plugin_nfo.authors)
    {
        plugin_nfo.authors = g_new0 (gchar *, 2);
        plugin_nfo.authors[0] = AUTHOR;
        plugin_nfo.authors[1] = NULL;
        plugin_nfo.comments = g_strdup (_("This is an example plugin that is mostly useful as a "
                                          "simple example for aspiring plugin hackers"));
    }

    return &plugin_nfo;
}
