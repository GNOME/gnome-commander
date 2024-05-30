/**
 * @file plugin_manager.cc
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

#include <dirent.h>
#include <gmodule.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "plugin_manager.h"
#include "utils.h"
#include "imageloader.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-about-plugin.h"
#include "dirlist.h"

using namespace std;


// The names of these functions shall never change
#define MODULE_INIT_FUNC "create_plugin"
#define MODULE_INFO_FUNC "get_plugin_info"


static GList *plugins = nullptr;

gchar* get_plugin_config_location();

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

    GSimpleActionGroup *actions = gnome_cmd_plugin_create_actions (data->plugin, data->action_group_name);
    gtk_widget_insert_action_group (*main_win, data->action_group_name, G_ACTION_GROUP (actions));

    GnomeCmdState *state = main_win->get_state();
    data->menu = gnome_cmd_plugin_create_main_menu (data->plugin, state);
}


static void inactivate_plugin (PluginData *data)
{
    if (!data->active)
        return;

    data->active = FALSE;
    gtk_widget_insert_action_group (*main_win, data->action_group_name, nullptr);
    g_clear_object (&data->menu);
}


static void scan_plugins_in_dir (const gchar *dpath)
{
    static int index = 0;
    auto gFileInfoList = sync_dir_list(dpath);

    if (g_list_length(gFileInfoList) == 0)
        return;

    for (auto gFileInfoListItem = gFileInfoList; gFileInfoListItem; gFileInfoListItem = gFileInfoListItem->next)
    {
        auto gFileInfo = (GFileInfo*) gFileInfoListItem->data;
        if (g_file_info_get_file_type(gFileInfo) != G_FILE_TYPE_REGULAR)
            continue;
        if (!g_str_has_suffix (g_file_info_get_name(gFileInfo), "." G_MODULE_SUFFIX))
            continue;

        // the direntry has the correct extension and is a regular file, let's accept it
        PluginData *data = g_new0 (PluginData, 1);
        data->fname = g_strdup (g_file_info_get_name(gFileInfo));
        data->fpath = g_build_filename (dpath, g_file_info_get_name(gFileInfo), nullptr);
        data->loaded = FALSE;
        data->active = FALSE;
        data->menu = nullptr;
        data->autoload = FALSE;
        data->action_group_name = g_strdup_printf ("plugin%d", index++);
        activate_plugin (data);
        if (!data->loaded)
        {
            g_free (data->fname);
            g_free (data->fpath);
            g_free (data->action_group_name);
            g_free (data);
        }
        else
            plugins = g_list_append (plugins, data);
    }
    g_list_free_full(gFileInfoList, g_object_unref);
}


void plugin_manager_init ()
{
    if (plugins)
    {
        g_warning ("plugin_manager already initiated");
        return;
    }

    // find user plugins
    gchar *user_dir = get_plugin_config_location();
    if (!is_dir_existing(user_dir))
    {
        create_dir (user_dir);
    }

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
            auto data = static_cast<PluginData*> (l2->data);
            if (strcmp (name, data->fname) == 0)
                data->autoload = TRUE;
        }
    }

    // deactivate plugins that shouldn't be autoloaded
    for (GList *l=plugins; l; l=l->next)
    {
        auto data = static_cast<PluginData*> (l->data);
        if (!data->autoload)
            inactivate_plugin (data);
    }

    main_win->plugins_updated();
}


void plugin_manager_shutdown ()
{
    GList *out = nullptr;

    for (GList *l=plugins; l; l=l->next)
    {
        auto data = static_cast<PluginData*> (l->data);
        if (data->active)
            out = g_list_append (out, data->fname);
    }

    gnome_cmd_data_set_auto_load_plugins (out);
}


GList *plugin_manager_get_all ()
{
    return plugins;
}


static PluginData *get_selected_plugin (GtkTreeView *view)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    PluginData *data;

    g_return_val_if_fail (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view), &model, &iter), nullptr);
    gtk_tree_model_get (model, &iter, 4, &data, -1);
    return data;
}


static void on_plugin_selection_changed (GtkTreeSelection *selection, GtkWidget *dialog)
{
    GtkWidget *toggle_button = lookup_widget (dialog, "toggle_button");
    GtkWidget *conf_button = lookup_widget (dialog, "conf_button");
    GtkWidget *about_button = lookup_widget (dialog, "about_button");
    GtkTreeModel *model;
    GtkTreeIter iter;
    PluginData *data;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, 4, &data, -1);
        gtk_widget_set_sensitive (about_button, TRUE);
        gtk_button_set_label (GTK_BUTTON (toggle_button), data->active ? _("Disable") : _("Enable"));
        gtk_widget_set_sensitive (toggle_button, TRUE);
        gtk_widget_set_sensitive (conf_button, data->active);
    }
    else
    {
        gtk_widget_set_sensitive (toggle_button, FALSE);
        gtk_widget_set_sensitive (about_button, FALSE);
        gtk_widget_set_sensitive (conf_button, FALSE);
    }
}


static void update_plugin_list (GtkTreeView *view, GtkWidget *dialog)
{
    GtkTreeModel *model = gtk_tree_view_get_model (view);
    GtkTreeIter iter;
    gboolean only_update = gtk_tree_model_get_iter_first (model, &iter);
    GtkListStore *store = GTK_LIST_STORE (model);

    GIcon *exec_icon = g_themed_icon_new ("system-run");

    for (GList *i=plugins; i; i=i->next)
    {
        auto data = static_cast<PluginData*> (i->data);

        if (only_update)
        {
            gtk_list_store_set (store, &iter,
                                0, data->active ? exec_icon : nullptr,
                                1, (gchar *) data->info->name,
                                4, data,
                                -1);
            gtk_tree_model_iter_next (model, &iter);
        }
        else
        {
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                0, data->active ? exec_icon : nullptr,
                                1, (gchar *) data->info->name,
                                2, (gchar *) data->info->version,
                                3, data->fname,
                                4, data,
                                -1);
        }
    }

    on_plugin_selection_changed (gtk_tree_view_get_selection (view), dialog);
}


inline void do_toggle (GtkWidget *dialog)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (dialog, "avail_view"));
    PluginData *data = get_selected_plugin (view);

    if (!data) return;

    if (data->active)
        inactivate_plugin (data);
    else
        activate_plugin (data);

    update_plugin_list (view, dialog);
    main_win->plugins_updated();
}


static void on_plugin_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, GtkWidget *dialog)
{
    do_toggle (dialog);
}


static void on_toggle (GtkButton *button, GtkWidget *dialog)
{
    do_toggle (dialog);
}


static void on_configure (GtkButton *button, GtkWidget *dialog)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (dialog, "avail_view"));
    PluginData *data = get_selected_plugin (view);

    g_return_if_fail (data != nullptr);
    g_return_if_fail (data->active);

    gnome_cmd_plugin_configure (data->plugin);
}


static void on_about (GtkButton *button, GtkWidget *dialog)
{
    GtkTreeView *view = GTK_TREE_VIEW (lookup_widget (dialog, "avail_view"));
    PluginData *data = get_selected_plugin (view);

    g_return_if_fail (data != nullptr);

    GtkWidget *about = gnome_cmd_about_plugin_new (data->info);

    gtk_window_set_transient_for (GTK_WINDOW (about), *main_win);
    gtk_window_present (GTK_WINDOW (about));
}


static void on_close (GtkButton *button, GtkWidget *dialog)
{
    gtk_window_destroy (GTK_WINDOW (dialog));
}


void plugin_manager_show ()
{
    GtkWidget *dialog, *hbox, *bbox, *button;
    GtkListStore *avail_store;
    GtkWidget *avail_view;

    dialog = gnome_cmd_dialog_new (_("Available plugins"));
    g_object_ref (dialog);

    hbox = create_vbox (dialog, FALSE, 6);
    avail_store = gtk_list_store_new (5, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    avail_view = create_treeview (dialog, "avail_view", GTK_TREE_MODEL (avail_store), 20, G_CALLBACK (on_plugin_selection_changed), nullptr);
    gtk_widget_set_hexpand (GTK_WIDGET (avail_view), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (avail_view), TRUE);
    create_treeview_column (avail_view, 0, 20, "");
    create_treeview_column (avail_view, 1, 200, _("Name"));
    create_treeview_column (avail_view, 2, 50, _("Version"));
    create_treeview_column (avail_view, 3, 50, _("File"));

    bbox = create_hbuttonbox (dialog);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    button = create_button (GTK_WIDGET (dialog), _("_Enable"), G_CALLBACK (on_toggle));
    g_object_set_data (G_OBJECT (dialog), "toggle_button", button);
    gtk_box_append (GTK_BOX (bbox), button);

    button = create_button (GTK_WIDGET (dialog), _("_Configure"), G_CALLBACK (on_configure));
    g_object_set_data (G_OBJECT (dialog), "conf_button", button);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_append (GTK_BOX (bbox), button);

    button = create_button (GTK_WIDGET (dialog), _("_About"), G_CALLBACK (on_about));
    g_object_set_data (G_OBJECT (dialog), "about_button", button);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_append (GTK_BOX (bbox), button);

    gtk_box_append (GTK_BOX (hbox), avail_view);
    gtk_box_append (GTK_BOX (hbox), bbox);

    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), hbox);

    avail_view = lookup_widget (avail_view, "avail_view");
    g_signal_connect (GTK_TREE_VIEW (avail_view), "row-activated", G_CALLBACK (on_plugin_activated), dialog);

    update_plugin_list (GTK_TREE_VIEW (avail_view), dialog);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_Close"), G_CALLBACK(on_close), dialog);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), *main_win);

    gtk_widget_set_size_request (GTK_WIDGET (dialog), 500, 300);
    gtk_window_set_resizable ((GtkWindow *) GTK_WIDGET (dialog), TRUE);
    gtk_widget_show_all (GTK_WIDGET (dialog));
}

gchar* get_plugin_config_location()
{
    gchar* returnString {nullptr};

    string userPluginConfigDir = get_package_config_dir();
    userPluginConfigDir += (char*) "/plugins";

    if (!is_dir_existing(userPluginConfigDir.c_str()))
    {
        create_dir (userPluginConfigDir.c_str());
    }

    returnString = g_strdup(userPluginConfigDir.c_str());

    return returnString;
}
