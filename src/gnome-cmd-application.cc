/**
 * @file gnome-cmd-application.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
 * @copyright (C) 2024 Andrey Kutejko\n
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-application.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-con.h"
#include "utils.h"
#include "imageloader.h"


using namespace std;


GnomeCmdMainWin *main_win = nullptr;
gchar *start_dir_left = nullptr;
gchar *start_dir_right = nullptr;
gchar *config_dir = nullptr;
gchar *debug_flags = nullptr;

GnomeCmdIconCache *icon_cache = nullptr;

static GOptionEntry options [] =
{
    {"debug", 'd', 0, G_OPTION_ARG_STRING, &debug_flags, N_("Specify debug flags to use"), nullptr},
    {"start-left-dir", 'l', 0, G_OPTION_ARG_STRING, &start_dir_left, N_("Specify the start directory for the left pane"), nullptr},
    {"start-right-dir", 'r', 0, G_OPTION_ARG_STRING, &start_dir_right, N_("Specify the start directory for the right pane"), nullptr},
    {"config-dir", 0, 0, G_OPTION_ARG_STRING, &config_dir, N_("Specify the directory for configuration files"), nullptr},
    {nullptr}
};


struct _GnomeCmdApplication
{
    GtkApplication parent;
};


G_DEFINE_TYPE (GnomeCmdApplication, gnome_cmd_application, GTK_TYPE_APPLICATION)


static void gnome_cmd_application_startup(GApplication *application)
{
    G_APPLICATION_CLASS (gnome_cmd_application_parent_class)->startup (application);

    if (debug_flags && strchr(debug_flags,'a'))
        debug_flags = g_strdup("giklmnpstuvwyzx");

    // disable beeping for the application
    g_object_set (gtk_settings_get_default (), "gtk-error-bell", FALSE, NULL);

    gtk_icon_theme_add_search_path (gtk_icon_theme_get_for_display (gdk_display_get_default ()), ICONS_DIR);

    gchar *conf_dir = get_package_config_dir();
    if (!is_dir_existing(conf_dir))
    {
        create_dir (conf_dir);
    }
    g_free (conf_dir);

    /* Load Settings */
    icon_cache = gnome_cmd_icon_cache_new();
    settings = gcmd_user_action_settings_new();
    gnome_cmd_data.gsettings_init();
    gnome_cmd_data.load();
}


static void gnome_cmd_application_activate(GApplication *application)
{
    GList *windows = gtk_application_get_windows (GTK_APPLICATION (application));

    if (windows != nullptr)
    {
        gtk_window_present (GTK_WINDOW (windows->data));
        return;
    }

    if (start_dir_left)
    {
        if (!strcmp(start_dir_left, "."))
        {
            g_free(start_dir_left);
            start_dir_left = g_get_current_dir ();
        }
        gnome_cmd_data.tabs[LEFT].push_back(make_pair(string(start_dir_left), make_tuple(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING,FALSE)));
    }

    if (start_dir_right)
    {
        if (!strcmp(start_dir_right, "."))
        {
            g_free(start_dir_right);
            start_dir_right = g_get_current_dir ();
        }
        gnome_cmd_data.tabs[RIGHT].push_back(make_pair(string(start_dir_right), make_tuple(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING,FALSE)));
    }

    main_win = new GnomeCmdMainWin;
    gtk_window_set_application (GTK_WINDOW (main_win), GTK_APPLICATION (application));

    gnome_cmd_data.connect_signals(main_win);

    gtk_widget_show (*main_win);
    main_win->restore_size_and_pos ();
}


static void gnome_cmd_application_shutdown(GApplication *application)
{
    gnome_cmd_icon_cache_free(icon_cache);
    icon_cache = nullptr;

    remove_temp_download_dir ();

    g_clear_pointer (&debug_flags, g_free);
    g_clear_pointer (&start_dir_left, g_free);
    g_clear_pointer (&start_dir_right, g_free);
    g_clear_pointer (&config_dir, g_free);

    G_APPLICATION_CLASS (gnome_cmd_application_parent_class)->shutdown (application);
}


static void gnome_cmd_application_class_init(GnomeCmdApplicationClass *klass)
{
    G_APPLICATION_CLASS(klass)->startup = gnome_cmd_application_startup;
    G_APPLICATION_CLASS(klass)->activate = gnome_cmd_application_activate;
    G_APPLICATION_CLASS(klass)->shutdown = gnome_cmd_application_shutdown;
}


static void gnome_cmd_application_init(GnomeCmdApplication *app)
{
}


static gboolean allow_multiple_instances ()
{
    GSettingsSchemaSource *global_schema_source = GnomeCmdData::GetGlobalSchemaSource();
    GSettingsSchema *global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_GENERAL, FALSE);
    GSettings *general_settings = g_settings_new_full (global_schema, nullptr, nullptr);

    gboolean allow_multiple_instances = g_settings_get_boolean (general_settings, GCMD_SETTINGS_MULTIPLE_INSTANCES);
    g_object_unref (general_settings);

    return allow_multiple_instances;
}


GnomeCmdApplication *gnome_cmd_application_new()
{
    GApplicationFlags flags = (GApplicationFlags) 0;

    if (allow_multiple_instances ())
        flags = G_APPLICATION_NON_UNIQUE;

    auto app = static_cast<GnomeCmdApplication *> (g_object_new (GNOME_CMD_TYPE_APPLICATION,
        "application-id", "org.gnome.gnome-commander",
        "flags", flags,
        NULL));

    g_application_add_main_option_entries (G_APPLICATION (app), options);

    return app;
}
