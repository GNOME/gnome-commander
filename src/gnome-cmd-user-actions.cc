/**
 * @file gnome-cmd-user-actions.cc
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

#include <gtk/gtk.h>
#include <set>
#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-con-home.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-dir-indicator.h"
#include "plugin_manager.h"
#include "cap.h"
#include "utils.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"
#include "dialogs/gnome-cmd-key-shortcuts-dialog.h"
#include "dialogs/gnome-cmd-manage-bookmarks-dialog.h"
#include "dialogs/gnome-cmd-mkdir-dialog.h"
#include "dialogs/gnome-cmd-search-dialog.h"
#include "dialogs/gnome-cmd-options-dialog.h"

using namespace std;

/***********************************
 * Functions for using GSettings
 ***********************************/

struct _GcmdUserActionSettings
{
    GObject parent;
    GSettings *filter;
    GSettings *general;
    GSettings *programs;
};

G_DEFINE_TYPE (GcmdUserActionSettings, gcmd_user_action_settings, G_TYPE_OBJECT)

static void gcmd_user_action_settings_finalize (GObject *object)
{
    G_OBJECT_CLASS (gcmd_user_action_settings_parent_class)->finalize (object);
}

static void gcmd_user_action_settings_dispose (GObject *object)
{
    GcmdUserActionSettings *gs = GCMD_USER_ACTIONS (object);

    g_clear_object (&gs->general);
    g_clear_object (&gs->programs);

    G_OBJECT_CLASS (gcmd_user_action_settings_parent_class)->dispose (object);
}

static void gcmd_user_action_settings_class_init (GcmdUserActionSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcmd_user_action_settings_finalize;
    object_class->dispose = gcmd_user_action_settings_dispose;
}

GcmdUserActionSettings *gcmd_user_action_settings_new ()
{
    return (GcmdUserActionSettings *) g_object_new (USER_ACTION_SETTINGS, nullptr);
}

static void gcmd_user_action_settings_init (GcmdUserActionSettings *gs)
{
    GSettingsSchemaSource   *global_schema_source;
    GSettingsSchema         *global_schema;

    global_schema_source = GnomeCmdData::GetGlobalSchemaSource();

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_FILTER, FALSE);
    gs->filter = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_GENERAL, FALSE);
    gs->general = g_settings_new_full (global_schema, nullptr, nullptr);

    global_schema = g_settings_schema_source_lookup (global_schema_source, GCMD_PREF_PROGRAMS, FALSE);
    gs->programs = g_settings_new_full (global_schema, nullptr, nullptr);
}

/***********************************
 * UserActions
 ***********************************/

#define GNOME_CMD_USER_ACTION(f)       extern "C" void f(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#define GNOME_CMD_USER_ACTION_TGL(f)   extern "C" void f(GSimpleAction *action, GVariant *parameter, gpointer user_data)

/************** File Menu **************/
GNOME_CMD_USER_ACTION(file_copy);
GNOME_CMD_USER_ACTION(file_copy_as);
GNOME_CMD_USER_ACTION(file_move);
GNOME_CMD_USER_ACTION(file_delete);
GNOME_CMD_USER_ACTION(file_view);
GNOME_CMD_USER_ACTION(file_internal_view);
GNOME_CMD_USER_ACTION(file_external_view);
GNOME_CMD_USER_ACTION(file_edit);
GNOME_CMD_USER_ACTION(file_edit_new_doc);
GNOME_CMD_USER_ACTION(file_search);
GNOME_CMD_USER_ACTION(file_quick_search);
GNOME_CMD_USER_ACTION(file_chmod);
GNOME_CMD_USER_ACTION(file_chown);
GNOME_CMD_USER_ACTION(file_mkdir);
GNOME_CMD_USER_ACTION(file_properties);
GNOME_CMD_USER_ACTION(file_diff);
GNOME_CMD_USER_ACTION(file_sync_dirs);
GNOME_CMD_USER_ACTION(file_rename);
GNOME_CMD_USER_ACTION(file_create_symlink);
GNOME_CMD_USER_ACTION(file_advrename);
GNOME_CMD_USER_ACTION(file_sendto);
GNOME_CMD_USER_ACTION(file_exit);

/************** Mark Menu **************/
GNOME_CMD_USER_ACTION(mark_toggle);
GNOME_CMD_USER_ACTION(mark_toggle_and_step);
GNOME_CMD_USER_ACTION(mark_select_all);
GNOME_CMD_USER_ACTION(mark_unselect_all);
GNOME_CMD_USER_ACTION(mark_select_all_files);
GNOME_CMD_USER_ACTION(mark_unselect_all_files);
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
GNOME_CMD_USER_ACTION(edit_filter);
GNOME_CMD_USER_ACTION(edit_copy_fnames);

/************** View Menu **************/
GNOME_CMD_USER_ACTION_TGL(view_conbuttons);
GNOME_CMD_USER_ACTION_TGL(view_devlist);
GNOME_CMD_USER_ACTION_TGL(view_toolbar);
GNOME_CMD_USER_ACTION_TGL(view_buttonbar);
GNOME_CMD_USER_ACTION_TGL(view_cmdline);
GNOME_CMD_USER_ACTION(view_dir_history);
GNOME_CMD_USER_ACTION_TGL(view_hidden_files);
GNOME_CMD_USER_ACTION_TGL(view_backup_files);
GNOME_CMD_USER_ACTION(view_up);
GNOME_CMD_USER_ACTION(view_first);
GNOME_CMD_USER_ACTION(view_back);
GNOME_CMD_USER_ACTION(view_forward);
GNOME_CMD_USER_ACTION(view_last);
GNOME_CMD_USER_ACTION(view_refresh);
GNOME_CMD_USER_ACTION(view_refresh_tab);
GNOME_CMD_USER_ACTION(view_equal_panes);
GNOME_CMD_USER_ACTION(view_maximize_pane);
GNOME_CMD_USER_ACTION(view_in_left_pane);
GNOME_CMD_USER_ACTION(view_in_right_pane);
GNOME_CMD_USER_ACTION(view_in_active_pane);
GNOME_CMD_USER_ACTION(view_in_inactive_pane);
GNOME_CMD_USER_ACTION(view_directory);
GNOME_CMD_USER_ACTION(view_home);
GNOME_CMD_USER_ACTION(view_root);
GNOME_CMD_USER_ACTION(view_new_tab);
GNOME_CMD_USER_ACTION(view_close_tab);
GNOME_CMD_USER_ACTION(view_close_all_tabs);
GNOME_CMD_USER_ACTION(view_close_duplicate_tabs);
GNOME_CMD_USER_ACTION(view_prev_tab);
GNOME_CMD_USER_ACTION(view_next_tab);
GNOME_CMD_USER_ACTION(view_in_new_tab);
GNOME_CMD_USER_ACTION(view_in_inactive_tab);
GNOME_CMD_USER_ACTION(view_toggle_tab_lock);
GNOME_CMD_USER_ACTION_TGL(view_horizontal_orientation);
GNOME_CMD_USER_ACTION(view_main_menu);
GNOME_CMD_USER_ACTION(view_step_up);
GNOME_CMD_USER_ACTION(view_step_down);

/************** Bookmarks Menu **************/
GNOME_CMD_USER_ACTION(bookmarks_add_current);
GNOME_CMD_USER_ACTION(bookmarks_edit);
GNOME_CMD_USER_ACTION(bookmarks_goto);
GNOME_CMD_USER_ACTION(bookmarks_view);

/************** Options Menu **************/
GNOME_CMD_USER_ACTION(options_edit);
GNOME_CMD_USER_ACTION(options_edit_shortcuts);

/************** Plugins Menu ***********/
GNOME_CMD_USER_ACTION(plugins_configure);

static GnomeCmdFileList *get_fl (GnomeCmdMainWin *main_win, const FileSelectorID fsID)
{
    GnomeCmdFileSelector *fs = main_win->fs(fsID);

    return fs ? fs->file_list() : nullptr;
}


inline gboolean append_real_path (string &s, const gchar *name)
{
    s += ' ';
    s += name;

    return TRUE;
}


static gboolean append_real_path (string &s, GnomeCmdFile *f)
{
    if (!f)
        return FALSE;

    gchar *name = g_shell_quote (f->get_real_path());

    append_real_path (s, name);

    g_free (name);

    return TRUE;
}


GnomeCmdUserActions gcmd_user_actions;


inline bool operator < (const GnomeCmdKeyPress &e1, const GnomeCmdKeyPress &e2)
{
    if (e1.keyval < e2.keyval)
        return true;

    if (e1.keyval > e2.keyval)
        return false;

    return (e1.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK)) < (e2.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));
}


DICT<GnomeCmdUserActionFunc> GnomeCmdUserActions::action_func;
DICT<GnomeCmdUserActionFunc> GnomeCmdUserActions::action_name;


extern "C" void register_user_action(GnomeCmdUserActions *actions,
    void (* func)(GSimpleAction *action, GVariant *parameter, gpointer user_data),
    const gchar *name,
    const gchar *description)
{
    actions->action_func.add(func, name);
    actions->action_name.add(func, description);
}

extern "C" void register_user_actions(GnomeCmdUserActions *actions);

void GnomeCmdUserActions::init()
{
    register_user_actions(this);

    register_action(GDK_KEY_F3, "file.view");
    register_action(GDK_KEY_F4, "file.edit");
    register_action(GDK_KEY_F5, "file.copy");
    register_action(GDK_KEY_F6, "file.rename");
    register_action(GDK_KEY_F7, "file.mkdir");
    register_action(GDK_KEY_F8, "file.delete");
    // register_action(GDK_KEY_F9, "edit.search");     //  do not register F9 here, as edit.search action wouldn't be checked for registration later
    settings = gcmd_user_action_settings_new();
}


void GnomeCmdUserActions::set_defaults()
{
    if (!registered("view.main_menu"))
        register_action(GDK_KEY_F12, "view.main_menu");

    if (!registered("bookmarks.edit"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_D, "bookmarks.edit");

    if (!registered("connections.new"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_N, "connections.new");

    if (!registered("connections.open"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_F, "connections.open");

    if (!registered("connections.close"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_F, "connections.close");

    if (!registered("connections.change_left"))
    {
        register_action(GDK_ALT_MASK, GDK_KEY_1, "connections.change_left");
        register_action(GDK_SUPER_MASK, GDK_KEY_1, "connections.change_left");
    }

    if (!registered("connections.change_right"))
    {
        register_action(GDK_ALT_MASK, GDK_KEY_2, "connections.change_right");
        register_action(GDK_SUPER_MASK, GDK_KEY_2, "connections.change_right");
    }

    if (!registered("edit.copy_filenames"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_C, "edit.copy_filenames");

    if (!registered("edit.filter"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_F12, "edit.filter");

    if (!registered("edit.search"))
    {
        register_action(GDK_ALT_MASK, GDK_KEY_F7, "edit.search");
        register_action(GDK_SUPER_MASK, GDK_KEY_F, "edit.search");
    }

    if (!registered("file.advrename"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_M, "file.advrename");

    if (!registered("file.copy_as"))
        register_action(GDK_SHIFT_MASK, GDK_KEY_F5, "file.copy_as");

    if (!registered("file.create_symlink"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_F5, "file.create_symlink");

    if (!registered("file.edit_new_doc"))
        register_action(GDK_SHIFT_MASK, GDK_KEY_F4, "file.edit_new_doc");

    if (!registered("file.exit"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_Q, "file.exit");

    if (!registered("file.external_view"))
        register_action(GDK_ALT_MASK, GDK_KEY_F3, "file.external_view");

    if (!registered("file.internal_view"))
        register_action(GDK_SHIFT_MASK, GDK_KEY_F3, "file.internal_view");

    if (!registered("mark.compare_directories"))
        register_action(GDK_SHIFT_MASK, GDK_KEY_F2, "mark.compare_directories");

    if (!registered("mark.select_all"))
    {
        register_action(GDK_CONTROL_MASK, GDK_KEY_A, "mark.select_all");
        register_action(GDK_CONTROL_MASK, GDK_KEY_equal, "mark.select_all");
        register_action(GDK_CONTROL_MASK, GDK_KEY_KP_Add, "mark.select_all");
    }

    if (!registered("mark.unselect_all"))
    {
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_A, "mark.unselect_all");
        register_action(GDK_CONTROL_MASK, GDK_KEY_minus, "mark.unselect_all");
        register_action(GDK_CONTROL_MASK, GDK_KEY_KP_Subtract, "mark.unselect_all");
    }

    if (!registered("options.edit"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_O, "options.edit");

    if (!registered("dir_history"))
    {
        register_action(GDK_ALT_MASK, GDK_KEY_Down, "view.dir_history");
        register_action(GDK_ALT_MASK, GDK_KEY_KP_Down, "view.dir_history");
    }

    if (!registered("view.up"))
    {
        register_action(GDK_CONTROL_MASK, GDK_KEY_Page_Up, "view.up");
        register_action(GDK_CONTROL_MASK, GDK_KEY_KP_Page_Up, "view.up");
    }

    if (!registered("view.equal_panes"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_plus, "view.equal_panes");

    if (!registered("view.in_active_pane"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_period, "view.in_active_pane");

    if (!registered("view.in_inactive_pane"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_greater, "view.in_inactive_pane");

    if (!registered("view.in_left_pane"))
    {
        register_action(GDK_CONTROL_MASK, GDK_KEY_Left, "view.in_left_pane");
        register_action(GDK_CONTROL_MASK, GDK_KEY_KP_Left, "view.in_left_pane");
    }

    if (!registered("view.in_right_pane"))
    {
        register_action(GDK_CONTROL_MASK, GDK_KEY_Right, "view.in_right_pane");
        register_action(GDK_CONTROL_MASK, GDK_KEY_KP_Right, "view.in_right_pane");
    }

    if (!registered("view.in_new_tab"))
    {
        register_action(GDK_CONTROL_MASK, GDK_KEY_Up, "view.in_new_tab");
        register_action(GDK_CONTROL_MASK, GDK_KEY_KP_Up, "view.in_new_tab");
    }

    if (!registered("view.in_inactive_tab"))
    {
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_Up, "view.in_inactive_tab");
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_KP_Up, "view.in_inactive_tab");
    }

    if (!registered("view.directory"))
    {
        register_action(GDK_CONTROL_MASK, GDK_KEY_Page_Down, "view.directory");
        register_action(GDK_CONTROL_MASK, GDK_KEY_KP_Page_Down, "view.directory");
    }

    if (!registered("view.home"))
    {
        register_action(GDK_CONTROL_MASK, GDK_KEY_quoteleft, "view.home");
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_asciitilde, "view.home");
    }

    if (!registered("view.root"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_backslash, "view.root");

    if (!registered("view.refresh"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_R, "view.refresh");

    if (!registered("view.new_tab"))
    {
        unregister(GDK_CONTROL_MASK, GDK_KEY_T);                       // unregister CTRL+T as it was used previously for file.advrename
        register_action(GDK_CONTROL_MASK, GDK_KEY_T, "view.new_tab");
    }

    if (!registered("view.close_tab"))
        register_action(GDK_CONTROL_MASK, GDK_KEY_W, "view.close_tab");

    if (!registered("view.close_all_tabs"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KEY_W, "view.close_all_tabs");

    unregister(GDK_KEY_F9);                                 // unregister F9 if defined in [key-bindings]
    register_action(GDK_KEY_F9, "edit.search");             // and overwrite it with edit.search action
 }


void GnomeCmdUserActions::shutdown()
{
    // unregister Fn to avoid writing 'hardcoded' Fn actions to config file

    unregister(GDK_KEY_F3);
    unregister(GDK_KEY_F4);
    unregister(GDK_KEY_F5);
    unregister(GDK_KEY_F6);
    unregister(GDK_KEY_F7);
    unregister(GDK_KEY_F8);
    unregister(GDK_KEY_F9);
}


gboolean GnomeCmdUserActions::register_action(guint state, guint keyval, const gchar *the_action_name, const char *user_data)
{
    GnomeCmdUserActionFunc func = action_func[the_action_name];

    if (!func)
        return FALSE;

    GnomeCmdKeyPress event;

    event.keyval = keyval;
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);
    if (action.find(event)!=action.end())
        return FALSE;

    if (!ascii_isalpha (keyval))
        action[event] = UserAction(func, user_data);
    else
    {
        event.keyval = g_ascii_toupper (keyval);
        action[event] = UserAction(func, user_data);
        event.keyval = g_ascii_tolower (keyval);
        action[event] = UserAction(func, user_data);
    }

    return TRUE;
}


void GnomeCmdUserActions::unregister(const gchar *the_action_name)
{
}


void GnomeCmdUserActions::unregister(guint state, guint keyval)
{
    GnomeCmdKeyPress event;

    event.keyval = keyval;
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);
    map <GnomeCmdKeyPress, UserAction>::iterator pos = action.find(event);

    if (pos!=action.end())
        action.erase(pos);
}


gboolean GnomeCmdUserActions::registered(const gchar *the_action_name)
{
    GnomeCmdUserActionFunc func = action_func[the_action_name];

    if (!func)
        return FALSE;

    for (ACTIONS_COLL::const_iterator i=action.begin(); i!=action.end(); ++i)
        if (i->second.func==func)
            return TRUE;

    return FALSE;
}


gboolean GnomeCmdUserActions::registered(guint state, guint keyval)
{
    GnomeCmdKeyPress event;

    event.keyval = keyval;
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);

    return action.find(event)!=action.end();
}


gboolean GnomeCmdUserActions::handle_key_event(GnomeCmdMainWin *mw, GnomeCmdFileList *fl, GnomeCmdKeyPress *event)
{
    map <GnomeCmdKeyPress, UserAction>::const_iterator pos = action.find(*event);

    if (pos==action.end())
        return FALSE;

    DEBUG('u', "Key event:  %s (%#x)\n", key2str(*event).c_str(), event->keyval);
    DEBUG('u', "Handling key event by %s()\n", action_func[pos->second.func].c_str());

    // This is a bit controversial. Majority of actions to not accept arguments
    // and those which accept expect a specific variant and not an arbitrary
    // string.
    GVariant *parameter = nullptr;
    if (!pos->second.user_data.empty())
        parameter = g_variant_new_string (pos->second.user_data.c_str());

    (*pos->second.func) (nullptr, parameter, (gpointer) mw);

    return TRUE;
}


template <typename F>
static void get_file_list (string &s, GList *sfl, F f)
{
    vector<string> a;

    for (GList *i = sfl; i; i = i->next)
    {
        auto value = (*f) (GNOME_CMD_FILE (i->data));

        if (value)
            a.push_back (value);
    }
    join (s, a.begin(), a.end());
}


template <typename F, typename T>
inline void get_file_list (string &s, GList *sfl, F f, T t)
{
    vector<string> a;

    for (GList *i = sfl; i; i = i->next)
    {
        auto value = (*f) (GNOME_CMD_FILE (i->data), t);

        if (value)
            a.push_back (value);
    }

    join (s, a.begin(), a.end());
}


/************** File Menu **************/

void file_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_delete_dialog (get_fl (main_win, ACTIVE));
}


void file_view (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_view (get_fl (main_win, ACTIVE), gnome_cmd_data.options.use_internal_viewer);
}


void file_internal_view (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_view (get_fl (main_win, ACTIVE), TRUE);
}


void file_external_view (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_view (get_fl (main_win, ACTIVE), FALSE);
}

void file_edit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GdkModifierType mask = get_modifiers_state();

    if (mask & GDK_SHIFT_MASK)
        gnome_cmd_file_selector_show_new_textfile_dialog (main_win->fs (ACTIVE));
    else
    {
        GnomeCmdFileList *fl = get_fl (main_win, ACTIVE);
        GList *sfl = fl->get_selected_files();

        GError *error = nullptr;
        int result = spawn_async_r(nullptr, sfl, gnome_cmd_data.options.editor, &error);
        switch (result)
        {
            case 0:
                break;
            case 1:
            case 2:
                DEBUG ('g', "Edit file command is not valid.\n");
                gnome_cmd_show_message (*main_win, _("No valid command given."));
                g_clear_error (&error);
                break;
            case 3:
            default:
                gnome_cmd_error_message (*main_win, _("Unable to execute command."), error);
                break;
        }
    }
}

void file_edit_new_doc (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_selector_show_new_textfile_dialog (main_win->fs (ACTIVE));
}


void file_search (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    if (gnome_cmd_data.options.use_internal_search)
    {
        auto dlg = main_win->get_or_create_search_dialog ();
        dlg->show_and_set_focus ();
    }
    else
    {
        GnomeCmdFileList *fl = get_fl (main_win, ACTIVE);
        GList *sfl = fl->get_selected_files();

        GError *error = nullptr;
        int result = spawn_async_r(nullptr, sfl, gnome_cmd_data.options.search, &error);
        switch (result)
        {
            case 0:
                break;
            case 1:
            case 2:
                DEBUG ('g', "Search command is empty.\n");
                gnome_cmd_show_message (*main_win, _("No search command given."), _("You can set a command for a search tool in the program options."));
                g_clear_error (&error);
                break;
            case 3:
            default:
                gnome_cmd_error_message (*main_win, _("Unable to execute command."), error);
                break;
        }
    }
}


void file_quick_search (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_quicksearch (get_fl (main_win, ACTIVE), 0);
}


void file_mkdir (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdDir *dir = fs->get_directory();

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gnome_cmd_dir_ref (dir);
    gnome_cmd_mkdir_dialog_new (dir, fs->file_list()->get_selected_file());
    gnome_cmd_dir_unref (dir);
}


void file_rename (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_rename_dialog (get_fl (main_win, ACTIVE));
}


void file_advrename (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GList *files = get_fl (main_win, ACTIVE)->get_selected_files();

    if (files)
    {
        auto dlg = main_win->get_or_create_advrename_dialog ();
        dlg->set(files);
        gtk_window_present (GTK_WINDOW (dlg));

        g_list_free (files);
    }
}


void file_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_properties_dialog (get_fl (main_win, ACTIVE));
}


void file_diff (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    if (!main_win->fs (ACTIVE)->is_local())
    {
        gnome_cmd_show_message (*main_win, _("Operation not supported on remote file systems"));
        return;
    }

    GnomeCmdFileList *active_fl = get_fl (main_win, ACTIVE);

    GList *sel_files = active_fl->get_selected_files();

    string files_to_differ;

    switch (g_list_length (sel_files))
    {
        case 0:
            return;

        case 1:
            if (!main_win->fs (INACTIVE)->is_local())
                gnome_cmd_show_message (*main_win, _("Operation not supported on remote file systems"));
            else
            {
                GnomeCmdFile *active_file = (GnomeCmdFile *) sel_files->data;
                GnomeCmdFile *inactive_file = get_fl (main_win, INACTIVE)->get_first_selected_file();

                if (inactive_file)
                {
                    append_real_path (files_to_differ, active_file);
                    append_real_path (files_to_differ, inactive_file);
                }
                else
                {
                    gnome_cmd_show_message (*main_win, _("No file selected"));
                }
            }
            break;

        case 2:
        case 3:
            for (GList *i = sel_files; i; i = i->next)
                append_real_path (files_to_differ, GNOME_CMD_FILE (i->data));
            break;

        default:
            gnome_cmd_show_message (*main_win, _("Too many selected files"));
            break;
    }

    g_list_free (sel_files);

    if (!files_to_differ.empty())
    {
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
        gchar *cmd = g_strdup_printf (gnome_cmd_data.options.differ, files_to_differ.c_str(), "");
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

        run_command (*main_win, cmd);

        g_free (cmd);
    }
}


void file_sync_dirs (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *active_fs = main_win->fs (ACTIVE);
    GnomeCmdFileSelector *inactive_fs = main_win->fs (INACTIVE);

    if (!active_fs->is_local() || !inactive_fs->is_local())
    {
        gnome_cmd_show_message (*main_win, _("Operation not supported on remote file systems"));
        return;
    }

    string s;

    append_real_path (s, GNOME_CMD_FILE (active_fs->get_directory()));
    append_real_path (s, GNOME_CMD_FILE (inactive_fs->get_directory()));

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    gchar *cmd = g_strdup_printf (gnome_cmd_data.options.differ, s.c_str(), "");
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

    run_command (*main_win, cmd);

    g_free (cmd);
}


void file_exit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gtk_window_destroy (GTK_WINDOW (main_win));
}


/************** Edit Menu **************/
void edit_cap_cut (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_cap_cut (get_fl (main_win, ACTIVE));
}


void edit_cap_copy (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_cap_copy (get_fl (main_win, ACTIVE));
}


void edit_cap_paste (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_selector_cap_paste (main_win->fs (ACTIVE));
}


void edit_filter (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->show_filter();
}


void edit_copy_fnames (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GdkModifierType mask = get_modifiers_state(); // TODO: get this via parameter

    GnomeCmdFileList *fl = get_fl (main_win, ACTIVE);
    GList *sfl = fl->get_selected_files();

    string fnames;

    fnames.reserve(2000);

    if (state_is_blank (mask))
        get_file_list (fnames, sfl, gnome_cmd_file_get_name);
    else
        if (state_is_shift (mask))
            get_file_list (fnames, sfl, gnome_cmd_file_get_real_path);
        else
            if (state_is_alt (mask))
                get_file_list (fnames, sfl, gnome_cmd_file_get_uri_str);

    gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (main_win)), fnames.c_str());

    g_list_free (sfl);
}


/************** Mark Menu **************/
void mark_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->toggle();
}


void mark_toggle_and_step (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->toggle_and_step();
}


void mark_select_all (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->select_all();
}


void mark_select_all_files (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->select_all_files();
}


void mark_unselect_all_files (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->unselect_all_files();
}


void mark_unselect_all (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->unselect_all();
}


void mark_select_with_pattern (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_selpat_dialog (get_fl (main_win, ACTIVE), TRUE);
}


void mark_unselect_with_pattern (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_file_list_show_selpat_dialog (get_fl (main_win, ACTIVE), FALSE);
}


void mark_invert_selection (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->invert_selection();
}


void mark_select_all_with_same_extension (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->select_all_with_same_extension();
}


void mark_unselect_all_with_same_extension (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->unselect_all_with_same_extension();
}


void mark_restore_selection (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    get_fl (main_win, ACTIVE)->restore_selection();
}


struct ltstr
{
    bool operator() (const char *s1, const char *s2) const   {  return strcmp(s1,s2) < 0;  }
};


template <typename T>
struct select2nd
{
    typename T::second_type operator()(T const& value) const    {  return value.second;  }
};


template <typename T>
inline select2nd<typename T::value_type> make_select2nd(T const& m)
{
    return select2nd<typename T::value_type>();
}


inline void selection_delta(GnomeCmdFileList &fl, set<GnomeCmdFile *> &prev_selection, set<GnomeCmdFile *> &new_selection)
{
    vector<GnomeCmdFile *> a;

    a.reserve(max(prev_selection.size(),new_selection.size()));

    set_difference(prev_selection.begin(),prev_selection.end(),
                   new_selection.begin(),new_selection.end(),
                   back_inserter(a));

    for (vector<GnomeCmdFile *>::iterator i=a.begin(); i!=a.end(); ++i)
        fl.unselect_file(*i);

    a.clear();

    set_difference(new_selection.begin(),new_selection.end(),
                   prev_selection.begin(),prev_selection.end(),
                   back_inserter(a));

    for (vector<GnomeCmdFile *>::iterator i=a.begin(); i!=a.end(); ++i)
        fl.select_file(*i);
}


void mark_compare_directories (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileList *fl1 = get_fl (main_win, ACTIVE);
    GnomeCmdFileList *fl2 = get_fl (main_win, INACTIVE);

    map<const char *,GnomeCmdFile *,ltstr> files2;          //  map (fname -> GnomeCmdFile *) of visible files (non dirs!) in fl2

    auto fl2_files = fl2->get_all_files();
    for (auto i2 = fl2_files.begin(); i2 != fl2_files.end(); ++i2)
    {
        GnomeCmdFile *f2 = *i2;

        if (!f2->is_dotdot && f2->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_DIRECTORY)
            files2[f2->get_name()] = f2;
    }

    set<GnomeCmdFile *> new_selection1, new_selection2;

    auto fl1_files = fl1->get_all_files();
    for (auto i1 = fl1_files.begin(); i1 != fl1_files.end(); ++i1)
    {
        GnomeCmdFile *f1 = *i1;

        if (f1->is_dotdot || f1->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
            continue;

        map<const char *,GnomeCmdFile *,ltstr>::iterator i2 = files2.find(f1->get_name());

        if (i2==files2.end())                           //  if f1 is not found in visible fl2 files...
        {
            new_selection1.insert(f1);                  //  ... add f1 to files to be selected in fl1
            continue;
        }

        GnomeCmdFile *f2 = i2->second;

        if (f1->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_TIME_MODIFIED)
            > f2->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_TIME_MODIFIED)) //  if f1 is newer than f2...
        {
            new_selection1.insert(f1);                  //  ... add f1 to files to be selected in fl1...
            files2.erase(i2);                           //  ... and remove f2 from fl2 files
            continue;
        }

        if (f1->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_TIME_MODIFIED)
            == f2->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_TIME_MODIFIED)) //  if the same mtime for f1, f2
        {
            if (f1->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_STANDARD_SIZE)
                == f2->GetGfileAttributeUInt64(G_FILE_ATTRIBUTE_STANDARD_SIZE)) //  ... then check f1, f2 sizes
                files2.erase(i2);                       //  ... and remove f2 from fl2 files
            else
                new_selection1.insert(f1);              //  ... or add f1 to files to be selected in fl1
        }
    }

    transform (files2.begin(), files2.end(), inserter(new_selection2,new_selection2.begin()), make_select2nd(files2));    // copy left files2 --> new_selection2

    auto fl1_marked = fl1->get_marked_files();
    selection_delta (*fl1, fl1_marked, new_selection1);

    auto fl2_marked = fl2->get_marked_files();
    selection_delta (*fl2, fl2_marked, new_selection2);
}

/* ***************************** View Menu ****************************** */
/* Changing of GSettings here will trigger functions in gnome-cmd-data.cc */
/* ********************************************************************** */

void view_conbuttons (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_SHOW_DEVBUTTONS, active);
}


void view_devlist (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_SHOW_DEVLIST, active);
}


void view_toolbar (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_SHOW_TOOLBAR, active);
}


void view_buttonbar (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_SHOW_BUTTONBAR, active);
}


void view_cmdline (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_SHOW_CMDLINE, active);
}


void view_dir_history (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_dir_indicator_show_history (GNOME_CMD_DIR_INDICATOR (main_win->fs (ACTIVE)->dir_indicator));
}


void view_hidden_files (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->filter, GCMD_SETTINGS_FILTER_HIDE_HIDDEN, !active);
}


void view_backup_files (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->filter, GCMD_SETTINGS_FILTER_HIDE_BACKUPS, !active);
}


void view_horizontal_orientation (GSimpleAction *action, GVariant *state, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto active = g_variant_get_boolean (state);
    g_simple_action_set_state (action, state);

    if (gtk_widget_get_realized (GTK_WIDGET (main_win)))
        g_settings_set_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_HORIZONTAL_ORIENTATION, active);
}

void view_step_up (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    fl->focus_prev();
}

void view_step_down (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    fl->focus_next();
}

void view_up (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    if (fl->locked)
        fs->new_tab(gnome_cmd_dir_get_parent (fl->cwd));
    else
        fs->goto_directory("..");
}

void view_main_menu (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    if (!gtk_widget_get_realized ( GTK_WIDGET (main_win))) return;

    gboolean mainmenu_visibility;
    mainmenu_visibility = g_settings_get_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY);
    g_settings_set_boolean (gcmd_user_actions.settings->general, GCMD_SETTINGS_MAINMENU_VISIBILITY, !mainmenu_visibility);
}

void view_first (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->first();
}


void view_back (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->back();
}


void view_forward (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->forward();
}


void view_last (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->last();
}


void view_refresh (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileList *fl = get_fl (main_win, ACTIVE);
    fl->reload();
}


void view_refresh_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gint fs_id, tab;
    g_variant_get (parameter, "(ii)", &fs_id, &tab);

    GnomeCmdFileSelector *fs = main_win->fs ((FileSelectorID) fs_id);
    GnomeCmdFileList *fl = fs->file_list(tab);
    fl->reload();
}


void view_equal_panes (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_equal_panes();
}


void view_maximize_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->maximize_pane();
}


void view_in_left_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(LEFT);
}


void view_in_right_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(RIGHT);
}


void view_in_active_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(ACTIVE);
}


void view_in_inactive_pane (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->set_fs_directory_to_opposite(INACTIVE);
}


void view_directory (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    GnomeCmdFile *f = fl->get_selected_file();
    if (f && f->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        fs->do_file_specific_action (fl, f);
}


void view_home (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    if (!fl->locked)
    {
        fl->set_connection(get_home_con ());
        fl->goto_directory("~");
    }
    else
        fs->new_tab(gnome_cmd_dir_new (get_home_con (), gnome_cmd_con_create_path (get_home_con (), g_get_home_dir ())));
}


void view_root (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    if (fl->locked)
        fs->new_tab(gnome_cmd_dir_new (fl->con, gnome_cmd_con_create_path (fl->con, "/")));
    else
        fs->goto_directory("/");
}


void view_new_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileList *fl = get_fl (main_win, ACTIVE);
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (gtk_widget_get_ancestor (*fl, GNOME_CMD_TYPE_FILE_SELECTOR));
    fs->new_tab(fl->cwd);
}


extern "C" void view_close_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data);


void view_close_all_tabs (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);
    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GtkNotebook *notebook = GTK_NOTEBOOK (fs->notebook);

    gint n = gtk_notebook_get_current_page (notebook);

    for (gint i = gtk_notebook_get_n_pages (notebook); i--;)
        if (i!=n && !fs->file_list(i)->locked)
            gtk_notebook_remove_page (notebook, i);

    fs->update_show_tabs ();
}


void view_close_duplicate_tabs (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);
    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GtkNotebook *notebook = GTK_NOTEBOOK (fs->notebook);

    typedef set<gint> TABS_COLL;
    typedef map <GnomeCmdDir *, TABS_COLL> DIRS_COLL;

    DIRS_COLL dirs;

    for (gint i = gtk_notebook_get_n_pages (notebook); i--;)
    {
        GnomeCmdFileList *fl = fs->file_list(i);

        if (fl && !fl->locked)
            dirs[fl->cwd].insert(i);
    }

    TABS_COLL duplicates;

    DIRS_COLL::iterator pos = dirs.find(fs->get_directory());       //  find tabs with the current dir...

    if (pos!=dirs.end())
    {
        duplicates.swap(pos->second);                               //  ... and mark them as to be closed...
        duplicates.erase(gtk_notebook_get_current_page (notebook)); //  ... but WITHOUT the current one
        dirs.erase(pos);
    }

    for (DIRS_COLL::const_iterator i=dirs.begin(); i!=dirs.end(); ++i)
    {
        TABS_COLL::iterator beg = i->second.begin();
        copy(++beg, i->second.end(), inserter(duplicates, duplicates.begin()));   //  for every dir, leave the first tab opened
    }

    for (TABS_COLL::const_reverse_iterator i=duplicates.rbegin(); i!=duplicates.rend(); ++i)
        gtk_notebook_remove_page (notebook, *i);

    fs->update_show_tabs ();
}


void view_prev_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->prev_tab();
}


void view_next_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->fs (ACTIVE)->next_tab();
}


void view_in_new_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileSelector *fs = main_win->fs (ACTIVE);
    GnomeCmdFile *file = fs->file_list()->get_selected_file();

    if (file && file->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        fs->new_tab(GNOME_CMD_DIR (file), FALSE);
    else
        fs->new_tab(fs->get_directory(), FALSE);
}


void view_in_inactive_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GnomeCmdFileList *fl = get_fl (main_win, ACTIVE);
    GnomeCmdFile *file = fl->get_selected_file();

    if (file && file->GetGfileAttributeUInt32(G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
        main_win->fs (INACTIVE)->new_tab(GNOME_CMD_DIR (file), FALSE);
    else
        main_win->fs (INACTIVE)->new_tab(fl->cwd, FALSE);
}


void view_toggle_tab_lock (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gboolean is_active;
    gint tab_index;
    g_variant_get (parameter, "(bi)", &is_active, &tab_index);

    GnomeCmdFileSelector *fs = main_win->fs (is_active ? ACTIVE : INACTIVE);
    GnomeCmdFileList *fl = fs->file_list(tab_index);

    if (fl)
    {
        fl->locked = !fl->locked;
        fs->update_tab_label(fl);
    }
}


/************** Options Menu **************/
static void options_edit_done (gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    main_win->update_view();
    gnome_cmd_data.save(main_win);
}


void options_edit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    GtkDialog *options_dialog = gnome_cmd_options_dialog (*main_win, gnome_cmd_data.options, options_edit_done, main_win);
    gtk_window_present (GTK_WINDOW (options_dialog));
}


void options_edit_shortcuts (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_key_shortcuts_dialog_new (*main_win, gcmd_user_actions);
}

/************** Bookmarks Menu **************/

void bookmarks_add_current (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_bookmark_add_current (main_win, main_win->fs (ACTIVE)->get_directory());
}


void bookmarks_edit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    auto dlg = main_win->get_or_create_bookmarks_dialog ();
    gtk_window_present (GTK_WINDOW (dlg));

    gnome_cmd_bookmarks_dialog_new (*main_win);
}


void bookmarks_goto (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    // auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    const gchar *con_uuid;
    const gchar *bookmark_name;
    g_variant_get (parameter, "(ss)", &con_uuid, &bookmark_name);

    GnomeCmdConList *con_list = gnome_cmd_con_list_get ();
    GnomeCmdCon *con = gnome_cmd_con_list_find_by_uuid (con_list, con_uuid);

    if (!con)
    {
        g_warning ("[%s] Unsupported bookmark group: '%s' - ignored", (char *) bookmark_name, con_uuid);
        return;
    }

    for (GList *bookmarks = gnome_cmd_con_get_bookmarks (con)->bookmarks; bookmarks; bookmarks = bookmarks->next)
    {
        auto bookmark = static_cast<GnomeCmdBookmark*> (bookmarks->data);

        if (g_strcmp0(bookmark_name, bookmark->name) == 0)
        {
            gnome_cmd_bookmark_goto (bookmark);
            return;
        }
    }

    g_warning ("[%s] Unknown bookmark name: '%s' - ignored", con->alias, bookmark_name);
}


void bookmarks_view (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    gnome_cmd_dir_indicator_show_bookmarks (GNOME_CMD_DIR_INDICATOR (main_win->fs (ACTIVE)->dir_indicator));
}


/************** Plugins Menu **************/

void plugins_configure (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto main_win = static_cast<GnomeCmdMainWin *>(user_data);

    plugin_manager_show (GTK_WINDOW (main_win));
}
