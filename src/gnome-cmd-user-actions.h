/**
 * @file gnome-cmd-user-actions.h
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

#include <string>
#include <map>

#include <gdk/gdkevents.h>

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-main-win.h"
#include "dict.h"

#define USER_ACTION_SETTINGS (gcmd_user_action_settings_get_type ())
G_DECLARE_FINAL_TYPE (GcmdUserActionSettings, gcmd_user_action_settings, GCMD, USER_ACTIONS, GObject)
GcmdUserActionSettings *gcmd_user_action_settings_new (void);

typedef void (*GnomeCmdUserActionFunc) (GSimpleAction *action, GVariant *parameter, gpointer user_data);


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
    if (state & GDK_SUPER_MASK)    key_name += gdk_modifiers_names[GDK_SUPER_MASK];
    if (state & GDK_HYPER_MASK)    key_name += gdk_modifiers_names[GDK_HYPER_MASK];
    if (state & GDK_META_MASK)     key_name += gdk_modifiers_names[GDK_META_MASK];

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
    s = g_ascii_strdown (s, -1);

    gchar *key = strrchr(s, '>');       // find last '>'
    key = key ? key+1 : s;

    key_val = gdk_key_names[key];
    state = 0;

     if (key_val==GDK_KEY_VoidSymbol)
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

        key_val = GDK_KEY_VoidSymbol;
        break;
    }

    g_free (s);

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

GnomeCmdFileSelector *get_fs (const FileSelectorID fsID);
GnomeCmdFileList     *get_fl (const FileSelectorID fsID);

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

  public:

    static DICT<GnomeCmdUserActionFunc> action_func;
    static DICT<GnomeCmdUserActionFunc> action_name;

    typedef std::map<GdkEventKey, UserAction> ACTIONS_COLL;

    ACTIONS_COLL action;


    void init();
    void set_defaults();
    void shutdown();
    GcmdUserActionSettings *settings;

    void clear()                                                            {   action.clear();               }

    gboolean has_action(const gchar *a)                                     {  return action_func[a]!=NULL;   }

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
        explicit const_iterator (const ACTIONS_COLL::iterator &i): ACTIONS_COLL::iterator(i) {}

        const ACTIONS_COLL::key_type &operator * () const                   {  return (ACTIONS_COLL::iterator::operator * ()).first;       }
    };

    const_iterator begin()                                                  {  return (const_iterator) action.begin();                                      }
    const_iterator end()                                                    {  return (const_iterator) action.end();                                        }
    unsigned size()                                                         {  return action.size();                                       }

    const gchar *name(const_iterator &i)                                    {  return action_func[i->second.func].c_str();                 }
    const gchar *name(const std::string &name_description)                  {  return action_func[action_name[name_description]].c_str();  }
    const gchar *description(const_iterator &i)                             {  return action_name[i->second.func].c_str();                 }
    const gchar *options(const_iterator &i)                                 {  return i->second.user_data.c_str();                         }
};


inline GnomeCmdUserActions::UserAction::UserAction(GnomeCmdUserActionFunc _func, const char *_user_data): func(_func)
{
    if (_user_data)
        user_data = _user_data;
}

inline gboolean GnomeCmdUserActions::register_action(guint keyval, const gchar *action_name_argument, const char *user_data)
{
    return register_action(0, keyval, action_name_argument, user_data);
}

int parse_command(string *cmd, const gchar *command);

void eventually_warn_if_xdg_email_is_used();

extern GnomeCmdUserActions gcmd_user_actions;


GtkTreeModel *gnome_cmd_user_actions_create_model ();

extern const GActionEntry FILE_ACTION_ENTRIES[];
extern const GActionEntry MARK_ACTION_ENTRIES[];
extern const GActionEntry EDIT_ACTION_ENTRIES[];
extern const GActionEntry COMMAND_ACTION_ENTRIES[];
extern const GActionEntry VIEW_ACTION_ENTRIES[];
extern const GActionEntry BOOKMARK_ACTION_ENTRIES[];
extern const GActionEntry OPTIONS_ACTION_ENTRIES[];
extern const GActionEntry CONNECTIONS_ACTION_ENTRIES[];
extern const GActionEntry PLUGINS_ACTION_ENTRIES[];
extern const GActionEntry HELP_ACTION_ENTRIES[];
