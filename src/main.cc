/** 
 * @file main.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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

extern "C"
{
    void gnome_authentication_manager_init ();
}

#include <config.h>
#include <glib/gi18n.h>
#include <locale.h>
#ifdef HAVE_UNIQUE
#include <unique/unique.h>
#endif
#include <libgnomeui/gnome-ui-init.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-mime-config.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-user-actions.h"
#include "owner.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-con.h"
#include "utils.h"
#include "ls_colors.h"
#include "imageloader.h"
#include "plugin_manager.h"
#include "gnome-cmd-python-plugin.h"
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


static const GOptionEntry options [] =
{
    {"debug", 'd', 0, G_OPTION_ARG_STRING, &debug_flags, N_("Specify debug flags to use"), NULL},
    {"start-left-dir", 'l', 0, G_OPTION_ARG_STRING, &start_dir_left, N_("Specify the start directory for the left pane"), NULL},
    {"start-right-dir", 'r', 0, G_OPTION_ARG_STRING, &start_dir_right, N_("Specify the start directory for the right pane"), NULL},
    {"config-dir", 0, 0, G_OPTION_ARG_STRING, &config_dir, N_("Specify the directory for configuration files"), NULL},
    {NULL}
};


#ifdef HAVE_UNIQUE
static UniqueResponse on_message_received (UniqueApp *app, UniqueCommand cmd, UniqueMessageData *msg, guint t, gpointer  user_data)
{
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

    return UNIQUE_RESPONSE_OK;
}
#endif


int main (int argc, char *argv[])
{
    GnomeProgram *program;
    GOptionContext *option_context;
#ifdef HAVE_UNIQUE
    UniqueApp *app;
#endif

    main_win = NULL;

#if GLIB_CHECK_VERSION (2, 32, 0)
    g_thread_init (NULL);
#endif

    if (!g_thread_supported ())
    {
        g_printerr ("GNOME Commander needs thread support in glib to work, bailing out...\n");
        return 0;
    }

    setlocale (LC_ALL, "");
    bindtextdomain (PACKAGE, DATADIR "/locale");
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);

    gnome_cmd_mime_config();

    option_context = g_option_context_new (PACKAGE);
    g_option_context_add_main_entries (option_context, options, NULL);
    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  GNOME_PARAM_GOPTION_CONTEXT, option_context,
                                  GNOME_PARAM_HUMAN_READABLE_NAME, _("File Manager"),
                                  GNOME_PARAM_APP_DATADIR, DATADIR,
                                  GNOME_PARAM_NONE);

    if (debug_flags && strchr(debug_flags,'a'))
        debug_flags = g_strdup("cdfgiklmnpstuvwyzx");

    gdk_rgb_init ();
    gnome_vfs_init ();

    gchar *conf_dir = g_build_filename (g_get_home_dir (), "." PACKAGE, NULL);
    create_dir_if_needed (conf_dir);
    g_free (conf_dir);

    /* Load Settings */
    IMAGE_init ();
    gcmd_user_actions.init();
    gnome_cmd_data.migrate_all_data_to_gsettings();
    gnome_cmd_data.load();

#ifdef HAVE_UNIQUE
    app = unique_app_new ("org.gnome.GnomeCommander", NULL);
#endif

#ifdef HAVE_UNIQUE
    if (!gnome_cmd_data.options.allow_multiple_instances && unique_app_is_running (app))
        unique_app_send_message (app, UNIQUE_ACTIVATE, NULL);
    else
    {
#endif
        if (start_dir_left)
            gnome_cmd_data.tabs[LEFT].push_back(make_pair(string(start_dir_left),make_triple(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING,FALSE)));

        if (start_dir_right)
            gnome_cmd_data.tabs[RIGHT].push_back(make_pair(string(start_dir_right),make_triple(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING,FALSE)));

        gcmd_user_actions.set_defaults();
        ls_colors_init ();
        gnome_cmd_data.load_more();

        gnome_authentication_manager_init ();

        gnome_cmd_style_create (gnome_cmd_data.options);

        main_win = new GnomeCmdMainWin;
        main_win_widget = *main_win;
#ifdef HAVE_UNIQUE
        unique_app_watch_window (app, *main_win);
        g_signal_connect (app, "message-received", G_CALLBACK (on_message_received), NULL);
#endif

        gtk_widget_show (*main_win);
        gcmd_owner.load_async();

        gcmd_tags_init();
        plugin_manager_init ();
#ifdef HAVE_PYTHON
        python_plugin_manager_init ();
#endif

        gtk_main ();

#ifdef HAVE_PYTHON
        python_plugin_manager_shutdown ();
#endif
        plugin_manager_shutdown ();
        gcmd_tags_shutdown ();
        gcmd_user_actions.shutdown();
        gnome_cmd_data.save();
        IMAGE_free ();

        remove_temp_download_dir ();
#ifdef HAVE_UNIQUE
    }
#endif

    gnome_vfs_shutdown ();

#ifdef HAVE_UNIQUE
    g_object_unref (app);
#endif
    g_object_unref (program);
    g_free (debug_flags);

    DEBUG ('c', "dirs total: %d remaining: %d\n", created_dirs_cnt, created_dirs_cnt - deleted_dirs_cnt);
    DEBUG ('c', "files total: %d remaining: %d\n", created_files_cnt, created_files_cnt - deleted_files_cnt);

    return 0;
}
