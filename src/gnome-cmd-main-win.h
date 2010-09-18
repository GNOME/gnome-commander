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
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-xml-config.h"
#include "plugin_manager.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"

#define GNOME_CMD_TYPE_MAIN_WIN          (gnome_cmd_main_win_get_type ())
#define GNOME_CMD_MAIN_WIN(obj)          GTK_CHECK_CAST(obj, GNOME_CMD_TYPE_MAIN_WIN, GnomeCmdMainWin)
#define GNOME_CMD_MAIN_WIN_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, GNOME_CMD_TYPE_MAIN_WIN, GnomeCmdMainWinClass)
#define GNOME_CMD_IS_MAIN_WIN(obj)       GTK_CHECK_TYPE(obj, GNOME_CMD_TYPE_MAIN_WIN)


GtkType gnome_cmd_main_win_get_type ();


struct GnomeCmdMainWin
{
    GnomeApp parent;

  public:       //  FIXME:  change to private

    void create_buttonbar();
    void update_drop_con_button(GnomeCmdFileList *fl);

  public:

    struct Private;

    Private *priv;

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_MAIN_WIN, NULL);  }
    void operator delete (void *p)      {  g_object_unref (p);  }

    operator GObject * () const         {  return G_OBJECT (this);         }
    operator GtkObject * () const       {  return GTK_OBJECT (this);       }
    operator GtkWidget * () const       {  return GTK_WIDGET (this);       }
    operator GtkWindow * () const       {  return GTK_WINDOW (this);       }

    GnomeCmdAdvrenameDialog *advrename_dlg;

    FileSelectorID fs() const;
    FileSelectorID fs(GnomeCmdFileSelector *fs) const;
    GnomeCmdFileSelector *fs(FileSelectorID id) const;

    gboolean key_pressed (GdkEventKey *event);

    void switch_fs(GnomeCmdFileSelector *fs);

    void set_fs_directory_to_opposite(FileSelectorID fsID);

    void set_equal_panes();
    GnomeCmdState *get_state() const;
    void set_cap_state(gboolean state);

    GtkWidget *get_vbox() const;
    GtkWidget *get_paned() const;
    GnomeCmdCmdline *get_cmdline() const;

    void focus_file_lists();
    void refocus();

    void update_style();
    void update_bookmarks();
    void update_toolbar_visibility();
    void update_treeview_visibility(gboolean visible);
    void update_cmdline_visibility();
    void update_buttonbar_visibility();
    void update_list_orientation();

    void add_plugin_menu(PluginData *data);

    friend XML::xstream &operator << (XML::xstream &xml, GnomeCmdMainWin &mw);
};


extern GnomeCmdMainWin *main_win;

#endif // __GNOME_CMD_MAIN_WIN_H__
