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

#include "gnome-cmd-includes.h"
#include "cap.h"
#include "gnome-cmd-xfer.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-main-win.h"

using namespace std;


#define GNOME_CMD_CUTTED 1
#define GNOME_CMD_COPIED 2

static int _type = 0;
static GList *_files = NULL;
static GnomeCmdFileList *_fl = NULL;


static void on_xfer_done (GList *files, gpointer data)
{
    if (files != NULL)
        gnome_cmd_file_list_free (files);
}


inline void update_refs (GnomeCmdFileList *fl, GList *files)
{
    if (_files != NULL)
        gnome_cmd_file_list_free (_files);

    _files = gnome_cmd_file_list_copy (files);
    _fl = fl;
}


inline void cut_and_paste (GnomeCmdDir *to)
{
    gnome_cmd_dir_ref (to);
    gnome_cmd_xfer_start (_files,
                          to,
                          _fl,
                          NULL,
                          GNOME_VFS_XFER_REMOVESOURCE,
                          GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
                          GTK_SIGNAL_FUNC (on_xfer_done), _files);
    _files = NULL;
    _fl = NULL;
    gnome_cmd_main_win_set_cap_state (main_win, FALSE);
}


inline void copy_and_paste (GnomeCmdDir *to)
{
    gnome_cmd_dir_ref (to);
    gnome_cmd_xfer_start (_files,
                          to,
                          _fl,
                          NULL,
                          GNOME_VFS_XFER_RECURSIVE,
                          GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
                          GTK_SIGNAL_FUNC (on_xfer_done), _files);
    _files = NULL;
    _fl = NULL;
    gnome_cmd_main_win_set_cap_state (main_win, FALSE);
}


void cap_cut_files (GnomeCmdFileList *fl, GList *files)
{
    update_refs (fl, files);

    _type = GNOME_CMD_CUTTED;
    gnome_cmd_main_win_set_cap_state (main_win, TRUE);
}



void cap_copy_files (GnomeCmdFileList *fl, GList *files)
{
    update_refs (fl, files);

    _type = GNOME_CMD_COPIED;
    gnome_cmd_main_win_set_cap_state (main_win, TRUE);
}


void cap_paste_files (GnomeCmdDir *dir)
{
    switch (_type)
    {
        case GNOME_CMD_CUTTED:
            cut_and_paste (dir);
            break;

        case GNOME_CMD_COPIED:
            copy_and_paste (dir);
            break;

        default:
            return;
    }
}
