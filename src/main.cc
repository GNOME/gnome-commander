/**
 * @file main.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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
#include <glib/gi18n.h>
#include <locale.h>
// ToDo: When migrating to GTK>2, keep note about https://docs.gtk.org/gtk3/migrating-libunique.html
#ifdef HAVE_UNIQUE
#include <unique/unique.h>
#endif

#include "gnome-cmd-includes.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-owner.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-con.h"
#include "utils.h"
#include "ls_colors.h"
#include "imageloader.h"
#include "plugin_manager.h"
#include "tags/gnome-cmd-tags.h"

using namespace std;

GnomeCmdMainWin *main_win;
GtkWidget *main_win_widget;
gchar *start_dir_left;
gchar *start_dir_right;
gchar *config_dir;
gchar *debug_flags;

extern gint created_files_cnt;
extern gint deleted_files_cnt;
extern int created_dirs_cnt;
extern int deleted_dirs_cnt;


static GOptionEntry options [] =
{
    {"debug", 'd', 0, G_OPTION_ARG_STRING, &debug_flags, N_("Specify debug flags to use"), nullptr},
    {"start-left-dir", 'l', 0, G_OPTION_ARG_STRING, &start_dir_left, N_("Specify the start directory for the left pane"), nullptr},
    {"start-right-dir", 'r', 0, G_OPTION_ARG_STRING, &start_dir_right, N_("Specify the start directory for the right pane"), nullptr},
    {"config-dir", 0, 0, G_OPTION_ARG_STRING, &config_dir, N_("Specify the directory for configuration files"), nullptr},
    {nullptr}
};


#ifdef HAVE_UNIQUE
static UniqueResponse on_message_received (UniqueApp *app, UniqueCommand cmd, UniqueMessageData *msg, guint t, gpointer  user_data)
{
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
    switch (cmd)
    {
        case UNIQUE_ACTIVATE:
            gtk_window_set_screen (*main_win, unique_message_data_get_screen (msg));
            gtk_window_present_with_time (*main_win, t);
            gdk_beep ();
            break;

        default:
            break;
    }
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    return UNIQUE_RESPONSE_OK;
}
#endif


int main (int argc, char *argv[])
{
    GError *error = nullptr;

#ifdef HAVE_UNIQUE
    UniqueApp *app;
#endif

    main_win = nullptr;

    setlocale (LC_ALL, "");
    bindtextdomain (PACKAGE, DATADIR "/locale");
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);

    if (!gtk_init_with_args (&argc, &argv,
                             _("File Manager"),
                             options,
                             PACKAGE,
                             &error))
    {
        g_printerr ("%s\n", error->message);
        return 1;
    }

    if (debug_flags && strchr(debug_flags,'a'))
        debug_flags = g_strdup("cdfgiklmnpstuvwyzx");

    // disable beeping for the application
    gtk_rc_parse_string("gtk-error-bell=0");

    gchar *conf_dir = get_package_config_dir();
    if (!is_dir_existing(conf_dir))
    {
        create_dir (conf_dir);
    }
    g_free (conf_dir);

    /* Load Settings */
    IMAGE_init ();
    gcmd_user_actions.init();
    gnome_cmd_data.gsettings_init();
    gnome_cmd_data.load();

#ifdef HAVE_UNIQUE
    app = unique_app_new ("org.gnome.GnomeCommander", nullptr);
#endif

#ifdef HAVE_UNIQUE
    if (!gnome_cmd_data.options.allow_multiple_instances && unique_app_is_running (app))
        unique_app_send_message (app, UNIQUE_ACTIVATE, nullptr);
    else
    {
#endif
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

        gcmd_user_actions.set_defaults();
        ls_colors_init ();

        // This had to be commented out as it is part of the deprecated libgnomeui.
        // To finally migrate away from it when switching over to GIO, see https://wiki.gnome.org/Attic/LibgnomeMustDie
        //gnome_authentication_manager_init ();

        gnome_cmd_style_create (gnome_cmd_data.options);

        main_win = new GnomeCmdMainWin;
        main_win_widget = *main_win;
#ifdef HAVE_UNIQUE
        unique_app_watch_window (app, *main_win);
        g_signal_connect (app, "message-received", G_CALLBACK (on_message_received), nullptr);
#endif

        gtk_widget_show (*main_win);
        gcmd_owner.load_async();

        gcmd_tags_init();
        plugin_manager_init ();

        gtk_main ();

        plugin_manager_shutdown ();
        gcmd_tags_shutdown ();
        gcmd_user_actions.shutdown();
        gnome_cmd_data.save();
        IMAGE_free ();

        remove_temp_download_dir ();
#ifdef HAVE_UNIQUE
    }
#endif

#ifdef HAVE_UNIQUE
    g_object_unref (app);
#endif
    g_free (debug_flags);
    g_free (start_dir_left);
    g_free (start_dir_right);
    g_free (config_dir);

    DEBUG ('c', "dirs total: %d remaining: %d\n", created_dirs_cnt, created_dirs_cnt - deleted_dirs_cnt);
    DEBUG ('c', "files total: %d remaining: %d\n", created_files_cnt, created_files_cnt - deleted_files_cnt);

    return 0;
}
