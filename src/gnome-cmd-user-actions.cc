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

#include <config.h>
#include <gtk/gtkclipboard.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-bookmark-dialog.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-ftp-dialog.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-options-dialog.h"
#include "gnome-cmd-prepare-copy-dialog.h"
#include "gnome-cmd-prepare-move-dialog.h"
#include "gnome-cmd-python-plugin.h"
#include "gnome-cmd-search-dialog.h"
#include "gnome-cmd-user-actions.h"
#include "plugin_manager.h"
#include "cap.h"
#include "dict.h"
#include "utils.h"

using namespace std;


inline GnomeCmdFileSelector *get_fs (const FileSelectorID fsID)
{
    return gnome_cmd_main_win_get_fs (main_win, fsID);
}


inline GnomeCmdFileList *get_fl (const FileSelectorID fsID)
{
    GnomeCmdFileSelector *fs = get_fs (fsID);

    return fs ? fs->list : NULL;
}


// The file returned from this function is not to be unrefed
inline GnomeCmdFile *get_selected_file (const FileSelectorID fsID)
{
    GnomeCmdFile *finfo = gnome_cmd_file_list_get_first_selected_file (get_fl (fsID));

    if (!finfo)
        create_error_dialog (_("No file selected"));

    return finfo;
}


inline gboolean append_real_path (string &s, const gchar *name)
{
    s += ' ';
    s += name;

    return TRUE;
}


inline gboolean append_real_path (string &s, GnomeCmdFile *finfo)
{
    if (!finfo)
        return FALSE;

    gchar *name = g_shell_quote (gnome_cmd_file_get_real_path (finfo));

    append_real_path (s, name);

    g_free (name);

    return TRUE;
}


GnomeCmdUserActions gcmd_user_actions;


static DICT<GnomeCmdUserActionFunc> actions(NULL);


inline bool operator < (const GdkEventKey &e1, const GdkEventKey &e2)
{
    if (e1.keyval < e2.keyval)
        return true;

    if (e1.keyval > e2.keyval)
        return false;

    return (e1.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK)) < (e2.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK));
}


inline string key2str(guint state, guint key_val)
{
    string key_name;

    if (state & GDK_SHIFT_MASK)    key_name += "<shift>";
    if (state & GDK_CONTROL_MASK)  key_name += "<control>";
    if (state & GDK_MOD1_MASK)     key_name += "<alt>";
    if (state & GDK_MOD4_MASK)     key_name += "<win>";

    if (g_ascii_isalnum (key_val))
        key_name += g_ascii_tolower (key_val);
    else
        key_name += gdk_key_names[key_val];

    return key_name;
}


inline string key2str(const GdkEventKey &event)
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
        if (strlen(key)==1 && g_ascii_isalnum (*key))
            key_val = *key;

    if (strstr (s, "<shift>"))    state |= GDK_SHIFT_MASK;
    if (strstr (s, "<control>"))  state |= GDK_CONTROL_MASK;
    if (strstr (s, "<alt>"))      state |= GDK_MOD1_MASK;
    if (strstr (s, "<win>"))      state |= GDK_MOD4_MASK;

    if (strstr (s, "<mod1>"))      state |= GDK_MOD1_MASK;
    if (strstr (s, "<mod4>"))      state |= GDK_MOD4_MASK;

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


void GnomeCmdUserActions::init()
{
    actions.add(bookmarks_add_current, "bookmarks.add_current");
    actions.add(bookmarks_edit, "bookmarks.edit");
    actions.add(bookmarks_goto, "bookmarks.goto");
    actions.add(connections_close_current, "connections.close");
    actions.add(connections_close_current, "connections.close_current");
    actions.add(connections_ftp_connect, "connections.ftp_connect");
    actions.add(connections_ftp_quick_connect, "connections.ftp_quick_connect");
    actions.add(edit_cap_copy, "edit.copy");
    actions.add(edit_copy_fnames, "edit.copy_filenames");
    actions.add(edit_cap_cut, "edit.cut");
    actions.add(file_delete, "edit.delete");
    actions.add(edit_filter, "edit.filter");
    actions.add(edit_cap_paste, "edit.paste");
    actions.add(edit_quick_search, "edit.quick_search");
    actions.add(edit_search, "edit.search");
    actions.add(file_advrename, "file.advrename");
    actions.add(file_chmod, "file.chmod");
    actions.add(file_chown, "file.chown");
    actions.add(file_copy, "file.copy");
    actions.add(file_create_symlink, "file.create_symlink");
    actions.add(file_diff, "file.diff");
    actions.add(file_edit, "file.edit");
    actions.add(file_exit, "file.exit");
    actions.add(file_external_view, "file.external_view");
    actions.add(file_internal_view, "file.internal_view");
    actions.add(file_mkdir, "file.mkdir");
    actions.add(file_move, "file.move");
    actions.add(file_properties, "file.properties");
    actions.add(file_rename, "file.rename");
    // actions.add(file_run, "file.run");
    actions.add(file_sync_dirs, "file.synchronize_directories");
    // actions.add(file_umount, "file.umount");
    actions.add(file_view, "file.view");
    actions.add(help_about, "help.about");
    actions.add(help_help, "help.help");
    actions.add(help_keyboard, "help.keyboard");
    actions.add(help_problem, "help.problem");
    actions.add(help_web, "help.web");
    actions.add(mark_compare_directories, "mark.compare_directories");
    actions.add(mark_select_all, "mark.select_all");
    actions.add(mark_toggle, "mark.toggle");
    actions.add(mark_toggle_and_step, "mark.toggle_and_step");
    actions.add(mark_unselect_all, "mark.unselect_all");
    actions.add(no_action, "no.action");
    actions.add(options_edit, "options.edit");
    actions.add(options_edit_mime_types, "options.edit_mime_types");
    actions.add(plugins_configure, "plugins.configure");
    actions.add(plugins_execute_python, "plugins.execute_python");
    actions.add(view_back, "view.back");
    actions.add(view_equal_panes, "view.equal_panes");
    actions.add(view_first, "view.first");
    actions.add(view_forward, "view.forward");
    actions.add(view_last, "view.last");
    actions.add(view_refresh, "view.refresh");
    actions.add(view_up, "view.up");

    register_action(GDK_F3, "file.view");
    register_action(GDK_F4, "file.mkdir");
    register_action(GDK_F5, "file.copy");
    register_action(GDK_F6, "file.rename");
    register_action(GDK_F7, "file.mkdir");
    register_action(GDK_F8, "file.delete");
    register_action(GDK_F9, "edit.search");
    register_action(GDK_F10, "file.exit");

    load("key-bindings");

    if (!registered("bookmarks.edit"))
        register_action(GDK_CONTROL_MASK, GDK_D, "bookmarks.edit");

    if (!registered("connections.close_current"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_F, "connections.close_current");

    if (!registered("connections.ftp_connect"))
        register_action(GDK_CONTROL_MASK, GDK_F, "connections.ftp_connect");

    if (!registered("connections.ftp_quick_connect"))
        register_action(GDK_CONTROL_MASK, GDK_G, "connections.ftp_quick_connect");

    if (!registered("edit.filter"))
        register_action(GDK_CONTROL_MASK, GDK_F12, "edit.filter");

    if (!registered("edit.search"))
        register_action(GDK_MOD1_MASK, GDK_F7, "edit.search");

    if (!registered("file.advrename"))
    {
        register_action(GDK_CONTROL_MASK, GDK_M, "file.advrename");
        register_action(GDK_CONTROL_MASK, GDK_T, "file.advrename");
    }

    if (!registered("file.create_symlink"))
        register_action(GDK_CONTROL_MASK | GDK_SHIFT_MASK, GDK_F5, "file.create_symlink");

    if (!registered("file.external_view"))
        register_action(GDK_MOD1_MASK, GDK_F3, "file.external_view");

    if (!registered("file.internal_view"))
        register_action(GDK_SHIFT_MASK, GDK_F3, "file.internal_view");

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
    }

    if (!registered("view.refresh"))
        register_action(GDK_CONTROL_MASK, GDK_R, "view.refresh");
 }


void GnomeCmdUserActions::shutdown()
{
    // don't write 'hardcoded' Fn actions to config file

    unregister(GDK_F3);
    unregister(GDK_F4);
    unregister(GDK_F5);
    unregister(GDK_F6);
    unregister(GDK_F7);
    unregister(GDK_F8);
    unregister(GDK_F9);
    unregister(GDK_F10);

    write("key-bindings");

    action.clear();
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
        DEBUG('a',"[%s]\t%s=%s\n", section, key, action_name);

        char *action_options = strchr(action_name, '|');

        if (action_options)
        {
            *action_options = '\0';
            g_strstrip (++action_options);
        }

        g_strstrip (action_name);
        g_strdown (action_name);

        if (actions[action_name])
        {
            guint keyval;
            guint state;

            str2key(key, state, keyval);

            if (keyval!=GDK_VoidSymbol)
                register_action(state, keyval, action_name, action_options);
            else
                g_warning ("[%s] invalid key name: '%s' - ignored", section, key);
        }
        else
            g_warning ("[%s] unknown user action: '%s' - ignored", section, action_name);

        g_free (key);
        g_free (action_name);
    }
}


void GnomeCmdUserActions::write(const gchar *section)
{
    string section_path = G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S;
           section_path += section;
           section_path += G_DIR_SEPARATOR;

    char *key = NULL;
    char *action_name = NULL;

    for (gpointer i=gnome_config_init_iterator (section_path.c_str()); (i=gnome_config_iterator_next (i, &key, &action_name)); )
    {
        string curr_key = key;
        string norm_key = key2str(str2key(key));    // <ALT><Ctrl>F3 -> <ctrl><alt>f3

        if (curr_key!=norm_key)
        {
            gnome_config_clean_key ((section_path+curr_key).c_str());
            gnome_config_set_string ((section_path+norm_key).c_str(), action_name);
        }

        g_free (key);
        g_free (action_name);
    }

    for (map<GdkEventKey, UserAction>::const_iterator i=action.begin(); i!=action.end(); ++i)
    {
        string path = section_path + key2str(i->first);

        string action_name = actions[i->second.func];

        if (!i->second.user_data.empty())
        {
            action_name += '|';
            action_name += i->second.user_data;
        }

        gnome_config_set_string (path.c_str(), action_name.c_str());
    }
}


gboolean GnomeCmdUserActions::register_action(guint state, guint keyval, const gchar *name, const char *user_data)
{
    GnomeCmdUserActionFunc func = actions[name];

    if (!func)
        return FALSE;

    GdkEventKey event;

    event.keyval = keyval;
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

    if (action.find(event)!=action.end())
        return FALSE;

    if (!g_ascii_isalpha (keyval))
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


void GnomeCmdUserActions::unregister(guint state, guint keyval)
{
    GdkEventKey event;

    event.keyval = keyval;
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

    map <GdkEventKey, UserAction>::iterator pos = action.find(event);

    if (pos!=action.end())
        action.erase(pos);
}


gboolean GnomeCmdUserActions::registered(const gchar *name)
{
    GnomeCmdUserActionFunc func = actions[name];

    if (!func)
        return FALSE;

    for (map<GdkEventKey, UserAction>::const_iterator i=action.begin(); i!=action.end(); ++i)
        if (i->second.func==func)
            return TRUE;

    return FALSE;
}


gboolean GnomeCmdUserActions::registered(guint state, guint keyval)
{
    GdkEventKey event;

    event.keyval = keyval;
    event.state = state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD4_MASK);

    return action.find(event)!=action.end();
}


gboolean GnomeCmdUserActions::handle_key_event(GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs, GnomeCmdFileList *fl, GdkEventKey *event)
{
    map <GdkEventKey, UserAction>::const_iterator pos = action.find(*event);

    if (pos==action.end())
        return FALSE;

    DEBUG('a', "Key event:  %s (%#x)\n", key2str(*event).c_str(), event->keyval);
    DEBUG('a', "Handling key event by %s()\n", actions[pos->second.func].c_str());

    (*pos->second.func) (NULL, (gpointer) (pos->second.user_data.empty() ? NULL : pos->second.user_data.c_str()));

    return TRUE;
}


/***************************************/
void no_action (GtkMenuItem *menuitem, gpointer not_used)
{
}


/************** File Menu **************/
void file_copy (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!main_win)  return;

    GnomeCmdFileSelector *src_fs = get_fs (ACTIVE);
    GnomeCmdFileSelector *dest_fs = get_fs (INACTIVE);

    if (src_fs && dest_fs)
        gnome_cmd_prepare_copy_dialog_show (src_fs, dest_fs);
}


void file_move (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!main_win)  return;

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
        gnome_cmd_file_selector_start_editor (get_fs (ACTIVE));
    else
        gnome_cmd_file_list_edit (get_fl (ACTIVE));
}


void file_chmod (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_chmod_dialog (get_fl (ACTIVE));
}


void file_chown (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_chown_dialog (get_fl (ACTIVE));
}


void file_mkdir (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_show_mkdir_dialog (get_fs (ACTIVE));
}


void file_create_symlink (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *inactive_fs = get_fs (INACTIVE);
    GList *f = gnome_cmd_file_list_get_selected_files (get_fl (ACTIVE));
    guint selected_files = g_list_length (f);

    if (selected_files > 1)
    {
        gchar *msg = g_strdup_printf (ngettext("Create symbolic links of %i file in %s?",
                                               "Create symbolic links of %i files in %s?",
                                               selected_files),
                                      selected_files, gnome_cmd_dir_get_display_path (gnome_cmd_file_selector_get_directory(inactive_fs)));

        gint choice = run_simple_dialog (GTK_WIDGET (main_win), TRUE, GTK_MESSAGE_QUESTION, msg, _("Create Symbolic Link"), 1, _("Cancel"), _("Create"), NULL);

        g_free (msg);

        if (choice==1)
            gnome_cmd_file_selector_create_symlinks (inactive_fs, f);
    }
   else
   {
        GnomeCmdFile *finfo = gnome_cmd_file_list_get_focused_file (get_fl (ACTIVE));
        gnome_cmd_file_selector_create_symlink (inactive_fs, finfo);
   }
}


void file_rename (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_rename_dialog (get_fl (ACTIVE));
}


void file_advrename (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_advrename_dialog (get_fl (ACTIVE));
}


void file_properties (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_properties_dialog (get_fl (ACTIVE));
}


void file_diff (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!gnome_cmd_file_selector_is_local (get_fs (ACTIVE)))
    {
        create_error_dialog (_("Operation not supported on remote file systems"));
        return;
    }

    GnomeCmdFileList *active_fl = get_fl (ACTIVE);

    GList *sel_files = gnome_cmd_file_list_get_selected_files (active_fl);

    string s;

    switch (g_list_length (sel_files))
    {
        case 0:
            return;

        case 1:
            if (!gnome_cmd_file_selector_is_local (get_fs (INACTIVE)))
                create_error_dialog (_("Operation not supported on remote file systems"));
            else
                if (!append_real_path (s, get_selected_file (ACTIVE)) || !append_real_path (s, get_selected_file (INACTIVE)))
                    s.clear();
            break;

        case 2:
        case 3:
            sel_files = gnome_cmd_file_list_sort_selection (sel_files, active_fl);

            for (GList *i = sel_files; i; i = i->next)
                append_real_path (s, GNOME_CMD_FILE (i->data));
            break;

        default:
            create_error_dialog (_("Too many selected files"));
            break;
    }

    g_list_free (sel_files);

    if (!s.empty())
    {
        gchar *cmd = g_strdup_printf (gnome_cmd_data_get_differ (), s.c_str(), "");

        g_print (_("running `%s'\n"), cmd);
        run_command (cmd, FALSE);

        g_free (cmd);
    }
}


void file_sync_dirs (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdFileSelector *active_fs = get_fs (ACTIVE);
    GnomeCmdFileSelector *inactive_fs = get_fs (INACTIVE);

    if (!gnome_cmd_file_selector_is_local (active_fs) || !gnome_cmd_file_selector_is_local (inactive_fs))
    {
        create_error_dialog (_("Operation not supported on remote file systems"));
        return;
    }

    GnomeVFSURI *active_dir_uri = gnome_cmd_dir_get_uri (gnome_cmd_file_selector_get_directory (active_fs));
    GnomeVFSURI *inactive_dir_uri = gnome_cmd_dir_get_uri (gnome_cmd_file_selector_get_directory (inactive_fs));
    gchar *active_dir = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (active_dir_uri), NULL);
    gchar *inactive_dir = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (inactive_dir_uri), NULL);

    gnome_vfs_uri_unref (active_dir_uri);
    gnome_vfs_uri_unref (inactive_dir_uri);

    string s;

    append_real_path (s, active_dir);
    append_real_path (s, inactive_dir);

    g_free (active_dir);
    g_free (inactive_dir);

    gchar *cmd = g_strdup_printf (gnome_cmd_data_get_differ (), s.c_str(), "");

    g_print (_("running `%s'\n"), cmd);
    run_command (cmd, FALSE);

    g_free (cmd);
}


void file_exit (GtkMenuItem *menuitem, gpointer not_used)
{
    gint x, y;

    switch (gnome_cmd_data_get_main_win_state())
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

    gtk_widget_destroy (GTK_WIDGET (main_win));
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
    GnomeCmdFileSelector *fs = get_fs (ACTIVE);
    GtkWidget *dialog = gnome_cmd_search_dialog_new (gnome_cmd_file_selector_get_directory (fs));
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


void edit_quick_search (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_show_quicksearch (get_fl (ACTIVE), 0);
}


void edit_filter (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_show_filter (get_fs (ACTIVE), 0);
}


void edit_copy_fnames (GtkMenuItem *menuitem, gpointer not_used)
{
    static gchar sep[] = " ";

    GdkModifierType mask;

    gdk_window_get_pointer (NULL, NULL, NULL, &mask);

    GnomeCmdFileList *fl = get_fl (ACTIVE);
    GList *sfl = gnome_cmd_file_list_get_selected_files (fl);
    gchar **fnames = g_new (char *, g_list_length (sfl) + 1);
    gchar **f = fnames;

    sfl = gnome_cmd_file_list_sort_selection (sfl, fl);

    for (GList *i = sfl; i; i = i->next)
    {
        GnomeCmdFile *finfo = GNOME_CMD_FILE (i->data);

        if (finfo)
          *f++ = (mask & GDK_SHIFT_MASK) ? (char *) gnome_cmd_file_get_real_path (finfo) :
                                           (char *) gnome_cmd_file_get_name (finfo);
    }

    *f = NULL;

    gchar *s = g_strjoinv(sep,fnames);

    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), s, -1);

    g_free (s);
    g_list_free (sfl);
    g_free (fnames);
}


/************** Mark Menu **************/
void mark_toggle (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_toggle (get_fl (ACTIVE));
}


void mark_toggle_and_step (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_toggle_and_step (get_fl (ACTIVE));
}


void mark_select_all (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_select_all (get_fl (ACTIVE));
}


void mark_unselect_all (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_unselect_all (get_fl (ACTIVE));
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
    gnome_cmd_file_list_invert_selection (get_fl (ACTIVE));
}


void mark_select_all_with_same_extension (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_select_all_with_same_extension (get_fl (ACTIVE));
}


void mark_unselect_all_with_same_extension (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_unselect_all_with_same_extension (get_fl (ACTIVE));
}


void mark_restore_selection (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_restore_selection (get_fl (ACTIVE));
}


void mark_compare_directories (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_list_compare_directories ();
}


/************** View Menu **************/

void view_conbuttons (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data_set_conbuttons_visibility (checkitem->active);
    gnome_cmd_file_selector_update_conbuttons_visibility (get_fs (ACTIVE));
    gnome_cmd_file_selector_update_conbuttons_visibility (get_fs (INACTIVE));
}


void view_toolbar (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data_set_toolbar_visibility (checkitem->active);
    gnome_cmd_main_win_update_toolbar_visibility (main_win);
}


void view_buttonbar (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data_set_buttonbar_visibility (checkitem->active);
    gnome_cmd_main_win_update_buttonbar_visibility (main_win);
}


void view_cmdline (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data_set_cmdline_visibility (checkitem->active);
    gnome_cmd_main_win_update_cmdline_visibility (main_win);
}


void view_hidden_files (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data_get_filter_settings()->hidden = !checkitem->active;
    gnome_cmd_file_selector_reload (get_fs (ACTIVE));
    gnome_cmd_file_selector_reload (get_fs (INACTIVE));
}


void view_backup_files (GtkMenuItem *menuitem, gpointer not_used)
{
    if (!GTK_WIDGET_REALIZED (main_win)) return;

    GtkCheckMenuItem *checkitem = (GtkCheckMenuItem *) menuitem;
    gnome_cmd_data_get_filter_settings()->backup = !checkitem->active;
    gnome_cmd_file_selector_reload (get_fs (ACTIVE));
    gnome_cmd_file_selector_reload (get_fs (INACTIVE));
}


void view_up (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_goto_directory (get_fs (ACTIVE), "..");
}


void view_first (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_first (get_fs (ACTIVE));
}


void view_back (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_back (get_fs (ACTIVE));
}


void view_forward (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_forward (get_fs (ACTIVE));
}


void view_last (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_last (get_fs (ACTIVE));
}


void view_refresh (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_file_selector_reload (get_fs (ACTIVE));
}


void view_equal_panes (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_main_win_set_equal_panes ((GnomeCmdMainWin *) GTK_WIDGET (main_win));
}


/************** Options Menu **************/
void options_edit (GtkMenuItem *menuitem, gpointer not_used)
{
    GtkWidget *dialog = gnome_cmd_options_dialog_new ();
    gtk_widget_ref (dialog);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_win));
    gtk_widget_show (dialog);
}


void options_edit_mime_types (GtkMenuItem *menuitem, gpointer not_used)
{
    edit_mimetypes (NULL, FALSE);
}


/************** Connections Menu **************/
void connections_ftp_connect (GtkMenuItem *menuitem, gpointer not_used)
{
    GtkWidget *dialog = gnome_cmd_ftp_dialog_new ();
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


void connections_ftp_quick_connect (GtkMenuItem *menuitem, gpointer not_used)
{
    show_ftp_quick_connect_dialog ();
}


void connections_change (GtkMenuItem *menuitem, gpointer con)
{
    gnome_cmd_file_selector_set_connection (get_fs (ACTIVE), (GnomeCmdCon *) con, NULL);
}


void connections_close (GtkMenuItem *menuitem, gpointer con)
{
    GnomeCmdFileSelector *active = get_fs (ACTIVE);
    GnomeCmdFileSelector *inactive = get_fs (INACTIVE);

    GnomeCmdCon *c1 = gnome_cmd_file_selector_get_connection (active);
    GnomeCmdCon *c2 = gnome_cmd_file_selector_get_connection (inactive);
    GnomeCmdCon *home = gnome_cmd_con_list_get_home (gnome_cmd_data_get_con_list ());

    if (con == c1)
        gnome_cmd_file_selector_set_connection (active, home, NULL);
    if (con == c2)
        gnome_cmd_file_selector_set_connection (inactive, home, NULL);

    gnome_cmd_con_close ((GnomeCmdCon *) con);
}


void connections_close_current (GtkMenuItem *menuitem, gpointer not_used)
{
    GnomeCmdCon *con = gnome_cmd_file_selector_get_connection (get_fs (ACTIVE));

    connections_close (menuitem, con);
}


/************** Bookmarks Menu **************/

void bookmarks_add_current (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_bookmark_add_current ();
}


void bookmarks_edit (GtkMenuItem *menuitem, gpointer not_used)
{
    GtkWidget *dialog = gnome_cmd_bookmark_dialog_new ();
    gtk_widget_ref (dialog);
    gtk_widget_show (dialog);
}


void bookmarks_goto (GtkMenuItem *menuitem, gpointer bookmark_name)
{
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
    gnome_cmd_help_display("gnome-commander.xml");
}


void help_keyboard (GtkMenuItem *menuitem, gpointer not_used)
{
    gnome_cmd_help_display("gnome-commander.xml","gnome-commander-keyboard");
}


void help_web (GtkMenuItem *menuitem, gpointer not_used)
{
    GError *error = NULL;

    if (!gnome_url_show("http://www.nongnu.org/gcmd/", &error))
        gnome_cmd_error_message(_("There was an error opening home page."), error);
}


void help_problem (GtkMenuItem *menuitem, gpointer not_used)
{
    GError *error = NULL;

    if (!gnome_url_show("http://bugzilla.gnome.org/browse.cgi?product=gnome-commander", &error))
        gnome_cmd_error_message(_("There was an error reporting problem."), error);
}


void help_about (GtkMenuItem *menuitem, gpointer not_used)
{
    static const gchar *authors[] = {
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        "Assaf Gordon <agordon88@gmail.com>",
        NULL
    };

    static const gchar *documenters[] = {
        "Marcus Bjurman <marbj499@student.liu.se>",
        "Piotr Eljasiak <epiotr@use.pl>",
        NULL
    };

    static const gchar copyright[] = "Copyright \xc2\xa9 2001-2006 Marcus Bjurman";

    static const gchar comments[] = N_("A fast and powerful file manager for the GNOME desktop");


    gtk_show_about_dialog (GTK_WINDOW (main_win),
                   "authors", authors,
                   "comments", _(comments),
                   "copyright", copyright,
                   "documenters", documenters,
                   "translator-credits", _("translator-credits"),
                   "version", VERSION,
                   "website", "http://www.nongnu.org/gcmd",
                   "name", "GNOME Commander",
                   "logo-icon-name", PACKAGE_NAME,
                   NULL);
}
