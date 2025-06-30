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
}


extern "C" void gnome_cmd_application_activate(GApplication *application, GnomeCmdMainWin *mw)
{
    main_win = mw;
}


extern "C" void gnome_cmd_application_shutdown()
{
    gnome_cmd_icon_cache_free(icon_cache);
    icon_cache = nullptr;

    remove_temp_download_dir ();
}
