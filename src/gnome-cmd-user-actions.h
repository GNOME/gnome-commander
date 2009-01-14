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

#ifndef __GNOME_CMD_USER_ACTIONS_H__
#define __GNOME_CMD_USER_ACTIONS_H__

#include <string>
#include <map>

#include <gdk/gdkevents.h>

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"
#include "dict.h"

#define GNOME_CMD_USER_ACTION(f)   void f(GtkMenuItem *menuitem=NULL, gpointer user_data=NULL)

typedef void (*GnomeCmdUserActionFunc) (GtkMenuItem *menuitem, gpointer user_data);


inline gboolean ascii_isalnum (guint key_val)
{
    return key_val<=G_MAXUINT8 && g_ascii_isalnum (key_val);
}

inline gboolean ascii_isalpha (guint key_val)
{
    return key_val<=G_MAXUINT8 && g_ascii_isalpha (key_val);
}

inline gboolean ascii_islower (const GdkEventKey &event)
{
    return event.keyval<=G_MAXUINT8 && g_ascii_islower (event.keyval);
}

inline gboolean ascii_isupper (const GdkEventKey &event)
{
    return event.keyval<=G_MAXUINT8 && g_ascii_isupper (event.keyval);
}

inline std::string key2str(guint state, guint key_val)
{
    std::string key_name;

    if (state & GDK_SHIFT_MASK)    key_name += gdk_modifiers_names[GDK_SHIFT_MASK];
    if (state & GDK_CONTROL_MASK)  key_name += gdk_modifiers_names[GDK_CONTROL_MASK];
    if (state & GDK_MOD1_MASK)     key_name += gdk_modifiers_names[GDK_MOD1_MASK];
#if GTK_CHECK_VERSION (2, 10, 0)
    if (state & GDK_SUPER_MASK)    key_name += gdk_modifiers_names[GDK_SUPER_MASK];
    if (state & GDK_HYPER_MASK)    key_name += gdk_modifiers_names[GDK_HYPER_MASK];
    if (state & GDK_META_MASK)     key_name += gdk_modifiers_names[GDK_META_MASK];
#else
    if (state & GDK_MOD4_MASK)     key_name += gdk_modifiers_names[GDK_MOD4_MASK];
#endif

    if (ascii_isalnum (key_val))
        key_name += g_ascii_tolower (key_val);
    else
        key_name += gdk_key_names[key_val];

    return key_name;
}

inline std::string key2str(const GdkEventKey &event)
{
    return key2str(event.state, event.keyval);
}

inline GdkEventKey str2key(gchar *s, guint &state, guint &key_val)
{
    g_strdown (s);

    gchar *key = strrchr(s, '>');       // find last '>'
    key = key ? key+1 : s;

    key_val = gdk_key_names[key];
    state = 0;

     if (key_val==GDK_VoidSymbol)
        if (strlen(key)==1 && ascii_isalnum (*key))
            key_val = *key;

    for (const gchar *beg=s; (beg=strchr(beg, '<')); ++beg)
    {
        if (const gchar *end = strchr(beg, '>'))
            if (guint modifier = gdk_modifiers_names[std::string(beg,end-beg+1)])
            {
                state |= modifier;
                beg = end;
                continue;
            }

        key_val = GDK_VoidSymbol;
        break;
    }

    GdkEventKey event;

    event.keyval = key_val;
    event.state = state;

    return event;
}

inline GdkEventKey str2key(gchar *s, GdkEventKey &event)
{
    return str2key(s, event.state, event.keyval);
}

inline GdkEventKey str2key(gchar *s)
{
    GdkEventKey event;

    return str2key(s, event);
}


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

    static DICT<GnomeCmdUserActionFunc> action_func;
    static DICT<GnomeCmdUserActionFunc> action_name;

    typedef std::map<GdkEventKey, UserAction> ACTIONS_COLL;

    ACTIONS_COLL action;


  public:

    void init();
    void shutdown();

    void load(const gchar *section);
    void write(const gchar *section);

    void clear ()                                                           {   action.clear();               }

    gboolean register_action(guint state, guint keyval, const gchar *name, const char *user_data=NULL);
    gboolean register_action(guint keyval, const gchar *name, const char *user_data=NULL);
    void unregister(const gchar *name);
    void unregister(guint state, guint keyval);
    void unregister(guint keyval)                                           {  unregister(0, keyval);         }
    gboolean registered(const gchar *name);
    gboolean registered(guint state, guint keyval);
    gboolean registered(guint keyval)                                       {  return registered(0, keyval);  }

    gboolean handle_key_event(GnomeCmdMainWin *mw, GnomeCmdFileList *fl, GdkEventKey *event);

    struct const_iterator: ACTIONS_COLL::iterator
    {
        const_iterator (const ACTIONS_COLL::iterator &i): ACTIONS_COLL::iterator(i)   {}

        const ACTIONS_COLL::key_type &operator * () const                   {  return (ACTIONS_COLL::iterator::operator * ()).first;  }
    };

    const_iterator begin()                                                  {  return action.begin();                                 }
    const_iterator end()                                                    {  return action.end();                                   }

    const gchar *name(const_iterator &i)                                    {  return action_func[i->second.func].c_str();            }
    const gchar *name(const std::string description)                        {  return action_func[action_name[description]].c_str();  }
    const gchar *description(const_iterator &i)                             {  return action_name[i->second.func].c_str();            }
    const gchar *options(const_iterator &i)                                 {  return i->second.user_data.c_str();                    }
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


GtkTreeModel *gnome_cmd_user_actions_create_model ();


GNOME_CMD_USER_ACTION(no_action);

/************** File Menu **************/
GNOME_CMD_USER_ACTION(file_copy);
GNOME_CMD_USER_ACTION(file_move);
GNOME_CMD_USER_ACTION(file_delete);
GNOME_CMD_USER_ACTION(file_view);
GNOME_CMD_USER_ACTION(file_internal_view);
GNOME_CMD_USER_ACTION(file_external_view);
GNOME_CMD_USER_ACTION(file_edit);
GNOME_CMD_USER_ACTION(file_edit_new_doc);
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
GNOME_CMD_USER_ACTION(file_sendto);
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

/************** Command Menu **************/
GNOME_CMD_USER_ACTION(command_execute);
GNOME_CMD_USER_ACTION(command_open_terminal);
GNOME_CMD_USER_ACTION(command_open_nautilus);
GNOME_CMD_USER_ACTION(command_open_nautilus_in_cwd);
GNOME_CMD_USER_ACTION(command_root_mode);

/************** View Menu **************/
GNOME_CMD_USER_ACTION(view_conbuttons);
GNOME_CMD_USER_ACTION(view_concombo);
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
GNOME_CMD_USER_ACTION(view_in_left_pane);
GNOME_CMD_USER_ACTION(view_in_right_pane);
GNOME_CMD_USER_ACTION(view_in_active_pane);
GNOME_CMD_USER_ACTION(view_in_inactive_pane);
GNOME_CMD_USER_ACTION(view_home);
GNOME_CMD_USER_ACTION(view_root);
GNOME_CMD_USER_ACTION(view_new_tab);
GNOME_CMD_USER_ACTION(view_close_tab);

/************** Bookmarks Menu **************/
GNOME_CMD_USER_ACTION(bookmarks_add_current);
GNOME_CMD_USER_ACTION(bookmarks_edit);
GNOME_CMD_USER_ACTION(bookmarks_goto);

/************** Options Menu **************/
GNOME_CMD_USER_ACTION(options_edit);
GNOME_CMD_USER_ACTION(options_edit_mime_types);
GNOME_CMD_USER_ACTION(options_edit_shortcuts);

/************** Connections Menu **************/
GNOME_CMD_USER_ACTION(connections_open);
GNOME_CMD_USER_ACTION(connections_new);
GNOME_CMD_USER_ACTION(connections_change);          // this function is NOT exposed to user as UserAction
GNOME_CMD_USER_ACTION(connections_close);           // this function is NOT exposed to user as UserAction
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

#endif // __GNOME_CMD_USER_ACTIONS_H__
