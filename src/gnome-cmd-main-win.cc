/**
 * @file gnome-cmd-main-win.cc
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
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-path.h"
#include "gnome-cmd-con-list.h"
#include "imageloader.h"
#include "utils.h"
#include "widget-factory.h"

using namespace std;


struct GnomeCmdMainWinPrivate
{

    bool state_saved;
};


extern "C" void gnome_cmd_main_win_save_tabs(GnomeCmdMainWin *, gboolean, gboolean);


extern "C" GnomeCmdFileSelector *gnome_cmd_main_win_get_fs (GnomeCmdMainWin *main_win, FileSelectorID id);
extern "C" void gnome_cmd_main_win_update_style(GnomeCmdMainWin *main_win);


static GnomeCmdMainWinPrivate *gnome_cmd_main_win_priv (GnomeCmdMainWin *mw)
{
    return (GnomeCmdMainWinPrivate *) g_object_get_data (G_OBJECT (mw), "priv");
}


/*******************************
 * Gtk class implementation
 *******************************/

extern "C" void gnome_cmd_main_win_dispose (GnomeCmdMainWin *main_win)
{
    auto priv = gnome_cmd_main_win_priv (main_win);

    if (!priv->state_saved)
    {
        gnome_cmd_main_win_save_tabs(main_win, gnome_cmd_data.options.save_tabs_on_exit, gnome_cmd_data.options.save_dirs_on_exit);

        gnome_cmd_data.save(main_win);
        priv->state_saved = true;
    }

}


extern "C" void gnome_cmd_main_win_init (GnomeCmdMainWin *mw)
{
    auto priv = g_new0 (GnomeCmdMainWinPrivate, 1);
    g_object_set_data_full (G_OBJECT (mw), "priv", priv, g_free);

    priv->state_saved = false;
}


GnomeCmdFileSelector *GnomeCmdMainWin::fs(FileSelectorID id)
{
    return gnome_cmd_main_win_get_fs (this, id);
}


void GnomeCmdMainWin::update_view()
{
    gnome_cmd_main_win_update_style(this);
}


void GnomeCmdMainWin::focus_file_lists()
{
    fs(ACTIVE)->set_active(TRUE);
    fs(INACTIVE)->set_active(FALSE);
    gtk_widget_grab_focus (GTK_WIDGET (fs(ACTIVE)));
}


void GnomeCmdMainWin::set_fs_directory_to_opposite(FileSelectorID fsID)
{
    GnomeCmdFileSelector *fselector =  this->fs(fsID);
    GnomeCmdFileSelector *other = this->fs(!fsID);

    GnomeCmdDir *dir = other->get_directory();
    gboolean fs_is_active = fselector->is_active();

    if (!fs_is_active)
    {
        GnomeCmdFile *file = other->file_list()->get_selected_file();

        if (file && (file->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY))
            dir = GNOME_CMD_IS_DIR (file) ? GNOME_CMD_DIR (file) : gnome_cmd_dir_new_from_gfileinfo (file->get_file_info(), dir);
    }

    if (gnome_cmd_file_selector_is_current_tab_locked (fselector))
        fselector->new_tab(dir);
    else
        fselector->file_list()->set_connection(other->get_connection(), dir);

    other->set_active(!fs_is_active);
    fselector->set_active(fs_is_active);
    if (fs_is_active)
        gtk_widget_grab_focus (GTK_WIDGET (fselector));
    else
        gtk_widget_grab_focus (GTK_WIDGET (other));
}


// FFI

void gnome_cmd_main_win_focus_file_lists(GnomeCmdMainWin *main_win)
{
    return main_win->focus_file_lists();
}

void gnome_cmd_main_win_update_view(GnomeCmdMainWin *main_win)
{
    main_win->update_view();
}
