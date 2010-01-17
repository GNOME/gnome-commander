/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2010 Piotr Eljasiak

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

#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-cmdline.h"
#include "plugin_manager.h"

#define GNOME_CMD_MAIN_WIN(obj)          GTK_CHECK_CAST(obj, gnome_cmd_main_win_get_type (), GnomeCmdMainWin)
#define GNOME_CMD_MAIN_WIN_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, gnome_cmd_main_win_get_type (), GnomeCmdMainWinClass)
#define GNOME_CMD_IS_MAIN_WIN(obj)       GTK_CHECK_TYPE(obj, gnome_cmd_main_win_get_type ())


GtkType gnome_cmd_main_win_get_type ();


struct GnomeCmdMainWin
{
    GnomeApp parent;

  public:       //  FIXME:  change to private

    void create_buttonbar();

  public:

    struct Private;

    Private *priv;

    operator GObject * ()               {  return G_OBJECT (this);         }
    operator GtkObject * ()             {  return GTK_OBJECT (this);       }
    operator GtkWidget * ()             {  return GTK_WIDGET (this);       }
    operator GtkWindow * ()             {  return GTK_WINDOW (this);       }

    GnomeCmdAdvrenameDialog *advrename_dlg;

    GnomeCmdFileSelector *fs(FileSelectorID id);

    gboolean key_pressed (GdkEventKey *event);

    void switch_fs(GnomeCmdFileSelector *fs);

    void set_equal_panes();
    GnomeCmdState *get_state();
    void set_cap_state(gboolean state);

    GnomeCmdCmdline *get_cmdline();

    void focus_file_lists();
    void refocus();

    void update_style();
    void update_bookmarks();
    void update_toolbar_visibility();
    void update_cmdline_visibility();
    void update_buttonbar_visibility();
    void update_list_orientation();

    void add_plugin_menu(PluginData *data);
};


GtkWidget *gnome_cmd_main_win_new ();


extern GnomeCmdMainWin *main_win;

#endif // __GNOME_CMD_MAIN_WIN_H__
