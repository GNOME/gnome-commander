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


extern "C" GType gnome_cmd_directory_button_get_type();


#if 0
static char *msgs[] = {N_("Search local directories only"),
                       N_("Files _not containing text")};
#endif


#define PBAR_MAX   50 // Absolute width of a progress bar

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


struct SearchData
{
    struct ProtectedData
    {
        GList  *files;
        gchar  *msg;
        GMutex  mutex;

        ProtectedData() : files(nullptr), msg(nullptr) {}
    };

    GnomeCmdSearchDialog *dialog;

    // the directory to start searching from
    GnomeCmdDir *start_dir = nullptr;

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

    void set_statusmsg(const gchar *msg = nullptr);
    gchar *BuildSearchCommand();

    // searches a given directory for files that matches the criteria given by data
    void SearchDirRecursive(GnomeCmdDir *dir, long level);

    // determines if the name of a file matches an regexp
    gboolean NameMatches(const char *name)  {  return name_filter->match(name);  }

    // determines if the content of a file matches an regexp
    gboolean ContentMatches(GFile *f, GFileInfo *info);

    // loads a file in chunks and returns the content
    gboolean ReadSearchFile(SearchFileData *searchFileData);
    gboolean StartGenericSearch();
    gboolean StartLocalSearch();
};


struct GnomeCmdSearchDialogPrivate
{
    // holds data needed by the search routines
    SearchData data;

    GtkWidget *grid;
    GtkSizeGroup *labels_size_group;
    GnomeCmdSelectionProfileComponent *profile_component;
    GtkWidget *dir_browser;
    GnomeCmdFileList *result_list;
    GtkWidget *status_label;
    GtkWidget *pbar;
    GtkWidget *profile_menu_button;

    GnomeCmdData::SearchConfig *defaults;
};


static GnomeCmdSearchDialogPrivate *search_dialog_private (GnomeCmdSearchDialog *dlg)
{
    return (GnomeCmdSearchDialogPrivate *) g_object_get_data (G_OBJECT (dlg), "search-dialog-priv");
}


static GMenu *create_placeholder_menu(GnomeCmdData::SearchConfig &cfg)
{
    GMenu *menu = g_menu_new ();

    g_menu_append (menu, _("_Save Profile As…"), "search.save-profile");

    if (!cfg.profiles.empty())
    {
        g_menu_append (menu, _("_Manage Profiles…"), "search.manage-profiles");

        GMenu *profiles = g_menu_new ();

        gint i = 0;
        for (vector<GnomeCmdData::SearchProfile>::const_iterator p=cfg.profiles.begin(); p!=cfg.profiles.end(); ++p, ++i)
        {
            GMenuItem *item = g_menu_item_new (p->name.c_str(), nullptr);
            g_menu_item_set_action_and_target (item, "search.load-profile", "i", i);
            g_menu_append_item (profiles, item);
        }
        g_menu_append_section (menu, nullptr, G_MENU_MODEL (profiles));
    }

    return menu;
}


extern "C" void gnome_cmd_search_dialog_update_profile_menu (GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);
    gtk_menu_button_set_menu_model (
        GTK_MENU_BUTTON (priv->profile_menu_button),
        G_MENU_MODEL (create_placeholder_menu(*priv->defaults)));
}


extern "C" void gnome_cmd_search_dialog_do_manage_profiles(GnomeCmdSearchDialog *dialog, GnomeCmdData::SearchConfig *cfg, gboolean new_profile);

static void do_manage_profiles(GnomeCmdSearchDialog *dialog, bool new_profile)
{
    auto priv = search_dialog_private (dialog);
    if (new_profile)
        gnome_cmd_search_profile_component_copy (priv->profile_component);

    gnome_cmd_search_dialog_do_manage_profiles (dialog, priv->defaults, new_profile);
}

static void save_profile(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdSearchDialog*>(user_data);
    do_manage_profiles(dialog, true);
}


static void manage_profiles(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdSearchDialog*>(user_data);
    do_manage_profiles(dialog, false);
}


static void load_profile(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto dialog = static_cast<GnomeCmdSearchDialog*>(user_data);
    auto priv = search_dialog_private (dialog);
    gsize profile_idx = g_variant_get_int32 (parameter);

    GnomeCmdData::SearchConfig &cfg = *priv->defaults;

    g_return_if_fail (profile_idx < cfg.profiles.size());

    cfg.default_profile = cfg.profiles[profile_idx];
    gnome_cmd_search_profile_component_update (priv->profile_component);
}


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


inline void SearchData::set_statusmsg(const gchar *msg)
{
    auto priv = search_dialog_private (dialog);
    gtk_label_set_text (GTK_LABEL (priv->status_label), msg ? msg : "");
}


void SearchData::SearchDirRecursive(GnomeCmdDir *dir, long level)
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
            auto priv = search_dialog_private (dialog);
            if (priv->defaults->default_profile.content_search)
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
                        SearchDirRecursive(new_dir, level-1);
                        gnome_cmd_dir_unref (new_dir);
                    }
                }
            }
        }

    if (!isGnomeCmdFile)
        g_list_free_full(files, g_object_unref);
}


static gpointer perform_search_operation (SearchData *data)
{
    auto priv = search_dialog_private (data->dialog);

    GnomeCmdDir *start_dir = data->start_dir;

    // unref all directories which contained matching files from last search
    if (data->match_dirs)
    {
        g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, nullptr);
        g_list_free (data->match_dirs);
        data->match_dirs = nullptr;
    }

    data->SearchDirRecursive(start_dir, priv->defaults->default_profile.max_depth);

    // free regexps
    delete data->name_filter;
    data->name_filter = nullptr;

    if (priv->defaults->default_profile.content_search)
    {
        regfree (data->content_regex);
        g_free (data->content_regex);
        data->content_regex = nullptr;
    }

    gnome_cmd_dir_unref (start_dir);

    data->search_done = TRUE;

    return nullptr;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static gboolean update_search_status_widgets (SearchData *data)
{
    auto priv = search_dialog_private (data->dialog);

    // update the progress bar
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->pbar));

    g_mutex_lock (&data->pdata.mutex);

    GList *files = data->pdata.files;
    data->pdata.files = nullptr;

    // update status bar with the latest message
    data->set_statusmsg(data->pdata.msg);

    g_mutex_unlock (&data->pdata.mutex);

    if (files)
    {
        GnomeCmdFileList *fl = priv->result_list;

        // add all files found since last update to the list
        for (GList *i = files; i; i = i->next)
            fl->append_file(GNOME_CMD_FILE (i->data));

        gnome_cmd_file_list_free (files);
    }

    if ((!data->search_done && !data->stopped) || data->pdata.files)
        return TRUE;

    if (!data->dialog_destroyed)
    {
        int matches = priv->result_list->size();

        gchar *fmt = data->stopped ? ngettext("Found %d match — search aborted", "Found %d matches — search aborted", matches) :
                                     ngettext("Found %d match", "Found %d matches", matches);

        gchar *msg = g_strdup_printf (fmt, matches);

        data->set_statusmsg(msg);
        g_free (msg);

        gtk_widget_hide (priv->pbar);

        gtk_dialog_set_response_sensitive (GTK_DIALOG (data->dialog), GCMD_RESPONSE_GOTO, matches>0);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (data->dialog), GCMD_RESPONSE_STOP, FALSE);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (data->dialog), GCMD_RESPONSE_FIND, TRUE);
        gtk_dialog_set_default_response (GTK_DIALOG (data->dialog), GCMD_RESPONSE_FIND);

        if (matches)
        {
            GnomeCmdFileList *flmatches = priv->result_list;
            gtk_widget_grab_focus (*flmatches);         // set focus to result list
            // select one file, as matches is non-zero, there should be at least one entry
            if (!flmatches->get_focused_file())
            {
                flmatches->select_row(0);
            }
        }
    }
    data->update_gui_timeout_id = 0;
    return FALSE;    // returning FALSE here stops the timeout callbacks
}

#pragma GCC diagnostic pop

/**
 * This function gets called then the search-dialog is about the be destroyed.
 * The function waits for the last search-thread to finish and then frees the
 * data structure that has been shared between the search threads and the
 * main thread.
 */
static gboolean join_thread_func (GnomeCmdSearchDialogPrivate *priv)
{
    SearchData &data = priv->data;

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


gboolean SearchData::StartGenericSearch()
{
    auto priv = search_dialog_private (dialog);

    // create an regex for file name matching
    name_filter = new Filter(priv->defaults->default_profile.filename_pattern.c_str(), priv->defaults->default_profile.match_case, priv->defaults->default_profile.syntax);

    // if we're going to search through file content create an regex for that too
    if (priv->defaults->default_profile.content_search)
    {
        content_regex = g_new0 (regex_t, 1);
        regcomp (content_regex, priv->defaults->default_profile.text_pattern.c_str(), priv->defaults->default_profile.match_case ? 0 : REG_ICASE);
    }

    cancellable = g_cancellable_new();

    gnome_cmd_dir_ref (start_dir);
    thread = g_thread_new (nullptr, (GThreadFunc) perform_search_operation, this);

    return TRUE;
}


/**
 * local search - using findutils
 */
gchar *SearchData::BuildSearchCommand()
{
    auto priv = search_dialog_private (dialog);

    gchar *file_pattern_utf8 = g_strdup (priv->defaults->default_profile.filename_pattern.c_str());
    GError *error = nullptr;

    switch (priv->defaults->default_profile.syntax)
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
    if (priv->defaults->default_profile.content_search)
    {
        const gchar *text_pattern_utf8 = priv->defaults->default_profile.text_pattern.c_str();
        gchar *text_pattern_locale = g_locale_from_utf8 (text_pattern_utf8, -1, nullptr, nullptr, &error);

        if (!text_pattern_locale)
        {
            gnome_cmd_error_message (nullptr, text_pattern_utf8, error);
            g_free (file_pattern_utf8);
            g_free (file_pattern_locale);
            return nullptr;
        }

        text_pattern_quoted = quote_if_needed (text_pattern_locale);
        g_free (text_pattern_locale);
    }

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
    if (priv->defaults->default_profile.max_depth!=-1)
        g_string_append_printf (command, " -maxdepth %i", priv->defaults->default_profile.max_depth+1);

    switch (priv->defaults->default_profile.syntax)
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

    if (priv->defaults->default_profile.content_search)
    {
        static const gchar GREP_COMMAND[] = "grep";

        if (priv->defaults->default_profile.match_case)
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


static gboolean handle_search_command_stdout_io (GIOChannel *ioc, GIOCondition condition, SearchData *data)
{
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
                auto priv = search_dialog_private (data->dialog);
                priv->result_list->append_file(f);
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


gboolean SearchData::StartLocalSearch()
{
    gchar *command = BuildSearchCommand();

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
    g_io_add_watch (ioc_stdout, GIOCondition (G_IO_IN | G_IO_HUP), (GIOFunc) handle_search_command_stdout_io, this);

    g_io_channel_unref (ioc_stdout);
    g_strfreev (argv);

    return TRUE;
}


static void on_dialog_show(GtkWidget *widget, GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);

    gnome_cmd_search_profile_component_update (priv->profile_component);

    priv->data.start_dir = main_win->fs(ACTIVE)->get_directory();

    g_object_set (priv->dir_browser, "file", GNOME_CMD_FILE(priv->data.start_dir)->get_file(), nullptr);
}


static void on_dialog_hide(GtkWidget *widget, GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);

    gnome_cmd_search_profile_component_copy (priv->profile_component);

    priv->result_list->clear();
}


extern "C" void gnome_cmd_viewer_search_text_add_to_history(const gchar *value);


static void on_dialog_response(GtkDialog *window, int response_id, GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);

    switch (response_id)
    {
        case GCMD_RESPONSE_STOP:
            {
                priv->data.stopped = TRUE;
                gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GCMD_RESPONSE_STOP, FALSE);
                gtk_dialog_set_default_response (GTK_DIALOG (dialog), GCMD_RESPONSE_FIND);
                if (priv->data.cancellable)
                    g_cancellable_cancel (priv->data.cancellable);
#ifdef G_OS_UNIX
                if (priv->data.pid)
                    kill (priv->data.pid, SIGTERM);
#endif
            }
            break;

        case GCMD_RESPONSE_FIND:
            {
                SearchData &data = priv->data;

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

                GFile *file = nullptr;
                g_object_get (priv->dir_browser, "file", &file, nullptr);
                auto uriString = file ? g_file_get_uri (file) : nullptr;
                auto gUri = g_uri_parse (uriString, G_URI_FLAGS_NONE, nullptr);

                g_free (uriString);

                auto dirPathString = g_uri_get_path (gUri);

                GnomeCmdCon *con = gnome_cmd_file_get_connection (GNOME_CMD_FILE (data.start_dir));

                const gchar *root_path = g_uri_get_path (gnome_cmd_con_get_uri (con));
                if (strncmp(dirPathString, root_path, strlen(root_path)) == 0)
                {
                    // starts with `root_path` of a `con` => it belongs to `con`
                    data.start_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dirPathString + strlen(root_path)));
                }
                else if (g_strcmp0(g_uri_get_scheme (gUri), "file") == 0)
                {
                    // has "file" scheme => it belongs to home connection
                    data.start_dir = gnome_cmd_dir_new (get_home_con (), gnome_cmd_con_create_path (get_home_con (), dirPathString));
                }
                else
                {
                    g_uri_unref (gUri);
                    string message = stringify(g_strdup_printf (_("Failed to change directory outside of %s"), root_path));
                    gnome_cmd_show_message (GTK_WINDOW (dialog), message.c_str());
                    break;
                }

                g_uri_unref (gUri);

                gnome_cmd_dir_ref (data.start_dir);

                // save default settings
                gnome_cmd_search_profile_component_copy (priv->profile_component);
                if (!priv->defaults->default_profile.filename_pattern.empty())
                {
                    gnome_cmd_data.search_defaults.name_patterns.add(priv->defaults->default_profile.filename_pattern.c_str());
                    gnome_cmd_search_profile_component_set_name_patterns_history (priv->profile_component, priv->defaults->name_patterns.ents);
                }

                if (priv->defaults->default_profile.content_search && !priv->defaults->default_profile.text_pattern.empty())
                {
                    gnome_cmd_data.search_defaults.content_patterns.add(priv->defaults->default_profile.text_pattern.c_str());
                    gnome_cmd_viewer_search_text_add_to_history(priv->defaults->default_profile.text_pattern.c_str());
                    gnome_cmd_search_profile_component_set_content_patterns_history (priv->profile_component, priv->defaults->content_patterns.ents);
                }

                data.search_done = FALSE;
                data.stopped = FALSE;

                priv->result_list->clear();

                gchar *base_dir_utf8 = GNOME_CMD_FILE (data.start_dir)->get_real_path();
                priv->result_list->set_base_dir(base_dir_utf8);

                if (gnome_cmd_con_is_local (con) ? data.StartLocalSearch() : data.StartGenericSearch())
                {
                    data.set_statusmsg();
                    gtk_widget_show (priv->pbar);
                    data.update_gui_timeout_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_search_status_widgets, &data);

                    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GCMD_RESPONSE_GOTO, FALSE);
                    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GCMD_RESPONSE_STOP, TRUE);
                    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GCMD_RESPONSE_FIND, FALSE);
                    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GCMD_RESPONSE_STOP);
                }

                gnome_cmd_dir_unref (data.start_dir);
            }
            break;

        case GCMD_RESPONSE_GOTO:
            {
                GnomeCmdFile *f = priv->result_list->get_selected_file();

                if (!f)
                    break;

                GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
                GnomeCmdCon *con = fs->get_connection();

                const gchar *root_path = g_uri_get_path (gnome_cmd_con_get_uri (con));
                size_t root_path_len = strlen (root_path);

                gchar *fpath = f->GetPathStringThroughParent();
                gsize offset = strncmp(fpath, root_path, root_path_len)==0 ? root_path_len : 0;
                gchar *dpath = g_path_get_dirname (fpath + offset);

                if (gnome_cmd_file_selector_is_current_tab_locked (fs))
                {
                    GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dpath));
                    fs->new_tab(dir);
                }
                else
                    fs->file_list()->goto_directory(dpath);

                fs->file_list()->focus_file(f->get_name(), TRUE);

                g_free (fpath);
                g_free (dpath);
            }

#if defined (__GNUC__) && __GNUC__ >= 7
        __attribute__ ((fallthrough));
#endif
        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_CLOSE:
            gtk_widget_hide (GTK_WIDGET (dialog));
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-search");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GCMD_RESPONSE_PROFILES:
            break;

        default :
            g_assert_not_reached ();
    }
}


extern "C" void gnome_cmd_search_dialog_init (GnomeCmdSearchDialog *dialog)
{
    static GActionEntry action_entries[] = {
        { "save-profile", save_profile },
        { "manage-profiles", manage_profiles },
        { "load-profile", load_profile, "i" }
    };
    GSimpleActionGroup* action_group = g_simple_action_group_new ();
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), action_entries, G_N_ELEMENTS(action_entries), dialog);
    gtk_widget_insert_action_group (GTK_WIDGET (dialog), "search", G_ACTION_GROUP (action_group));

    auto priv = g_new0 (GnomeCmdSearchDialogPrivate, 1);
    priv->data.dialog = dialog;
    g_object_set_data_full (G_OBJECT (dialog), "search-dialog-priv", priv, g_free);

    auto grid = gtk_grid_new ();
    priv->grid = grid;
    gtk_widget_set_margin_top (grid, 12);
    gtk_widget_set_margin_bottom (grid, 12);
    gtk_widget_set_margin_start (grid, 12);
    gtk_widget_set_margin_end (grid, 12);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid);

    priv->labels_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    // search in
    priv->dir_browser = GTK_WIDGET (g_object_new (gnome_cmd_directory_button_get_type (), nullptr));
    auto dir_browser_label = create_label_with_mnemonic (GTK_WIDGET (dialog), _("_Look in folder:"), priv->dir_browser);
    gtk_label_set_xalign (GTK_LABEL (dir_browser_label), 0.0);
    gtk_size_group_add_widget (priv->labels_size_group, dir_browser_label);

    gtk_grid_attach (GTK_GRID (grid), dir_browser_label, 0, 0, 1, 1);
    gtk_widget_set_hexpand (priv->dir_browser, TRUE);
    gtk_grid_attach (GTK_GRID (grid), priv->dir_browser, 1, 0, 1, 1);

    // file list
    GtkWidget *sw = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (sw, TRUE);
    gtk_grid_attach (GTK_GRID (grid), sw, 0, 2, 2, 1);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    priv->result_list = new GnomeCmdFileList(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING);
    gtk_widget_set_size_request (*priv->result_list, -1, 200);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), *priv->result_list);

    // status
    auto statusbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_grid_attach (GTK_GRID (grid), statusbar, 0, 3, 2, 1);

    priv->status_label = gtk_label_new(nullptr);
    gtk_box_append (GTK_BOX (statusbar), priv->status_label);

    // progress
    priv->pbar = create_progress_bar (GTK_WIDGET (dialog));
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (priv->pbar), FALSE);
    gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (priv->pbar), 1.0 / (gdouble) PBAR_MAX);
    gtk_box_append (GTK_BOX (statusbar), priv->pbar);


    priv->result_list->update_style();

    gtk_widget_hide (priv->pbar);

    g_mutex_init(&priv->data.pdata.mutex);
}


extern "C" void gnome_cmd_search_dialog_dispose (GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);
    SearchData &data = priv->data;

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


void gnome_cmd_search_dialog_show_and_set_focus(GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);
    gtk_widget_show (GTK_WIDGET (dialog));
    gnome_cmd_search_profile_component_set_focus (priv->profile_component);
}


void gnome_cmd_search_dialog_update_style(GnomeCmdSearchDialog *dialog)
{
    auto priv = search_dialog_private (dialog);
    priv->result_list->update_style();
}


GnomeCmdSearchDialog *gnome_cmd_search_dialog_new (GnomeCmdData::SearchConfig *cfg)
{
    auto dialog = (GnomeCmdSearchDialog *) g_object_new (gnome_cmd_search_dialog_get_type (), nullptr);

    auto priv = search_dialog_private (dialog);
    priv->defaults = cfg;

    if (gnome_cmd_data.options.search_window_is_transient)
    {
        gtk_window_set_transient_for (GTK_WINDOW (dialog), *main_win);
    }

    priv->profile_menu_button = gtk_menu_button_new ();
    gtk_menu_button_set_label (GTK_MENU_BUTTON (priv->profile_menu_button), _("Profiles…"));
    gtk_menu_button_set_menu_model (
        GTK_MENU_BUTTON (priv->profile_menu_button),
        G_MENU_MODEL (create_placeholder_menu(*cfg)));

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), priv->profile_menu_button, GCMD_RESPONSE_PROFILES);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            _("_Help"), GTK_RESPONSE_HELP,
                            _("_Close"), GTK_RESPONSE_CLOSE,
                            _("_Jump to"), GCMD_RESPONSE_GOTO,
                            _("_Stop"), GCMD_RESPONSE_STOP,
                            _("_Find"), GCMD_RESPONSE_FIND,
                            nullptr);

    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GCMD_RESPONSE_GOTO, FALSE);
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GCMD_RESPONSE_STOP, FALSE);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GCMD_RESPONSE_FIND);

    gtk_window_set_hide_on_close (GTK_WINDOW (dialog), TRUE);

    priv->profile_component = gnome_cmd_search_profile_component_new(&cfg->default_profile, priv->labels_size_group);
    gtk_grid_attach (GTK_GRID (priv->grid), GTK_WIDGET (priv->profile_component), 0, 1, 2, 1);

    if (!priv->defaults->name_patterns.empty())
        gnome_cmd_search_profile_component_set_name_patterns_history (priv->profile_component, priv->defaults->name_patterns.ents);

    gnome_cmd_search_profile_component_set_content_patterns_history (priv->profile_component, priv->defaults->content_patterns.ents);

    gnome_cmd_search_profile_component_set_default_activation (priv->profile_component, GTK_WINDOW (dialog));

    g_signal_connect (dialog, "show", G_CALLBACK (on_dialog_show), dialog);
    g_signal_connect (dialog, "hide", G_CALLBACK (on_dialog_hide), dialog);
    g_signal_connect (dialog, "response", G_CALLBACK (on_dialog_response), dialog);

    return dialog;
}
