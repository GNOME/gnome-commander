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

#include "gnome-cmd-includes.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-user-actions.h"
#include "imageloader.h"


using namespace std;


GnomeCmdMainWin *main_win = nullptr;
gchar *debug_flags = nullptr;

GnomeCmdIconCache *icon_cache = nullptr;


extern "C" void gnome_cmd_application_startup(GApplication *application, gchar *debug_option)
{
    debug_flags = debug_option;

    /* Load Settings */
    icon_cache = gnome_cmd_icon_cache_new();
    settings = gcmd_user_action_settings_new();
    gnome_cmd_data.gsettings_init();
    gnome_cmd_data.load();
}


extern "C" void gnome_cmd_application_activate(GApplication *application, const gchar *start_dir_left, const gchar *start_dir_right)
{
    if (start_dir_left)
        gnome_cmd_data.tabs[LEFT].push_back(make_pair(string(start_dir_left), make_tuple(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING,FALSE)));

    if (start_dir_right)
        gnome_cmd_data.tabs[RIGHT].push_back(make_pair(string(start_dir_right), make_tuple(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING,FALSE)));

    main_win = new GnomeCmdMainWin;
    gtk_window_set_application (GTK_WINDOW (main_win), GTK_APPLICATION (application));

    gnome_cmd_data.connect_signals(main_win);

    gtk_widget_show (*main_win);
    main_win->restore_size_and_pos ();
}


extern "C" void gnome_cmd_application_shutdown()
{
    gnome_cmd_icon_cache_free(icon_cache);
    icon_cache = nullptr;

    remove_temp_download_dir ();
}
