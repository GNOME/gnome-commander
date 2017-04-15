/** 
 * @file gnome-cmd-search-dialog.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2017 Uwe Scholz\n
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
#include "gnome-cmd-manage-profiles-dialog.h"
#include "filter.h"
#include "utils.h"

using namespace std;


#if 0
static char *msgs[] = {N_("Search local directories only"),
                       N_("Files _not containing text")};
#endif


#define PBAR_MAX   50 /**< Absolute width of a progress bar */

#define SEARCH_JUMP_SIZE     4096U
#define SEARCH_BUFFER_SIZE  (SEARCH_JUMP_SIZE * 10U)

#define GNOME_SEARCH_TOOL_REFRESH_DURATION  50000


struct GnomeCmdSearchDialogClass
{
    GtkDialogClass parent_class;
};


struct SearchFileData
{
    GnomeVFSResult  result;
    gchar          *uri_str;
    GnomeVFSHandle *handle;
    gint            offset;
    guint           len;
    gchar           mem[SEARCH_BUFFER_SIZE];     /**< memory to search in the content of a file */
};


struct SearchData
{
    struct ProtectedData
    {
        GList  *files;
        gchar  *msg;
        GMutex *mutex;

        ProtectedData(): files(0), msg(0), mutex(0)     {}
    };

    GnomeCmdSearchDialog *dialog;

    GnomeCmdDir *start_dir;                     /**< the directory to start searching from */

    Filter *name_filter;
    regex_t *content_regex;
    gint context_id;                            /**< the context id of the status bar */
    GList *match_dirs;                          /**< the directories which we found matching files in */
    GThread *thread;
    ProtectedData pdata;
    gint update_gui_timeout_id;

    gboolean search_done;
    gboolean stopped;                           /**< stops the search routine if set to TRUE. This is done by the stop_button */
    gboolean dialog_destroyed;                  /**< set when the search dialog is destroyed, also stops the search of course */

    explicit SearchData(GnomeCmdSearchDialog *dlg);

    void set_statusmsg(const gchar *msg=NULL);
    gchar *build_search_command();
    void search_dir_r(GnomeCmdDir *dir, long level);  /**< searches a given directory for files that matches the criteria given by data */

    gboolean name_matches(gchar *name)   {  return name_filter->match(name);  }     /**< determines if the name of a file matches an regexp */
    gboolean content_matches(GnomeCmdFile *f);                                      /**< determines if the content of a file matches an regexp */
    gboolean read_search_file(SearchFileData *, GnomeCmdFile *f);                   /**< loads a file in chunks and returns the content */
    gboolean start_generic_search();
    gboolean start_local_search();

    static gboolean join_thread_func(SearchData *data);
};


struct GnomeCmdSearchDialog::Private
{
    SearchData data;                            /**< holds data needed by the search routines */

    GtkWidget *vbox;
    GnomeCmdSelectionProfileComponent *profile_component;
    GtkWidget *dir_browser;
    GnomeCmdFileList *result_list;
    GtkWidget *statusbar;
    GtkWidget *pbar;
    GtkWidget *profile_menu_button;

    explicit Private(GnomeCmdSearchDialog *dlg);
    ~Private();

    static gchar *translate_menu(const gchar *path, gpointer data);

    GtkWidget *create_placeholder_menu(GnomeCmdData::SearchConfig &cfg);
    GtkWidget *create_button_with_menu(gchar *label_text, GnomeCmdData::SearchConfig &cfg);

    static void manage_profiles(GnomeCmdSearchDialog::Private *priv, guint unused, GtkWidget *menu);
    static void load_profile(GnomeCmdSearchDialog::Private *priv, guint profile_idx, GtkWidget *menu);

    static gboolean on_list_keypressed (GtkWidget *result_list, GdkEventKey *event, gpointer unused);

    static void on_dialog_show (GtkWidget *widget, GnomeCmdSearchDialog *dialog);
    static void on_dialog_hide (GtkWidget *widget, GnomeCmdSearchDialog *dialog);
    static gboolean on_dialog_delete (GtkWidget *widget, GdkEvent *event, GnomeCmdSearchDialog *dialog);
    static void on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdSearchDialog *dialog);
    static void on_dialog_response (GtkDialog *window, int response_id, GnomeCmdSearchDialog *dialog);
};


inline GnomeCmdSearchDialog::Private::Private(GnomeCmdSearchDialog *dlg): data(dlg)
{
    vbox = NULL;
    profile_component = NULL;
    dir_browser = NULL;
    result_list = NULL;
    statusbar = NULL;
    pbar = NULL;
    profile_menu_button = NULL;
}


inline GnomeCmdSearchDialog::Private::~Private()
{
}


gchar *GnomeCmdSearchDialog::Private::translate_menu(const gchar *path, gpointer data)
{
    return _(path);
}


inline GtkWidget *GnomeCmdSearchDialog::Private::create_placeholder_menu(GnomeCmdData::SearchConfig &cfg)
{
    guint items_size = cfg.profiles.empty() ? 1 : cfg.profiles.size()+3;
    GtkItemFactoryEntry *items = g_try_new0 (GtkItemFactoryEntry, items_size);

    g_return_val_if_fail (items!=NULL, NULL);

    GtkItemFactoryEntry *i = items;

    i->path = g_strdup (_("/_Save Profile As..."));
    i->callback = (GtkItemFactoryCallback) manage_profiles;
    i->callback_action = TRUE;
    i->item_type = (gchar*) "<StockItem>";
    i->extra_data = GTK_STOCK_SAVE_AS;
    ++i;

    if (!cfg.profiles.empty())
    {
        i->path = g_strdup (_("/_Manage Profiles..."));
        i->callback = (GtkItemFactoryCallback) manage_profiles;
        i->item_type = (gchar*) "<StockItem>";
        i->extra_data = GTK_STOCK_EDIT;
        ++i;

        i->path = g_strdup ("/");
        i->item_type = (gchar*) "<Separator>";
        ++i;

        for (vector<GnomeCmdData::Selection>::const_iterator p=cfg.profiles.begin(); p!=cfg.profiles.end(); ++p, ++i)
        {
            i->path = g_strconcat ("/", p->name.c_str(), NULL);
            i->callback = (GtkItemFactoryCallback) load_profile;
            i->callback_action = (i-items)-3;
            i->item_type = (gchar*) "<StockItem>";
            i->extra_data = GTK_STOCK_REVERT_TO_SAVED;
        }
    }

    GtkItemFactory *ifac = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);

    gtk_item_factory_create_items (ifac, items_size, items, this);

    for (guint i=0; i<items_size; ++i)
        g_free (items[i].path);

    g_free (items);

    return gtk_item_factory_get_widget (ifac, "<main>");
}


inline GtkWidget *GnomeCmdSearchDialog::Private::create_button_with_menu(gchar *label_text, GnomeCmdData::SearchConfig &cfg)
{
    profile_menu_button = gnome_cmd_button_menu_new (label_text, create_placeholder_menu(cfg));

    return profile_menu_button;
}


void GnomeCmdSearchDialog::Private::manage_profiles(GnomeCmdSearchDialog::Private *priv, guint new_profile, GtkWidget *widget)
{
    GnomeCmdSearchDialog *dialog = (GnomeCmdSearchDialog *) gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_SEARCH_DIALOG);

    g_return_if_fail (dialog!=NULL);

    if (new_profile)
        priv->profile_component->copy();

    if (GnomeCmd::ManageProfilesDialog<GnomeCmdData::SearchConfig,GnomeCmdData::Selection,GnomeCmdSelectionProfileComponent> (*dialog,dialog->defaults,new_profile,_("Profiles"),"gnome-commander-search"))
    {
        GtkWidget *menu = widget->parent;

        gnome_cmd_button_menu_disconnect_handler (priv->profile_menu_button, menu);
        g_object_unref (gtk_item_factory_from_widget (menu));
        gnome_cmd_button_menu_connect_handler (priv->profile_menu_button, priv->create_placeholder_menu(dialog->defaults));
    }
}


void GnomeCmdSearchDialog::Private::load_profile(GnomeCmdSearchDialog::Private *priv, guint profile_idx, GtkWidget *widget)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_SEARCH_DIALOG);

    g_return_if_fail (dialog!=NULL);

    GnomeCmdData::SearchConfig &cfg = GNOME_CMD_SEARCH_DIALOG(dialog)->defaults;

    g_return_if_fail (profile_idx<cfg.profiles.size());

    cfg.default_profile = cfg.profiles[profile_idx];
    priv->profile_component->update();
}


inline SearchData::SearchData(GnomeCmdSearchDialog *dlg): dialog(dlg)
{
    start_dir = NULL;

    name_filter = NULL;
    content_regex = NULL;
    context_id = 0;
    match_dirs = NULL;
    thread = NULL;
    update_gui_timeout_id = 0;

    search_done = TRUE;
    stopped = TRUE;
    dialog_destroyed = FALSE;
}


G_DEFINE_TYPE (GnomeCmdSearchDialog, gnome_cmd_search_dialog, GTK_TYPE_DIALOG)


inline void free_search_file_data (SearchFileData *searchfile_data)
{
    if (searchfile_data->handle)
        gnome_vfs_close (searchfile_data->handle);

    g_free (searchfile_data->uri_str);
    g_free (searchfile_data);
}


gboolean SearchData::read_search_file(SearchFileData *searchfile_data, GnomeCmdFile *f)
{
    if (stopped)     // if the stop button was pressed, let's abort here
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


inline gboolean SearchData::content_matches(GnomeCmdFile *f)
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

    while (read_search_file(search_file, f))
        if (regexec (content_regex, search_file->mem, 1, &match, 0) != REG_NOMATCH)
            return TRUE;        // stop on first match

    return FALSE;
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


inline void SearchData::set_statusmsg(const gchar *msg)
{
    gtk_statusbar_push (GTK_STATUSBAR (dialog->priv->statusbar), context_id, msg ? msg : "");
}


void SearchData::search_dir_r(GnomeCmdDir *dir, long level)
{
    if (!dir)
        return;

    if (stopped)     // if the stop button was pressed, let's abort here
        return;

    // update the search status data
    if (!dialog_destroyed)
    {
        g_mutex_lock (pdata.mutex);

        g_free (pdata.msg);
        pdata.msg = g_strdup_printf (_("Searching in: %s"), gnome_cmd_dir_get_display_path (dir));

        g_mutex_unlock (pdata.mutex);
    }

    gnome_cmd_dir_list_files (dir, FALSE);

    // let's iterate through all files
    for (GList *i=gnome_cmd_dir_get_files (dir); i; i=i->next)
    {
        if (stopped)         // if the stop button was pressed, let's abort here
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
                    search_dir_r(new_dir, level-1);
                    gnome_cmd_dir_unref (new_dir);
                }
            }
        }
        else                                                            // if the file is a regular one, it might match the search criteria
            if (f->info->type == GNOME_VFS_FILE_TYPE_REGULAR)
            {
                if (!name_matches(f->info->name))                       // if the name doesn't match, let's go to the next file
                    continue;

                if (dialog->defaults.default_profile.content_search && !content_matches(f))              // if the user wants to we should do some content matching here
                    continue;

                g_mutex_lock (pdata.mutex);                             // the file matched the search criteria, let's add it to the list
                pdata.files = g_list_append (pdata.files, f->ref());
                g_mutex_unlock (pdata.mutex);

                if (g_list_index (match_dirs, dir) == -1)               // also ref each directory that has a matching file
                    match_dirs = g_list_append (match_dirs, gnome_cmd_dir_ref (dir));
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

    data->search_dir_r(data->start_dir, data->dialog->defaults.default_profile.max_depth);

    // free regexps
    delete data->name_filter;
    data->name_filter = NULL;

    if (data->dialog->defaults.default_profile.content_search)
    {
        regfree (data->content_regex);
        g_free (data->content_regex);
        data->content_regex = NULL;
    }

    gnome_cmd_dir_unref (data->start_dir);      //  FIXME:  ???
    data->start_dir = NULL;

    data->search_done = TRUE;

    return NULL;
}


static gboolean update_search_status_widgets (SearchData *data)
{
    progress_bar_update (data->dialog->priv->pbar, PBAR_MAX);        // update the progress bar

    if (data->pdata.mutex)
    {
        g_mutex_lock (data->pdata.mutex);

        GList *files = data->pdata.files;
        data->pdata.files = NULL;

        data->set_statusmsg(data->pdata.msg);                       // update status bar with the latest message

        g_mutex_unlock (data->pdata.mutex);

        GnomeCmdFileList *fl = data->dialog->priv->result_list;

        for (GList *i = files; i; i = i->next)                      // add all files found since last update to the list
            fl->append_file(GNOME_CMD_FILE (i->data));

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

        data->set_statusmsg(msg);
        g_free (msg);

        gtk_widget_hide (data->dialog->priv->pbar);

        gtk_dialog_set_response_sensitive (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_GOTO, matches>0);
        gtk_dialog_set_response_sensitive (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_STOP, FALSE);
        gtk_dialog_set_response_sensitive (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_FIND, TRUE);
	gtk_dialog_set_default_response (*data->dialog, GnomeCmdSearchDialog::GCMD_RESPONSE_FIND);
	
        if (matches)
	{
	    GnomeCmdFileList *fl = data->dialog->priv->result_list;
	    gtk_widget_grab_focus (*fl);         // set focus to result list
	    // select one file, as matches is non-zero, there should be at least one entry
	    if (!fl->get_focused_file())
	    {
		fl->select_row(0);
	    }
	}
    }

    return FALSE;    // returning FALSE here stops the timeout callbacks
}


/**
 * This function gets called then the search-dialog is about the be destroyed.
 * The function waits for the last search-thread to finish and then frees the
 * data structure that has been shared between the search threads and the
 * main thread.
 */
gboolean SearchData::join_thread_func (SearchData *data)
{
    if (data->thread)
        g_thread_join (data->thread);

    if (data->pdata.mutex)
        g_mutex_free (data->pdata.mutex);

    return FALSE;
}


gboolean SearchData::start_generic_search()
{
    // create an re for file name matching
    name_filter = new Filter(dialog->defaults.default_profile.filename_pattern.c_str(), dialog->defaults.default_profile.match_case, dialog->defaults.default_profile.syntax);

    // if we're going to search through file content create an re for that too
    if (dialog->defaults.default_profile.content_search)
    {
        content_regex = g_new0 (regex_t, 1);
        regcomp (content_regex, dialog->defaults.default_profile.text_pattern.c_str(), dialog->defaults.default_profile.match_case ? 0 : REG_ICASE);
    }

    if (!pdata.mutex)
        pdata.mutex = g_mutex_new ();

    thread = g_thread_create ((GThreadFunc) perform_search_operation, this, TRUE, NULL);

    return TRUE;
}


/**
 * local search - using findutils
 */
gchar *SearchData::build_search_command()
{
    gchar *file_pattern_utf8 = g_strdup (dialog->defaults.default_profile.filename_pattern.c_str());
    GError *error = NULL;

    switch (dialog->defaults.default_profile.syntax)
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
    gchar *look_in_folder_utf8 = GNOME_CMD_FILE (start_dir)->get_real_path();
    gchar *look_in_folder_locale = g_locale_from_utf8 (look_in_folder_utf8, -1, NULL, NULL, NULL);

    if (!look_in_folder_locale)     // if for some reason a path was not returned, fallback to the user's home directory
        look_in_folder_locale = g_strconcat (g_get_home_dir (), G_DIR_SEPARATOR_S, NULL);

    gchar *look_in_folder_quoted = quote_if_needed (look_in_folder_locale);

    GString *command = g_string_sized_new (512);

    g_string_append (command, "find ");
    g_string_append (command, look_in_folder_quoted);

    if (dialog->defaults.default_profile.max_depth!=-1)
        g_string_append_printf (command, " -maxdepth %i", dialog->defaults.default_profile.max_depth+1);

    switch (dialog->defaults.default_profile.syntax)
    {
        case Filter::TYPE_FNMATCH:
            g_string_append_printf (command, " -iname '%s'", file_pattern_utf8);
            break;

        case Filter::TYPE_REGEX:
            g_string_append_printf (command, " -regextype posix-extended -iregex '.*/.*%s.*'", file_pattern_utf8);
            break;
    }

    if (dialog->defaults.default_profile.content_search)
    {
        static const gchar GREP_COMMAND[] = "grep";

        if (dialog->defaults.default_profile.match_case)
            g_string_append_printf (command, " '!' -type p -exec %s -E -q '%s' {} \\;", GREP_COMMAND, dialog->defaults.default_profile.text_pattern.c_str());
        else
            g_string_append_printf (command, " '!' -type p -exec %s -E -q -i '%s' {} \\;", GREP_COMMAND, dialog->defaults.default_profile.text_pattern.c_str());
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


gboolean SearchData::start_local_search()
{
    gchar *command = build_search_command();

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
    g_io_add_watch (ioc_stdout, GIOCondition (G_IO_IN | G_IO_HUP), (GIOFunc) handle_search_command_stdout_io, this);

    g_io_channel_unref (ioc_stdout);
    g_strfreev (argv);

    return TRUE;
}


gboolean GnomeCmdSearchDialog::Private::on_list_keypressed(GtkWidget *result_list, GdkEventKey *event, gpointer unused)
{
    if (GNOME_CMD_FILE_LIST (result_list)->key_pressed(event) ||
        handle_list_keypress (GNOME_CMD_FILE_LIST (result_list), event))
    {
        g_signal_stop_emission_by_name (result_list, "key-press-event");
        return TRUE;
    }

    return FALSE;
}


void GnomeCmdSearchDialog::Private::on_dialog_show(GtkWidget *widget, GnomeCmdSearchDialog *dialog)
{
    dialog->priv->profile_component->update();

    dialog->priv->data.start_dir = main_win->fs(ACTIVE)->get_directory();

    gchar *uri = gnome_cmd_dir_get_uri_str (dialog->priv->data.start_dir);
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog->priv->dir_browser), uri);
    g_free (uri);
}


void GnomeCmdSearchDialog::Private::on_dialog_hide(GtkWidget *widget, GnomeCmdSearchDialog *dialog)
{
    dialog->priv->profile_component->copy();

    dialog->priv->result_list->remove_all_files();
}


gboolean GnomeCmdSearchDialog::Private::on_dialog_delete(GtkWidget *widget, GdkEvent *event, GnomeCmdSearchDialog *dialog)
{
    return event->type==GDK_DELETE;
}


void GnomeCmdSearchDialog::Private::on_dialog_size_allocate(GtkWidget *widget, GtkAllocation *allocation, GnomeCmdSearchDialog *dialog)
{
    dialog->defaults.width  = allocation->width;
    dialog->defaults.height = allocation->height;
}


void GnomeCmdSearchDialog::Private::on_dialog_response(GtkDialog *window, int response_id, GnomeCmdSearchDialog *dialog)
{
    switch (response_id)
    {
        case GCMD_RESPONSE_STOP:
            {
                dialog->priv->data.stopped = TRUE;
                gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_STOP, FALSE);
		gtk_dialog_set_default_response (*dialog, GCMD_RESPONSE_FIND);
            }
            break;

        case GCMD_RESPONSE_FIND:
            {
                SearchData &data = dialog->priv->data;

                if (data.thread)
                {
                    g_thread_join (data.thread);
                    data.thread = NULL;
                }

                data.search_done = TRUE;
                data.stopped = TRUE;
                data.dialog_destroyed = FALSE;

                data.context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (dialog->priv->statusbar), "info");
                data.content_regex = NULL;
                data.match_dirs = NULL;

                gchar *dir_str = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->priv->dir_browser));
                GnomeVFSURI *uri = gnome_vfs_uri_new (dir_str);
                g_free (dir_str);

                dir_str = gnome_vfs_unescape_string (gnome_vfs_uri_get_path (uri), NULL);
                gchar *dir_path = g_strconcat (dir_str, G_DIR_SEPARATOR_S, NULL);
                g_free (dir_str);

                GnomeCmdCon *con = gnome_cmd_dir_get_connection (data.start_dir);

                if (strncmp(dir_path, gnome_cmd_con_get_root_path (con), con->root_path->len)!=0)
                {
                    if (!gnome_vfs_uri_is_local (uri))
                    {
                        gnome_cmd_show_message (*dialog, stringify(g_strdup_printf (_("Failed to change directory outside of %s"),
                                                                                              gnome_cmd_con_get_root_path (con))));
                        gnome_vfs_uri_unref (uri);
                        g_free (dir_path);

                        break;
                    }
                    else
                        data.start_dir = gnome_cmd_dir_new (get_home_con (), gnome_cmd_con_create_path (get_home_con (), dir_path));
                }
                else
                    data.start_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dir_path + con->root_path->len));

                gnome_cmd_dir_ref (data.start_dir);

                gnome_vfs_uri_unref (uri);
                g_free (dir_path);

                // save default settings
                dialog->priv->profile_component->copy();
                if (!dialog->defaults.default_profile.filename_pattern.empty())
                {
                    gnome_cmd_data.search_defaults.name_patterns.add(dialog->defaults.default_profile.filename_pattern.c_str());
                    dialog->priv->profile_component->set_name_patterns_history(dialog->defaults.name_patterns.ents);
                }

                if (dialog->defaults.default_profile.content_search && !dialog->defaults.default_profile.text_pattern.empty())
                {
                    gnome_cmd_data.search_defaults.content_patterns.add(dialog->defaults.default_profile.text_pattern.c_str());
                    gnome_cmd_data.intviewer_defaults.text_patterns.add(dialog->defaults.default_profile.text_pattern.c_str());
                    dialog->priv->profile_component->set_content_patterns_history(dialog->defaults.content_patterns.ents);
                }

                data.search_done = FALSE;
                data.stopped = FALSE;

                dialog->priv->result_list->remove_all_files();

                gchar *base_dir_utf8 = GNOME_CMD_FILE (data.start_dir)->get_real_path();
                dialog->priv->result_list->set_base_dir(base_dir_utf8); 

                if (gnome_cmd_con_is_local (con) ? data.start_local_search() : data.start_generic_search())
                {
                    data.set_statusmsg();
                    gtk_widget_show (dialog->priv->pbar);
                    data.update_gui_timeout_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_search_status_widgets, &data);

                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_GOTO, FALSE);
                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_STOP, TRUE);
                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_FIND, FALSE);
		    gtk_dialog_set_default_response (*dialog, GCMD_RESPONSE_STOP);
                }
            }
            break;

        case GCMD_RESPONSE_GOTO:
            {
                GnomeCmdFile *f = dialog->priv->result_list->get_selected_file();

                if (!f)
                    break;

                GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
                GnomeCmdCon *con = fs->get_connection();

                gchar *fpath = f->get_path();
                gsize offset = strncmp(fpath, gnome_cmd_con_get_root_path (con), con->root_path->len)==0 ? con->root_path->len : 0;
                gchar *dpath = g_path_get_dirname (fpath + offset);

                if (fs->file_list()->locked)
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

        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_CLOSE:
            gtk_widget_hide (*dialog);
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


static void gnome_cmd_search_dialog_init (GnomeCmdSearchDialog *dialog)
{
    dialog->priv = new GnomeCmdSearchDialog::Private(dialog);

    gtk_window_set_title (*dialog, _("Search..."));
    gtk_window_set_resizable (*dialog, TRUE);
    gtk_dialog_set_has_separator (*dialog, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

    dialog->priv->vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->vbox), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), dialog->priv->vbox, TRUE, TRUE, 0);


    // file list
    GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (dialog->priv->vbox), sw, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    dialog->priv->result_list = new GnomeCmdFileList(GnomeCmdFileList::COLUMN_NAME,GTK_SORT_ASCENDING);
    gtk_widget_set_size_request (*dialog->priv->result_list, -1, 200);
    gtk_container_add (GTK_CONTAINER (sw), *dialog->priv->result_list);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->result_list), 4);


    // status
    dialog->priv->statusbar = gtk_statusbar_new ();
    gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (dialog->priv->statusbar), FALSE);
    gtk_box_pack_start (GTK_BOX (dialog->priv->vbox), dialog->priv->statusbar, FALSE, TRUE, 0);


    // progress
    dialog->priv->pbar = create_progress_bar (*dialog);
    gtk_progress_set_show_text (GTK_PROGRESS (dialog->priv->pbar), FALSE);
    gtk_progress_set_activity_mode (GTK_PROGRESS (dialog->priv->pbar), TRUE);
    gtk_progress_configure (GTK_PROGRESS (dialog->priv->pbar), 0, 0, PBAR_MAX);
    gtk_box_pack_start (GTK_BOX (dialog->priv->statusbar),dialog->priv-> pbar, FALSE, TRUE, 0);


    dialog->priv->result_list->update_style();

    gtk_widget_show (dialog->priv->vbox);
    gtk_widget_show_all (sw);
    gtk_widget_show (dialog->priv->statusbar);
    gtk_widget_hide (dialog->priv->pbar);
}


static void gnome_cmd_search_dialog_finalize (GObject *object)
{
    GnomeCmdSearchDialog *dialog = GNOME_CMD_SEARCH_DIALOG (object);
    SearchData &data = dialog->priv->data;

    if (!data.search_done)
        g_source_remove (data.update_gui_timeout_id);

    // stop and wait for search thread to exit
    data.stopped = TRUE;
    data.dialog_destroyed = TRUE;
    g_timeout_add (1, (GSourceFunc) SearchData::join_thread_func, &data);

    // unref all directories which contained matching files from last search
    if (data.pdata.mutex)
    {
        g_mutex_lock (data.pdata.mutex);
        if (data.match_dirs)
        {
            g_list_foreach (data.match_dirs, (GFunc) gnome_cmd_dir_unref, NULL);
            g_list_free (data.match_dirs);
            data.match_dirs = NULL;
        }
        g_mutex_unlock (data.pdata.mutex);
    }

    delete dialog->priv;

    G_OBJECT_CLASS (gnome_cmd_search_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_search_dialog_class_init (GnomeCmdSearchDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_search_dialog_finalize;
}


void GnomeCmdSearchDialog::show_and_set_focus()
{
    gtk_widget_show(*this);
    priv->profile_component->set_focus();
}

GnomeCmdSearchDialog::GnomeCmdSearchDialog(GnomeCmdData::SearchConfig &cfg): defaults(cfg)
{
    gtk_window_set_default_size (*this, defaults.width, defaults.height);
    gtk_window_set_transient_for (*this, *main_win);

    GtkWidget *button = priv->create_button_with_menu(_("Profiles..."), cfg);

    gtk_dialog_add_action_widget (*this, button, GCMD_RESPONSE_PROFILES);

    gtk_dialog_add_buttons (*this,
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                            GTK_STOCK_JUMP_TO, GCMD_RESPONSE_GOTO,
                            GTK_STOCK_STOP, GCMD_RESPONSE_STOP,
                            GTK_STOCK_FIND, GCMD_RESPONSE_FIND,
                            NULL);

    gtk_dialog_set_response_sensitive (*this, GCMD_RESPONSE_GOTO, FALSE);
    gtk_dialog_set_response_sensitive (*this, GCMD_RESPONSE_STOP, FALSE);

    gtk_dialog_set_default_response (*this, GCMD_RESPONSE_FIND);

    // search in
    priv->dir_browser =  gtk_file_chooser_button_new (_("Select Directory"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->dir_browser), FALSE);

    priv->profile_component = new GnomeCmdSelectionProfileComponent(cfg.default_profile, priv->dir_browser, _("_Look in folder:"));

    gtk_box_pack_start (GTK_BOX (priv->vbox), *priv->profile_component, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (priv->vbox), *priv->profile_component, 0);

    if (!defaults.name_patterns.empty())
        priv->profile_component->set_name_patterns_history(defaults.name_patterns.ents);

    priv->profile_component->set_content_patterns_history(defaults.content_patterns.ents);

    priv->profile_component->set_default_activation(*this);

    gtk_widget_show_all (button);
    gtk_widget_show (*priv->profile_component);

    g_signal_connect (priv->result_list, "key-press-event", G_CALLBACK (Private::on_list_keypressed), this);

    g_signal_connect (this, "show", G_CALLBACK (Private::on_dialog_show), this);
    g_signal_connect (this, "hide", G_CALLBACK (Private::on_dialog_hide), this);
    g_signal_connect (this, "delete-event", G_CALLBACK (Private::on_dialog_delete), this);
    g_signal_connect (this, "size-allocate", G_CALLBACK (Private::on_dialog_size_allocate), this);
    g_signal_connect (this, "response", G_CALLBACK (Private::on_dialog_response), this);
}


GnomeCmdSearchDialog::~GnomeCmdSearchDialog()
{
}
