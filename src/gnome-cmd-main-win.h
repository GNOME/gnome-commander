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

#ifndef __GNOME_CMD_MAIN_WIN_H__
#define __GNOME_CMD_MAIN_WIN_H__

#define GNOME_CMD_MAIN_WIN(obj) \
    GTK_CHECK_CAST(obj, gnome_cmd_main_win_get_type (), GnomeCmdMainWin)
#define GNOME_CMD_MAIN_WIN_CLASS(klass) \
    GTK_CHECK_CLASS_CAST(klass, gnome_cmd_main_win_get_type (), GnomeCmdMainWinClass)
#define GNOME_CMD_IS_MAIN_WIN(obj) \
    GTK_CHECK_TYPE(obj, gnome_cmd_main_win_get_type ())

struct GnomeCmdMainWinPrivate;

#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-cmdline.h"
#include "plugin_manager.h"

struct GnomeCmdMainWin
{
    GnomeApp parent;
    GnomeCmdAdvrenameDialog *advrename_dlg;
    GnomeCmdMainWinPrivate *priv;
};

struct GnomeCmdMainWinClass
{
    GnomeAppClass parent_class;

    void (* switch_fs) (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs);
};


extern GnomeCmdMainWin *main_win;


GtkType gnome_cmd_main_win_get_type ();

GtkWidget *gnome_cmd_main_win_new ();

GnomeCmdFileSelector *gnome_cmd_main_win_get_fs (GnomeCmdMainWin *mw, FileSelectorID fs);
void gnome_cmd_main_win_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs);

GnomeCmdCmdline *gnome_cmd_main_win_get_cmdline (GnomeCmdMainWin *mw);

void gnome_cmd_main_win_update_style (GnomeCmdMainWin *mw);

void gnome_cmd_main_win_new_cwd (GnomeCmdMainWin *mw, const gchar *cwd);

void gnome_cmd_main_win_focus_cmdline (GnomeCmdMainWin *mw);
void gnome_cmd_main_win_focus_file_lists (GnomeCmdMainWin *mw);
void gnome_cmd_main_win_refocus (GnomeCmdMainWin *mw);

gboolean gnome_cmd_main_win_keypressed (GnomeCmdMainWin *mw, GdkEventKey *event);

void gnome_cmd_main_win_update_bookmarks (GnomeCmdMainWin *mw);
void gnome_cmd_main_win_update_toolbar_visibility (GnomeCmdMainWin *mw);
void gnome_cmd_main_win_update_cmdline_visibility (GnomeCmdMainWin *mw);
void gnome_cmd_main_win_update_buttonbar_visibility (GnomeCmdMainWin *mw);
void gnome_cmd_main_win_update_list_orientation (GnomeCmdMainWin *mw);

void gnome_cmd_main_win_add_plugin_menu (GnomeCmdMainWin *mw, PluginData *data);

GnomeCmdState *gnome_cmd_main_win_get_state (GnomeCmdMainWin *mw);

void gnome_cmd_main_win_set_cap_state (GnomeCmdMainWin *mw, gboolean state);

void gnome_cmd_main_win_set_equal_panes (GnomeCmdMainWin *mw);

#endif // __GNOME_CMD_MAIN_WIN_H__
