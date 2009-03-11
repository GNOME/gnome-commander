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
#include <dirent.h>
#include <gmodule.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "plugin_manager.h"
#include "utils.h"
#include "imageloader.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-about-plugin.h"

using namespace std;


// The names of these functions shall never change
#define MODULE_INIT_FUNC "create_plugin"
#define MODULE_INFO_FUNC "get_plugin_info"


static GList *plugins = NULL;
static GdkPixmap *exec_pixmap = NULL;
static GdkBitmap *exec_mask = NULL;
static GdkPixmap *blank_pixmap = NULL;
static GdkBitmap *blank_mask = NULL;


static void load_plugin (PluginData *data)
{
    GModule *module = g_module_open (data->fpath, G_MODULE_BIND_LAZY);
    PluginInfoFunc info_func;
    PluginConstructorFunc init_func;
    GnomeCmdPlugin *plugin;

    if (!module)
    {
        g_printerr ("ERROR: Failed to load the plugin '%s': %s\n", data->fname, g_module_error ());
        return;
    }

    // Try to get a reference to the "get_plugin_info" function
    if (!g_module_symbol (module, MODULE_INFO_FUNC, (gpointer *) &info_func))
    {
        g_printerr ("ERROR: The plugin-file '%s' has no function named '%s'.\n", data->fname, MODULE_INFO_FUNC);
        g_module_close (module);
        return;
    }

    // Try to get the plugin info
    data->info = info_func ();
    if (!data->info)
    {
        g_printerr ("ERROR: The plugin-file '%s' did not return valid plugin info:\n", data->fname);
        g_printerr ("  The function '%s' returned NULL\n", MODULE_INFO_FUNC);
        g_module_close (module);
        return;
    }

    // Check that the plugin is compatible
    if (data->info->plugin_system_version != GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION)
    {
        g_printerr ("ERROR: The plugin '%s' is not compatible with this version of %s:\n", data->info->name, PACKAGE);
        g_printerr ("  Plugin system supported by the plugin is %d, it should be %d\n", data->info->plugin_system_version,
                    GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION);
        return;
    }

    // Try to get a reference to the "create_plugin" function
    if (!g_module_symbol (module, MODULE_INIT_FUNC, (gpointer *) &init_func))
    {
        g_printerr ("ERROR: The plugin '%s' has no '%s' function\n", data->info->name, MODULE_INIT_FUNC);
        g_module_close (module);
        return;
    }

    // Try to initialize the plugin
    plugin = init_func ();
    if (!plugin)
    {
        g_printerr ("ERROR: The plugin '%s' could not be initialized:\n", data->info->name);
        g_printerr ("  The '%s' function returned NULL\n", MODULE_INIT_FUNC);
        g_module_close (module);
        return;
    }

    // All is OK, everyone is happy
    data->plugin = plugin;
    data->module = module;
    data->loaded = TRUE;
}


static void activate_plugin (PluginData *data)
{
    if (data->active)
        return;

    if (!data->loaded)
        load_plugin (data);

    if (!data->loaded)
        return;

    data->active = TRUE;

    GnomeCmdState *state = gnome_cmd_main_win_get_state (main_win);
    data->menu = gnome_cmd_plugin_create_main_menu (data->plugin, state);
    if (data->menu)
        gnome_cmd_main_win_add_plugin_menu (main_win, data);
}


static void inactivate_plugin (PluginData *data)
{
    if (!data->active)
        return;

    data->active = FALSE;
    if (data->menu)
        gtk_object_destroy (GTK_OBJECT (data->menu));
}


static void scan_plugins_in_dir (const gchar *dpath)
{
    DIR *dir = opendir (dpath);
    char buff[256];
    char *prev_dir;
    struct dirent *ent;

    if (dir == NULL)
    {
        gchar *msg = g_strdup_printf ("Could not list files in %s: %s\n", dpath, strerror (errno));
        warn_print (msg);
        g_free (msg);
        return;
    }

    prev_dir = getcwd (buff, sizeof(buff));
    chdir (dpath);

    while ((ent = readdir (dir)) != NULL)
    {
        struct stat buf;

        if (strcmp (ent->d_name+strlen(ent->d_name)-3, ".so") != 0)
            continue;

        if (stat (ent->d_name, &buf) == 0)
        {
            if (buf.st_mode & S_IFREG)
            {
                // the direntry has the .so extension and is a regular file, lets accept it
                PluginData *data = g_new0 (PluginData, 1);
                data->fname = g_strdup (ent->d_name);
                data->fpath = g_build_path (G_DIR_SEPARATOR_S, dpath, ent->d_name, NULL);
                data->loaded = FALSE;
                data->active = FALSE;
                data->menu = NULL;
                data->autoload = FALSE;
                activate_plugin (data);
                if (!data->loaded)
                {
                    g_free (data->fname);
                    g_free (data->fpath);
                    g_free (data);
                }
                else
                    plugins = g_list_append (plugins, data);
            }
            else
                printf ("%s is not a regular file\n", ent->d_name);
        }
        else
            printf ("Failed to stat %s\n", ent->d_name);
    }

    closedir (dir);

    if (prev_dir)
        chdir (prev_dir);
}


void plugin_manager_init ()
{
    if (plugins)
    {
        warn_print ("plugin_manager already initiated\n");
        return;
    }

    // find user plugins
    gchar *user_dir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir(), ".gnome-commander/plugins", NULL);
    create_dir_if_needed (user_dir);
    scan_plugins_in_dir (user_dir);
    g_free (user_dir);

    // find system plugins
    scan_plugins_in_dir (PLUGIN_DIR);

    // activate plugins
    for (GList *l=gnome_cmd_data_get_auto_load_plugins (); l; l=l->next)
    {
        char *name = (gchar *) l->data;

        for (GList *l2 = plugins; l2; l2 = l2->next)
        {
            PluginData *data = (PluginData *) l2->data;
            if (strcmp (name, data->fname) == 0)
                data->autoload = TRUE;
        }
    }

    // inactivate plugins that shouldn't be autoloaded
    for (GList *l=plugins; l; l=l->next)
    {
        PluginData *data = (PluginData *) l->data;
        if (!data->autoload)
            inactivate_plugin (data);
    }
}


void plugin_manager_shutdown ()
{
    GList *out = NULL;

    for (GList *l=plugins; l; l=l->next)
    {
        PluginData *data = (PluginData *) l->data;
        if (data->active)
            out = g_list_append (out, data->fname);
    }

    gnome_cmd_data_set_auto_load_plugins (out);
}


GList *plugin_manager_get_all ()
{
    return plugins;
}


PluginData *get_selected_plugin (GtkCList *list)
{
    return (PluginData *) gtk_clist_get_row_data (list, list->focus_row);
}


static void update_plugin_list (GtkCList *list, GtkWidget *dialog)
{
    gint old_focus = list->focus_row;
    gint row = 0;
    gboolean only_update = (list->rows > 0);

    for (GList *tmp=plugins; tmp; tmp=tmp->next)
    {
        PluginData *data = (PluginData *) tmp->data;
        gchar *text[5];

        text[0] = NULL;
        text[1] = data->info->name;
        text[2] = data->info->version;
        text[3] = data->fname;
        text[4] = NULL;

        if (only_update)
            gtk_clist_set_text (list, row, 1, text[1]);
        else
            gtk_clist_append (list, text);

        if (data->active)
            gtk_clist_set_pixmap (list, row, 0, exec_pixmap, exec_mask);
        else
            gtk_clist_set_pixmap (list, row, 0, blank_pixmap, blank_mask);

        gtk_clist_set_row_data (list, row, data);

        row++;
    }

    gtk_clist_select_row (list, old_focus, 0);
}


inline void do_toggle (GtkWidget *dialog)
{
    GtkCList *list = GTK_CLIST (lookup_widget (dialog, "avail_list"));
    PluginData *data = get_selected_plugin (list);

    if (!data) return;

    if (data->active)
        inactivate_plugin (data);
    else
        activate_plugin (data);

    update_plugin_list (list, dialog);
}


static void
on_plugin_selected (GtkCList *list, gint row, gint column,
                    GdkEventButton *event, GtkWidget *dialog)
{
    GtkWidget *toggle_button = lookup_widget (dialog, "toggle_button");
    GtkWidget *conf_button = lookup_widget (dialog, "conf_button");
    GtkWidget *about_button = lookup_widget (dialog, "about_button");

    PluginData *data = get_selected_plugin (list);
    g_return_if_fail (data != NULL);

    if (event && event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
        do_toggle (dialog);
        return;
    }

    gtk_widget_set_sensitive (about_button, TRUE);
    gtk_button_set_label (GTK_BUTTON (toggle_button), data->active ? _("Disable") : _("Enable"));
    gtk_widget_set_sensitive (toggle_button, TRUE);
    gtk_widget_set_sensitive (conf_button, data->active);
}


static void
on_plugin_unselected (GtkCList *list, gint row, gint column,
                      GdkEventButton *event, GtkWidget *dialog)
{
    GtkWidget *toggle_button = lookup_widget (dialog, "toggle_button");
    GtkWidget *conf_button = lookup_widget (dialog, "conf_button");
    GtkWidget *about_button = lookup_widget (dialog, "about_button");

    gtk_widget_set_sensitive (toggle_button, FALSE);
    gtk_widget_set_sensitive (about_button, FALSE);
    gtk_widget_set_sensitive (conf_button, FALSE);
}


static void on_toggle (GtkButton *button, GtkWidget *dialog)
{
    do_toggle (dialog);
}


static void on_configure (GtkButton *button, GtkWidget *dialog)
{
    GtkCList *list = GTK_CLIST (lookup_widget (dialog, "avail_list"));
    PluginData *data = get_selected_plugin (list);

    g_return_if_fail (data != NULL);
    g_return_if_fail (data->active);

    gnome_cmd_plugin_configure (data->plugin);
}


static void on_about (GtkButton *button, GtkWidget *dialog)
{
    GtkCList *list = GTK_CLIST (lookup_widget (dialog, "avail_list"));
    PluginData *data = get_selected_plugin (list);
    GtkWidget *about = gnome_cmd_about_plugin_new (data->info);

    g_return_if_fail (data != NULL);

    gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (main_win));
    gtk_widget_ref (about);
    gtk_widget_show (about);
}


static void on_close (GtkButton *button, GtkWidget *dialog)
{
    gtk_widget_destroy (dialog);
}


void plugin_manager_show ()
{
    GtkWidget *dialog, *hbox, *bbox, *button;
    GtkWidget *avail_list;

    dialog = gnome_cmd_dialog_new (_("Available plugins"));
    gtk_widget_ref (dialog);

    hbox = create_vbox (dialog, FALSE, 6);
    avail_list = create_clist (dialog, "avail_list", 4, 20, GTK_SIGNAL_FUNC (on_plugin_selected), NULL);
    create_clist_column (avail_list, 0, 20, "");
    create_clist_column (avail_list, 1, 200, _("Name"));
    create_clist_column (avail_list, 2, 50, _("Version"));
    create_clist_column (avail_list, 3, 50, _("File"));

    bbox = create_hbuttonbox (dialog);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    button = create_button (GTK_WIDGET (dialog), _("_Enable"), GTK_SIGNAL_FUNC (on_toggle));
    gtk_object_set_data (GTK_OBJECT (dialog), "toggle_button", button);
    gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, FALSE, 0);

    button = create_button (GTK_WIDGET (dialog), _("_Configure"), GTK_SIGNAL_FUNC (on_configure));
    gtk_object_set_data (GTK_OBJECT (dialog), "conf_button", button);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, FALSE, 0);

    button = create_button (GTK_WIDGET (dialog), _("_About"), GTK_SIGNAL_FUNC (on_about));
    gtk_object_set_data (GTK_OBJECT (dialog), "about_button", button);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, FALSE, 0);

    gtk_box_pack_start (GTK_BOX (hbox), avail_list, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);

    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), hbox);

    avail_list = lookup_widget (avail_list, "avail_list");
    gtk_signal_connect (GTK_OBJECT (avail_list), "unselect-row", GTK_SIGNAL_FUNC (on_plugin_unselected), dialog);

    if (!exec_pixmap)
    {
        exec_pixmap = IMAGE_get_pixmap (PIXMAP_EXEC_WHEEL);
        exec_mask = IMAGE_get_mask (PIXMAP_EXEC_WHEEL);
    }

    if (!blank_pixmap)
    {
        blank_pixmap = IMAGE_get_pixmap (PIXMAP_FLIST_ARROW_BLANK);
        blank_mask = IMAGE_get_mask (PIXMAP_FLIST_ARROW_BLANK);
    }

    update_plugin_list (GTK_CLIST (avail_list), dialog);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_SIGNAL_FUNC(on_close), dialog);
    gnome_cmd_dialog_set_transient_for (GNOME_CMD_DIALOG (dialog), GTK_WINDOW (main_win));

    gtk_widget_set_size_request (GTK_WIDGET (dialog), 500, 300);
    gtk_window_set_resizable((GtkWindow *) GTK_WIDGET (dialog), TRUE);
    gtk_widget_show_all (GTK_WIDGET (dialog));
}
