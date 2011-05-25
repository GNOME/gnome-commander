/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2011 Piotr Eljasiak

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
/*
 *GNOME Search Tool
 *
 * File:  gsearchtool.c
 *
 * (C) 1998,2002 the Free Software Foundation
 *
 * Authors:    Dennis Cranston  <dennis_cranston@yahoo.com>
 *             George Lebl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>
#include <sys/types.h>
#include <regex.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-search-dialog.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-file-list.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-con-list.h"
#include "filter.h"
#include "utils.h"

using namespace std;


#if 0
static char *msgs[] = {N_("Search local directories only"),
                       N_("Files _not containing text")};
#endif


#define PBAR_MAX   50

#define SEARCH_JUMP_SIZE     4096U
#define SEARCH_BUFFER_SIZE  (SEARCH_JUMP_SIZE * 10U)

#define GNOME_SEARCH_TOOL_REFRESH_DURATION  50000


struct SearchData
{
    struct ProtectedData
    {
        GList *files;
        gchar *msg;
        GMutex *mutex;
    };

    const gchar *name_pattern;              // the pattern that file names should match to end up in the file list
    const gchar *content_pattern;           // the pattern that the content of a file should match to end up in the file list

    int max_depth;                          // should we recurse or just search in the selected directory ?
    Filter::Type name_filter_type;
    gboolean content_search;                // should we do content search ?
    gboolean case_sens;                     // case sensitive content search ?

    Filter *name_filter;
    regex_t *content_regex;
    gint context_id;                        // the context id of the status bar
    GnomeCmdSearchDialog *dialog;
    GList *match_dirs;                      // the directories which we found matching files in
    GnomeCmdDir *start_dir;                 // the directory to start searching from
    GThread *thread;
    ProtectedData pdata;
    gint update_gui_timeout_id;

    gboolean search_done;
    gboolean stopped;                       // stops the search routine if set to TRUE. This is done by the stop_button
    gboolean dialog_destroyed;              // set when the search dialog is destroyed, also stops the search of course
};


struct SearchFileData
{
    GnomeVFSResult  result;
    gchar          *uri_str;
    GnomeVFSHandle *handle;
    gint            offset;
    guint           len;
    gchar           mem[SEARCH_BUFFER_SIZE];     // memory to search in the content of a file
};


struct GnomeCmdSearchDialogClass
{
    GnomeCmdDialogClass parent_class;
};


struct GnomeCmdSearchDialog::Private
{
    SearchData *data;                       // holds data needed by the search routines

    GnomeCmdCon *con;

    GtkWidget *filter_type_combo;
    GtkWidget *pattern_combo;
    GtkWidget *dir_browser;
    GtkWidget *recurse_combo;
    GtkWidget *find_text_combo;
    GtkWidget *find_text_check;
    GnomeCmdFileList *result_list;
    GtkWidget *statusbar;

    GtkWidget *goto_button;
    GtkWidget *stop_button;
    GtkWidget *search_button;

    GtkWidget *case_check;
    GtkWidget *pbar;
};


G_DEFINE_TYPE (GnomeCmdSearchDialog, gnome_cmd_search_dialog, GNOME_CMD_TYPE_DIALOG)


/**
 * Puts a string in the statusbar.
 *
 */
inline void set_statusmsg (SearchData *data, const gchar *msg=NULL)
{
    gtk_statusbar_push (GTK_STATUSBAR (data->dialog->priv->statusbar), data->context_id, msg ? msg : "");
}


inline void free_search_file_data (SearchFileData *searchfile_data)
{
    if (searchfile_data->handle)
        gnome_vfs_close (searchfile_data->handle);

    g_free (searchfile_data->uri_str);
    g_free (searchfile_data);
}


/**
 * Loads a file in chunks and returns the content.
 */
static gboolean read_search_file (SearchData *data, SearchFileData *searchfile_data, GnomeCmdFile *f)
{
    if (data->stopped)    // if the stop button was pressed, let's abort here
    {
        free_search_file_data (searchfile_data);
        return FALSE;
    }

    if (searchfile_data->len)
    {
      if ((searchfile_data->offset + searchfile_data->len) >= f->info->size)   // end, all has been read
      {
          free_search_file_data (searchfile_data);
          return FALSE;
      }

      // jump a big step backward to give the regex a chance
      searchfile_data->offset += searchfile_data->len - SEARCH_JUMP_SIZE;
      if (f->info->size < (searchfile_data->offset + (SEARCH_BUFFER_SIZE - 1)))
          searchfile_data->len = f->info->size - searchfile_data->offset;
      else
          searchfile_data->len = SEARCH_BUFFER_SIZE - 1;
    }
    else   // first time call of this function
        searchfile_data->len = MIN (f->info->size, SEARCH_BUFFER_SIZE - 1);

    searchfile_data->result = gnome_vfs_seek (searchfile_data->handle, GNOME_VFS_SEEK_START, searchfile_data->offset);
    if (searchfile_data->result != GNOME_VFS_OK)
    {
        g_warning (_("Failed to read file %s: %s"), searchfile_data->uri_str, gnome_vfs_result_to_string (searchfile_data->result));
        free_search_file_data (searchfile_data);
        return FALSE;
    }

    GnomeVFSFileSize ret;
    searchfile_data->result = gnome_vfs_read (searchfile_data->handle, searchfile_data->mem, searchfile_data->len, &ret);
    if (searchfile_data->result != GNOME_VFS_OK)
    {
        g_warning (_("Failed to read file %s: %s"), searchfile_data->uri_str, gnome_vfs_result_to_string (searchfile_data->result));
        free_search_file_data (searchfile_data);
        return FALSE;
    }

    searchfile_data->mem[searchfile_data->len] = '\0';

    return TRUE;
}


/**
 * Determines if the content of a file matches an regexp
 *
 */
inline gboolean content_matches (GnomeCmdFile *f, SearchData *data)
{
    g_return_val_if_fail (f != NULL, FALSE);
    g_return_val_if_fail (f->info != NULL, FALSE);

    if (f->info->size==0)
        return FALSE;

    SearchFileData *search_file = g_new0 (SearchFileData, 1);
    search_file->uri_str = f->get_uri_str();
    search_file->result  = gnome_vfs_open (&search_file->handle, search_file->uri_str, GNOME_VFS_OPEN_READ);

    if (search_file->result != GNOME_VFS_OK)
    {
        g_warning (_("Failed to read file %s: %s"), search_file->uri_str, gnome_vfs_result_to_string (search_file->result));
        free_search_file_data (search_file);
        return FALSE;
    }

    regmatch_t match;

    while (read_search_file (data, search_file, f))
        if (regexec (data->content_regex, search_file->mem, 1, &match, 0) != REG_NOMATCH)
            return TRUE;        // stop on first match

    return FALSE;
}


/**
 * Determines if the name of a file matches an regexp
 *
 */
inline gboolean name_matches (gchar *name, SearchData *data)
{
    return data->name_filter->match(name);
}


/**
 * Searches a given directory for files that matches the criteria given by data.
 *
 */
static void search_dir_r (GnomeCmdDir *dir, SearchData *data, long level)
{
    if (!dir)
        return;

    // update the search status data
    if (!data->dialog_destroyed)
    {
        g_mutex_lock (data->pdata.mutex);

        g_free (data->pdata.msg);
        data->pdata.msg = g_strdup_printf (_("Searching in: %s"), gnome_cmd_dir_get_display_path (dir));

        g_mutex_unlock (data->pdata.mutex);
    }

    if (data->stopped)    // if the stop button was pressed, let's abort here
        return;

    gnome_cmd_dir_list_files (dir, FALSE);

    // let's iterate through all files
    for (GList *i=gnome_cmd_dir_get_files (dir); i; i=i->next)
    {
        if (data->stopped)        // if the stop button was pressed, let's abort here
            return;

        GnomeCmdFile *f = (GnomeCmdFile *) i->data;

        // if the current file is a directory, let's continue our recursion
        if (GNOME_CMD_IS_DIR (f) && level!=0)
        {
            // we don't want to go backwards or to follow symlinks
            if (!f->is_dotdot && strcmp (f->info->name, ".") != 0 && !GNOME_VFS_FILE_INFO_SYMLINK (f->info))
            {
                GnomeCmdDir *new_dir = GNOME_CMD_DIR (f);

                if (new_dir)
                {
                    gnome_cmd_dir_ref (new_dir);
                    search_dir_r (new_dir, data, level-1);
                    gnome_cmd_dir_unref (new_dir);
                }
            }
        }
        // if the file is a regular one it might match the search criteria
        else
            if (f->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
            {
                // if the name doesn't match, let's go to the next file
                if (!name_matches (f->info->name, data))
                    continue;

                // if the user wants to we should do some content matching here
                if (data->content_search && !content_matches (f, data))
                    continue;

                // the file matched the search criteria, let's add it to the list
                g_mutex_lock (data->pdata.mutex);
                data->pdata.files = g_list_append (data->pdata.files, f->ref());
                g_mutex_unlock (data->pdata.mutex);

                // also ref each directory that has a matching file
                if (g_list_index (data->match_dirs, dir) == -1)
                    data->match_dirs = g_list_append (data->match_dirs, gnome_cmd_dir_ref (dir));
            }
    }
}


static gpointer perform_search_operation (SearchData *data)
{
    // unref all directories which contained matching files from last search
    if (data->match_dirs)
    {
        g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, NULL);
        g_list_free (data->match_dirs);
        data->match_dirs = NULL;
    }

    search_dir_r (data->start_dir, data, data->max_depth);

    // free regexps
    delete data->name_filter;
    data->name_filter = NULL;

    if (data->content_search)
    {
        regfree (data->content_regex);
        g_free (data->content_regex);
        data->content_regex = NULL;
    }

    gnome_cmd_dir_unref (data->start_dir);
    data->start_dir = NULL;

    data->search_done = TRUE;

    return NULL;
}


static gboolean update_search_status_widgets (SearchData *data)
{
    progress_bar_update (data->dialog->priv->pbar, PBAR_MAX);       // update the progress bar

    if (data->pdata.mutex)
    {
        g_mutex_lock (data->pdata.mutex);

        GList *files = data->pdata.files;
        data->pdata.files = NULL;

        set_statusmsg (data, data->pdata.msg);                      // update status bar with the latest message

        g_mutex_unlock (data->pdata.mutex);

        for (GList *i = files; i; i = i->next)                      // add all files found since last update to the list
            data->dialog->priv->result_list->append_file(GNOME_CMD_FILE (i->data));

        gnome_cmd_file_list_free (files);
    }

    if (!data->search_done && !data->stopped || data->pdata.files)
        return TRUE;

    if (!data->dialog_destroyed)
    {
        int matches = data->dialog->priv->result_list->size();

        gchar *fmt = data->stopped ? ngettext("Found %d match - search aborted", "Found %d matches - search aborted", matches) :
                                     ngettext("Found %d match", "Found %d matches", matches);

        gchar *msg = g_strdup_printf (fmt, matches);

        set_statusmsg (data, msg);
        g_free (msg);

        gtk_widget_hide (data->dialog->priv->pbar);

        gtk_widget_set_sensitive (data->dialog->priv->goto_button, matches>0);
        gtk_widget_set_sensitive (data->dialog->priv->stop_button, FALSE);
        gtk_widget_set_sensitive (data->dialog->priv->search_button, TRUE);

        gtk_widget_grab_focus (GTK_WIDGET (data->dialog->priv->result_list));        // set focus to result list
    }

    return FALSE;    // returning FALSE here stops the timeout callbacks
}


/*
 * This function gets called then the search-dialog is about the be destroyed.
 * The function waits for the last search-thread to finish and then frees the
 * data structure that has been shared between the search threads and the
 * main thread.
 */
static gboolean join_thread_func (SearchData *data)
{
    if (data->thread)
        g_thread_join (data->thread);

    if (data->pdata.mutex)
        g_mutex_free (data->pdata.mutex);

    g_free (data);

    return FALSE;
}


static void on_dialog_destroy (GnomeCmdSearchDialog *dialog, gpointer user_data)
{
    SearchData *data = dialog->priv->data;

    if (data)
    {
        if (!data->search_done)
            g_source_remove (data->update_gui_timeout_id);

        // stop and wait for search thread to exit
        data->stopped = TRUE;
        data->dialog_destroyed = TRUE;
        g_timeout_add (1, (GSourceFunc) join_thread_func, data);

        // unref all directories which contained matching files from last search
        if (data->pdata.mutex)
        {
            g_mutex_lock (data->pdata.mutex);
            if (data->match_dirs)
            {
                g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, NULL);
                g_list_free (data->match_dirs);
                data->match_dirs = NULL;
            }
            g_mutex_unlock (data->pdata.mutex);
        }
    }

    GtkAllocation allocation;

    gtk_widget_get_allocation (GTK_WIDGET (dialog), &allocation);
    gnome_cmd_data.search_defaults.width = allocation.width;
    gnome_cmd_data.search_defaults.height = allocation.height;

    gnome_cmd_data.search_defaults.default_profile.syntax = (Filter::Type) gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->filter_type_combo));
    gnome_cmd_data.search_defaults.default_profile.max_depth = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->recurse_combo)) - 1;
}


static gboolean start_generic_search (SearchData *data)
{
    // create an re for file name matching
    data->name_filter = new Filter(data->name_pattern, data->case_sens, data->name_filter_type);

    // if we're going to search through file content create an re for that too
    if (data->content_search)
    {
        data->content_regex = g_new0 (regex_t, 1);
        regcomp (data->content_regex, data->content_pattern, data->case_sens ? 0 : REG_ICASE);
    }

    if (!data->pdata.mutex)
        data->pdata.mutex = g_mutex_new ();

    data->thread = g_thread_create ((GThreadFunc) perform_search_operation, data, TRUE, NULL);

    return TRUE;
}


//  local search - using findutils

static gchar *build_search_command (SearchData *data)
{
    gchar *file_pattern_utf8 = g_strdup (data->name_pattern);
    GError *error = NULL;

    switch (data->name_filter_type)
    {
        case Filter::TYPE_FNMATCH:
            if (!file_pattern_utf8 || !*file_pattern_utf8)
            {
                g_free (file_pattern_utf8);
                file_pattern_utf8 = g_strdup ("*");
            }
            else
                if (!g_utf8_strchr (file_pattern_utf8, -1, '*') && !g_utf8_strchr (file_pattern_utf8, -1, '?'))
                {
                    gchar *tmp = file_pattern_utf8;
                    file_pattern_utf8 = g_strconcat ("*", file_pattern_utf8, "*", NULL);
                    g_free (tmp);
                }
            break;

        case Filter::TYPE_REGEX:
            break;

        default:
            break;
    }

    gchar *file_pattern_locale = g_locale_from_utf8 (file_pattern_utf8, -1, NULL, NULL, &error);

    if (!file_pattern_locale)
    {
        gnome_cmd_error_message (file_pattern_utf8, error);
        g_free (file_pattern_utf8);
        return NULL;
    }

    gchar *file_pattern_quoted = quote_if_needed (file_pattern_locale);
    gchar *look_in_folder_utf8 = GNOME_CMD_FILE (data->start_dir)->get_real_path();
    gchar *look_in_folder_locale = g_locale_from_utf8 (look_in_folder_utf8, -1, NULL, NULL, NULL);

    if (!look_in_folder_locale)     // if for some reason a path was not returned, fallback to the user's home directory
        look_in_folder_locale = g_strconcat (g_get_home_dir (), G_DIR_SEPARATOR_S, NULL);

    gchar *look_in_folder_quoted = quote_if_needed (look_in_folder_locale);

    GString *command = g_string_sized_new (512);

    g_string_append (command, "find ");
    g_string_append (command, look_in_folder_quoted);

    if (data->max_depth!=-1)
        g_string_append_printf (command, " -maxdepth %i", data->max_depth+1);

    switch (data->name_filter_type)
    {
        case Filter::TYPE_FNMATCH:
            g_string_append_printf (command, " -iname '%s'", file_pattern_utf8);
            break;

        case Filter::TYPE_REGEX:
            g_string_append_printf (command, " -regextype posix-extended -iregex '.*/.*%s.*'", file_pattern_utf8);
            break;
    }

    if (data->content_search)
    {
        static const gchar GREP_COMMAND[] = "grep";

        if (data->case_sens)
            g_string_append_printf (command, " '!' -type p -exec %s -E -q '%s' {} \\;", GREP_COMMAND, data->content_pattern);
        else
            g_string_append_printf (command, " '!' -type p -exec %s -E -q -i '%s' {} \\;", GREP_COMMAND, data->content_pattern);
    }

    g_string_append (command, " -print");

    g_free (file_pattern_utf8);
    g_free (file_pattern_locale);
    g_free (file_pattern_quoted);
    g_free (look_in_folder_utf8);
    g_free (look_in_folder_locale);
    g_free (look_in_folder_quoted);

    return g_string_free (command, FALSE);
}


static void child_command_set_pgid_cb (gpointer unused)
{
    if (setpgid (0, 0) < 0)
        g_print (_("Failed to set process group id of child %d: %s.\n"), getpid (), g_strerror (errno));
}


static gboolean handle_search_command_stdout_io (GIOChannel *ioc, GIOCondition condition, SearchData *data)
{
    gboolean broken_pipe = FALSE;

    if (condition & G_IO_IN)
    {
        GError *error = NULL;

        GString *string = g_string_new (NULL);

        GTimer *timer = g_timer_new ();
        g_timer_start (timer);

        while (!ioc->is_readable);

        do
        {
            gint status;

            if (data->stopped)
            {
                broken_pipe = TRUE;
                break;
            }

            do
            {
                status = g_io_channel_read_line_string (ioc, string, NULL, &error);

                if (status == G_IO_STATUS_EOF)
                    broken_pipe = TRUE;
                else
                    if (status == G_IO_STATUS_AGAIN)
                        while (gtk_events_pending ())
                        {
                            if (data->stopped)
                                return FALSE;

                            gtk_main_iteration ();
                        }
            }
            while (status == G_IO_STATUS_AGAIN && !broken_pipe);

            if (broken_pipe)
                break;

            if (status != G_IO_STATUS_NORMAL)
            {
                if (error)
                {
                    g_warning ("handle_search_command_stdout_io(): %s", error->message);
                    g_error_free (error);
                }
                continue;
            }

            string = g_string_truncate (string, string->len - 1);

            if (string->len <= 1)
                continue;

            gchar *utf8 = g_filename_display_name (string->str);

            GnomeCmdFile *f = gnome_cmd_file_new (utf8);

            if (f)
                data->dialog->priv->result_list->append_file(f);

            g_free (utf8);

            gulong duration;

            g_timer_elapsed (timer, &duration);

            if (duration > GNOME_SEARCH_TOOL_REFRESH_DURATION)
            {
                while (gtk_events_pending ())
                {
                    if (data->stopped)
                        return FALSE;

                    gtk_main_iteration ();
                }

                g_timer_reset (timer);
            }
        }
        while (g_io_channel_get_buffer_condition (ioc) & G_IO_IN);

        g_string_free (string, TRUE);
        g_timer_destroy (timer);
    }

    if (!(condition & G_IO_IN) || broken_pipe)
    {
        g_io_channel_shutdown (ioc, TRUE, NULL);

        data->search_done = TRUE;

        return FALSE;
    }

    return TRUE;
}


static gboolean start_local_search (SearchData *data)
{
    gchar *command = build_search_command (data);

    g_return_val_if_fail (command!=NULL, FALSE);

    DEBUG ('g', "running: %s\n", command);

    GError *error = NULL;
    gchar **argv  = NULL;
    gint child_stdout;

    if (!g_shell_parse_argv (command, NULL, &argv, &error))
    {
        gnome_cmd_error_message (_("Error parsing the search command."), error);

        g_free (command);
        g_strfreev (argv);

        return FALSE;
    }

    g_free (command);

    if (!g_spawn_async_with_pipes (NULL, argv, NULL, GSpawnFlags (G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL), child_command_set_pgid_cb, NULL, NULL, NULL, &child_stdout, NULL, &error))
    {
        gnome_cmd_error_message (_("Error running the search command."), error);

        g_strfreev (argv);

        return FALSE;
    }

#ifdef G_OS_WIN32
    GIOChannel *ioc_stdout = g_io_channel_win32_new_fd (child_stdout);
#else
    GIOChannel *ioc_stdout = g_io_channel_unix_new (child_stdout);
#endif

    g_io_channel_set_encoding (ioc_stdout, NULL, NULL);
    g_io_channel_set_flags (ioc_stdout, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch (ioc_stdout, GIOCondition (G_IO_IN | G_IO_HUP), (GIOFunc) handle_search_command_stdout_io, data);

    g_io_channel_unref (ioc_stdout);
    g_strfreev (argv);

    return TRUE;
}


/**
 * The user has clicked on the search button
 *
 */
static void on_search (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    SearchData *data = dialog->priv->data;

    if (data->thread)
    {
        g_thread_join (data->thread);
        data->thread = NULL;
    }

    data->search_done = TRUE;
    data->stopped = TRUE;
    data->dialog_destroyed = FALSE;

    data->dialog = dialog;
    data->name_pattern = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dialog->priv->pattern_combo));
    data->content_pattern = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dialog->priv->find_text_combo));
    data->max_depth = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->recurse_combo)) - 1;
    data->name_filter_type = (Filter::Type) gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->filter_type_combo));
    data->content_search = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->find_text_check));
    data->case_sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check));

    data->context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (dialog->priv->statusbar), "info");
    data->content_regex = NULL;
    data->match_dirs = NULL;

    gchar *dir_str = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->priv->dir_browser));
    GnomeVFSURI *uri = gnome_vfs_uri_new (dir_str);
    g_free (dir_str);

    dir_str = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (uri), NULL);
    gchar *dir_path = g_strconcat (dir_str, G_DIR_SEPARATOR_S, NULL);
    g_free (dir_str);

    if (strncmp(dir_path, gnome_cmd_con_get_root_path (dialog->priv->con), dialog->priv->con->root_path->len)!=0)
    {
        if (!gnome_vfs_uri_is_local (uri))
        {
            gnome_cmd_show_message (GTK_WINDOW (dialog), stringify(g_strdup_printf (_("Failed to change directory outside of %s"),
                                                                                    gnome_cmd_con_get_root_path (dialog->priv->con))));
            gnome_vfs_uri_unref (uri);
            g_free (dir_path);

            return;
        }
        else
            data->start_dir = gnome_cmd_dir_new (get_home_con (), gnome_cmd_con_create_path (get_home_con (), dir_path));
    }
    else
        data->start_dir = gnome_cmd_dir_new (dialog->priv->con, gnome_cmd_con_create_path (dialog->priv->con, dir_path + dialog->priv->con->root_path->len));

    gnome_cmd_dir_ref (data->start_dir);

    gnome_vfs_uri_unref (uri);
    g_free (dir_path);

    // save default settings
    gnome_cmd_data.search_defaults.default_profile.match_case = data->case_sens;
    gnome_cmd_data.search_defaults.default_profile.max_depth = data->max_depth;
    gnome_cmd_data.search_defaults.name_patterns.add(data->name_pattern);

    if (data->content_search)
    {
        gnome_cmd_data.search_defaults.content_patterns.add(data->content_pattern);
        gnome_cmd_data.intviewer_defaults.text_patterns.add(data->content_pattern);
    }

    data->search_done = FALSE;
    data->stopped = FALSE;

    dialog->priv->result_list->remove_all_files();

    if (gnome_cmd_con_is_local (dialog->priv->con) ? start_local_search (data) : start_generic_search (data))
    {
        set_statusmsg (data);
        gtk_widget_show (data->dialog->priv->pbar);
        data->update_gui_timeout_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_search_status_widgets, data);

        gtk_widget_set_sensitive (dialog->priv->goto_button, FALSE);
        gtk_widget_set_sensitive (dialog->priv->stop_button, TRUE);
        gtk_widget_set_sensitive (dialog->priv->search_button, FALSE);
    }
}


/**
 * The user has clicked on the help button
 *
 */
static void on_help (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-search");
}


/**
 * The user has clicked on the close button
 *
 */
static void on_close (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


/**
 * The user has clicked on the stop button
 *
 */
static void on_stop (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    g_return_if_fail (dialog != NULL);
    g_return_if_fail (dialog->priv != NULL);
    g_return_if_fail (dialog->priv->data != NULL);

    dialog->priv->data->stopped = TRUE;

    gtk_widget_set_sensitive (dialog->priv->stop_button, FALSE);
}


//  the user has clicked on the "go to" button
static void on_goto (GtkButton *button, GnomeCmdSearchDialog *dialog)
{
    GnomeCmdFile *f = dialog->priv->result_list->get_selected_file();

    if (!f)
        return;

    gchar *fpath = f->get_path();
    gchar *dpath = g_path_get_dirname (fpath);

    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);

    if (fs->file_list()->locked)
        fs->new_tab(f->get_parent_dir());
    else
        fs->file_list()->goto_directory(dpath);

    fs->file_list()->focus_file(f->get_name(), TRUE);

    g_free (fpath);
    g_free (dpath);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}


inline gboolean handle_list_keypress (GnomeCmdFileList *fl, GdkEventKey *event)
{
    switch (event->keyval)
    {
        case GDK_F3:
            gnome_cmd_file_list_view (fl, -1);
            return TRUE;

        case GDK_F4:
            gnome_cmd_file_list_edit (fl);
            return TRUE;
    }

    return FALSE;
}


static gboolean on_list_keypressed (GtkWidget *result_list,  GdkEventKey *event, gpointer unused)
{
    if (GNOME_CMD_FILE_LIST (result_list)->key_pressed(event) ||
        handle_list_keypress (GNOME_CMD_FILE_LIST (result_list), event))
    {
        stop_kp (GTK_OBJECT (result_list));
        return TRUE;
    }

    return FALSE;
}


static void on_filter_type_changed (GtkComboBox *combo, GnomeCmdSearchDialog *dialog)
{
    gtk_widget_grab_focus (dialog->priv->pattern_combo);
}


// the user has clicked on the "search by content" checkbutton
static void find_text_toggled (GtkToggleButton *togglebutton, GnomeCmdSearchDialog *dialog)
{
    if (gtk_toggle_button_get_active (togglebutton))
    {
        gtk_widget_set_sensitive (dialog->priv->find_text_combo, TRUE);
        gtk_widget_grab_focus (dialog->priv->find_text_combo);
    }
    else
        gtk_widget_set_sensitive (dialog->priv->find_text_combo, FALSE);
}


/*******************************
 * Gtk class implementation
 *******************************/


/*
 * create a gtk_combo_box_entry_new_text. gtk_combo is deprecated.
 */
inline GtkWidget *create_combo_box_entry (GtkWidget *parent)
{
    GtkWidget *combo = gtk_combo_box_entry_new_text ();
    g_object_ref (combo);
    g_object_set_data_full (G_OBJECT (parent), "combo", combo, g_object_unref);
    gtk_widget_show (combo);
    return combo;
}


/*
 * callback function for 'g_list_foreach' to add default value to dropdownbox
 */
static void combo_box_insert_text (const gchar *text, GtkComboBox *widget)
{
    gtk_combo_box_append_text (widget, text);
}


static void gnome_cmd_search_dialog_finalize (GObject *object)
{
    GnomeCmdSearchDialog *dialog = GNOME_CMD_SEARCH_DIALOG (object);

    g_free (dialog->priv);

    G_OBJECT_CLASS (gnome_cmd_search_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_search_dialog_init (GnomeCmdSearchDialog *dialog)
{
    GnomeCmdData::SearchConfig &defaults = gnome_cmd_data.search_defaults;

    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *sw;
    GtkWidget *pbar;

    dialog->priv = g_new0 (GnomeCmdSearchDialog::Private, 1);
    dialog->priv->data = g_new0 (SearchData, 1);

    window = GTK_WIDGET (dialog);
    g_object_set_data (G_OBJECT (window), "window", window);
    gtk_window_set_title (GTK_WINDOW (window), _("Search..."));
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);
    gtk_window_set_default_size (GTK_WINDOW (window), defaults.width, defaults.height);

    vbox = create_vbox (window, FALSE, 0);
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), vbox);

    table = create_table (window, 5, 2);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 6);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);


    // search for
    dialog->priv->filter_type_combo = gtk_combo_box_new_text ();
    gtk_combo_box_append_text (GTK_COMBO_BOX (dialog->priv->filter_type_combo), _("Name matches regex:"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (dialog->priv->filter_type_combo), _("Name contains:"));
    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->filter_type_combo), (int) gnome_cmd_data.search_defaults.default_profile.syntax);
    gtk_widget_show (dialog->priv->filter_type_combo);
    dialog->priv->pattern_combo = create_combo_box_entry (window);
    table_add (table, dialog->priv->filter_type_combo, 0, 0, GTK_FILL);
    table_add (table, dialog->priv->pattern_combo, 1, 0, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));

    if (!defaults.name_patterns.empty())
        g_list_foreach (defaults.name_patterns.ents, (GFunc) combo_box_insert_text, dialog->priv->pattern_combo);

    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->pattern_combo), 0);
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (dialog->priv->pattern_combo)), "activate", G_CALLBACK (gtk_window_activate_default), dialog);


    // search in
    dialog->priv->dir_browser =  gtk_file_chooser_button_new (_("Select Directory"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog->priv->dir_browser), FALSE);
    gtk_widget_show (dialog->priv->dir_browser);
    table_add (table, create_label_with_mnemonic (window, _("_Look in folder:"), dialog->priv->dir_browser), 0, 1, GTK_FILL);

    table_add (table, dialog->priv->dir_browser, 1, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));


    // recurse check
    dialog->priv->recurse_combo = gtk_combo_box_new_text ();

    gtk_combo_box_append_text (GTK_COMBO_BOX (dialog->priv->recurse_combo), _("Unlimited depth"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (dialog->priv->recurse_combo), _("Current directory only"));
    for (int i=1; i<=40; ++i)
    {
       gchar *item = g_strdup_printf (ngettext("%i level", "%i levels", i), i);
       gtk_combo_box_append_text (GTK_COMBO_BOX (dialog->priv->recurse_combo), item);
       g_free (item);
    }

    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->recurse_combo), defaults.default_profile.max_depth+1);
    gtk_widget_show (dialog->priv->recurse_combo);

    table_add (table, create_label_with_mnemonic (window, _("Search _recursively:"), dialog->priv->recurse_combo), 0, 2, GTK_FILL);
    table_add (table, dialog->priv->recurse_combo, 1, 2, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));


    // find text
    dialog->priv->find_text_check = create_check_with_mnemonic (window, _("Contains _text:"), "find_text");
    table_add (table, dialog->priv->find_text_check, 0, 3, GTK_FILL);

    dialog->priv->find_text_combo = create_combo_box_entry (window);
    table_add (table, dialog->priv->find_text_combo, 1, 3, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL));
    gtk_widget_set_sensitive (dialog->priv->find_text_combo, FALSE);
    if (!defaults.content_patterns.empty())
        g_list_foreach (defaults.content_patterns.ents, (GFunc) combo_box_insert_text, dialog->priv->find_text_combo);

    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->find_text_combo), 0);
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (dialog->priv->find_text_combo)), "activate", G_CALLBACK (gtk_window_activate_default), dialog);

    // case check
    dialog->priv->case_check = create_check_with_mnemonic (window, _("Case sensiti_ve"), "case_check");
    gtk_table_attach (GTK_TABLE (table), dialog->priv->case_check, 1, 2, 4, 5,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->case_check), defaults.default_profile.match_case);

    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_HELP, GTK_SIGNAL_FUNC (on_help), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_SIGNAL_FUNC (on_close), dialog);
    dialog->priv->goto_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("_Go to"), GTK_SIGNAL_FUNC (on_goto), dialog);
    dialog->priv->stop_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_STOP, GTK_SIGNAL_FUNC (on_stop), dialog);
    dialog->priv->search_button = gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GTK_STOCK_FIND, GTK_SIGNAL_FUNC (on_search), dialog);

    gtk_widget_set_sensitive (dialog->priv->goto_button, FALSE);
    gtk_widget_set_sensitive (dialog->priv->stop_button, FALSE);

    sw = gtk_scrolled_window_new (NULL, NULL);
    g_object_ref (sw);
    g_object_set_data_full (G_OBJECT (window), "sw", sw, g_object_unref);
    gtk_widget_show (sw);
    gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    dialog->priv->result_list = new GnomeCmdFileList(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING);
    g_object_ref (dialog->priv->result_list);
    g_object_set_data_full (G_OBJECT (window), "result_list", GTK_WIDGET (dialog->priv->result_list), g_object_unref);
    gtk_widget_set_size_request (GTK_WIDGET (dialog->priv->result_list), -1, 200);
    gtk_widget_show (GTK_WIDGET (dialog->priv->result_list));
    gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (dialog->priv->result_list));
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->result_list), 4);


    dialog->priv->statusbar = gtk_statusbar_new ();
    gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (dialog->priv->statusbar), FALSE);
    g_object_ref (dialog->priv->statusbar);
    g_object_set_data_full (G_OBJECT (window), "statusbar", dialog->priv->statusbar, g_object_unref);
    gtk_widget_show (dialog->priv->statusbar);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->priv->statusbar, FALSE, TRUE, 0);

    pbar = create_progress_bar (window);
    gtk_widget_hide (pbar);
    gtk_progress_set_show_text (GTK_PROGRESS (pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (pbar), 0, 0, PBAR_MAX);
    gtk_box_pack_start (GTK_BOX (dialog->priv->statusbar), pbar, FALSE, TRUE, 0);
    dialog->priv->pbar = pbar;

    g_signal_connect (dialog, "destroy", G_CALLBACK (on_dialog_destroy), NULL);
    g_signal_connect (dialog->priv->result_list, "key-press-event", G_CALLBACK (on_list_keypressed), NULL);

    g_signal_connect (dialog->priv->filter_type_combo, "changed", G_CALLBACK (on_filter_type_changed), dialog);
    g_signal_connect (dialog->priv->find_text_check, "toggled", G_CALLBACK (find_text_toggled), dialog);

    gtk_window_set_keep_above (GTK_WINDOW (dialog), FALSE);

    gtk_widget_grab_focus (dialog->priv->pattern_combo);
    dialog->priv->result_list->update_style();
}


static void gnome_cmd_search_dialog_class_init (GnomeCmdSearchDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_search_dialog_finalize;
}


GtkWidget *gnome_cmd_search_dialog_new (GnomeCmdDir *default_dir)
{
    GnomeCmdSearchDialog *dialog = (GnomeCmdSearchDialog *) g_object_new (GNOME_CMD_TYPE_SEARCH_DIALOG, NULL);

    dialog->priv->con = gnome_cmd_dir_get_connection (default_dir);

    gchar *uri = gnome_cmd_dir_get_uri_str (default_dir);

    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog->priv->dir_browser), uri);

    g_free (uri);

    return GTK_WIDGET (dialog);
}
