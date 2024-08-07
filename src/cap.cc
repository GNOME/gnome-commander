/**
 * @file cap.cc
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


struct PasteClosure
{
    GList *files;
    GnomeCmdDir *dir;
};


static void on_xfer_done (gboolean success, gpointer user_data)
{
    PasteClosure *paste = (PasteClosure *) user_data;

    if (paste->dir)
        gnome_cmd_dir_relist_files (GTK_WINDOW (main_win), paste->dir, FALSE);

    main_win->focus_file_lists ();

    if (paste->files != NULL)
        gnome_cmd_file_list_free (paste->files);

    g_free (paste);
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
    auto paste = g_new0 (PasteClosure, 1);
    paste->files = _files;
    paste->dir = to;

    gnome_cmd_move_gfiles_start (GTK_WINDOW (main_win),
                                 gnome_cmd_file_list_to_gfile_list (_files),
                                 gnome_cmd_dir_ref (to),
                                 NULL,
                                 G_FILE_COPY_NONE,
                                 GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                 on_xfer_done, paste);
    _files = NULL;
    _fl = NULL;
    main_win->set_cap_state(FALSE);
}


inline void copy_and_paste (GnomeCmdDir *to)
{
    auto paste = g_new0 (PasteClosure, 1);
    paste->files = _files;
    paste->dir = to;

    gnome_cmd_copy_gfiles_start (GTK_WINDOW (main_win),
                                 gnome_cmd_file_list_to_gfile_list (_files),
                                 gnome_cmd_dir_ref (to),
                                 NULL,
                                 G_FILE_COPY_NONE,
                                 GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
                                 on_xfer_done, paste);
    _files = NULL;
    _fl = NULL;
    main_win->set_cap_state(FALSE);
}


void cap_cut_files (GnomeCmdFileList *fl, GList *files)
{
    update_refs (fl, files);

    _type = GNOME_CMD_CUTTED;
    main_win->set_cap_state(TRUE);
}



void cap_copy_files (GnomeCmdFileList *fl, GList *files)
{
    update_refs (fl, files);

    _type = GNOME_CMD_COPIED;
    main_win->set_cap_state(TRUE);
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
