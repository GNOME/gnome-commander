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
#include "gnome-cmd-manage-profiles-dialog.h"
#include "dirlist.h"
#include "filter.h"
#include "utils.h"

using namespace std;


#if 0
static char *msgs[] = {N_("Search local directories only"),
                       N_("Files _not containing text")};
#endif


#define PBAR_MAX   50 // Absolute width of a progress bar

#define SEARCH_JUMP_SIZE     4096U
#define SEARCH_BUFFER_SIZE  (SEARCH_JUMP_SIZE * 10U)

#define GNOME_SEARCH_TOOL_REFRESH_DURATION  50000


struct GnomeCmdSearchDialogClass
{
    GtkDialogClass parent_class;
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

    // the context id of the status bar
    gint context_id = 0;

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

    explicit SearchData(GnomeCmdSearchDialog *dlg);

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


struct GnomeCmdSearchDialog::Private
{
    // holds data needed by the search routines
    SearchData data;

    GtkWidget *vbox;
    GnomeCmdSelectionProfileComponent *profile_component;
    GtkWidget *dir_browser;
    GnomeCmdFileList *result_list;
    GtkWidget *statusbar;
    GtkWidget *pbar;
    GtkWidget *profile_menu_button;

    explicit Private(GnomeCmdSearchDialog *dlg);
    ~Private();

    GtkWidget *create_placeholder_menu(GnomeCmdData::SearchConfig &cfg);
    GtkWidget *create_button_with_menu(gchar *label_text, GnomeCmdData::SearchConfig &cfg);

    static void save_profile(GtkWidget *menu_item, GnomeCmdSearchDialog::Private *priv);
    static void manage_profiles(GtkWidget *menu_item, GnomeCmdSearchDialog::Private *priv);
    static void do_manage_profiles(GnomeCmdSearchDialog::Private *priv, bool new_profile, GtkWidget *menu_item);
    static void load_profile(GtkWidget *menu_item, GnomeCmdSearchDialog::Private *priv);

    static gboolean join_thread_func(GnomeCmdSearchDialog::Private *priv);

    static gboolean on_list_keypressed (GtkWidget *result_list, GdkEventKey *event, gpointer unused);

    static void on_dialog_show (GtkWidget *widget, GnomeCmdSearchDialog *dialog);
    static void on_dialog_hide (GtkWidget *widget, GnomeCmdSearchDialog *dialog);
    static gboolean on_dialog_delete (GtkWidget *widget, GdkEvent *event, GnomeCmdSearchDialog *dialog);
    static void on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdSearchDialog *dialog);
    static void on_dialog_response (GtkDialog *window, int response_id, GnomeCmdSearchDialog *dialog);
};


inline GnomeCmdSearchDialog::Private::Private(GnomeCmdSearchDialog *dlg): data(dlg)
{
    vbox = nullptr;
    profile_component = nullptr;
    dir_browser = nullptr;
    result_list = nullptr;
    statusbar = nullptr;
    pbar = nullptr;
    profile_menu_button = nullptr;
}


inline GnomeCmdSearchDialog::Private::~Private()
{
}


inline GtkWidget *GnomeCmdSearchDialog::Private::create_placeholder_menu(GnomeCmdData::SearchConfig &cfg)
{
    GtkMenu *menu = GTK_MENU (gtk_menu_new ());

    {
        GtkImageMenuItem *item = GTK_IMAGE_MENU_ITEM (gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE_AS, NULL));
        gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item), TRUE);
        gtk_menu_item_set_label (GTK_MENU_ITEM (item), _("_Save Profile As…"));
        g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (save_profile), this);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (item));
    }

    if (!cfg.profiles.empty())
    {
        {
            GtkImageMenuItem *item = GTK_IMAGE_MENU_ITEM (gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT, NULL));
            gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item), TRUE);
            gtk_menu_item_set_label (GTK_MENU_ITEM (item), _("_Manage Profiles…"));
            g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (manage_profiles), this);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (item));
        }

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

        gsize i = 0;
        for (vector<GnomeCmdData::SearchProfile>::const_iterator p=cfg.profiles.begin(); p!=cfg.profiles.end(); ++p, ++i)
        {
            GtkImageMenuItem *item = GTK_IMAGE_MENU_ITEM (gtk_image_menu_item_new_from_stock (GTK_STOCK_REVERT_TO_SAVED, NULL));
            gtk_menu_item_set_label (GTK_MENU_ITEM (item), p->name.c_str());
            g_object_set_data (G_OBJECT (item), "profile-index", reinterpret_cast<gpointer> (i));
            g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (load_profile), this);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (item));
        }
    }

    gtk_widget_show_all (GTK_WIDGET (menu));
    return GTK_WIDGET (menu);
}


inline GtkWidget *GnomeCmdSearchDialog::Private::create_button_with_menu(gchar *label_text, GnomeCmdData::SearchConfig &cfg)
{
    profile_menu_button = gnome_cmd_button_menu_new (label_text, create_placeholder_menu(cfg));

    return profile_menu_button;
}


void GnomeCmdSearchDialog::Private::save_profile(GtkWidget *menu_item, GnomeCmdSearchDialog::Private *priv)
{
    GnomeCmdSearchDialog::Private::do_manage_profiles(priv, true, menu_item);
}


void GnomeCmdSearchDialog::Private::manage_profiles(GtkWidget *menu_item, GnomeCmdSearchDialog::Private *priv)
{
    GnomeCmdSearchDialog::Private::do_manage_profiles(priv, false, menu_item);
}


void GnomeCmdSearchDialog::Private::do_manage_profiles(GnomeCmdSearchDialog::Private *priv, bool new_profile, GtkWidget *menu_item)
{
    auto *dialog = reinterpret_cast<GnomeCmdSearchDialog*> (gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_SEARCH_DIALOG));

    g_return_if_fail (dialog != nullptr);

    if (new_profile)
        priv->profile_component->copy();

    if (GnomeCmd::ManageProfilesDialog<GnomeCmdData::SearchConfig,GnomeCmdData::SearchProfile,GnomeCmdSelectionProfileComponent> (*dialog,dialog->defaults,new_profile,_("Profiles"),"gnome-commander-search"))
    {
        GtkWidget *menu = gtk_widget_get_parent (menu_item);

        gnome_cmd_button_menu_disconnect_handler (priv->profile_menu_button, menu);
        gnome_cmd_button_menu_connect_handler (priv->profile_menu_button, priv->create_placeholder_menu(dialog->defaults));
    }
}


void GnomeCmdSearchDialog::Private::load_profile(GtkWidget *menu_item, GnomeCmdSearchDialog::Private *priv)
{
    GtkWidget *dialog = gtk_widget_get_ancestor (priv->profile_menu_button, GNOME_CMD_TYPE_SEARCH_DIALOG);

    g_return_if_fail (dialog!=nullptr);

    GnomeCmdData::SearchConfig &cfg = GNOME_CMD_SEARCH_DIALOG(dialog)->defaults;

    gsize profile_idx = reinterpret_cast<gsize> (g_object_get_data (G_OBJECT (menu_item), "profile-index"));

    g_return_if_fail (profile_idx<cfg.profiles.size());

    cfg.default_profile = cfg.profiles[profile_idx];
    priv->profile_component->update();
}


inline SearchData::SearchData(GnomeCmdSearchDialog *dlg): dialog(dlg)
{
}


G_DEFINE_TYPE (GnomeCmdSearchDialog, gnome_cmd_search_dialog, GTK_TYPE_DIALOG)


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


inline gboolean handle_list_keypress (GnomeCmdFileList *fl, GdkEventKey *event)
{
    switch (event->keyval)
    {
        case GDK_F3:
            gnome_cmd_file_list_view (fl, gnome_cmd_data.options.use_internal_viewer);
            return TRUE;

        case GDK_F4:
            gnome_cmd_file_list_edit (fl);
            return TRUE;
        default:
            return FALSE;
    }
}


inline void SearchData::set_statusmsg(const gchar *msg)
{
    gtk_statusbar_push (GTK_STATUSBAR (dialog->priv->statusbar), context_id, msg ? msg : "");
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
    if (!files || gnome_cmd_dir_is_local (dir))
    {
        // if list is not available or it's a local directory then create a new list, otherwise use already available list
        // gnome_cmd_dir_list_files is not used for creating a list, beacause it's tied to the GUI and that's not usable from other threads
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
            if (dialog->defaults.default_profile.content_search)
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
    GnomeCmdDir *start_dir = data->start_dir;

    // unref all directories which contained matching files from last search
    if (data->match_dirs)
    {
        g_list_foreach (data->match_dirs, (GFunc) gnome_cmd_dir_unref, nullptr);
        g_list_free (data->match_dirs);
        data->match_dirs = nullptr;
    }

    data->SearchDirRecursive(start_dir, data->dialog->defaults.default_profile.max_depth);

    // free regexps
    delete data->name_filter;
    data->name_filter = nullptr;

    if (data->dialog->defaults.default_profile.content_search)
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
    // update the progress bar
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (data->dialog->priv->pbar));

    g_mutex_lock (&data->pdata.mutex);

    GList *files = data->pdata.files;
    data->pdata.files = nullptr;

    // update status bar with the latest message
    data->set_statusmsg(data->pdata.msg);

    g_mutex_unlock (&data->pdata.mutex);

    if (files)
    {
        GnomeCmdFileList *fl = data->dialog->priv->result_list;

        // add all files found since last update to the list
        for (GList *i = files; i; i = i->next)
            fl->append_file(GNOME_CMD_FILE (i->data));

        gnome_cmd_file_list_free (files);
    }

    if ((!data->search_done && !data->stopped) || data->pdata.files)
        return TRUE;

    if (!data->dialog_destroyed)
    {
        int matches = data->dialog->priv->result_list->size();

        gchar *fmt = data->stopped ? ngettext("Found %d match — search aborted", "Found %d matches — search aborted", matches) :
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
            GnomeCmdFileList *flmatches = data->dialog->priv->result_list;
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
gboolean GnomeCmdSearchDialog::Private::join_thread_func (GnomeCmdSearchDialog::Private *priv)
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

    delete priv;

    return FALSE;
}


gboolean SearchData::StartGenericSearch()
{
    // create an regex for file name matching
    name_filter = new Filter(dialog->defaults.default_profile.filename_pattern.c_str(), dialog->defaults.default_profile.match_case, dialog->defaults.default_profile.syntax);

    // if we're going to search through file content create an regex for that too
    if (dialog->defaults.default_profile.content_search)
    {
        content_regex = g_new0 (regex_t, 1);
        regcomp (content_regex, dialog->defaults.default_profile.text_pattern.c_str(), dialog->defaults.default_profile.match_case ? 0 : REG_ICASE);
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
    gchar *file_pattern_utf8 = g_strdup (dialog->defaults.default_profile.filename_pattern.c_str());
    GError *error = nullptr;

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
        gnome_cmd_error_message (file_pattern_utf8, error);
        g_free (file_pattern_utf8);
        return nullptr;
    }

    gchar *text_pattern_quoted = nullptr;
    if (dialog->defaults.default_profile.content_search)
    {
        const gchar *text_pattern_utf8 = dialog->defaults.default_profile.text_pattern.c_str();
        gchar *text_pattern_locale = g_locale_from_utf8 (text_pattern_utf8, -1, nullptr, nullptr, &error);

        if (!text_pattern_locale)
        {
            gnome_cmd_error_message (text_pattern_utf8, error);
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
    if (dialog->defaults.default_profile.max_depth!=-1)
        g_string_append_printf (command, " -maxdepth %i", dialog->defaults.default_profile.max_depth+1);

    switch (dialog->defaults.default_profile.syntax)
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

    if (dialog->defaults.default_profile.content_search)
    {
        static const gchar GREP_COMMAND[] = "grep";

        if (dialog->defaults.default_profile.match_case)
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
                else
                    if (status == G_IO_STATUS_AGAIN)
                        while (gtk_events_pending ())
                        {
                            if (data->stopped)
                            {
                                broken_pipe = TRUE;
                                break;
                            }

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
                    {
                        broken_pipe = TRUE;
                        break;
                    }

                    gtk_main_iteration ();
                }

                if (broken_pipe)
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
        gnome_cmd_error_message (_("Error parsing the search command."), error);

        g_free (command);
        g_strfreev (argv);

        return FALSE;
    }

    g_free (command);

    if (!g_spawn_async_with_pipes (nullptr, argv, nullptr, GSpawnFlags (G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL), child_command_set_pgid_cb, nullptr, &pid, nullptr, &child_stdout, nullptr, &error))
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

    g_io_channel_set_encoding (ioc_stdout, nullptr, nullptr);
    g_io_channel_set_flags (ioc_stdout, G_IO_FLAG_NONBLOCK, nullptr);
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

    dialog->priv->result_list->clear();
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
                if (dialog->priv->data.cancellable)
                    g_cancellable_cancel (dialog->priv->data.cancellable);
#ifdef G_OS_UNIX
                if (dialog->priv->data.pid)
                    kill (dialog->priv->data.pid, SIGTERM);
#endif
            }
            break;

        case GCMD_RESPONSE_FIND:
            {
                SearchData &data = dialog->priv->data;

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

                data.context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (dialog->priv->statusbar), "info");
                data.content_regex = nullptr;
                data.match_dirs = nullptr;

                auto uriString = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->priv->dir_browser));
                auto gUri = g_uri_parse (uriString, G_URI_FLAGS_NONE, nullptr);

                g_free (uriString);

                auto dirPathString = g_uri_get_path (gUri);

                GnomeCmdCon *con = gnome_cmd_dir_get_connection (data.start_dir);

                if (strncmp(dirPathString, gnome_cmd_con_get_root_path (con), con->root_path->len) != 0)
                {
                    if (!g_strcmp0(g_uri_get_scheme (gUri), "file") != 0)
                    {
                        g_uri_unref (gUri);
                        gnome_cmd_show_message (*dialog, stringify(g_strdup_printf (_("Failed to change directory outside of %s"),
                                                                                              gnome_cmd_con_get_root_path (con))));
                        break;
                    }
                    else
                        data.start_dir = gnome_cmd_dir_new (get_home_con (), gnome_cmd_con_create_path (get_home_con (), dirPathString));
                }
                else
                    data.start_dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, dirPathString + con->root_path->len));

                g_uri_unref (gUri);

                gnome_cmd_dir_ref (data.start_dir);

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

                dialog->priv->result_list->clear();

                gchar *base_dir_utf8 = GNOME_CMD_FILE (data.start_dir)->get_real_path();
                dialog->priv->result_list->set_base_dir(base_dir_utf8);

                if (gnome_cmd_con_is_local (con) ? data.StartLocalSearch() : data.StartGenericSearch())
                {
                    data.set_statusmsg();
                    gtk_widget_show (dialog->priv->pbar);
                    data.update_gui_timeout_id = g_timeout_add (gnome_cmd_data.gui_update_rate, (GSourceFunc) update_search_status_widgets, &data);

                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_GOTO, FALSE);
                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_STOP, TRUE);
                    gtk_dialog_set_response_sensitive (*dialog, GCMD_RESPONSE_FIND, FALSE);
                    gtk_dialog_set_default_response (*dialog, GCMD_RESPONSE_STOP);
                }

                gnome_cmd_dir_unref (data.start_dir);
            }
            break;

        case GCMD_RESPONSE_GOTO:
            {
                GnomeCmdFile *f = dialog->priv->result_list->get_selected_file();

                if (!f)
                    break;

                GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
                GnomeCmdCon *con = fs->get_connection();

                gchar *fpath = f->GetPathStringThroughParent();
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

#if defined (__GNUC__) && __GNUC__ >= 7
        __attribute__ ((fallthrough));
#endif
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

    gtk_window_set_title (*dialog, _("Search…"));
    gtk_window_set_resizable (*dialog, TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);

    dialog->priv->vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->priv->vbox), 5);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), dialog->priv->vbox, TRUE, TRUE, 0);


    // file list
    GtkWidget *sw = gtk_scrolled_window_new (nullptr, nullptr);
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
    gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (dialog->priv->pbar), FALSE);
    gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (dialog->priv->pbar), 1.0 / (gdouble) PBAR_MAX);
    gtk_box_pack_start (GTK_BOX (dialog->priv->statusbar),dialog->priv-> pbar, FALSE, TRUE, 0);


    dialog->priv->result_list->update_style();

    gtk_widget_show (dialog->priv->vbox);
    gtk_widget_show_all (sw);
    gtk_widget_show (dialog->priv->statusbar);
    gtk_widget_hide (dialog->priv->pbar);

    g_mutex_init(&dialog->priv->data.pdata.mutex);
}


static void gnome_cmd_search_dialog_finalize (GObject *object)
{
    GnomeCmdSearchDialog *dialog = GNOME_CMD_SEARCH_DIALOG (object);
    SearchData &data = dialog->priv->data;

    if (data.update_gui_timeout_id)
        g_source_remove (data.update_gui_timeout_id);

#ifdef G_OS_UNIX
    if (data.pid)
        kill (data.pid, SIGTERM);
#endif

    // stop and wait for search thread to exit
    data.stopped = TRUE;
    data.dialog_destroyed = TRUE;
    if (data.cancellable)
        g_cancellable_cancel (data.cancellable);
    g_timeout_add (1, (GSourceFunc) GnomeCmdSearchDialog::Private::join_thread_func, dialog->priv);

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

void GnomeCmdSearchDialog::update_style()
{
    priv->result_list->update_style();
}

GnomeCmdSearchDialog::GnomeCmdSearchDialog(GnomeCmdData::SearchConfig &cfg): defaults(cfg)
{
    gtk_window_set_default_size (*this, defaults.width, defaults.height);
    if (gnome_cmd_data.options.search_window_is_transient)
    {
        gtk_window_set_transient_for (*this, *main_win);
    }
    else
    {
        gtk_window_set_type_hint (*this, GDK_WINDOW_TYPE_HINT_NORMAL);
    }

    GtkWidget *button = priv->create_button_with_menu(_("Profiles…"), cfg);

    gtk_dialog_add_action_widget (*this, button, GCMD_RESPONSE_PROFILES);

    gtk_dialog_add_buttons (*this,
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                            GTK_STOCK_JUMP_TO, GCMD_RESPONSE_GOTO,
                            GTK_STOCK_STOP, GCMD_RESPONSE_STOP,
                            GTK_STOCK_FIND, GCMD_RESPONSE_FIND,
                            nullptr);

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
