/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include <locale.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "owner.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-con.h"
#include "utils.h"
#include "ls_colors.h"
#include "imageloader.h"
#include "plugin_manager.h"

GnomeCmdMainWin *main_win;
GtkWidget *main_win_widget;
gchar *start_dir_left;
gchar *start_dir_right;
gchar *debug_flags;

#ifdef HAVE_LOCALE_H
struct lconv *locale_information;
#endif

extern gint created_files_cnt;
extern gint deleted_files_cnt;
extern int created_dirs_cnt;
extern int deleted_dirs_cnt;
extern GList *all_dirs;
extern GHashTable *all_dirs_map;
extern GList *all_files;


struct poptOption popt_options[] = {                                          
    {"debug", 'd', POPT_ARG_STRING, &debug_flags, 0, N_("Specify debug flags to use"), NULL},
    {"start-left-dir", 'l', POPT_ARG_STRING, &start_dir_left, 0, N_("Specify the start directory for the left pane"), NULL},
    {"start-right-dir", 'r', POPT_ARG_STRING, &start_dir_right, 0, N_("Specify the start directory for the right pane"), NULL},
    {NULL, 0, 0, NULL, 0, NULL, NULL}                                       
};                                                                          


static const GOptionEntry options [] =
{
    {"debug", 'd', 0, G_OPTION_ARG_STRING, &debug_flags, N_("Specify debug flags to use"), NULL},
    {"start-left-dir", 'l', 0, G_OPTION_ARG_STRING, &start_dir_left, N_("Specify the start directory for the left pane"), NULL},
    {"start-right-dir", 'r', 0, G_OPTION_ARG_STRING, &start_dir_right, N_("Specify the start directory for the right pane"), NULL},
    {NULL}
};
  

int
main (int argc, char *argv[])
{
    GnomeProgram *program;
    GOptionContext *option_context;
    gchar *conf_dir;

    main_win = NULL;

    g_thread_init (NULL);

    if (!g_thread_supported ()) {
        g_printerr ("GNOME Commander needs thread support in glib to work, bailing out...\n");
        return 0;
    }

#ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);
#endif

#ifdef HAVE_LOCALE_H
    if (setlocale(LC_ALL, "") == NULL)
        g_warning("Error while processing locales, call to setlocale failed.\n");
    locale_information = localeconv();
#endif

    option_context = g_option_context_new (PACKAGE);
    g_option_context_add_main_entries (option_context, options, NULL);
    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  // GNOME_PARAM_GOPTION_CONTEXT, option_context,
                                  GNOME_PARAM_POPT_TABLE, popt_options,
                                  GNOME_PARAM_HUMAN_READABLE_NAME, _("File Manager"),
                                  GNOME_PARAM_APP_DATADIR, DATA_DIR,
                                  GNOME_PARAM_NONE);

    ls_colors_init ();
    gdk_rgb_init ();
    gnome_vfs_init ();
    conf_dir = g_build_path ("/", g_get_home_dir(), ".gnome-commander", NULL);
    create_dir_if_needed (conf_dir);
    g_free (conf_dir);
    gnome_cmd_data_load ();
    IMAGE_init ();
    gnome_cmd_data_load_more ();

    gnome_cmd_style_create ();
    OWNER_init ();

    main_win_widget = gnome_cmd_main_win_new ();
    main_win = GNOME_CMD_MAIN_WIN (main_win_widget);

    gtk_widget_show (GTK_WIDGET (main_win));
    plugin_manager_init ();

    gtk_main ();

    plugin_manager_shutdown ();
    gnome_cmd_data_save ();
    gnome_vfs_shutdown ();
    OWNER_free ();
    IMAGE_free ();

    remove_temp_download_dir ();

    g_object_unref (program);

    DEBUG ('c', "dirs total: %d remaining: %d\n", created_dirs_cnt, created_dirs_cnt - deleted_dirs_cnt);
    DEBUG ('c', "files total: %d remaining: %d\n", created_files_cnt, created_files_cnt - deleted_files_cnt);

    return 0;
}
