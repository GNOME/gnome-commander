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

#ifndef __GNOME_CMD_USER_ACTIONS_H__
#define __GNOME_CMD_USER_ACTIONS_H__

#include <string>
#include <map>

#include <gdk/gdkevents.h>

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"

G_BEGIN_DECLS

#define GNOME_CMD_USER_ACTION(f)   void f(GtkMenuItem *menuitem, gpointer user_data)
typedef void (*GnomeCmdUserActionFunc) (GtkMenuItem *menuitem, gpointer user_data);


class GnomeCmdUserActions
{
  private:

    struct UserAction
    {
        GnomeCmdUserActionFunc func;
        std::string user_data;

        UserAction(): func(NULL)      {}

        UserAction(GnomeCmdUserActionFunc _func, const char *_user_data);
    };

    std::map <GdkEventKey, UserAction> action;

  public:

    void init();
    void shutdown();

    void load(const gchar *section);
    void write(const gchar *section);

    gboolean register_action(guint state, guint keyval, const gchar *name, const char *user_data=NULL);
    gboolean register_action(guint keyval, const gchar *name, const char *user_data=NULL);
    void unregister(const gchar *name);
    void unregister(guint state, guint keyval);
    void unregister(guint keyval)                                           {  unregister(0, keyval);         }
    gboolean registered(const gchar *name);
    gboolean registered(guint state, guint keyval);
    gboolean registered(guint keyval)                                       {  return registered(0, keyval);  }

    gboolean handle_key_event(GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs, GnomeCmdFileList *fl, GdkEventKey *event);
};


inline GnomeCmdUserActions::UserAction::UserAction(GnomeCmdUserActionFunc _func, const char *_user_data): func(_func)
{
    if (_user_data)
        user_data = _user_data;
}

inline gboolean GnomeCmdUserActions::register_action(guint keyval, const gchar *name, const char *user_data)
{
    return register_action(0, keyval, name, user_data);
}


extern GnomeCmdUserActions gcmd_user_actions;


/************** File Menu **************/
GNOME_CMD_USER_ACTION(file_copy);
GNOME_CMD_USER_ACTION(file_move);
GNOME_CMD_USER_ACTION(file_delete);
GNOME_CMD_USER_ACTION(file_view);
GNOME_CMD_USER_ACTION(file_internal_view);
GNOME_CMD_USER_ACTION(file_external_view);
GNOME_CMD_USER_ACTION(file_edit);
GNOME_CMD_USER_ACTION(file_chmod);
GNOME_CMD_USER_ACTION(file_chown);
GNOME_CMD_USER_ACTION(file_mkdir);
GNOME_CMD_USER_ACTION(file_properties);
GNOME_CMD_USER_ACTION(file_diff);
GNOME_CMD_USER_ACTION(file_sync_dirs);
GNOME_CMD_USER_ACTION(file_rename);
GNOME_CMD_USER_ACTION(file_create_symlink);
GNOME_CMD_USER_ACTION(file_advrename);
GNOME_CMD_USER_ACTION(file_run);
GNOME_CMD_USER_ACTION(file_umount);
GNOME_CMD_USER_ACTION(file_exit);

/************** Mark Menu **************/
GNOME_CMD_USER_ACTION(mark_toggle);
GNOME_CMD_USER_ACTION(mark_toggle_and_step);
GNOME_CMD_USER_ACTION(mark_select_all);
GNOME_CMD_USER_ACTION(mark_unselect_all);
GNOME_CMD_USER_ACTION(mark_select_with_pattern);
GNOME_CMD_USER_ACTION(mark_unselect_with_pattern);
GNOME_CMD_USER_ACTION(mark_invert_selection);
GNOME_CMD_USER_ACTION(mark_select_all_with_same_extension);
GNOME_CMD_USER_ACTION(mark_unselect_all_with_same_extension);
GNOME_CMD_USER_ACTION(mark_restore_selection);
GNOME_CMD_USER_ACTION(mark_compare_directories);

/************** Edit Menu **************/
GNOME_CMD_USER_ACTION(edit_cap_cut);
GNOME_CMD_USER_ACTION(edit_cap_copy);
GNOME_CMD_USER_ACTION(edit_cap_paste);
GNOME_CMD_USER_ACTION(edit_search);
GNOME_CMD_USER_ACTION(edit_quick_search);
GNOME_CMD_USER_ACTION(edit_filter);
GNOME_CMD_USER_ACTION(edit_copy_fnames);

/************** View Menu **************/
GNOME_CMD_USER_ACTION(view_conbuttons);
GNOME_CMD_USER_ACTION(view_toolbar);
GNOME_CMD_USER_ACTION(view_buttonbar);
GNOME_CMD_USER_ACTION(view_cmdline);
GNOME_CMD_USER_ACTION(view_hidden_files);
GNOME_CMD_USER_ACTION(view_backup_files);
GNOME_CMD_USER_ACTION(view_up);
GNOME_CMD_USER_ACTION(view_first);
GNOME_CMD_USER_ACTION(view_back);
GNOME_CMD_USER_ACTION(view_forward);
GNOME_CMD_USER_ACTION(view_last);
GNOME_CMD_USER_ACTION(view_refresh);
GNOME_CMD_USER_ACTION(view_equal_panes);

/************** Bookmarks Menu **************/
GNOME_CMD_USER_ACTION(bookmarks_add_current);
GNOME_CMD_USER_ACTION(bookmarks_edit);
GNOME_CMD_USER_ACTION(bookmarks_goto);

/************** Options Menu **************/
GNOME_CMD_USER_ACTION(options_edit);
GNOME_CMD_USER_ACTION(options_edit_mime_types);

/************** Connections Menu **************/
GNOME_CMD_USER_ACTION(connections_ftp_connect);
GNOME_CMD_USER_ACTION(connections_ftp_quick_connect);
GNOME_CMD_USER_ACTION(connections_change);
GNOME_CMD_USER_ACTION(connections_close);
GNOME_CMD_USER_ACTION(connections_close_current);

/************** Plugins Menu ***********/
GNOME_CMD_USER_ACTION(plugins_configure);
GNOME_CMD_USER_ACTION(plugins_execute_python);

/************** Help Menu **************/
GNOME_CMD_USER_ACTION(help_help);
GNOME_CMD_USER_ACTION(help_keyboard);
GNOME_CMD_USER_ACTION(help_web);
GNOME_CMD_USER_ACTION(help_problem);
GNOME_CMD_USER_ACTION(help_about);

G_END_DECLS

#endif // __GNOME_CMD_USER_ACTIONS_H__
