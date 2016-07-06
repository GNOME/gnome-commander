/** 
 * @file gnome-cmd-user-actions.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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
#include <gtk/gtkclipboard.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-url.h>
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
#include "gnome-cmd-python-plugin.h"
#include "gnome-cmd-user-actions.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-style.h"
#include "plugin_manager.h"
#include "cap.h"
#include "utils.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"
#include "dialogs/gnome-cmd-chmod-dialog.h"
#include "dialogs/gnome-cmd-chown-dialog.h"
#include "dialogs/gnome-cmd-con-dialog.h"
#include "dialogs/gnome-cmd-remote-dialog.h"
#include "dialogs/gnome-cmd-key-shortcuts-dialog.h"
#include "dialogs/gnome-cmd-make-copy-dialog.h"
#include "dialogs/gnome-cmd-manage-bookmarks-dialog.h"
#include "dialogs/gnome-cmd-mkdir-dialog.h"
#include "dialogs/gnome-cmd-search-dialog.h"
#include "dialogs/gnome-cmd-options-dialog.h"
#include "dialogs/gnome-cmd-prepare-copy-dialog.h"
#include "dialogs/gnome-cmd-prepare-move-dialog.h"

using namespace std;


inline GnomeCmdFileSelector *get_fs (const FileSelectorID fsID)
{
    return main_win->fs(fsID);
}


inline GnomeCmdFileList *get_fl (const FileSelectorID fsID)
{
    GnomeCmdFileSelector *fs = get_fs (fsID);

    return fs ? fs->file_list() : NULL;
}


// The file returned from this function is not to be unrefed
inline GnomeCmdFile *get_selected_file (const FileSelectorID fsID)
{
    GnomeCmdFile *f = get_fl (fsID)->get_first_selected_file();

    if (!f)
        gnome_cmd_show_message (*main_win, _("No file selected"));

    return f;
}


inline gboolean append_real_path (string &s, const gchar *name)
{
    s += ' ';
    s += name;

    return TRUE;
}


inline gboolean append_real_path (string &s, GnomeCmdFile *f)
{
    if (!f)
        return FALSE;

    gchar *name = g_shell_quote (f->get_real_path());

    append_real_path (s, name);

    g_free (name);

    return TRUE;
}


int parse_command(string *cmd, gchar * command);

GnomeCmdUserActions gcmd_user_actions;


inline bool operator < (const GdkEventKey &e1, const GdkEventKey &e2)
{
    if (e1.keyval < e2.keyval)
        return true;

    if (e1.keyval > e2.keyval)
        return false;

#if GTK_CHECK_VERSION (2, 10, 0)
    return (e1.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK)) < (e2.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));
#else
    return (e1.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK)) < (e2.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK));
#endif
}


DICT<GnomeCmdUserActionFunc> GnomeCmdUserActions::action_func;
DICT<GnomeCmdUserActionFunc> GnomeCmdUserActions::action_name;


struct UserActionData
{
    GnomeCmdUserActionFunc func;
    const gchar *name;
    const gchar *description;
};


static UserActionData user_actions_data[] = {
                                             {bookmarks_add_current, "bookmarks.add_current", N_("Bookmark current directory")},
                                             {bookmarks_edit, "bookmarks.edit", N_("Manage bookmarks")},
                                             {bookmarks_goto, "bookmarks.goto", N_("Go to bookmarked location")},
                                             {command_execute, "command.execute", N_("Execute command")},
                                             {command_open_nautilus, "command.open_folder", N_("Open folder")},
                                             {command_open_terminal, "command.open_terminal", N_("Open terminal")},
                                             {command_open_terminal_as_root, "command.open_terminal_as_root", N_("Open terminal as root")},
                                             {command_root_mode, "command.root_mode", N_("Start GNOME Commander as root")},
                                             {connections_close_current, "connections.close", N_("Close connection")},
                                             {connections_new, "connections.new", N_("New connection")},
                                             {connections_open, "connections.open", N_("Open connection")},
                                             {connections_change_left, "connections.change_left", N_("Change left connection")},
                                             {connections_change_right, "connections.change_right", N_("Change right connection")},
                                             {edit_cap_copy, "edit.copy", N_("Copy")},
                                             {edit_copy_fnames, "edit.copy_filenames", N_("Copy file names")},
                                             {edit_cap_cut, "edit.cut", N_("Cut")},
                                             {file_delete, "edit.delete", N_("Delete")},
                                             {edit_filter, "edit.filter", N_("Show user defined files")},
                                             {edit_cap_paste, "edit.paste", N_("Paste")},
                                             {edit_quick_search, "edit.quick_search", N_("Quick search")},
                                             {edit_search, "edit.search", N_("Search")},
                                             {file_advrename, "file.advrename", N_("Advanced rename tool")},
                                             {file_chmod, "file.chmod", N_("Change permissions")},
                                             {file_chown, "file.chown", N_("Change owner/group")},
                                             {file_copy, "file.copy", N_("Copy files")},
                                             {file_copy_as, "file.copy_as", N_("Copy files with rename")},
                                             {file_create_symlink, "file.create_symlink", N_("Create symbolic link")},
                                             {file_delete, "file.delete", N_("Delete files")},
                                             {file_diff, "file.diff", N_("Compare files (diff)")},
                                             {file_edit, "file.edit", N_("Edit file")},
                                             {file_edit_new_doc, "file.edit_new_doc", N_("Edit a new file")},
                                             {file_exit, "file.exit", N_("Quit")},
                                             {file_external_view, "file.external_view", N_("View with external viewer")},
                                             {file_internal_view, "file.internal_view", N_("View with internal viewer")},
                                             {file_mkdir, "file.mkdir", N_("Create directory")},
                                             {file_move, "file.move", N_("Move files")},
                                             {file_properties, "file.properties", N_("Properties")},
                                             {file_rename, "file.rename", N_("Rename files")},
                                             // {file_run, "file.run"},
                                             {file_sendto, "file.sendto", N_("Send files")},
                                             {file_sync_dirs, "file.synchronize_directories", N_("Synchronize directories")},
                                             // {file_umount, "file.umount"},
                                             {file_view, "file.view", N_("View file")},
                                             {help_about, "help.about", N_("About GNOME Commander")},
                                             {help_help, "help.help", N_("Help contents")},
                                             {help_keyboard, "help.keyboard", N_("Help on keyboard shortcuts")},
                                             {help_problem, "help.problem", N_("Report a problem")},
                                             {help_web, "help.web", N_("GNOME Commander on the web")},
                                             {mark_compare_directories, "mark.compare_directories", N_("Compare directories")},
                                             {mark_invert_selection, "mark.invert", N_("Invert selection")},
                                             {mark_select_all, "mark.select_all", N_("Select all")},
                                             {mark_toggle, "mark.toggle", N_("Toggle selection")},
                                             {mark_toggle_and_step, "mark.toggle_and_step", N_("Toggle selection and move cursor downward")},
                                             {mark_unselect_all, "mark.unselect_all", N_("Unselect all")},
                                             {no_action, "no.action", N_("Do nothing")},
                                             {options_edit, "options.edit", N_("Options")},
                                             {options_edit_shortcuts, "options.shortcuts", N_("Keyboard shortcuts")},
                                             {plugins_configure, "plugins.configure", N_("Configure plugins")},
                                             {plugins_execute_python, "plugins.execute_python", N_("Execute Python plugin")},
                                             {view_back, "view.back", N_("Back one directory")},
                                             {view_close_tab, "view.close_tab", N_("Close the current tab")},
                                             {view_close_all_tabs, "view.close_all_tabs", N_("Close all tabs")},
                                             {view_close_duplicate_tabs, "view.close_duplicate_tabs", N_("Close duplicate tabs")},
                                             {view_directory, "view.directory", N_("Change directory")},
                                             {view_dir_history, "view.dir_history", N_("Show directory history")},
                                             {view_equal_panes, "view.equal_panes", N_("Equal panel size")},
                                             {view_maximize_pane, "view.maximize_pane", N_("Maximize panel size")},
                                             {view_first, "view.first", N_("Back to the first directory")},
                                             {view_forward, "view.forward", N_("Forward one directory")},
                                             {view_home, "view.home", N_("Home directory")},
                                             {view_in_active_pane, "view.in_active_pane", N_("Open directory in the active window")},
                                             {view_in_inactive_pane, "view.in_inactive_pane", N_("Open directory in the inactive window")},
                                             {view_in_left_pane, "view.in_left_pane", N_("Open directory in the left window")},
                                             {view_in_right_pane, "view.in_right_pane", N_("Open directory in the right window")},
                                             {view_in_new_tab, "view.in_new_tab", N_("Open directory in the new tab")},
                                             {view_in_inactive_tab, "view.in_inactive_tab", N_("Open directory in the new tab (inactive window)")},
                                             {view_last, "view.last", N_("Forward to the last directory")},
                                             {view_next_tab, "view.next_tab", N_("Next tab")},
                                             {view_new_tab, "view.new_tab", N_("Open directory in a new tab")},
                                             {view_prev_tab, "view.prev_tab", N_("Previous tab")},
                                             {view_refresh, "view.refresh", N_("Refresh")},
                                             {view_root, "view.root", N_("Root directory")},
                                             {view_toggle_tab_lock, "view.toggle_lock_tab", N_("Lock/unlock tab")},
#if 0
                                             {view_terminal, "view.terminal", N_("Show terminal")},
#endif
                                             {view_up, "view.up", N_("Up one directory")},
                                             
											 {view_main_menu, "view.main.menu", N_("Display main menu")},
                                            };


void GnomeCmdUserActions::init()
{
    for (guint i=0; i<G_N_ELEMENTS(user_actions_data); ++i)
    {
        action_func.add(user_actions_data[i].func, user_actions_data[i].name);
        action_name.add(user_actions_data[i].func, _(user_actions_data[i].description));
    }

    register_action(GDK_F3, "file.view");
    register_action(GDK_F4, "file.edit");
    register_action(GDK_F5, "file.copy");
    register_action(GDK_F6, "file.rename");
    register_action(GDK_F7, "file.mkdir");
    register_action(GDK_F8, "file.delete");
    // register_action(GDK_F9, "edit.search");     //  do not register F9 here, as edit.search action wouldn't be checked for registration later
}


 void GnomeCmdUserActions::set_defaults()
 {
   if (!registered("bookmarks.edit"))
        register_action(GDK_CONTROL_MASK, GDK_D, "bookmarks.edit");

    if (!registered("connections.new"))
        register_action(GDK_CONTROL_MASK, GDK_N, "connections.new");

    if (!registered("connections.open"))
        register_action(GDK_CONTROL_MASK, GDK_F, "connections.open");

    if (!registered("connections.close"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_F, "connections.close");

    if (!registered("connections.change_left"))
    {
        register_action(GDK_MOD1_MASK, GDK_1, "connections.change_left");
#if GTK_CHECK_VERSION (2, 10, 0)
        register_action(GDK_SUPER_MASK, GDK_1, "connections.change_left");
#else
        register_action(GDK_MOD4_MASK, GDK_1, "connections.change_left");
#endif
    }

    if (!registered("connections.change_right"))
    {
        register_action(GDK_MOD1_MASK, GDK_2, "connections.change_right");
#if GTK_CHECK_VERSION (2, 10, 0)
        register_action(GDK_SUPER_MASK, GDK_2, "connections.change_right");
#else
        register_action(GDK_MOD4_MASK, GDK_2, "connections.change_right");
#endif
    }

    if (!registered("edit.copy_filenames"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_C, "edit.copy_filenames");

    if (!registered("edit.filter"))
        register_action(GDK_CONTROL_MASK, GDK_F12, "edit.filter");

    if (!registered("edit.search"))
    {
        register_action(GDK_MOD1_MASK, GDK_F7, "edit.search");
#if GTK_CHECK_VERSION (2, 10, 0)
        register_action(GDK_SUPER_MASK, GDK_F, "edit.search");
#else
        register_action(GDK_MOD4_MASK, GDK_F, "edit.search");
#endif
    }

    if (!registered("file.advrename"))
        register_action(GDK_CONTROL_MASK, GDK_M, "file.advrename");

    if (!registered("file.copy_as"))
        register_action(GDK_SHIFT_MASK, GDK_F5, "file.copy_as");

    if (!registered("file.create_symlink"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_F5, "file.create_symlink");

    if (!registered("file.edit_new_doc"))
        register_action(GDK_SHIFT_MASK, GDK_F4, "file.edit_new_doc");

    if (!registered("file.exit"))
        register_action(GDK_CONTROL_MASK, GDK_Q, "file.exit");

    if (!registered("file.external_view"))
        register_action(GDK_MOD1_MASK, GDK_F3, "file.external_view");

    if (!registered("file.internal_view"))
        register_action(GDK_SHIFT_MASK, GDK_F3, "file.internal_view");

    if (!registered("mark.compare_directories"))
        register_action(GDK_SHIFT_MASK, GDK_F2, "mark.compare_directories");

    if (!registered("mark.select_all"))
    {
        register_action(GDK_CONTROL_MASK, GDK_A, "mark.select_all");
        register_action(GDK_CONTROL_MASK, GDK_equal, "mark.select_all");
        register_action(GDK_CONTROL_MASK, GDK_KP_Add, "mark.select_all");
    }

    if (!registered("mark.unselect_all"))
    {
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_A, "mark.unselect_all");
        register_action(GDK_CONTROL_MASK, GDK_minus, "mark.unselect_all");
        register_action(GDK_CONTROL_MASK, GDK_KP_Subtract, "mark.unselect_all");
    }

    if (!registered("options.edit"))
        register_action(GDK_CONTROL_MASK, GDK_O, "options.edit");

    if (!registered("plugins.execute_python"))
    {
        register_action(GDK_CONTROL_MASK, GDK_5, "plugins.execute_python", "md5sum");
        register_action(GDK_CONTROL_MASK, GDK_KP_5, "plugins.execute_python", "md5sum");
        register_action(GDK_CONTROL_MASK, GDK_KP_Begin, "plugins.execute_python", "md5sum");
    }

    if (!registered("dir_history"))
    {
        register_action(GDK_MOD1_MASK, GDK_Down, "view.dir_history");
        register_action(GDK_MOD1_MASK, GDK_KP_Down, "view.dir_history");
    }

    if (!registered("view.up"))
    {
        register_action(GDK_CONTROL_MASK, GDK_Page_Up, "view.up");
        register_action(GDK_CONTROL_MASK, GDK_KP_Page_Up, "view.up");
    }

    if (!registered("view.equal_panes"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_plus, "view.equal_panes");

    if (!registered("view.in_active_pane"))
        register_action(GDK_CONTROL_MASK, GDK_period, "view.in_active_pane");

    if (!registered("view.in_inactive_pane"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_greater, "view.in_inactive_pane");

    if (!registered("view.in_left_pane"))
    {
        register_action(GDK_CONTROL_MASK, GDK_Left, "view.in_left_pane");
        register_action(GDK_CONTROL_MASK, GDK_KP_Left, "view.in_left_pane");
    }

    if (!registered("view.in_right_pane"))
    {
        register_action(GDK_CONTROL_MASK, GDK_Right, "view.in_right_pane");
        register_action(GDK_CONTROL_MASK, GDK_KP_Right, "view.in_right_pane");
    }

    if (!registered("view.in_new_tab"))
    {
        register_action(GDK_CONTROL_MASK, GDK_Up, "view.in_new_tab");
        register_action(GDK_CONTROL_MASK, GDK_KP_Up, "view.in_new_tab");
    }

    if (!registered("view.in_inactive_tab"))
    {
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_Up, "view.in_inactive_tab");
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_KP_Up, "view.in_inactive_tab");
    }

    if (!registered("view.directory"))
    {
        register_action(GDK_CONTROL_MASK, GDK_Page_Down, "view.directory");
        register_action(GDK_CONTROL_MASK, GDK_KP_Page_Down, "view.directory");
    }

    if (!registered("view.home"))
    {
        register_action(GDK_CONTROL_MASK, GDK_quoteleft, "view.home");
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_asciitilde, "view.home");
    }

    if (!registered("view.root"))
        register_action(GDK_CONTROL_MASK, GDK_backslash, "view.root");

    if (!registered("view.refresh"))
        register_action(GDK_CONTROL_MASK, GDK_R, "view.refresh");

    if (!registered("view.new_tab"))
    {
        unregister(GDK_CONTROL_MASK, GDK_T);                       // unregister CTRL+T as it was used previously for file.advrename
        register_action(GDK_CONTROL_MASK, GDK_T, "view.new_tab");
    }

    if (!registered("view.close_tab"))
        register_action(GDK_CONTROL_MASK, GDK_W, "view.close_tab");

    if (!registered("view.close_all_tabs"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_W, "view.close_all_tabs");

    unregister(GDK_F9);                                 // unregister F9 if defined in [key-bindings]
    register_action(GDK_F9, "edit.search");             // and overwrite it with edit.search action
 }


void GnomeCmdUserActions::shutdown()
{
    // unregister Fn to avoid writing 'hardcoded' Fn actions to config file

    unregister(GDK_F3);
    unregister(GDK_F4);
    unregister(GDK_F5);
    unregister(GDK_F6);
    unregister(GDK_F7);
    unregister(GDK_F8);
    unregister(GDK_F9);
}


void GnomeCmdUserActions::load(const gchar *section)
{
    string section_path = G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S;
           section_path += section;
           section_path += G_DIR_SEPARATOR;

    char *key = NULL;
    char *action_name = NULL;

    for (gpointer i=gnome_config_init_iterator(section_path.c_str()); (i=gnome_config_iterator_next(i, &key, &action_name)); )
    {
        DEBUG('u',"[%s]\t%s=%s\n", section, key, action_name);

        char *action_options = strchr(action_name, '|');

        if (action_options)
        {
            *action_options = '\0';
            g_strstrip (++action_options);
        }

        gchar *action_name__lowercase = g_ascii_strdown (g_strstrip (action_name), -1);

        if (action_func[action_name__lowercase])
        {
            guint keyval;
            guint state;

            str2key(key, state, keyval);

            if (keyval!=GDK_VoidSymbol)
                register_action(state, keyval, action_name__lowercase, action_options);
            else
                g_warning ("[%s] invalid key name: '%s' - ignored", section, key);
        }
        else
            g_warning ("[%s] unknown user action: '%s' - ignored", section, action_name__lowercase);

        g_free (key);
        g_free (action_name);
        g_free (action_name__lowercase);
    }
}


gboolean GnomeCmdUserActions::register_action(guint state, guint keyval, const gchar *name, const char *user_data)
{
    GnomeCmdUserActionFunc func = action_func[name];

    if (!func)
        return FALSE;

    GdkEventKey event;

    event.keyval = keyval;
#if GTK_CHECK_VERSION (2, 10, 0)
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);
#else
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);
#endif
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


void GnomeCmdUserActions::unregister(const gchar *name)
{
}


void GnomeCmdUserActions::unregister(guint state, guint keyval)
{
    GdkEventKey event;

    event.keyval = keyval;
#if GTK_CHECK_VERSION (2, 10, 0)
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);
#else
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);
#endif
    map <GdkEventKey, UserAction>::iterator pos = action.find(event);

    if (pos!=action.end())
        action.erase(pos);
}


gboolean GnomeCmdUserActions::registered(const gchar *name)
{
    GnomeCmdUserActionFunc func = action_func[name];

    if (!func)
        return FALSE;

    for (ACTIONS_COLL::const_iterator i=action.begin(); i!=action.end(); ++i)
        if (i->second.func==func)
            return TRUE;

    return FALSE;
}


gboolean GnomeCmdUserActions::registered(guint state, guint keyval)
{
    GdkEventKey event;

    event.keyval = keyval;
#if GTK_CHECK_VERSION (2, 10, 0)
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK);
#else
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);
#endif

    return action.find(event)!=action.end();
}


gboolean GnomeCmdUserActions::handle_key_event(GnomeCmdMainWin *mw, GnomeCmdFileList *fl, GdkEventKey *event)
{
    map <GdkEventKey, UserAction>::const_iterator pos = action.find(*event);

    if (pos==action.end())
        return FALSE;

    DEBUG('u', "Key event:  %s (%#x)\n", key2str(*event).c_str(), event->keyval);
    DEBUG('u', "Handling key event by %s()\n", action_func[pos->second.func].c_str());

    (*pos->second.func) (NULL, (gpointer) (pos->second.user_data.empty() ? NULL : pos->second.user_data.c_str()));

    return TRUE;
}

XML::xstream &operator << (XML::xstream &xml, GnomeCmdUserActions &usr)
{
    xml << XML::tag("KeyBindings");
    for (GnomeCmdUserActions::ACTIONS_COLL::const_iterator i=usr.action.begin(); i!=usr.action.end(); ++i)
        if (!ascii_isupper (i->first))                                         // ignore lowercase keys as they duplicate uppercase ones
        {
            guint state = i->first.state;
            guint key_val = i->first.keyval;

            xml << XML::tag("Key") << XML::attr("name");
            if (ascii_isalnum (key_val))
                xml << (gchar) key_val;
            else
                xml << gdk_key_names[key_val];

            if (state & GDK_SHIFT_MASK)    xml << XML::attr("shift") << 1;
            if (state & GDK_CONTROL_MASK)  xml << XML::attr("control") << 1;
            if (state & GDK_MOD1_MASK)     xml << XML::attr("alt") << 1;
#if GTK_CHECK_VERSION (2, 10, 0)
            if (state & GDK_SUPER_MASK)    xml << XML::attr("super") << 1;
            if (state & GDK_HYPER_MASK)    xml << XML::attr("hyper") << 1;
            if (state & GDK_META_MASK)     xml << XML::attr("meta") << 1;
#else
            if (state & GDK_MOD4_MASK)     xml << XML::attr("super") << 1;
#endif

            xml << XML::attr("action") << usr.action_func[i->second.func];
            if (!i->second.user_data.empty())
                xml << XML::attr("options") << XML::escape(i->second.user_data);

            xml << XML::endtag();
        }

    xml << XML::endtag();

    return xml;
}


static int sort_by_description (const void *data1, const void *data2)
{
    const gchar *s1 = ((UserActionData *) data1)->description;
    const gchar *s2 = ((UserActionData *) data2)->description;

    if (!s1 && !s2)
        return 0;

    if (!s1)
        return 1;

    if (!s2)
        return -1;

    // compare s1 and s2 in UTF8 aware way, case insensitive
    gchar *is1 = g_utf8_casefold (_(s1), -1);
    gchar *is2 = g_utf8_casefold (_(s2), -1);

    gint retval = g_utf8_collate (is1, is2);

    g_free (is1);
    g_free (is2);

    return retval;
}


GtkTreeModel *gnome_cmd_user_actions_create_model ()
{
    UserActionData *data = (UserActionData *) g_memdup (user_actions_data, sizeof(user_actions_data));

    qsort (data, G_N_ELEMENTS(user_actions_data), sizeof(UserActionData), sort_by_description);

    GtkListStore *model = gtk_list_store_new (3, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING);
    GtkTreeIter iter;

    for (guint i=0; i<G_N_ELEMENTS(user_actions_data); ++i)
    {
        gtk_list_store_append (model, &iter);

        gtk_list_store_set (model, &iter,
                                   0, data[i].func,
                                   1, data[i].name,
                                   2, _(data[i].description),
                                   -1);
    }

    g_free (data);

    return GTK_TREE_MODEL (model);
}


template <typename F>
inline void get_file_list (string &s, GList *sfl, F f)
{
    vector<string> a;

    for (GList *i = sfl; i; i = i->next)
        a.push_back ((*f) (GNOME_CMD_FILE (i->data)));

    join (s, a.begin(), a.end());
}


template <typename F, typename T>
inline void get_file_list (string &s, GList *sfl, F f, T t)
{
    vector<string> a;

    for (GList *i = sfl; i; i = i->next)
        a.push_back ((*f) (GNOME_CMD_FILE (i->data), t));

    join (s, a.begin(), a.end());
}


/***************************************/
void no_action (GtkMenuItem *menuitem, gpointer not_used)
{
}


/************** File Menu **************/
void file_copy (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *src_fs = get_fs (ACTIVE);
    GnomeCmdFileSelector *dest_fs = get_fs (INACTIVE);

    if (src_fs && dest_fs)
        gnome_cmd_prepare_copy_dialog_show (src_fs, dest_fs);
}


void file_copy_as (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GnomeCmdFile *f = fs->file_list()->get_selected_file();

    if (GNOME_CMD_IS_FILE (f))
    {
        GtkWidget *dialog = gnome_cmd_make_copy_dialog_new (f, fs->get_directory());

        g_object_ref (dialog);
        gtk_widget_show (dialog);
    }
}


void file_move (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *src_fs = get_fs (ACTIVE);
    GnomeCmdFileSelector *dest_fs = get_fs (INACTIVE);

    if (src_fs && dest_fs)
        gnome_cmd_prepare_move_dialog_show (src_fs, dest_fs);
}


void file_delete (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_delete_dialog (get_fl (ACTIVE));
}


void file_view (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_view (get_fl (ACTIVE), -1);
}


void file_internal_view (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_view (get_fl (ACTIVE), TRUE);
}


void file_external_view (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_view (get_fl (ACTIVE), FALSE);
}

void file_edit (GtkMenuItem *menuitem, gpointer not_used)
{
    GdkModifierType mask;

    gdk_window_get_pointer (NULL, NULL, NULL, &mask);

    if (mask & GDK_SHIFT_MASK)
        gnome_cmd_file_selector_show_new_textfile_dialog (get_fs (ACTIVE));
    else
    {
	GnomeCmdDir *dir = NULL;
	gchar       *command;

	command = g_strdup (gnome_cmd_data.options.editor);
	g_return_if_fail (command != NULL);

	string dir_path;
	string cmd;
	
	cmd.reserve(2000);
	if (parse_command(&cmd, command) == 0)
	{
	    DEBUG ('g', "Edit file command is not valid.\n");
	    gnome_cmd_show_message (*main_win, _("No valid command given."));
	    return;
	}
	else
	{
	    gint     argc;
	    gchar  **argv  = NULL;
	    GError  *error = NULL;
	    DEBUG ('g', "Edit file: %s\n", cmd.c_str());
	    
	    g_shell_parse_argv (cmd.c_str(), &argc, &argv, NULL);
	    if (!g_spawn_async (gnome_cmd_dir_is_local (dir) ? dir_path.c_str() : NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error))
		gnome_cmd_error_message (_("Unable to execute command."), error);
	    
	    g_strfreev (argv);
	    g_free (command);
	}
    }
}

void file_edit_new_doc (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_show_new_textfile_dialog (get_fs (ACTIVE));
}


void file_chmod (GtkMenuItem *menuitem, gpointer not_used)
{
    GList *files = get_fl (ACTIVE)->get_selected_files();

    if (files)
    {
        GtkWidget *dialog = gnome_cmd_chmod_dialog_new (files);

        g_object_ref (dialog);
        gtk_widget_show (dialog);
        g_list_free (files);
    }
}


void file_chown (GtkMenuItem *menuitem, gpointer not_used)
{
    GList *files = get_fl (ACTIVE)->get_selected_files();

    if (files)
    {
        GtkWidget *dialog = gnome_cmd_chown_dialog_new (files);

        g_object_ref (dialog);
        gtk_widget_show (dialog);
        g_list_free (files);
    }
}


void file_mkdir (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GnomeCmdDir *dir = fs->get_directory();

    g_return_if_fail (GNOME_CMD_IS_DIR (dir));

    gnome_cmd_dir_ref (dir);
    gnome_cmd_mkdir_dialog_new (dir, fs->file_list()->get_selected_file());
    gnome_cmd_dir_unref (dir);
}


void file_create_symlink (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *inactive_fs = get_fs (INACTIVE);
    GList *f = get_fl (ACTIVE)->get_selected_files();
    guint selected_files = g_list_length (f);

    if (selected_files > 1)
    {
        gchar *msg = g_strdup_printf (ngettext("Create symbolic links of %i file in %s?",
                                               "Create symbolic links of %i files in %s?",
                                               selected_files),
                                      selected_files, gnome_cmd_dir_get_display_path (inactive_fs->get_directory()));

        gint choice = run_simple_dialog (*main_win, TRUE, GTK_MESSAGE_QUESTION, msg, _("Create Symbolic Link"), 1, _("Cancel"), _("Create"), NULL);

        g_free (msg);

        if (choice==1)
            gnome_cmd_file_selector_create_symlinks (inactive_fs, f);
    }
   else
   {
        GnomeCmdFile *f = get_fl (ACTIVE)->get_focused_file();
        gnome_cmd_file_selector_create_symlink (inactive_fs, f);
   }
}


void file_rename (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_rename_dialog (get_fl (ACTIVE));
}


void file_advrename (GtkMenuItem *menuitem, gpointer not_used)
{
    GList *files = get_fl (ACTIVE)->get_selected_files();

    if (files)
    {
        files = get_fl (ACTIVE)->sort_selection(files);

        if (!main_win->advrename_dlg)
        {
            main_win->advrename_dlg = new GnomeCmdAdvrenameDialog(gnome_cmd_data.advrename_defaults);
            // g_object_ref (main_win->advrename_dlg);      //  FIXME:  ???
            main_win->advrename_dlg->set(files);
            gtk_widget_show_all (*main_win->advrename_dlg);
        }
        else
        {
            main_win->advrename_dlg->set(files);
            gtk_widget_show (*main_win->advrename_dlg);
            // gdk_window_raise (GTK_WIDGET (main_win->advrename_dlg)->window);     //  FIXME:  bring dlg to top ???
        }

        g_list_free (files);
    }
}


void file_sendto (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdDir  *dir     = NULL;
    gchar        *command = NULL;
    string        dir_path;
    string        cmd;

    command = g_strdup (gnome_cmd_data.options.sendto);
    g_return_if_fail (command != NULL);
    
    cmd.reserve(2000);

    if (parse_command(&cmd, g_strdup(gnome_cmd_data.options.sendto)) == 0)
    {
	DEBUG ('g', "Sendto command is not valid.\n");
	gnome_cmd_show_message (*main_win, _("No valid command given."));
	return;
    }
    else
    {
	gint     argc;
	gchar  **argv  = NULL;
	GError  *error = NULL;
	DEBUG ('g', "Invoking 'Send files': %s\n", cmd.c_str());
	g_shell_parse_argv (cmd.c_str(), &argc, &argv, NULL);
	if (!g_spawn_async (gnome_cmd_dir_is_local (dir) ? dir_path.c_str() : NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error))
	    gnome_cmd_error_message (_("Unable to execute command."), error);
	g_free (command);
	g_strfreev (argv);
    }
}


void file_properties (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_properties_dialog (get_fl (ACTIVE));
}


void file_diff (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!get_fs (ACTIVE)->is_local())
    {
        gnome_cmd_show_message (*main_win, _("Operation not supported on remote file systems"));
        return;
    }

    GnomeCmdFileList *active_fl = get_fl (ACTIVE);

    GList *sel_files = active_fl->get_selected_files();

    string s;

    switch (g_list_length (sel_files))
    {
        case 0:
            return;

        case 1:
            if (!get_fs (INACTIVE)->is_local())
                gnome_cmd_show_message (*main_win, _("Operation not supported on remote file systems"));
            else
                if (!append_real_path (s, get_selected_file (ACTIVE)) || !append_real_path (s, get_selected_file (INACTIVE)))
                    s.clear();
            break;

        case 2:
        case 3:
            sel_files = active_fl->sort_selection(sel_files);

            for (GList *i = sel_files; i; i = i->next)
                append_real_path (s, GNOME_CMD_FILE (i->data));
            break;

        default:
            gnome_cmd_show_message (*main_win, _("Too many selected files"));
            break;
    }

    g_list_free (sel_files);

    if (!s.empty())
    {
        gchar *cmd = g_strdup_printf (gnome_cmd_data.options.differ, s.c_str(), "");

        run_command (cmd);

        g_free (cmd);
    }
}


void file_sync_dirs (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *active_fs = get_fs (ACTIVE);
    GnomeCmdFileSelector *inactive_fs = get_fs (INACTIVE);

    if (!active_fs->is_local() || !inactive_fs->is_local())
    {
        gnome_cmd_show_message (*main_win, _("Operation not supported on remote file systems"));
        return;
    }

    string s;

    append_real_path (s, GNOME_CMD_FILE (active_fs->get_directory()));
    append_real_path (s, GNOME_CMD_FILE (inactive_fs->get_directory()));

    gchar *cmd = g_strdup_printf (gnome_cmd_data.options.differ, s.c_str(), "");

    run_command (cmd);

    g_free (cmd);
}


void file_exit (GtkMenuItem *menuitem, gpointer not_used)
{
    gint x, y;

    switch (gnome_cmd_data.main_win_state)
    {
        case GDK_WINDOW_STATE_MAXIMIZED:
        case GDK_WINDOW_STATE_FULLSCREEN:
        case GDK_WINDOW_STATE_ICONIFIED:
            break;

        default:
            gdk_window_get_root_origin (GTK_WIDGET (main_win)->window, &x, &y);
            gnome_cmd_data_set_main_win_pos (x, y);
            break;
    }

    gtk_widget_destroy (*main_win);
}


/************** Edit Menu **************/
void edit_cap_cut (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_cap_cut (get_fl (ACTIVE));
}


void edit_cap_copy (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_cap_copy (get_fl (ACTIVE));
}


void edit_cap_paste (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_cap_paste (get_fs (ACTIVE));
}


void edit_search (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!main_win->file_search_dlg)
        main_win->file_search_dlg = new GnomeCmdSearchDialog(gnome_cmd_data.search_defaults);

    main_win->file_search_dlg->show_and_set_focus();
}


void edit_quick_search (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_quicksearch (get_fl (ACTIVE), 0);
}


void edit_filter (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fs (ACTIVE)->show_filter();
}


void edit_copy_fnames (GtkMenuItem *menuitem, gpointer not_used)
{
    GdkModifierType mask;

    gdk_window_get_pointer (NULL, NULL, NULL, &mask);

    GnomeCmdFileList *fl = get_fl (ACTIVE);
    GList *sfl = fl->get_selected_files();
    sfl = fl->sort_selection(sfl);

    string fnames;

    fnames.reserve(2000);

    if (state_is_blank (mask))
        get_file_list (fnames, sfl, gnome_cmd_file_get_name);
    else
        if (state_is_shift (mask))
            get_file_list (fnames, sfl, gnome_cmd_file_get_real_path);
        else
            if (state_is_alt (mask))
                get_file_list (fnames, sfl, gnome_cmd_file_get_uri_str, GNOME_VFS_URI_HIDE_PASSWORD);

    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), fnames.c_str(), -1);

    g_list_free (sfl);
}


/************** Command Menu **************/
void command_execute (GtkMenuItem *menuitem, gpointer command)
{
    g_return_if_fail (command != NULL);

    DEBUG ('g', "invoking: %s\n", command);

    string dir_path;
    string quoted_dir_path;
    string cmd;

    GnomeCmdDir *dir = NULL;

    GnomeCmdFileList *fl = get_fl (ACTIVE);
    GList *sfl = fl->get_selected_files();
    GList *i = sfl = fl->sort_selection(sfl);

    if (i)
    {
        dir = GNOME_CMD_FILE (i->data)->get_parent_dir();
        i = i->next;
    }

    for (; i && GNOME_CMD_FILE (i->data)->get_parent_dir()==dir; i=i->next);

    if (i)
        dir = NULL;

    if (dir)
    {
        stringify (dir_path, GNOME_CMD_FILE (dir)->get_real_path());
        stringify (quoted_dir_path, gnome_cmd_file_get_quoted_real_path (GNOME_CMD_FILE (dir)));
    }

    cmd.reserve(2000);
    if (parse_command(&cmd, (gchar*) command) == 0)
    {
	DEBUG ('g', "Command is not valid.\n");
	gnome_cmd_show_message (*main_win, _("No valid command given."));
	return;
    }
    else
    {
	gint argc;
	gchar **argv;
	GError *error = NULL;
	
	DEBUG ('g', "Running: %s\n", cmd.c_str());

	g_shell_parse_argv (cmd.c_str(), &argc, &argv, NULL);
	if (!g_spawn_async (gnome_cmd_dir_is_local (dir) ? dir_path.c_str() : NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error))
	    gnome_cmd_error_message (_("Unable to execute command."), error);

	g_strfreev (argv);
	g_list_free (sfl);
    }
}


void command_open_terminal__internal (GtkMenuItem *menuitem, gpointer not_used)           // this function is NOT exposed to user as UserAction
{
    GdkModifierType mask;

    gdk_window_get_pointer (NULL, NULL, NULL, &mask);

    if (mask & GDK_SHIFT_MASK)
        command_open_terminal_as_root (menuitem, NULL);
    else
        command_open_terminal (menuitem, NULL);
}

/** 
 * Executes the command stored in gnome_cmd_data.options.termopen in the
 * active directory.
 */
void command_open_terminal (GtkMenuItem *menuitem, gpointer not_used)
{
    gint argc;
    gchar **argv;
    gchar *command;
    gchar *dpath = GNOME_CMD_FILE (get_fs (ACTIVE)->get_directory())->get_real_path();
    GError *error = NULL;

    command = g_strdup (gnome_cmd_data.options.termopen);
    g_return_if_fail (command[0] != '\0');

    DEBUG ('g', "running: %s\n", command);

    g_shell_parse_argv (command, &argc, &argv, NULL);
    if (!g_spawn_async (dpath, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error))
        gnome_cmd_error_message (_("Unable to execute command."), error);

    g_strfreev (argv);
    g_free (command);
    g_free (dpath);
}

/** 
 * Combines the command stored in gnome_cmd_data.options.termopen with a
 * command for a GUI for root login screen. The command is executed in
 * the active directory afterwards.
 */
void command_open_terminal_as_root (GtkMenuItem *menuitem, gpointer not_used)
{
    
    int argc = 1;
    char **argv = g_new0 (char *, argc+1);  //initialize argv
    gchar *command;

    command = g_strdup (gnome_cmd_data.options.termopen);
    // Parse the termopen string to get the number of arguments in argc
    g_shell_parse_argv (command, &argc, &argv, NULL);
    g_free (argv);

    argv = g_new0 (char *, argc+1);
    argv[0] = command;

    if (gnome_cmd_prepend_su_to_vector (argc, argv))
    {
        gchar *dpath = GNOME_CMD_FILE (get_fs (ACTIVE)->get_directory())->get_real_path();
        GError *error = NULL;

        if (!g_spawn_async (dpath, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL, NULL, NULL, NULL, &error))
            gnome_cmd_error_message (_("Unable to open terminal in root mode."), error);

        g_free (dpath);
    }
    else
        gnome_cmd_show_message (NULL, _("xdg-su, gksu, gnomesu, kdesu or beesu is not found."));

    g_free (argv);
    g_free (command);
}


inline void open_uri_in_nautilus (gchar *uri)
{
    if (!uri)
        return;

    char const *argv[5];

    argv[0] = "nautilus";
    argv[1] = "--no-desktop";
    argv[2] = "--no-default-window";
    argv[3] = uri;
    argv[4] = NULL;

    GError *error = NULL;

    if (!g_spawn_async (NULL, (char **) argv, NULL, GSpawnFlags (G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL), NULL, NULL, NULL, &error))
        gnome_cmd_error_message (_("Unable to start Nautilus."), error);

    g_free (uri);
}


void command_open_nautilus (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFile *f = get_fl (ACTIVE)->get_selected_file();

    open_uri_in_nautilus ((GNOME_CMD_IS_DIR (f) ? f : GNOME_CMD_FILE (get_fs (ACTIVE)->get_directory()))->get_uri_str());
}


void command_open_nautilus_in_cwd (GtkMenuItem *menuitem, gpointer not_used)
{
    open_uri_in_nautilus (GNOME_CMD_FILE (get_fs (ACTIVE)->get_directory())->get_uri_str());
}


void command_root_mode (GtkMenuItem *menuitem, gpointer not_used)
{
    int argc = 1;
    char **argv = g_new0 (char *, argc+1);

    argv[0] = g_strdup (g_get_prgname ());

    if (gnome_cmd_prepend_su_to_vector (argc, argv))
    {
        GError *error = NULL;

        if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL, NULL, NULL, NULL, &error))
            gnome_cmd_error_message (_("Unable to start GNOME Commander in root mode."), error);
    }
    else
        gnome_cmd_show_message (NULL, _("xdg-su, gksu, gnomesu, kdesu or beesu is not found"));

    g_strfreev (argv);
}


/************** Mark Menu **************/
void mark_toggle (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->toggle();
}


void mark_toggle_and_step (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->toggle_and_step();
}


void mark_select_all (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->select_all();
}


void mark_unselect_all (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->unselect_all();
}


void mark_select_with_pattern (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_selpat_dialog (get_fl (ACTIVE), TRUE);
}


void mark_unselect_with_pattern (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_selpat_dialog (get_fl (ACTIVE), FALSE);
}


void mark_invert_selection (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->invert_selection();
}


void mark_select_all_with_same_extension (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->select_all_with_same_extension();
}


void mark_unselect_all_with_same_extension (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->unselect_all_with_same_extension();
}


void mark_restore_selection (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fl (ACTIVE)->restore_selection();
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


void mark_compare_directories (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileList *fl1 = get_fl (ACTIVE);
    GnomeCmdFileList *fl2 = get_fl (INACTIVE);

    map<const char *,GnomeCmdFile *,ltstr> files2;          //  map (fname -> GnomeCmdFile *) of visible files (non dirs!) in fl2

    for (GList *i2=fl2->get_visible_files(); i2; i2=i2->next)
    {
        GnomeCmdFile *f2 = (GnomeCmdFile *) i2->data;

        if (!f2->is_dotdot && f2->info->type!=GNOME_VFS_FILE_TYPE_DIRECTORY)
            files2[f2->get_name()] = f2;
    }

    set<GnomeCmdFile *> new_selection1, new_selection2;

    for (GList *i1=fl1->get_visible_files(); i1; i1=i1->next)
    {
        GnomeCmdFile *f1 = (GnomeCmdFile *) i1->data;

        if (f1->is_dotdot || f1->info->type==GNOME_VFS_FILE_TYPE_DIRECTORY)
            continue;

        map<const char *,GnomeCmdFile *,ltstr>::iterator i2 = files2.find(f1->get_name());

        if (i2==files2.end())                           //  if f1 is not found in visible fl2 files...
        {
            new_selection1.insert(f1);                  //  ... add f1 to files to be selected in fl1
            continue;
        }

        GnomeCmdFile *f2 = i2->second;

        if (f1->info->mtime > f2->info->mtime)          //  if f1 is newer than f2...
        {
            new_selection1.insert(f1);                  //  ... add f1 to files to be selected in fl1...
            files2.erase(i2);                           //  ... and remove f2 from fl2 files
            continue;
        }

        if (f1->info->mtime == f2->info->mtime)         //  if the same mtime for f1, f2
        {
            if (f1->info->size == f2->info->size)       //  ... then check f1, f2 sizes
                files2.erase(i2);                       //  ... and remove f2 from fl2 files
            else
                new_selection1.insert(f1);              //  ... or add f1 to files to be selected in fl1
        }
    }

    transform (files2.begin(), files2.end(), inserter(new_selection2,new_selection2.begin()), make_select2nd(files2));    // copy left files2 --> new_selection2

    selection_delta (*fl1, fl1->get_marked_files(), new_selection1);
    selection_delta (*fl2, fl2->get_marked_files(), new_selection2);
}

/************** View Menu **************/

void view_conbuttons (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data.conbuttons_visibility = checkitem->active;
    get_fs (ACTIVE)->update_conbuttons_visibility();
    get_fs (INACTIVE)->update_conbuttons_visibility();
}


void view_concombo (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data.concombo_visibility = checkitem->active;
    get_fs (ACTIVE)->update_concombo_visibility();
    get_fs (INACTIVE)->update_concombo_visibility();
}


void view_toolbar (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data.toolbar_visibility = checkitem->active;
    main_win->update_toolbar_visibility();
}


void view_buttonbar (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data.buttonbar_visibility = checkitem->active;
    main_win->update_buttonbar_visibility();
}


void view_cmdline (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data.cmdline_visibility = checkitem->active;
    main_win->update_cmdline_visibility();
}


void view_dir_history (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_dir_indicator_show_history (GNOME_CMD_DIR_INDICATOR (get_fs (ACTIVE)->dir_indicator));
}


void view_hidden_files (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data.options.filter.hidden = !checkitem->active;
    get_fl (ACTIVE)->reload();
    get_fl (INACTIVE)->reload();
}


void view_backup_files (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data.options.filter.backup = !checkitem->active;
    get_fl (ACTIVE)->reload();
    get_fl (INACTIVE)->reload();
}


void view_up (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    if (fl->locked)
        fs->new_tab(gnome_cmd_dir_get_parent (fl->cwd));
    else
        fs->goto_directory("..");
}

void view_main_menu (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    gnome_cmd_data.mainmenu_visibility = !gnome_cmd_data.mainmenu_visibility;
    main_win->update_mainmenu_visibility();
}

void view_first (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fs (ACTIVE)->first();
}


void view_back (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fs (ACTIVE)->back();
}


void view_forward (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fs (ACTIVE)->forward();
}


void view_last (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fs (ACTIVE)->last();
}


void view_refresh (GtkMenuItem *menuitem, gpointer file_list)
{
    GnomeCmdFileList *fl = file_list ? GNOME_CMD_FILE_LIST (file_list) : get_fl (ACTIVE);
    fl->reload();
}


void view_equal_panes (GtkMenuItem *menuitem, gpointer not_used)
{
    main_win->set_equal_panes();
}


void view_maximize_pane (GtkMenuItem *menuitem, gpointer not_used)
{
    main_win->maximize_pane();
}


void view_in_left_pane (GtkMenuItem *menuitem, gpointer not_used)
{
    main_win->set_fs_directory_to_opposite(LEFT);
}


void view_in_right_pane (GtkMenuItem *menuitem, gpointer not_used)
{
    main_win->set_fs_directory_to_opposite(RIGHT);
}


void view_in_active_pane (GtkMenuItem *menuitem, gpointer not_used)
{
    main_win->set_fs_directory_to_opposite(ACTIVE);
}


void view_in_inactive_pane (GtkMenuItem *menuitem, gpointer not_used)
{
    main_win->set_fs_directory_to_opposite(INACTIVE);
}


void view_directory (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    GnomeCmdFile *f = fl->get_selected_file();
    if (f && f->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        fs->do_file_specific_action (fl, f);
}


void view_home (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    if (!fl->locked)
    {
        fl->set_connection(get_home_con ());
        fl->goto_directory("~");
    }
    else
        fs->new_tab(gnome_cmd_dir_new (get_home_con (), gnome_cmd_con_create_path (get_home_con (), g_get_home_dir ())));
}


void view_root (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GnomeCmdFileList *fl = fs->file_list();

    if (fl->locked)
        fs->new_tab(gnome_cmd_dir_new (fl->con, gnome_cmd_con_create_path (fl->con, "/")));
    else
        fs->goto_directory("/");
}


void view_new_tab (GtkMenuItem *menuitem, gpointer file_list)
{
    GnomeCmdFileList *fl = file_list ? GNOME_CMD_FILE_LIST (file_list) : get_fl (ACTIVE);
    GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (gtk_widget_get_ancestor (*fl, GNOME_CMD_TYPE_FILE_SELECTOR));
    fs->new_tab(fl->cwd);
}


void view_close_tab (GtkMenuItem *menuitem, gpointer file_list)
{
    if (file_list)
    {
        GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (file_list);
        GnomeCmdFileSelector *fs = GNOME_CMD_FILE_SELECTOR (gtk_widget_get_ancestor (*fl, GNOME_CMD_TYPE_FILE_SELECTOR));

        if (fs->notebook->size()>1)
            if (!fl->locked || gnome_cmd_prompt_message (*main_win, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, _("The tab is locked, close anyway?"))==GTK_RESPONSE_OK)
                fs->close_tab(gtk_notebook_page_num (*fs->notebook, gtk_widget_get_parent (*fl)));
    }
    else
    {
        GnomeCmdFileSelector *fs = get_fs (ACTIVE);

        if (fs->notebook->size()>1)
            if (!fs->file_list()->locked || gnome_cmd_prompt_message (*main_win, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, _("The tab is locked, close anyway?"))==GTK_RESPONSE_OK)
                fs->close_tab();
    }
}


void view_close_all_tabs (GtkMenuItem *menuitem, gpointer file_selector)
{
    GnomeCmdFileSelector *fs = file_selector ? GNOME_CMD_FILE_SELECTOR (file_selector) : get_fs (ACTIVE);
    GnomeCmdNotebook *notebook = fs->notebook;
    gint n = notebook->get_current_page();

    for (gint i=notebook->size(); i--;)
        if (i!=n && !fs->file_list(i)->locked)
            notebook->remove_page(i);
}


void view_close_duplicate_tabs (GtkMenuItem *menuitem, gpointer file_selector)
{
    GnomeCmdFileSelector *fs = file_selector ? GNOME_CMD_FILE_SELECTOR (file_selector) : get_fs (ACTIVE);
    GnomeCmdNotebook *notebook = fs->notebook;

    typedef set<gint> TABS_COLL;
    typedef map <GnomeCmdDir *, TABS_COLL> DIRS_COLL;

    DIRS_COLL dirs;

    for (gint i=notebook->size(); i--;)
    {
        GnomeCmdFileList *fl = GNOME_CMD_FILE_LIST (gtk_bin_get_child (GTK_BIN (notebook->page(i))));

        if (fl && !fl->locked)
            dirs[fl->cwd].insert(i);
    }

    TABS_COLL duplicates;

    DIRS_COLL::iterator pos = dirs.find(fs->get_directory());       //  find tabs with the current dir...

    if (pos!=dirs.end())
    {
        duplicates.swap(pos->second);                               //  ... and mark them as to be closed...
        duplicates.erase(notebook->get_current_page());             //  ... but WITHOUT the current one
        dirs.erase(pos);
    }

    for (DIRS_COLL::const_iterator i=dirs.begin(); i!=dirs.end(); ++i)
    {
        TABS_COLL::iterator beg = i->second.begin();
        copy(++beg, i->second.end(), inserter(duplicates, duplicates.begin()));   //  for every dir, leave the first tab opened
    }

    for (TABS_COLL::const_reverse_iterator i=duplicates.rbegin(); i!=duplicates.rend(); ++i)
        notebook->remove_page(*i);
}


void view_prev_tab (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fs (ACTIVE)->notebook->prev_page();
}


void view_next_tab (GtkMenuItem *menuitem, gpointer not_used)
{
    get_fs (ACTIVE)->notebook->next_page();
}


void view_in_new_tab (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GnomeCmdFile *file = fs->file_list()->get_selected_file();

    if (file && file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        fs->new_tab(GNOME_CMD_DIR (file), FALSE);
    else
        fs->new_tab(fs->get_directory(), FALSE);
}


void view_in_inactive_tab (GtkMenuItem *menuitem, gpointer file_list)
{
    GnomeCmdFileList *fl = file_list ? GNOME_CMD_FILE_LIST (file_list) : get_fl (ACTIVE);
    GnomeCmdFile *file = fl->get_selected_file();

    if (file && file->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
        get_fs (INACTIVE)->new_tab(GNOME_CMD_DIR (file), FALSE);
    else
        get_fs (INACTIVE)->new_tab(fl->cwd, FALSE);
}


void view_toggle_tab_lock (GtkMenuItem *menuitem, gpointer page)
{
    //  0       -> current tab
    //  1 .. n  -> tab #n for active fs
    // -1 .. -n -> tab #n for inactive fs

    int n = GPOINTER_TO_INT (page);
    GnomeCmdFileSelector *fs = get_fs (n>=0 ? ACTIVE : INACTIVE);
    GnomeCmdFileList *fl = n==0 ? get_fl (ACTIVE) : fs->file_list(ABS(n)-1);

    if (fs && fl)
    {
        fl->locked = !fl->locked;
        fs->update_tab_label(fl);
    }
}


/************** Options Menu **************/
void options_edit (GtkMenuItem *menuitem, gpointer not_used)
{
    if (gnome_cmd_options_dialog (*main_win, gnome_cmd_data.options))
    {
        gnome_cmd_style_create (gnome_cmd_data.options);
        main_win->update_style();

        gnome_cmd_data.save();
    }
}


void options_edit_shortcuts (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_key_shortcuts_dialog_new (gcmd_user_actions);
}

/************** Connections Menu **************/
void connections_open (GtkMenuItem *menuitem, gpointer not_used)
{
    GtkWidget *dialog = gnome_cmd_remote_dialog_new ();
    g_object_ref (dialog);
    gtk_widget_show (dialog);
}


void connections_new (GtkMenuItem *menuitem, gpointer not_used)
{
    show_quick_connect_dialog ();
}


void connections_change (GtkMenuItem *menuitem, gpointer con)           // this function is NOT exposed to user as UserAction
{
    get_fl (ACTIVE)->set_connection((GnomeCmdCon *) con);
}


void connections_change_left (GtkMenuItem *menuitem, gpointer con)
{
    main_win->change_connection(LEFT);
}


void connections_change_right (GtkMenuItem *menuitem, gpointer con)
{
    main_win->change_connection(RIGHT);
}


void connections_close (GtkMenuItem *menuitem, gpointer con)            // this function is NOT exposed to user as UserAction
{
    GnomeCmdFileSelector *active = get_fs (ACTIVE);
    GnomeCmdFileSelector *inactive = get_fs (INACTIVE);

    GnomeCmdCon *home = get_home_con ();

    if (con == active->get_connection())
        active->set_connection(home);
    if (con == inactive->get_connection())
        inactive->set_connection(home);

    gnome_cmd_con_close ((GnomeCmdCon *) con);
}


void connections_close_current (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdCon *con = get_fs (ACTIVE)->get_connection();

    if (!GNOME_CMD_IS_CON_HOME (con))
        connections_close (menuitem, con);
}


/************** Bookmarks Menu **************/

void bookmarks_add_current (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_bookmark_add_current (get_fs (ACTIVE)->get_directory());
}


void bookmarks_edit (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_bookmark_dialog_new (_("Bookmarks"), *main_win);
}


void bookmarks_goto (GtkMenuItem *menuitem, gpointer bookmark_name)
{
    if (!bookmark_name)
        return;

    vector<string> a;
    guint n = split(static_cast<gchar*>(bookmark_name), a, "|");
    string group = n<2 || a[0].empty() ? "local" : a[0];
    string name = a[n<2 ? 0 : 1];

    transform(group.begin(), group.end(), group.begin(), (int(*)(int))tolower);

    if (group=="local")
    {
        for (GList *bookmarks = gnome_cmd_con_get_bookmarks (get_home_con ())->bookmarks; bookmarks; bookmarks = bookmarks->next)
        {
            GnomeCmdBookmark *bookmark = (GnomeCmdBookmark *) bookmarks->data;

            if (name == bookmark->name)
            {
                gnome_cmd_bookmark_goto (bookmark);
                return;
            }
        }

        g_warning ("[%s] Unknown bookmark name: '%s' - ignored", (char *) bookmark_name, name.c_str());
    }
#ifdef HAVE_SAMBA
    else
        if (group=="smb" || group=="samba")
        {
            for (GList *bookmarks = gnome_cmd_con_get_bookmarks (get_smb_con ())->bookmarks; bookmarks; bookmarks = bookmarks->next)
            {
                GnomeCmdBookmark *bookmark = (GnomeCmdBookmark *) bookmarks->data;

                if (name == bookmark->name)
                {
                    gnome_cmd_bookmark_goto (bookmark);
                    return;
                }
            }

            g_warning ("[%s] Unknown bookmark name: '%s' - ignored", (char *) bookmark_name, name.c_str());
        }
#endif
        else
            g_warning ("[%s] Unsupported bookmark group: '%s' - ignored", (char *) bookmark_name, group.c_str());
}


/************** Plugins Menu **************/

void plugins_configure (GtkMenuItem *menuitem, gpointer not_used)
{
    plugin_manager_show ();
}


void plugins_execute_python (GtkMenuItem *menuitem, gpointer python_script)
{
    if (!python_script)
        return;

#ifdef HAVE_PYTHON
    for (GList *py_plugins = gnome_cmd_python_plugin_get_list(); py_plugins; py_plugins = py_plugins->next)
    {
        PythonPluginData *py_plugin = (PythonPluginData *) py_plugins->data;

        if (!g_ascii_strcasecmp (py_plugin->name, (gchar *) python_script))
        {
            gnome_cmd_python_plugin_execute (py_plugin, main_win);
            return;
        }
    }
#else
    g_warning ("Python plugins not supported");
#endif
}


/************** Help Menu **************/

void help_help (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_help_display ("gnome-commander.xml");
}


void help_keyboard (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-keyboard");
}


void help_web (GtkMenuItem *menuitem, gpointer not_used)
{
    GError *error = NULL;

#if GTK_CHECK_VERSION (2, 14, 0)
    if (!gtk_show_uri (NULL, "http://gcmd.github.io/", GDK_CURRENT_TIME, &error))
        gnome_cmd_error_message (_("There was an error opening home page."), error);
#else
    if (!gnome_url_show ("http://gcmd.github.io/", &error))
        gnome_cmd_error_message (_("There was an error opening home page."), error);
#endif
}


void help_problem (GtkMenuItem *menuitem, gpointer not_used)
{
    GError *error = NULL;

#if GTK_CHECK_VERSION (2, 14, 0)
    if (!gtk_show_uri (NULL, "http://bugzilla.gnome.org/browse.cgi?product=gnome-commander", GDK_CURRENT_TIME, &error))
        gnome_cmd_error_message (_("There was an error reporting problem."), error);
#else
    if (!gnome_url_show("http://bugzilla.gnome.org/browse.cgi?product=gnome-commander", &error))
        gnome_cmd_error_message (_("There was an error reporting problem."), error);
#endif
}


void help_about (GtkMenuItem *menuitem, gpointer not_used)
{
    static const gchar *authors[] = {
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        "Assaf Gordon <agordon88@gmail.com>",
        "Uwe Scholz <u.scholz83@gmx.de>",
        NULL
    };

    static const gchar *documenters[] = {
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        "Laurent Coudeur <laurentc@eircom.net>",
        "Uwe Scholz <u.scholz83@gmx.de>",
        NULL
    };

    static const gchar copyright[] = "Copyright \xc2\xa9 2001-2006 Marcus Bjurman\n"
                                     "Copyright \xc2\xa9 2007-2012 Piotr Eljasiak\n"
                                     "Copyright \xc2\xa9 2013-2016 Uwe Scholz";

    static const gchar comments[] = N_("A fast and powerful file manager for the GNOME desktop");


    static const gchar *license[] = {
        N_("GNOME Commander is free software; you can redistribute it and/or modify "
           "it under the terms of the GNU General Public License as published by "
           "the Free Software Foundation; either version 2 of the License, or "
           "(at your option) any later version."),
        N_("GNOME Commander is distributed in the hope that it will be useful, "
           "but WITHOUT ANY WARRANTY; without even the implied warranty of "
           "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
           "GNU General Public License for more details."),
        N_("You should have received a copy of the GNU General Public License "
           "along with GNOME Commander; if not, write to the Free Software Foundation, Inc., "
           "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.")
    };

    gchar *license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]), _(license[2]), NULL);

    gtk_show_about_dialog (*main_win,
                           "name", "GNOME Commander",
                           "version", VERSION,
                           "comments", _(comments),
                           "copyright", copyright,
                           "license", license_trans,
                           "wrap-license", TRUE,
                           "authors", authors,
                           "documenters", documenters,
                           "logo-icon-name", PACKAGE_NAME,
                           "translator-credits", _("translator-credits"),
                           "website", "http://gcmd.github.io",
                           "website-label", "GNOME Commander Website",
                           NULL);

    g_free (license_trans);
}

/** 
 * Parses a command, stored in a char array, and substitutes a certain
 * symbol with path or URI names. The result is stored in a string
 * variable, which is expanded if needed. Possible symbols are:
 * \li \%f: file name (or list for multiple selections)
 * \li \%F: quoted filename (or list for multiple selections)
 * \li \%p: full file system path (or list for multiple selections)
 * \li \%P: quoted full file system path (or list for multiple selections)
 * \li \%s: synonym for %P (for compatibility with previous versions of gcmd)
 * \li \%u: fully qualified URI for the file (or list for multiple selections)
 * \li \%d: full path to the directory containing file
 * \li \%D: quoted full path to the directory containg file
 * \li \%%: percent sign
 * Trailing blanks are removed.
 * 
 * \param[in] command A char array to be parsed
 * \param[out] cmd A string with parsed symbols listed above
 * \returns Length of the cmd string
 */
int parse_command(string *cmd, gchar *command)
{
    gboolean percent = FALSE;
    gboolean blcheck = FALSE;
    string filename;
    string quoted_filename;
    string file_path;
    string quoted_file_path;
    string dir_path;
    string quoted_dir_path;
    string uri;
    unsigned cmdcap;
    unsigned cmdlen;

    GnomeCmdFileList *fl = get_fl (ACTIVE);
    GList *sfl = fl->get_selected_files();

    cmdcap = cmd->capacity();
    cmdlen = cmd->length();

    for (const char *s=(const char *) command; *s; ++s)
    {
        if (!percent)
        {
            percent = (*s == '%');

            if (!percent)
	    {
		if (cmdcap < cmdlen + 1)
		{
		    cmd->reserve(cmdlen +1);
		    cmdcap = cmd->capacity();
		}
		if (blcheck) // copy letter by letter, but remove trailing blanks
		    *cmd += *s;
		else
		{
		    if (*s == ' ' || *s == '\t') 
		    {
			continue;
		    }
		    else
		    {
			blcheck = TRUE;
			*cmd += *s;
		    }
		}
		cmdlen = cmd->length();
	    }

            continue;
        }

        switch (*s)
        {
            case 'f':           // %f  file name (or list for multiple selections)
                if (filename.empty())
                    get_file_list (filename, sfl, gnome_cmd_file_get_name);
		if (cmdcap < cmdlen + filename.length())
		{
		    cmd->reserve(cmdlen + filename.length());
		    cmdcap = cmd->capacity();
		}
                *cmd += filename;
		cmdlen = cmd->length();
                break;

            case 'F':           // %F  quoted filename (or list for multiple selections)
                if (quoted_filename.empty())
                    get_file_list (quoted_filename, sfl, gnome_cmd_file_get_quoted_name);
		if (cmdcap < cmdlen + quoted_filename.length())
		{
		    cmd->reserve(cmdlen + quoted_filename.length());
		    cmdcap = cmd->capacity();
		}
                *cmd += quoted_filename;
		cmdlen = cmd->length();
                break;

            case 'p':           // %p  full file system path (or list for multiple selections)
                if (file_path.empty())
                    get_file_list (file_path, sfl, gnome_cmd_file_get_real_path);
		if (cmdcap < cmdlen + file_path.length())
		{
		    cmd->reserve(cmdlen + file_path.length());
		    cmdcap = cmd->capacity();
		}
                *cmd += file_path;
		cmdlen = cmd->length();
                break;

            case 'P':           // %P  quoted full file system path (or list for multiple selections)
            case 's':           // %s  synonym for %P (for compatibility with previous versions of gcmd)
                if (quoted_file_path.empty())
                    get_file_list (quoted_file_path, sfl, gnome_cmd_file_get_quoted_real_path);
		if (cmdcap < cmdlen + quoted_file_path.length())
		{
		    cmd->reserve(cmdlen + quoted_file_path.length());
		    cmdcap = cmd->capacity();
		}
                *cmd += quoted_file_path;
		cmdlen = cmd->length();
                break;

            case 'u':           // %u  fully qualified URI for the file (or list for multiple selections)
                if (uri.empty())
                    get_file_list (uri, sfl, gnome_cmd_file_get_uri_str, GNOME_VFS_URI_HIDE_PASSWORD);
		if (cmdcap < cmdlen + uri.length())
		{
		    cmd->reserve(cmdlen + uri.length());
		    cmdcap = cmd->capacity();
		}
                *cmd += uri;
		cmdlen = cmd->length();
                break;

            case 'd':           // %d  full path to the directory containing file
		if (cmdcap < cmdlen + dir_path.length())
		{
		    cmd->reserve(cmdlen + dir_path.length());
		    cmdcap = cmd->capacity();
		}
                *cmd += dir_path;
		cmdlen = cmd->length();
                break;

            case 'D':           // %D  quoted full path to the directory containg file
		if (cmdcap < cmdlen + quoted_dir_path.length())
		{
		    cmd->reserve(cmdlen + quoted_dir_path.length());
		    cmdcap = cmd->capacity();
		}
                *cmd += quoted_dir_path;
		cmdlen = cmd->length();
                break;

            default:
		if (cmdcap < cmdlen + 1)
		{
		    cmd->reserve(cmdlen +1);
		    cmdcap = cmd->capacity();
		}
                *cmd += '%';
		cmdlen = cmd->length();
            case '%':           // %%  percent sign
		if (cmdcap < cmdlen + 1)
		{
		    cmd->reserve(cmdlen +1);
		    cmdcap = cmd->capacity();
		}
                *cmd += *s;
		cmdlen = cmd->length();
                break;
        }

        percent = FALSE;
    }

    if (percent)
        *cmd += '%';

    return cmd->length();
}
