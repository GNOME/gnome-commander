/**
 * @file gnome-cmd-search-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2021 Uwe Scholz\n
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
#include "gnome-cmd-selection-profile-component.h"
#include "dirlist.h"
#include "filter.h"
#include "utils.h"
#include "widget-factory.h"

using namespace std;


#if 0
static char *msgs[] = {N_("Search local directories only"),
                       N_("Files _not containing text")};
#endif


#define SEARCH_JUMP_SIZE     4096U
#define SEARCH_BUFFER_SIZE  (SEARCH_JUMP_SIZE * 10U)

#define GNOME_SEARCH_TOOL_REFRESH_DURATION  50000


enum {
    GCMD_RESPONSE_PROFILES = 123,
    GCMD_RESPONSE_GOTO,
    GCMD_RESPONSE_STOP,
    GCMD_RESPONSE_FIND
};


struct SearchFileData
{
    GFileInputStream *inputStream;
    gchar          *uriString;
    guint64         size;
    guint64         offset;
    guint           len;
    gchar           mem[SEARCH_BUFFER_SIZE];     // memory to search in the content of a file
};


struct ProtectedData
{
    GList  *files;
    gchar  *msg;
    GMutex  mutex;

    ProtectedData() : files(nullptr), msg(nullptr) {}
};


struct SearchData
{
    Filter *name_filter = nullptr;
    regex_t *content_regex = nullptr;

    //the directories which we found matching files in
    GList *match_dirs = nullptr;
    GThread *thread = nullptr;
    ProtectedData pdata;
    gint update_gui_timeout_id {0};
    GPid pid = 0;
    GCancellable* cancellable = nullptr;

    gboolean search_done = TRUE;

    //stops the search routine if set to TRUE. This is done by the stop_button
    gboolean stopped = TRUE;

    // set when the search dialog is destroyed, also stops the search of course
    gboolean dialog_destroyed = FALSE;

    // searches a given directory for files that matches the criteria given by data
    void SearchDirRecursive(GnomeCmdDir *dir, long level, gboolean content_search);

    // determines if the name of a file matches an regexp
    gboolean NameMatches(const char *name)  {  return name_filter->match(name);  }

    // determines if the content of a file matches an regexp
    gboolean ContentMatches(GFile *f, GFileInfo *info);

    // loads a file in chunks and returns the content
    gboolean ReadSearchFile(SearchFileData *searchFileData);
    gboolean StartGenericSearch(GnomeCmdSearchDialog *dialog);
    gboolean StartLocalSearch(GnomeCmdSearchDialog *dialog);
};


static SearchData *search_dialog_private (GnomeCmdSearchDialog *dlg)
{
    return (SearchData *) g_object_get_data (G_OBJECT (dlg), "search-dialog-priv");
}


extern "C" SearchProfile *gnome_cmd_search_dialog_get_default_profile (GnomeCmdSearchDialog *dialog);


inline void free_search_file_data (SearchFileData *searchfileData)
{
    if (searchfileData->inputStream)
        g_object_unref(searchfileData->inputStream);

    g_free (searchfileData->uriString);
    g_free (searchfileData);
}


gboolean SearchData::ReadSearchFile(SearchFileData *searchFileData)
{
    if (stopped)     // if the stop button was pressed, let's abort here
    {
        return FALSE;
    }

    if (searchFileData->len)
    {
        if ((searchFileData->offset + searchFileData->len) >= searchFileData->size)   // end, all has been read
        {
            return FALSE;
        }

        // jump a big step backward to give the regex a chance
        searchFileData->offset += searchFileData->len - SEARCH_JUMP_SIZE;
        searchFileData->len = MIN (searchFileData->size - searchFileData->offset, SEARCH_BUFFER_SIZE - 1);
    }
    else   // first time call of this function
    {
        searchFileData->len = MIN (searchFileData->size, SEARCH_BUFFER_SIZE - 1);

        if (!g_seekable_can_seek ((GSeekable *) searchFileData->inputStream))
        {
            g_warning (_("Failed to read file %s: File is not searchable"), searchFileData->uriString);
            return FALSE;
        }
    }

    GError *error = nullptr;

    if (!g_seekable_seek ((GSeekable *) searchFileData->inputStream, searchFileData->offset, G_SEEK_SET, cancellable, &error)
        || error)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning (_("Failed to read file %s: %s"), searchFileData->uriString, error->message);
        g_error_free(error);
        return FALSE;
    }

    g_input_stream_read ((GInputStream*) searchFileData->inputStream,
                         searchFileData->mem,
                         searchFileData->len,
                         cancellable,
                         &error);
    if (error)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning (_("Failed to read file %s: %s"), searchFileData->uriString, error->message);
        g_error_free(error);
        return FALSE;
    }

    searchFileData->mem[searchFileData->len] = '\0';

    return TRUE;
}


inline gboolean SearchData::ContentMatches(GFile *f, GFileInfo *info)
{
    g_return_val_if_fail (f != nullptr, FALSE);

    guint64 size = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    if (size == 0)
        return FALSE;

    GError *error = nullptr;

    auto searchFileData = g_new0(SearchFileData, 1);
    searchFileData->size = size;
    searchFileData->uriString = g_file_get_uri (f);
    searchFileData->inputStream = g_file_read (f, cancellable, &error);

    if (error)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning (_("Failed to read file %s: %s"), searchFileData->uriString, error->message);
        free_search_file_data (searchFileData);
        g_error_free(error);
        return FALSE;
    }

    regmatch_t match;
    gboolean retValue = FALSE;

    while (ReadSearchFile(searchFileData))
        if (regexec (content_regex, searchFileData->mem, 1, &match, 0) != REG_NOMATCH) // ToDo: this only works on text files (i.e. files which don't contain character '\0')
        {
            // stop on first match
            retValue = TRUE;
            break;
        }

    free_search_file_data (searchFileData);
    return retValue;
}


inline void set_statusmsg(GnomeCmdSearchDialog *dialog, const gchar *msg)
{
    GtkLabel *status_label;
    g_object_get (dialog, "status_label", &status_label, nullptr);
    gtk_label_set_text (status_label, msg ? msg : "");
}


void SearchData::SearchDirRecursive(GnomeCmdDir *dir, long level, gboolean content_search)
{
    if (!dir)
        return;

    if (stopped)     // if the stop button was pressed, let's abort here
        return;

    // update the search status data
    if (!dialog_destroyed)
    {
        g_mutex_lock (&pdata.mutex);

        g_free (pdata.msg);
        gchar *path = gnome_cmd_dir_get_display_path (dir);
        pdata.msg = g_strdup_printf (_("Searching in: %s"), path);
        g_free (path);

        g_mutex_unlock (&pdata.mutex);
    }

    auto files = gnome_cmd_dir_get_files (dir);
    gboolean isGnomeCmdFile = TRUE;
    if (!files || gnome_cmd_file_is_local (GNOME_CMD_FILE (dir)))
    {
        // if list is not available or it's a local directory then create a new list, otherwise use already available list
        // gnome_cmd_dir_list_files is not used for creating a list, because it's tied to the GUI and that's not usable from other threads
        files = sync_dir_list(GNOME_CMD_FILE (dir)->get_file(), cancellable);
        isGnomeCmdFile = FALSE;
    }

    // first process the files
    for (GList *i=files; i; i=i->next)
    {
        if (stopped)         // if the stop button was pressed, let's abort here
            break;

        GFileInfo *info = isGnomeCmdFile ? ((GnomeCmdFile *) i->data)->get_file_info() : (GFileInfo *) i->data;

        // if the file is a regular one, it might match the search criteria
        if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_REGULAR)
        {
            // if the name doesn't match, let's go to the next file
            if (!NameMatches (g_file_info_get_display_name (info)))
                continue;

            // if the user wants to we should do some content matching here
            if (content_search)
            {
                GFile *file = isGnomeCmdFile ? ((GnomeCmdFile *) i->data)->get_file() : g_file_get_child (((GnomeCmdFile *) dir)->get_file(), g_file_info_get_name (info));
                gboolean matches = ContentMatches (file, info);
                if (!isGnomeCmdFile)
                    g_object_unref (file);
                if (!matches)
                    continue;
            }

            // the file matched the search criteria, let's add it to the list
            GnomeCmdFile *f;
            if (isGnomeCmdFile)
                f = (GnomeCmdFile *) i->data;
            else
            {
                g_object_ref (info);
                f = gnome_cmd_file_new (info, dir);
            }
            g_mutex_lock (&pdata.mutex);
            pdata.files = g_list_append (pdata.files, f->ref());
            g_mutex_unlock (&pdata.mutex);

            // also ref each directory that has a matching file
            if (g_list_index (match_dirs, dir) == -1)
                match_dirs = g_list_append (match_dirs, gnome_cmd_dir_ref (dir));
        }
    }

    // then process the directories
    if (level && !stopped)
        for (GList *i=files; i; i=i->next)
        {
            if (stopped)         // if the stop button was pressed, let's abort here
                break;

            GFileInfo *info = isGnomeCmdFile ? ((GnomeCmdFile *) i->data)->get_file_info() : (GFileInfo *) i->data;

            // if the current file is a directory, let's continue our recursion
            if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) == G_FILE_TYPE_DIRECTORY)
            {
                // we don't want to go backwards or to follow symlinks
                const gchar *displayName = g_file_info_get_display_name(info);
                if (strcmp (displayName, "..") != 0
                    && strcmp (displayName, ".") != 0
                    && !g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK))
                {
                    GnomeCmdDir *new_dir = (isGnomeCmdFile && GNOME_CMD_IS_DIR (i->data)) ? (GnomeCmdDir *) i->data : gnome_cmd_dir_new_from_gfileinfo (info, dir);

                    if (new_dir)
                    {
                        gnome_cmd_dir_ref (new_dir);
                        SearchDirRecursive(new_dir, level-1, content_search);
                        gnome_cmd_dir_unref (new_dir);
                    }
                }
            }
        }

    if (!isGnomeCmdFile)
        g_list_free_full(files, g_object_unref);
}


static gpointer perform_search_operation (GnomeCmdSearchDialog *dialog)
{
    auto data = search_dialog_private(dialog);

    GnomeCmdDir *start_dir = nullptr;
    g_object_get (dialog, "start-dir", &start_dir, nullptr);

    // unref all directories which contained matching files from last search
    if (data->match_dirs)
    {
        g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, nullptr);
        g_list_free (data->match_dirs);
        data->match_dirs = nullptr;
    }

    auto default_profile = gnome_cmd_search_dialog_get_default_profile (dialog);

    gint max_depth;
    gboolean content_search;
    g_object_get (default_profile,
        "max-depth", &max_depth,
        "content-search", &content_search,
        nullptr);

    data->SearchDirRecursive(start_dir, max_depth, content_search);

    // free regexps
    delete data->name_filter;
    data->name_filter = nullptr;

    if (content_search)
    {
        regfree (data->content_regex);
        g_free (data->content_regex);
        data->content_regex = nullptr;
    }

    gnome_cmd_dir_unref (start_dir);

    data->search_done = TRUE;

    return nullptr;
}


extern "C" void gnome_cmd_search_dialog_search_finished (GnomeCmdSearchDialog *, gboolean stopped);

static gboolean update_search_status_widgets (GnomeCmdSearchDialog *dialog)
{
    auto data = search_dialog_private(dialog);

    GnomeCmdFileList *result_list;
    GtkProgressBar *progress_bar;
    g_object_get (dialog,
        "result-list", &result_list,
        "progress-bar", &progress_bar,
        nullptr);

    // update the progress bar
    gtk_progress_bar_pulse (progress_bar);

    g_mutex_lock (&data->pdata.mutex);

    GList *files = data->pdata.files;
    data->pdata.files = nullptr;

    // update status bar with the latest message
    set_statusmsg(dialog, data->pdata.msg);

    g_mutex_unlock (&data->pdata.mutex);

    if (files)
    {
        // add all files found since last update to the list
        for (GList *i = files; i; i = i->next)
            result_list->append_file(GNOME_CMD_FILE (i->data));

        gnome_cmd_file_list_free (files);
    }

    if ((!data->search_done && !data->stopped) || data->pdata.files)
        return TRUE;

    if (!data->dialog_destroyed)
    {
        gnome_cmd_search_dialog_search_finished (dialog, data->stopped);
    }
    data->update_gui_timeout_id = 0;
    return FALSE;    // returning FALSE here stops the timeout callbacks
}


/**
 * This function gets called then the search-dialog is about the be destroyed.
 * The function waits for the last search-thread to finish and then frees the
 * data structure that has been shared between the search threads and the
 * main thread.
 */
static gboolean join_thread_func (SearchData *priv)
{
    SearchData &data = *priv;

    if (data.thread)
        g_thread_join (data.thread);

    if (data.cancellable)
        g_object_unref (data.cancellable);

    // unref all directories which contained matching files from last search
    g_mutex_lock (&data.pdata.mutex);
    if (data.match_dirs)
    {
        g_list_foreach (data.match_dirs, (GFunc) gnome_cmd_dir_unref, nullptr);
        g_list_free (data.match_dirs);
        data.match_dirs = nullptr;
    }
    g_mutex_unlock (&data.pdata.mutex);

    g_mutex_clear (&data.pdata.mutex);

    return FALSE;
}


gboolean SearchData::StartGenericSearch(GnomeCmdSearchDialog *dialog)
{
    auto default_profile = gnome_cmd_search_dialog_get_default_profile (dialog);

    gchar *filename_pattern;
    gchar *text_pattern;
    gboolean match_case;
    gint syntax;
    gboolean content_search;

    g_object_get (default_profile,
        "filename-pattern", &filename_pattern,
        "text-pattern", &text_pattern,
        "match-case", &match_case,
        "syntax", &syntax,
        "content-search", &content_search,
        nullptr);

    // create an regex for file name matching
    name_filter = new Filter(filename_pattern, match_case, (Filter::Type) syntax);

    // if we're going to search through file content create an regex for that too
    if (content_search)
    {
        content_regex = g_new0 (regex_t, 1);
        regcomp (content_regex, text_pattern, match_case ? 0 : REG_ICASE);
    }

    g_free (filename_pattern);
    g_free (text_pattern);

    cancellable = g_cancellable_new();

    thread = g_thread_new (nullptr, (GThreadFunc) perform_search_operation, dialog);

    return TRUE;
}


/**
 * local search - using findutils
 */
static gchar *BuildSearchCommand(GnomeCmdSearchDialog *dialog)
{
    auto default_profile = gnome_cmd_search_dialog_get_default_profile (dialog);

    gchar *file_pattern_utf8;
    gchar *text_pattern;
    gboolean match_case;
    gint max_depth;
    gint syntax;
    gboolean content_search;

    g_object_get (default_profile,
        "filename-pattern", &file_pattern_utf8,
        "text-pattern", &text_pattern,
        "match-case", &match_case,
        "max-depth", &max_depth,
        "syntax", &syntax,
        "content-search", &content_search,
        nullptr);

    GError *error = nullptr;

    switch (syntax)
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
                    file_pattern_utf8 = g_strconcat ("*", file_pattern_utf8, "*", nullptr);
                    g_free (tmp);
                }
            break;

        case Filter::TYPE_REGEX:
            if (gchar *tmp = file_pattern_utf8)
            {
                file_pattern_utf8 = g_strconcat (".*/.*", file_pattern_utf8, ".*", nullptr);
                g_free (tmp);
            }
            break;

        default:
            break;
    }

    gchar *file_pattern_locale = g_locale_from_utf8 (file_pattern_utf8, -1, nullptr, nullptr, &error);

    if (!file_pattern_locale)
    {
        gnome_cmd_error_message (nullptr, file_pattern_utf8, error);
        g_free (file_pattern_utf8);
        return nullptr;
    }

    gchar *text_pattern_quoted = nullptr;
    if (content_search)
    {
        const gchar *text_pattern_utf8 = text_pattern;
        gchar *text_pattern_locale = g_locale_from_utf8 (text_pattern_utf8, -1, nullptr, nullptr, &error);

        if (!text_pattern_locale)
        {
            gnome_cmd_error_message (nullptr, text_pattern_utf8, error);
            g_free (text_pattern);
            g_free (file_pattern_utf8);
            g_free (file_pattern_locale);
            return nullptr;
        }

        text_pattern_quoted = quote_if_needed (text_pattern_locale);
        g_free (text_pattern_locale);
    }
    g_free (text_pattern);

    GnomeCmdDir *start_dir = nullptr;
    g_object_get (dialog, "start-dir", &start_dir, nullptr);

    gchar *file_pattern_quoted = quote_if_needed (file_pattern_locale);
    gchar *look_in_folder_utf8 = GNOME_CMD_FILE (start_dir)->get_real_path();
    gchar *look_in_folder_locale = g_locale_from_utf8 (look_in_folder_utf8, -1, nullptr, nullptr, nullptr);

    if (!look_in_folder_locale)     // if for some reason a path was not returned, fallback to the user's home directory
        look_in_folder_locale = g_strconcat (g_get_home_dir (), G_DIR_SEPARATOR_S, nullptr);

    gchar *look_in_folder_quoted = quote_if_needed (look_in_folder_locale);

    GString *command = g_string_sized_new (512);

    g_string_append (command, "find ");
    g_string_append (command, look_in_folder_quoted);

    g_string_append (command, " -mindepth 1"); // exclude the directory itself
    if (max_depth != -1)
        g_string_append_printf (command, " -maxdepth %i", max_depth + 1);

    switch (syntax)
    {
        case Filter::TYPE_FNMATCH:
            g_string_append_printf (command, " -iname %s", file_pattern_quoted);
            break;

        case Filter::TYPE_REGEX:
            g_string_append_printf (command, " -regextype posix-extended -iregex %s", file_pattern_quoted);
            break;
        default:
            ;
    }

    if (content_search)
    {
        static const gchar GREP_COMMAND[] = "grep";

        if (match_case)
            g_string_append_printf (command, " '!' -type p -exec %s -E -q %s {} \\;", GREP_COMMAND, text_pattern_quoted);
        else
            g_string_append_printf (command, " '!' -type p -exec %s -E -q -i %s {} \\;", GREP_COMMAND, text_pattern_quoted);

        g_free (text_pattern_quoted);
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


static gboolean handle_search_command_stdout_io (GIOChannel *ioc, GIOCondition condition, GnomeCmdSearchDialog *dialog)
{
    auto data = search_dialog_private(dialog);

    gboolean broken_pipe = FALSE;

    if (condition & G_IO_IN)
    {
        GError *error = nullptr;

        GString *string = g_string_new (nullptr);

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
                status = g_io_channel_read_line_string (ioc, string, nullptr, &error);

                if (status == G_IO_STATUS_EOF)
                    broken_pipe = TRUE;
                else if (status == G_IO_STATUS_AGAIN)
                {
                    while (!data->stopped)
                        g_main_context_iteration (NULL, TRUE);
                    broken_pipe = TRUE;
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

            GnomeCmdFile *f = gnome_cmd_file_new_from_path (utf8);

            if (f)
            {
                GnomeCmdFileList *result_list;
                g_object_get (dialog, "result-list", &result_list, nullptr);
                result_list->append_file(f);
            }

            g_free (utf8);

            gulong duration;

            g_timer_elapsed (timer, &duration);

            if (duration > GNOME_SEARCH_TOOL_REFRESH_DURATION)
            {
                while (!data->stopped)
                    g_main_context_iteration (NULL, TRUE);
                broken_pipe = TRUE;
                break;

                g_timer_reset (timer);
            }
        }
        while (g_io_channel_get_buffer_condition (ioc) & G_IO_IN);

        g_string_free (string, TRUE);
        g_timer_destroy (timer);
    }

    if (!(condition & G_IO_IN) || broken_pipe)
    {
        g_io_channel_shutdown (ioc, TRUE, nullptr);

        g_spawn_close_pid(data->pid);
        data->pid = 0;

        data->search_done = TRUE;

        return FALSE;
    }

    return TRUE;
}


gboolean SearchData::StartLocalSearch(GnomeCmdSearchDialog *dialog)
{
    gchar *command = BuildSearchCommand(dialog);

    g_return_val_if_fail (command!=nullptr, FALSE);

    DEBUG ('g', "running: %s\n", command);

    GError *error = nullptr;
    gchar **argv  = nullptr;
    gint child_stdout;

    if (!g_shell_parse_argv (command, nullptr, &argv, &error))
    {
        gnome_cmd_error_message (nullptr, _("Error parsing the search command."), error);

        g_free (command);
        g_strfreev (argv);

        return FALSE;
    }

    g_free (command);

    if (!g_spawn_async_with_pipes (nullptr, argv, nullptr, GSpawnFlags (G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL), child_command_set_pgid_cb, nullptr, &pid, nullptr, &child_stdout, nullptr, &error))
    {
        gnome_cmd_error_message (nullptr, _("Error running the search command."), error);

        g_strfreev (argv);

        return FALSE;
    }

#ifdef G_OS_WIN32
    GIOChannel *ioc_stdout = g_io_channel_win32_new_fd (child_stdout);
#else
    GIOChannel *ioc_stdout = g_io_channel_unix_new (child_stdout);
#endif

    g_io_channel_set_encoding (ioc_stdout, nullptr, nullptr);
    g_io_channel_set_flags (ioc_stdout, G_IO_FLAG_NONBLOCK, nullptr);
    g_io_add_watch (ioc_stdout, GIOCondition (G_IO_IN | G_IO_HUP), (GIOFunc) handle_search_command_stdout_io, dialog);

    g_io_channel_unref (ioc_stdout);
    g_strfreev (argv);

    return TRUE;
}


extern "C" void gnome_cmd_search_dialog_stop (GnomeCmdSearchDialog *dialog)
{
    auto data = search_dialog_private (dialog);

    data->stopped = TRUE;
    if (data->cancellable)
        g_cancellable_cancel (data->cancellable);
#ifdef G_OS_UNIX
    if (data->pid)
        kill (data->pid, SIGTERM);
#endif
}


extern "C" void gnome_cmd_search_dialog_save_default_settings (GnomeCmdSearchDialog *dialog);

extern "C" gboolean gnome_cmd_search_dialog_find (GnomeCmdSearchDialog *dialog, GnomeCmdDir *start_dir)
{
    auto priv = search_dialog_private (dialog);

    GnomeCmdSelectionProfileComponent *profile_component;
    GnomeCmdFileList *result_list;
    GtkWidget *progress_bar;
    g_object_get (dialog,
        "profile-component", &profile_component,
        "result-list", &result_list,
        "progress-bar", &progress_bar,
        nullptr);

    SearchData &data = *priv;

    data.search_done = TRUE;
    data.stopped = TRUE;
    data.dialog_destroyed = FALSE;

    if (data.cancellable)
        g_cancellable_cancel (data.cancellable);
    if (data.thread)
    {
        g_thread_join (data.thread);
        data.thread = nullptr;
    }
    if (data.cancellable)
    {
        g_object_unref (data.cancellable);
        data.cancellable = nullptr;
    }

    data.content_regex = nullptr;
    data.match_dirs = nullptr;

    g_object_set (dialog, "start-dir", start_dir, nullptr);

    gnome_cmd_search_dialog_save_default_settings (dialog);

    data.search_done = FALSE;
    data.stopped = FALSE;

    result_list->clear();

    gchar *base_dir_utf8 = GNOME_CMD_FILE (start_dir)->get_real_path();
    result_list->set_base_dir(base_dir_utf8);

    auto con = gnome_cmd_file_get_connection (GNOME_CMD_FILE (start_dir));

    if (gnome_cmd_con_is_local (con) ? data.StartLocalSearch(dialog) : data.StartGenericSearch(dialog))
    {
        set_statusmsg(dialog, nullptr);
        gtk_widget_show (progress_bar);
        data.update_gui_timeout_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_search_status_widgets, dialog);

        return true;
    }

    return false;
}


extern "C" void gnome_cmd_search_dialog_goto (GnomeCmdSearchDialog *dialog, GnomeCmdFile *f)
{
    auto priv = search_dialog_private (dialog);

    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);

    auto dir = f->get_parent_dir();
    if (gnome_cmd_file_selector_is_current_tab_locked (fs))
    {
        fs->new_tab(dir);
    }
    else
    {
        fs->file_list()->set_connection(gnome_cmd_file_get_connection(f));
        gchar *dpath = gnome_cmd_file_get_path_string_through_parent (GNOME_CMD_FILE(dir));
        fs->file_list()->goto_directory(dpath);
        g_free (dpath);
    }

    fs->file_list()->focus_file(f->get_name(), TRUE);

    gtk_widget_grab_focus (GTK_WIDGET (fs->file_list()));
}


extern "C" void gnome_cmd_search_dialog_init (GnomeCmdSearchDialog *dialog)
{
    auto priv = g_new0 (SearchData, 1);
    g_object_set_data_full (G_OBJECT (dialog), "search-dialog-priv", priv, g_free);

    g_mutex_init(&priv->pdata.mutex);
}


extern "C" void gnome_cmd_search_dialog_dispose (GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);
    SearchData &data = *priv;

    if (data.update_gui_timeout_id)
    {
        g_source_remove (data.update_gui_timeout_id);
        data.update_gui_timeout_id = 0;
    }

#ifdef G_OS_UNIX
    if (data.pid)
    {
        kill (data.pid, SIGTERM);
        data.pid = 0;
    }
#endif

    // stop and wait for search thread to exit
    data.stopped = TRUE;
    data.dialog_destroyed = TRUE;
    if (data.cancellable)
        g_cancellable_cancel (data.cancellable);

    g_timeout_add (1, (GSourceFunc) join_thread_func, priv);
}
