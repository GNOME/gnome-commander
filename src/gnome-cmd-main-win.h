/**
 * @file gnome-cmd-main-win.h
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

#pragma once

#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-cmdline.h"


struct GnomeCmdMainWin
{
    GtkApplicationWindow parent;

  public:

    operator GtkWindow * () const       {  return GTK_WINDOW (this);       }

    GnomeCmdFileSelector *fs(FileSelectorID id);

    void focus_file_lists();

    void update_view();

};

extern GnomeCmdMainWin *main_win;

extern "C" GnomeCmdFileSelector *gnome_cmd_main_win_get_fs(GnomeCmdMainWin *main_win, FileSelectorID id);

extern "C" void gnome_cmd_main_win_focus_file_lists(GnomeCmdMainWin *main_win);

extern "C" void gnome_cmd_main_win_update_view(GnomeCmdMainWin *main_win);

extern "C" GnomeCmdCmdline *gnome_cmd_main_win_get_cmdline(GnomeCmdMainWin *main_win);

